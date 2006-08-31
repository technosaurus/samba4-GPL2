/* 
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak 2006
   
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
#include "lib/cmdline/popt_common.h"
#include "lib/events/events.h"
#include "auth/credentials/credentials.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "libcli/security/security.h"
#include "librpc/rpc/dcerpc.h"
#include "torture/torture.h"
#include "torture/rpc/rpc.h"


static BOOL test_opendomain_samr(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle, struct lsa_String *domname)
{
	NTSTATUS status;
	struct policy_handle h, domain_handle;
	struct samr_Connect r1;
	struct samr_LookupDomain r2;
	struct samr_OpenDomain r3;
	
	printf("connecting\n");
	
	r1.in.system_name = 0;
	r1.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r1.out.connect_handle = &h;
	
	status = dcerpc_samr_Connect(p, mem_ctx, &r1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Connect failed - %s\n", nt_errstr(status));
		return False;
	}
	
	r2.in.connect_handle = &h;
	r2.in.domain_name = domname;

	printf("domain lookup on %s\n", domname->string);

	status = dcerpc_samr_LookupDomain(p, mem_ctx, &r2);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupDomain failed - %s\n", nt_errstr(status));
		return False;
	}

	r3.in.connect_handle = &h;
	r3.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r3.in.sid = r2.out.sid;
	r3.out.domain_handle = &domain_handle;

	printf("opening domain\n");

	status = dcerpc_samr_OpenDomain(p, mem_ctx, &r3);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenDomain failed - %s\n", nt_errstr(status));
		return False;
	} else {
		*handle = domain_handle;
	}

	return True;
}


static BOOL test_opendomain_lsa(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				struct policy_handle **handle, struct lsa_String *domname)
{
	NTSTATUS status;
	struct lsa_OpenPolicy2 open;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;

	*handle = talloc_zero(mem_ctx, struct policy_handle);
	if (*handle == NULL) {
		return False;
	}

	ZERO_STRUCT(attr);
	ZERO_STRUCT(qos);

	qos.len                 = 0;
	qos.impersonation_level = 2;
	qos.context_mode        = 1;
	qos.effective_only      = 0;
	
	attr.sec_qos = &qos;

	open.in.system_name = domname->string;
	open.in.attr        = &attr;
	open.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	open.out.handle     = *handle;
	
	status = dcerpc_lsa_OpenPolicy2(p, mem_ctx, &open);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	return True;
}


BOOL torture_domain_open_lsa(struct torture_context *torture)
{
	NTSTATUS status;
	struct libnet_context *ctx;
	struct libnet_DomainOpen r;
	struct dcerpc_binding *binding;
	const char *bindstr;
	
	bindstr = lp_parm_string(-1, "torture", "binding");
	status = dcerpc_parse_binding(torture, bindstr, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("failed to parse binding string\n");
		return False;
	}

	ctx = libnet_context_init(NULL);
	if (ctx == NULL) {
		d_printf("failed to create libnet context\n");
		return False;
	}

	ctx->cred = cmdline_credentials;

	r.in.type = DOMAIN_LSA;
	r.in.domain_name = binding->host;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;

	status = libnet_DomainOpen(ctx, torture, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("failed to open domain on lsa service: %s\n", nt_errstr(status));
		return False;
	}

	talloc_free(ctx);

	return True;
}


BOOL torture_domain_close_lsa(struct torture_context *torture)
{
	BOOL ret;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_context *ctx;
	struct lsa_String domain_name;
	struct dcerpc_binding *binding;
	const char *bindstr;
	struct policy_handle *h;
	struct dcerpc_pipe *p;
	struct libnet_DomainClose r;
	struct lsa_QueryInfoPolicy2 r2;

	bindstr = lp_parm_string(-1, "torture", "binding");
	status = dcerpc_parse_binding(torture, bindstr, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("failed to parse binding string\n");
		return False;
	}

	mem_ctx = talloc_init("torture_domain_close_lsa");
	ctx = libnet_context_init(NULL);
	if (ctx == NULL) {
		d_printf("failed to create libnet context\n");
		ret = False;
		goto done;
	}

	ctx->cred = cmdline_credentials;

	status = torture_rpc_connection(mem_ctx,
					&p,
					&dcerpc_table_lsarpc);
	if (!NT_STATUS_IS_OK(status)) {
		ret = False;
		goto done;
	}

	domain_name.string = lp_workgroup();
	if (!test_opendomain_lsa(p, mem_ctx, &h, &domain_name)) {
		ret = False;
		goto done;
	}
	
	/* simulate opening by means of libnet api functions */
	ctx->lsa.pipe   = p;
	ctx->lsa.name   = domain_name.string;
	ctx->lsa.handle = *h;

	r2.in.handle = &ctx->lsa.handle;
	r2.in.level  = 1;
	
	status = dcerpc_lsa_QueryInfoPolicy2(ctx->lsa.pipe, mem_ctx, &r2);
	
	r.in.type = DOMAIN_LSA;
	r.in.domain_name = domain_name.string;
	
	status = libnet_DomainClose(ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		ret = False;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	talloc_free(ctx);
	return ret;
}


BOOL torture_domain_open_samr(struct torture_context *torture)
{
	NTSTATUS status;
	const char *binding;
	struct libnet_context *ctx;
	struct event_context *evt_ctx;
	TALLOC_CTX *mem_ctx;
	struct policy_handle domain_handle, handle;
	struct lsa_String name;
	struct libnet_DomainOpen io;
	struct samr_Close r;
	BOOL ret = True;

	mem_ctx = talloc_init("test_domainopen_lsa");
	binding = lp_parm_string(-1, "torture", "binding");

	evt_ctx = event_context_find(torture);
	ctx = libnet_context_init(evt_ctx);

	name.string = lp_workgroup();

	/*
	 * Testing synchronous version
	 */
	printf("opening domain\n");
	
	io.in.type         = DOMAIN_SAMR;
	io.in.domain_name  = name.string;
	io.in.access_mask  = SEC_FLAG_MAXIMUM_ALLOWED;

	status = libnet_DomainOpen(ctx, mem_ctx, &io);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Composite domain open failed - %s\n", nt_errstr(status));
		ret = False;
		goto done;
	}

	domain_handle = io.out.domain_handle;

	r.in.handle   = &domain_handle;
	r.out.handle  = &handle;
	
	printf("closing domain handle\n");
	
	status = dcerpc_samr_Close(ctx->samr.pipe, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Close failed - %s\n", nt_errstr(status));
		ret = False;
		goto done;
	}

done:
	talloc_free(mem_ctx);

	return ret;
}