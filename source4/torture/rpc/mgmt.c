/* 
   Unix SMB/CIFS implementation.
   test suite for mgmt rpc operations

   Copyright (C) Andrew Tridgell 2003
   
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


/*
  ask the server what interface IDs are available on this endpoint
*/
static BOOL test_inq_if_ids(struct dcerpc_pipe *p, 
			    TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_inq_if_ids r;
	int i;
	
	status = dcerpc_mgmt_inq_if_ids(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("inq_if_ids failed - %s\n", nt_errstr(status));
		return False;
	}

	if (!W_ERROR_IS_OK(r.out.result)) {
		printf("inq_if_ids gave error code %s\n", win_errstr(r.out.result));
		return False;
	}

	if (!r.out.if_id_vector) {
		printf("inq_if_ids gave NULL if_id_vector\n");
		return False;
	}

	for (i=0;i<r.out.if_id_vector->count;i++) {
		struct dcerpc_syntax_id *id = r.out.if_id_vector->if_id[i].id;
		if (!id) continue;
		printf("\tuuid %s  version 0x%04x:0x%04x\n",
		       GUID_string(mem_ctx, &id->uuid),
		       id->major_version, id->minor_version);
	}

	return True;
}

static BOOL test_inq_stats(struct dcerpc_pipe *p, 
			   TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_inq_stats r;

	r.in.max_count = MGMT_STATS_ARRAY_MAX_SIZE;
	r.in.unknown = 0;

	status = dcerpc_mgmt_inq_stats(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("inq_stats failed - %s\n", nt_errstr(status));
		return False;
	}

	if (r.out.statistics.count != MGMT_STATS_ARRAY_MAX_SIZE) {
		printf("Unexpected array size %d\n", r.out.statistics.count);
		return False;
	}

	printf("\tcalls_in %6d  calls_out %6d\n\tpkts_in  %6d  pkts_out  %6d\n",
	       r.out.statistics.statistics[MGMT_STATS_CALLS_IN],
	       r.out.statistics.statistics[MGMT_STATS_CALLS_OUT],
	       r.out.statistics.statistics[MGMT_STATS_PKTS_IN],
	       r.out.statistics.statistics[MGMT_STATS_PKTS_OUT]);

	return True;
}

static BOOL test_inq_princ_name(struct dcerpc_pipe *p, 
				TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_inq_princ_name r;
	int i;
	BOOL ret = False;

	for (i=0;i<100;i++) {
		r.in.authn_proto = i;  /* DCERPC_AUTH_TYPE_* */
		r.in.princ_name_size = 100;

		status = dcerpc_mgmt_inq_princ_name(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			continue;
		}
		if (W_ERROR_IS_OK(r.out.result)) {
			ret = True;
			printf("\tprinciple name for proto %u is '%s'\n", 
			       i, r.out.princ_name);
		}
	}

	if (!ret) {
		printf("\tno principle names?\n");
	}

	return True;
}

static BOOL test_is_server_listening(struct dcerpc_pipe *p, 
				     TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_is_server_listening r;

	status = dcerpc_mgmt_is_server_listening(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("is_server_listening failed - %s\n", nt_errstr(status));
		return False;
	}

	if (r.out.status != 0 || r.out.result == 0) {
		printf("\tserver is NOT listening\n");
	} else {
		printf("\tserver is listening\n");
	}

	return True;
}

static BOOL test_stop_server_listening(struct dcerpc_pipe *p, 
				       TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_stop_server_listening r;

	status = dcerpc_mgmt_stop_server_listening(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("stop_server_listening failed - %s\n", nt_errstr(status));
		return False;
	}

	if (!W_ERROR_IS_OK(r.out.result)) {
		printf("\tserver refused to stop listening - %s\n", win_errstr(r.out.result));
	} else {
		printf("\tserver allowed a stop_server_listening request\n");
		return False;
	}

	return True;
}


BOOL torture_rpc_mgmt(int dummy)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	BOOL ret = True;
	int i;
	char *host = lp_parm_string(-1, "torture", "host");
	uint32 port;

	mem_ctx = talloc_init("torture_rpc_mgmt");

	for (i=0;dcerpc_pipes[i];i++) {		
		char *transport = lp_parm_string(-1, "torture", "transport");

		/* some interfaces are not mappable */
		if (dcerpc_pipes[i]->num_calls == 0 ||
		    strcmp(dcerpc_pipes[i]->name, "mgmt") == 0) {
			continue;
		}

		printf("\nTesting pipe '%s'\n", dcerpc_pipes[i]->name);

		/* on TCP we need to find the right endpoint */
		if (strcasecmp(transport, "ncacn_ip_tcp") == 0) {
			status = dcerpc_epm_map_tcp_port(host, 
							 dcerpc_pipes[i]->uuid, 
							 dcerpc_pipes[i]->if_version, 
							 &port);
			if (!NT_STATUS_IS_OK(status)) {
				ret = False;
				continue;
			}

			lp_set_cmdline("torture:share", 
				       talloc_asprintf(mem_ctx, "%u", port));
		}

		status = torture_rpc_connection(&p, 
						dcerpc_pipes[i]->name,
						DCERPC_MGMT_UUID,
						DCERPC_MGMT_VERSION);
		if (!NT_STATUS_IS_OK(status)) {
			ret = False;
			continue;
		}
	
		p->flags |= DCERPC_DEBUG_PRINT_BOTH;

		if (!test_is_server_listening(p, mem_ctx)) {
			ret = False;
		}

		if (!test_stop_server_listening(p, mem_ctx)) {
			ret = False;
		}

		if (!test_inq_stats(p, mem_ctx)) {
			ret = False;
		}

		if (!test_inq_princ_name(p, mem_ctx)) {
			ret = False;
		}

		if (!test_inq_if_ids(p, mem_ctx)) {
			ret = False;
		}

		torture_rpc_close(p);
	}

	return ret;
}
