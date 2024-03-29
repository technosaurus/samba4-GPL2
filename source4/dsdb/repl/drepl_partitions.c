/* 
   Unix SMB/CIFS mplementation.
   DSDB replication service
   
   Copyright (C) Stefan Metzmacher 2007
    
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
*/

#include "includes.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/events/events.h"
#include "lib/messaging/irpc.h"
#include "dsdb/repl/drepl_service.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"

static WERROR dreplsrv_refresh_partitions(struct dreplsrv_service *s);

WERROR dreplsrv_load_partitions(struct dreplsrv_service *s)
{
	WERROR status;
	struct ldb_dn *basedn;
	struct ldb_result *r;
	struct ldb_message_element *el;
	static const char *attrs[] = { "namingContexts", NULL };
	uint32_t i;
	int ret;

	basedn = ldb_dn_new(s, s->samdb, NULL);
	W_ERROR_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->samdb, basedn, LDB_SCOPE_BASE, 
			 "(objectClass=*)", attrs, &r);
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return WERR_FOOBAR;
	} else if (r->count != 1) {
		talloc_free(r);
		return WERR_FOOBAR;
	}
	talloc_steal(s, r);

	el = ldb_msg_find_element(r->msgs[0], "namingContexts");
	if (!el) {
		return WERR_FOOBAR;
	}

	for (i=0; el && i < el->num_values; i++) {
		const char *v = (const char *)el->values[i].data;
		struct ldb_dn *pdn;
		struct dreplsrv_partition *p;

		pdn = ldb_dn_new(s, s->samdb, v);
		if (!ldb_dn_validate(pdn)) {
			return WERR_FOOBAR;
		}

		p = talloc_zero(s, struct dreplsrv_partition);
		W_ERROR_HAVE_NO_MEMORY(p);

		p->dn = talloc_steal(p, pdn);

		DLIST_ADD(s->partitions, p);

		DEBUG(2, ("dreplsrv_partition[%s] loaded\n", v));
	}

	talloc_free(r);

	status = dreplsrv_refresh_partitions(s);
	W_ERROR_NOT_OK_RETURN(status);

	return WERR_OK;
}

static WERROR dreplsrv_out_connection_attach(struct dreplsrv_service *s,
					     const struct repsFromTo1 *rft,
					     struct dreplsrv_out_connection **_conn)
{
	struct dreplsrv_out_connection *cur, *conn = NULL;
	const char *hostname;

	if (!rft->other_info) {
		return WERR_FOOBAR;
	}

	if (!rft->other_info->dns_name) {
		return WERR_FOOBAR;
	}

	hostname = rft->other_info->dns_name;

	for (cur = s->connections; cur; cur = cur->next) {		
		if (strcmp(cur->binding->host, hostname) == 0) {
			conn = cur;
			break;
		}
	}

	if (!conn) {
		NTSTATUS nt_status;
		char *binding_str;

		conn = talloc_zero(s, struct dreplsrv_out_connection);
		W_ERROR_HAVE_NO_MEMORY(conn);

		conn->service	= s;

		binding_str = talloc_asprintf(conn, "ncacn_ip_tcp:%s[krb5,seal]",
					      hostname);
		W_ERROR_HAVE_NO_MEMORY(binding_str);
		nt_status = dcerpc_parse_binding(conn, binding_str, &conn->binding);
		talloc_free(binding_str);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return ntstatus_to_werror(nt_status);
		}

		DLIST_ADD_END(s->connections, conn, struct dreplsrv_out_connection *);

		DEBUG(2,("dreplsrv_out_connection_attach(%s): create\n", conn->binding->host));
	} else {
		DEBUG(2,("dreplsrv_out_connection_attach(%s): attach\n", conn->binding->host));
	}

	*_conn = conn;
	return WERR_OK;
}

static WERROR dreplsrv_partition_add_source_dsa(struct dreplsrv_service *s,
						struct dreplsrv_partition *p,
						const struct ldb_val *val)
{
	WERROR status;
	NTSTATUS nt_status;
	struct dreplsrv_partition_source_dsa *source;

	source = talloc_zero(p, struct dreplsrv_partition_source_dsa);
	W_ERROR_HAVE_NO_MEMORY(source);

	nt_status = ndr_pull_struct_blob(val, source, &source->_repsFromBlob,
					 (ndr_pull_flags_fn_t)ndr_pull_repsFromToBlob);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return ntstatus_to_werror(nt_status);
	}
	/* NDR_PRINT_DEBUG(repsFromToBlob, &source->_repsFromBlob); */
	if (source->_repsFromBlob.version != 1) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	source->partition	= p;
	source->repsFrom1	= &source->_repsFromBlob.ctr.ctr1;

	status = dreplsrv_out_connection_attach(s, source->repsFrom1, &source->conn);
	W_ERROR_NOT_OK_RETURN(status);

	DLIST_ADD_END(p->sources, source, struct dreplsrv_partition_source_dsa *);
	return WERR_OK;
}

static WERROR dreplsrv_refresh_partition(struct dreplsrv_service *s,
					 struct dreplsrv_partition *p,
					 TALLOC_CTX *mem_ctx)
{
	WERROR status;
	NTSTATUS nt_status;
	const struct ldb_val *ouv_value;
	struct replUpToDateVectorBlob ouv;
	struct dom_sid *nc_sid;
	struct ldb_message_element *orf_el = NULL;
	struct ldb_result *r;
	uint32_t i;
	int ret;
	static const char *attrs[] = {
		"objectSid",
		"objectGUID",
		"replUpToDateVector",
		"repsFrom",
		NULL
	};

	DEBUG(2, ("dreplsrv_refresh_partition(%s)\n",
		ldb_dn_get_linearized(p->dn)));

	ret = ldb_search(s->samdb, p->dn, LDB_SCOPE_BASE,
			 "(objectClass=*)", attrs, &r);
	if (ret != LDB_SUCCESS) {
		return WERR_FOOBAR;
	} else if (r->count != 1) {
		talloc_free(r);
		return WERR_FOOBAR;
	}
	talloc_steal(mem_ctx, r);

	ZERO_STRUCT(p->nc);
	p->nc.dn	= ldb_dn_alloc_linearized(p, p->dn);
	W_ERROR_HAVE_NO_MEMORY(p->nc.dn);
	p->nc.guid	= samdb_result_guid(r->msgs[0], "objectGUID");
	nc_sid		= samdb_result_dom_sid(p, r->msgs[0], "objectSid");
	if (nc_sid) {
		p->nc.sid	= *nc_sid;
	}

	ouv_value = ldb_msg_find_ldb_val(r->msgs[0], "replUpToDateVector");
	if (ouv_value) {
		nt_status = ndr_pull_struct_blob(ouv_value, mem_ctx, &ouv,
						 (ndr_pull_flags_fn_t)ndr_pull_replUpToDateVectorBlob);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return ntstatus_to_werror(nt_status);
		}
		/* NDR_PRINT_DEBUG(replUpToDateVectorBlob, &ouv); */
		if (ouv.version != 2) {
			return WERR_DS_DRA_INTERNAL_ERROR;
		}

		p->uptodatevector.count		= ouv.ctr.ctr2.count;
		p->uptodatevector.reserved	= ouv.ctr.ctr2.reserved;
		p->uptodatevector.cursors	= talloc_steal(p, ouv.ctr.ctr2.cursors);
	}

	/*
	 * TODO: add our own uptodatevector cursor
	 */


	orf_el = ldb_msg_find_element(r->msgs[0], "repsFrom");
	if (orf_el) {
		for (i=0; i < orf_el->num_values; i++) {
			status = dreplsrv_partition_add_source_dsa(s, p, &orf_el->values[i]);
			W_ERROR_NOT_OK_RETURN(status);	
		}
	}

	talloc_free(r);

	return WERR_OK;
}

static WERROR dreplsrv_refresh_partitions(struct dreplsrv_service *s)
{
	WERROR status;
	struct dreplsrv_partition *p;

	for (p = s->partitions; p; p = p->next) {
		status = dreplsrv_refresh_partition(s, p, p);
		W_ERROR_NOT_OK_RETURN(status);
	}

	return WERR_OK;
}
