/* 
   Unix SMB/CIFS implementation.
   remote dcerpc operations

   Copyright (C) Stefan (metze) Metzmacher 2004
   
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
#include "rpc_server/dcerpc_server.h"
#include "auth/auth.h"
#include "auth/credentials/credentials.h"
#include "librpc/rpc/dcerpc_table.h"


struct dcesrv_remote_private {
	struct dcerpc_pipe *c_pipe;
};

static NTSTATUS remote_op_reply(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	return NT_STATUS_OK;
}

static NTSTATUS remote_op_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface)
{
        NTSTATUS status;
		const struct dcerpc_interface_table *table;
        struct dcesrv_remote_private *private;
	const char *binding = lp_parm_string(-1, "dcerpc_remote", "binding");
	const char *user, *pass, *domain;
	struct cli_credentials *credentials;
	BOOL machine_account;

	machine_account = lp_parm_bool(-1, "dcerpc_remote", "use_machine_account", False);

	private = talloc(dce_call->conn, struct dcesrv_remote_private);
	if (!private) {
		return NT_STATUS_NO_MEMORY;	
	}
	
	private->c_pipe = NULL;
	dce_call->context->private = private;

	if (!binding) {
		DEBUG(0,("You must specify a DCE/RPC binding string\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	user = lp_parm_string(-1, "dcerpc_remote", "user");
	pass = lp_parm_string(-1, "dcerpc_remote", "password");
	domain = lp_parm_string(-1, "dceprc_remote", "domain");

	table = idl_iface_by_uuid(&iface->syntax_id.uuid); /* FIXME: What about if_version ? */
	if (!table) {
		dce_call->fault_code = DCERPC_FAULT_UNK_IF;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	if (user && pass) {
		DEBUG(5, ("dcerpc_remote: RPC Proxy: Using specified account\n"));
		credentials = cli_credentials_init(private);
		if (!credentials) {
			return NT_STATUS_NO_MEMORY;
		}
		cli_credentials_set_conf(credentials);
		cli_credentials_set_username(credentials, user, CRED_SPECIFIED);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		cli_credentials_set_password(credentials, pass, CRED_SPECIFIED);
	} else if (machine_account) {
		DEBUG(5, ("dcerpc_remote: RPC Proxy: Using machine account\n"));
		credentials = cli_credentials_init(private);
		cli_credentials_set_conf(credentials);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		status = cli_credentials_set_machine_account(credentials);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else if (dce_call->conn->auth_state.session_info->credentials) {
		DEBUG(5, ("dcerpc_remote: RPC Proxy: Using delegated credentials\n"));
		credentials = dce_call->conn->auth_state.session_info->credentials;
	} else {
		DEBUG(1,("dcerpc_remote: RPC Proxy: You must supply binding, user and password or have delegated credentials\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_pipe_connect(private, 
				     &(private->c_pipe), binding, table,
				     credentials, dce_call->event_ctx);

	talloc_free(credentials);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;	
}

static void remote_op_unbind(struct dcesrv_connection_context *context, const struct dcesrv_interface *iface)
{
	struct dcesrv_remote_private *private = context->private;

	talloc_free(private->c_pipe);

	return;	
}

static NTSTATUS remote_op_ndr_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_pull *pull, void **r)
{
	NTSTATUS status;
	const struct dcerpc_interface_table *table = dce_call->context->iface->private;
	uint16_t opnum = dce_call->pkt.u.request.opnum;

	dce_call->fault_code = 0;

	if (opnum >= table->num_calls) {
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	*r = talloc_size(mem_ctx, table->calls[opnum].struct_size);
	if (!*r) {
		return NT_STATUS_NO_MEMORY;
	}

        /* unravel the NDR for the packet */
	status = table->calls[opnum].ndr_pull(pull, NDR_IN, *r);
	if (!NT_STATUS_IS_OK(status)) {
		dcerpc_log_packet(table, opnum, NDR_IN,
				  &dce_call->pkt.u.request.stub_and_verifier);
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

static NTSTATUS remote_op_dispatch(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	struct dcesrv_remote_private *private = dce_call->context->private;
	uint16_t opnum = dce_call->pkt.u.request.opnum;
	const struct dcerpc_interface_table *table = dce_call->context->iface->private;
	const struct dcerpc_interface_call *call;
	const char *name;

	name = table->calls[opnum].name;
	call = &table->calls[opnum];

	if (private->c_pipe->conn->flags & DCERPC_DEBUG_PRINT_IN) {
		ndr_print_function_debug(call->ndr_print, name, NDR_IN | NDR_SET_VALUES, r);		
	}

	private->c_pipe->conn->flags |= DCERPC_NDR_REF_ALLOC;

	/* we didn't use the return code of this function as we only check the last_fault_code */
	dcerpc_ndr_request(private->c_pipe, NULL, table, opnum, mem_ctx,r);

	dce_call->fault_code = private->c_pipe->last_fault_code;
	if (dce_call->fault_code != 0) {
		DEBUG(0,("dcesrv_remote: call[%s] failed with: %s!\n",name, dcerpc_errstr(mem_ctx, dce_call->fault_code)));
		return NT_STATUS_NET_WRITE_FAULT;
	}

	if ((dce_call->fault_code == 0) && 
	    (private->c_pipe->conn->flags & DCERPC_DEBUG_PRINT_OUT)) {
		ndr_print_function_debug(call->ndr_print, name, NDR_OUT, r);		
	}

	return NT_STATUS_OK;
}

static NTSTATUS remote_op_ndr_push(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_push *push, const void *r)
{
	NTSTATUS status;
	const struct dcerpc_interface_table *table = dce_call->context->iface->private;
	uint16_t opnum = dce_call->pkt.u.request.opnum;

        /* unravel the NDR for the packet */
	status = table->calls[opnum].ndr_push(push, NDR_OUT, r);
	if (!NT_STATUS_IS_OK(status)) {
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

static NTSTATUS remote_register_one_iface(struct dcesrv_context *dce_ctx, const struct dcesrv_interface *iface)
{
	int i;
	const struct dcerpc_interface_table *table = iface->private;

	for (i=0;i<table->endpoints->count;i++) {
		NTSTATUS ret;
		const char *name = table->endpoints->names[i];

		ret = dcesrv_interface_register(dce_ctx, name, iface, NULL);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1,("remote_op_init_server: failed to register endpoint '%s'\n",name));
			return ret;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS remote_op_init_server(struct dcesrv_context *dce_ctx, const struct dcesrv_endpoint_server *ep_server)
{
	int i;
	const char **ifaces = str_list_make(dce_ctx, lp_parm_string(-1,"dcerpc_remote","interfaces"),NULL);

	if (!ifaces) {
		DEBUG(3,("remote_op_init_server: no interfaces configured\n"));
		return NT_STATUS_OK;
	}

	for (i=0;ifaces[i];i++) {
		NTSTATUS ret;
		struct dcesrv_interface iface;
		
		if (!ep_server->interface_by_name(&iface, ifaces[i])) {
			DEBUG(0,("remote_op_init_server: failed to find interface = '%s'\n", ifaces[i]));
			talloc_free(ifaces);
			return NT_STATUS_UNSUCCESSFUL;
		}

		ret = remote_register_one_iface(dce_ctx, &iface);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(0,("remote_op_init_server: failed to register interface = '%s'\n", ifaces[i]));
			talloc_free(ifaces);
			return ret;
		}
	}

	talloc_free(ifaces);
	return NT_STATUS_OK;
}

static BOOL remote_fill_interface(struct dcesrv_interface *iface, const struct dcerpc_interface_table *if_tabl)
{
	iface->name = if_tabl->name;
	iface->syntax_id = if_tabl->syntax_id;
	
	iface->bind = remote_op_bind;
	iface->unbind = remote_op_unbind;

	iface->ndr_pull = remote_op_ndr_pull;
	iface->dispatch = remote_op_dispatch;
	iface->reply = remote_op_reply;
	iface->ndr_push = remote_op_ndr_push;

	iface->private = if_tabl;

	return True;
}

static BOOL remote_op_interface_by_uuid(struct dcesrv_interface *iface, const struct GUID *uuid, uint32_t if_version)
{
	const struct dcerpc_interface_list *l;

	for (l=librpc_dcerpc_pipes();l;l=l->next) {
		if (l->table->syntax_id.if_version == if_version &&
			GUID_equal(&l->table->syntax_id.uuid, uuid)==0) {
			return remote_fill_interface(iface, l->table);
		}
	}

	return False;	
}

static BOOL remote_op_interface_by_name(struct dcesrv_interface *iface, const char *name)
{
	const struct dcerpc_interface_table *tbl = idl_iface_by_name(name);

	if (tbl)
		return remote_fill_interface(iface, tbl);

	return False;	
}

NTSTATUS dcerpc_server_remote_init(void)
{
	NTSTATUS ret;
	struct dcesrv_endpoint_server ep_server;

	ZERO_STRUCT(ep_server);

	/* fill in our name */
	ep_server.name = "remote";

	/* fill in all the operations */
	ep_server.init_server = remote_op_init_server;

	ep_server.interface_by_uuid = remote_op_interface_by_uuid;
	ep_server.interface_by_name = remote_op_interface_by_name;

	/* register ourselves with the DCERPC subsystem. */
	ret = dcerpc_register_ep_server(&ep_server);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'remote' endpoint server!\n"));
		return ret;
	}

	/* We need the full DCE/RPC interface table */
	dcerpc_table_init();

	return ret;
}
