/* 
   Unix SMB/CIFS implementation.

   server side dcerpc core code

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
  find the set of endpoint operations for an endpoint server
*/
static const struct dcesrv_endpoint_ops *find_endpoint(struct server_context *smb,
						       const struct dcesrv_endpoint *endpoint)
{
	struct dce_endpoint *ep;
	for (ep=smb->dcesrv.endpoint_list; ep; ep=ep->next) {
		if (ep->endpoint_ops->query_endpoint(endpoint)) {
			return ep->endpoint_ops;
		}
	}
	return NULL;
}

/*
  find a call that is pending in our call list
*/
static struct dcesrv_call_state *dcesrv_find_call(struct dcesrv_state *dce, uint16 call_id)
{
	struct dcesrv_call_state *c;
	for (c=dce->call_list;c;c=c->next) {
		if (c->pkt.call_id == call_id) {
			return c;
		}
	}
	return NULL;
}

/*
  register an endpoint server
*/
BOOL dcesrv_endpoint_register(struct server_context *smb, 
			      const struct dcesrv_endpoint_ops *ops)
{
	struct dce_endpoint *ep;
	ep = malloc(sizeof(*ep));
	if (!ep) {
		return False;
	}
	ep->endpoint_ops = ops;
	DLIST_ADD(smb->dcesrv.endpoint_list, ep);
	return True;
}

/*
  connect to a dcerpc endpoint
*/
NTSTATUS dcesrv_endpoint_connect(struct server_context *smb,
				 const struct dcesrv_endpoint *endpoint,
				 struct dcesrv_state **p)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	const struct dcesrv_endpoint_ops *ops;

	/* make sure this endpoint exists */
	ops = find_endpoint(smb, endpoint);
	if (!ops) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	mem_ctx = talloc_init("dcesrv_endpoint_connect");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	*p = talloc_p(mem_ctx, struct dcesrv_state);
	if (! *p) {
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	(*p)->smb = smb;
	(*p)->mem_ctx = mem_ctx;
	(*p)->endpoint = *endpoint;
	(*p)->ops = ops;
	(*p)->private = NULL;
	(*p)->call_list = NULL;
	(*p)->cli_max_recv_frag = 0;
	(*p)->ndr = NULL;
	(*p)->dispatch = NULL;
	(*p)->handles = NULL;
	(*p)->next_handle = 0;

	/* make sure the endpoint server likes the connection */
	status = ops->connect(*p);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_destroy(mem_ctx);
		return status;
	}
	
	return NT_STATUS_OK;
}


/*
  disconnect a link to an endpoint
*/
void dcesrv_endpoint_disconnect(struct dcesrv_state *p)
{
	p->ops->disconnect(p);

	/* destroy any handles */
	while (p->handles) {
		TALLOC_CTX *m = p->handles->mem_ctx;
		DLIST_REMOVE(p->handles, p->handles);
		talloc_destroy(m);
	}
	
	talloc_destroy(p->mem_ctx);
}

/*
  return a dcerpc fault
*/
static NTSTATUS dcesrv_fault(struct dcesrv_call_state *call, uint32 fault_code)
{
	struct ndr_push *push;
	struct dcerpc_packet pkt;
	struct dcesrv_call_reply *rep;
	NTSTATUS status;

	/* setup a bind_ack */
	pkt.rpc_vers = 5;
	pkt.rpc_vers_minor = 0;
	pkt.drep[0] = 0x10; /* Little endian */
	pkt.drep[1] = 0;
	pkt.drep[2] = 0;
	pkt.drep[3] = 0;
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_FAULT;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.fault.alloc_hint = 0;
	pkt.u.fault.context_id = 0;
	pkt.u.fault.cancel_count = 0;
	pkt.u.fault.status = fault_code;

	/* now form the NDR for the fault */
	push = ndr_push_init_ctx(call->mem_ctx);
	if (!push) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ndr_push_dcerpc_packet(push, NDR_SCALARS|NDR_BUFFERS, &pkt);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	rep = talloc_p(call->mem_ctx, struct dcesrv_call_reply);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	rep->data = ndr_push_blob(push);
	SSVAL(rep->data.data,  DCERPC_FRAG_LEN_OFFSET, rep->data.length);

	DLIST_ADD_END(call->replies, rep, struct dcesrv_call_reply *);

	return NT_STATUS_OK;	
}


/*
  return a dcerpc fault from a ntstatus code
*/
static NTSTATUS dcesrv_fault_nt(struct dcesrv_call_state *call, NTSTATUS status)
{
	uint32 fault_code = DCERPC_FAULT_OTHER;

	/* TODO: we need to expand this table to include more mappings */
	if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_HANDLE)) {
		fault_code = DCERPC_FAULT_CONTEXT_MISMATCH;
	}

	return dcesrv_fault(call, fault_code);
}


/*
  return a dcerpc bind_nak
*/
static NTSTATUS dcesrv_bind_nak(struct dcesrv_call_state *call, uint32 reason)
{
	struct ndr_push *push;
	struct dcerpc_packet pkt;
	struct dcesrv_call_reply *rep;
	NTSTATUS status;

	/* setup a bind_ack */
	pkt.rpc_vers = 5;
	pkt.rpc_vers_minor = 0;
	pkt.drep[0] = 0x10; /* Little endian */
	pkt.drep[1] = 0;
	pkt.drep[2] = 0;
	pkt.drep[3] = 0;
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_BIND_NAK;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.bind_nak.reject_reason = reason;
	pkt.u.bind_nak.num_versions = 0;

	/* now form the NDR for the bind_nak */
	push = ndr_push_init_ctx(call->mem_ctx);
	if (!push) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ndr_push_dcerpc_packet(push, NDR_SCALARS|NDR_BUFFERS, &pkt);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	rep = talloc_p(call->mem_ctx, struct dcesrv_call_reply);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	rep->data = ndr_push_blob(push);
	SSVAL(rep->data.data,  DCERPC_FRAG_LEN_OFFSET, rep->data.length);

	DLIST_ADD_END(call->replies, rep, struct dcesrv_call_reply *);

	return NT_STATUS_OK;	
}


/*
  handle a bind request
*/
static NTSTATUS dcesrv_bind(struct dcesrv_call_state *call)
{
	const char *uuid, *transfer_syntax;
	uint32 if_version, transfer_syntax_version;
	struct dcerpc_packet pkt;
	struct ndr_push *push;
	struct dcesrv_call_reply *rep;
	NTSTATUS status;
	uint32 result=0, reason=0;

	if (call->pkt.u.bind.num_contexts != 1 ||
	    call->pkt.u.bind.ctx_list[0].num_transfer_syntaxes < 1) {
		return dcesrv_bind_nak(call, 0);
	}

	if_version = call->pkt.u.bind.ctx_list[0].abstract_syntax.major_version;
	uuid = GUID_string(call->mem_ctx, &call->pkt.u.bind.ctx_list[0].abstract_syntax.uuid);
	if (!uuid) {
		return dcesrv_bind_nak(call, 0);
	}

	transfer_syntax_version = call->pkt.u.bind.ctx_list[0].transfer_syntaxes[0].major_version;
	transfer_syntax = GUID_string(call->mem_ctx, 
				      &call->pkt.u.bind.ctx_list[0].transfer_syntaxes[0].uuid);
	if (!transfer_syntax ||
	    strcasecmp(NDR_GUID, transfer_syntax) != 0 ||
	    NDR_GUID_VERSION != transfer_syntax_version) {
		/* we only do NDR encoded dcerpc */
		return dcesrv_bind_nak(call, 0);
	}

	if (!call->dce->ops->set_interface(call->dce, uuid, if_version)) {
		DEBUG(2,("Request for unknown dcerpc interface %s/%d\n", uuid, if_version));
		/* we don't know about that interface */
		result = DCERPC_BIND_PROVIDER_REJECT;
		reason = DCERPC_BIND_REASON_ASYNTAX;
	}

	if (call->dce->cli_max_recv_frag == 0) {
		call->dce->cli_max_recv_frag = call->pkt.u.bind.max_recv_frag;
	}

	/* setup a bind_ack */
	pkt.rpc_vers = 5;
	pkt.rpc_vers_minor = 0;
	pkt.drep[0] = 0x10; /* Little endian */
	pkt.drep[1] = 0;
	pkt.drep[2] = 0;
	pkt.drep[3] = 0;
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_BIND_ACK;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.bind_ack.max_xmit_frag = 0x2000;
	pkt.u.bind_ack.max_recv_frag = 0x2000;
	pkt.u.bind_ack.assoc_group_id = call->pkt.u.bind.assoc_group_id;
	if (call->dce->ndr) {
		pkt.u.bind_ack.secondary_address = talloc_asprintf(call->mem_ctx, "\\PIPE\\%s", 
								   call->dce->ndr->name);
	} else {
		pkt.u.bind_ack.secondary_address = "";
	}
	pkt.u.bind_ack.num_results = 1;
	pkt.u.bind_ack.ctx_list = talloc_p(call->mem_ctx, struct dcerpc_ack_ctx);
	if (!pkt.u.bind_ack.ctx_list) {
		return NT_STATUS_NO_MEMORY;
	}
	pkt.u.bind_ack.ctx_list[0].result = result;
	pkt.u.bind_ack.ctx_list[0].reason = reason;
	GUID_from_string(uuid, &pkt.u.bind_ack.ctx_list[0].syntax.uuid);
	pkt.u.bind_ack.ctx_list[0].syntax.major_version = if_version;
	pkt.u.bind_ack.ctx_list[0].syntax.minor_version = 0;
	pkt.u.bind_ack.auth_info = data_blob(NULL, 0);

	/* now form the NDR for the bind_ack */
	push = ndr_push_init_ctx(call->mem_ctx);
	if (!push) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ndr_push_dcerpc_packet(push, NDR_SCALARS|NDR_BUFFERS, &pkt);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	rep = talloc_p(call->mem_ctx, struct dcesrv_call_reply);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	rep->data = ndr_push_blob(push);
	SSVAL(rep->data.data,  DCERPC_FRAG_LEN_OFFSET, rep->data.length);

	DLIST_ADD_END(call->replies, rep, struct dcesrv_call_reply *);

	return NT_STATUS_OK;
}


/*
  handle a dcerpc request packet
*/
static NTSTATUS dcesrv_request(struct dcesrv_call_state *call)
{
	struct ndr_pull *pull;
	struct ndr_push *push;
	uint16 opnum;
	void *r;
	NTSTATUS status;
	DATA_BLOB stub;

	opnum = call->pkt.u.request.opnum;

	if (opnum >= call->dce->ndr->num_calls) {
		return dcesrv_fault(call, DCERPC_FAULT_OP_RNG_ERROR);
	}

	pull = ndr_pull_init_blob(&call->pkt.u.request.stub_and_verifier, call->mem_ctx);
	if (!pull) {
		return NT_STATUS_NO_MEMORY;
	}

	r = talloc(call->mem_ctx, call->dce->ndr->calls[opnum].struct_size);
	if (!r) {
		return NT_STATUS_NO_MEMORY;
	}

	/* unravel the NDR for the packet */
	status = call->dce->ndr->calls[opnum].ndr_pull(pull, NDR_IN, r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault(call, DCERPC_FAULT_NDR);
	}

	/* call the dispatch function */
	status = call->dce->dispatch[opnum](call->dce, call->mem_ctx, r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault_nt(call, status);
	}

	/* form the reply NDR */
	push = ndr_push_init_ctx(call->mem_ctx);
	if (!push) {
		return NT_STATUS_NO_MEMORY;
	}

	status = call->dce->ndr->calls[opnum].ndr_push(push, NDR_OUT, r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault(call, DCERPC_FAULT_NDR);
	}

	stub = ndr_push_blob(push);

	do {
		uint32 length;
		struct dcesrv_call_reply *rep;
		struct dcerpc_packet pkt;

		rep = talloc_p(call->mem_ctx, struct dcesrv_call_reply);
		if (!rep) {
			return NT_STATUS_NO_MEMORY;
		}

		length = stub.length;
		if (length + DCERPC_RESPONSE_LENGTH > call->dce->cli_max_recv_frag) {
			length = call->dce->cli_max_recv_frag - DCERPC_RESPONSE_LENGTH;
		}

		/* form the dcerpc response packet */
		pkt.rpc_vers = 5;
		pkt.rpc_vers_minor = 0;
		pkt.drep[0] = 0x10; /* Little endian */
		pkt.drep[1] = 0;
		pkt.drep[2] = 0;
		pkt.drep[3] = 0;
		pkt.auth_length = 0;
		pkt.call_id = call->pkt.call_id;
		pkt.ptype = DCERPC_PKT_RESPONSE;
		pkt.pfc_flags = 0;
		if (!call->replies) {
			pkt.pfc_flags |= DCERPC_PFC_FLAG_FIRST;
		}
		if (length == stub.length) {
			pkt.pfc_flags |= DCERPC_PFC_FLAG_LAST;
		}
		pkt.u.response.alloc_hint = stub.length;
		pkt.u.response.context_id = call->pkt.u.request.context_id;
		pkt.u.response.cancel_count = 0;
		pkt.u.response.stub_and_verifier.data = stub.data;
		pkt.u.response.stub_and_verifier.length = length;

		push = ndr_push_init_ctx(call->mem_ctx);
		if (!push) {
			return NT_STATUS_NO_MEMORY;
		}
		
		status = ndr_push_dcerpc_packet(push, NDR_SCALARS|NDR_BUFFERS, &pkt);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		
		rep->data = ndr_push_blob(push);
		SSVAL(rep->data.data,  DCERPC_FRAG_LEN_OFFSET, rep->data.length);

		DLIST_ADD_END(call->replies, rep, struct dcesrv_call_reply *);
		
		stub.data += length;
		stub.length -= length;
	} while (stub.length != 0);

	return NT_STATUS_OK;
}


/*
  provide some input to a dcerpc endpoint server. This passes data
  from a dcerpc client into the server
*/
NTSTATUS dcesrv_input(struct dcesrv_state *dce, const DATA_BLOB *data)
{
	struct ndr_pull *ndr;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	struct dcesrv_call_state *call;

	mem_ctx = talloc_init("dcesrv_input");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}
	call = talloc_p(mem_ctx, struct dcesrv_call_state);
	if (!call) {
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	call->mem_ctx = mem_ctx;
	call->dce = dce;
	call->replies = NULL;

	ndr = ndr_pull_init_blob(data, mem_ctx);
	if (!ndr) {
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	status = ndr_pull_dcerpc_packet(ndr, NDR_SCALARS|NDR_BUFFERS, &call->pkt);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_destroy(mem_ctx);
		return status;
	}

	/* see if this is a continued packet */
	if (!(call->pkt.pfc_flags & DCERPC_PFC_FLAG_FIRST)) {
		struct dcesrv_call_state *call2 = call;
		uint32 alloc_size;

		/* we only allow fragmented requests, no other packet types */
		if (call->pkt.ptype != DCERPC_PKT_REQUEST) {
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}

		/* this is a continuation of an existing call - find the call then
		   tack it on the end */
		call = dcesrv_find_call(dce, call2->pkt.call_id);
		if (!call) {
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}

		if (call->pkt.ptype != call2->pkt.ptype) {
			/* trying to play silly buggers are we? */
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}

		alloc_size = call->pkt.u.request.stub_and_verifier.length +
			call2->pkt.u.request.stub_and_verifier.length;
		if (call->pkt.u.request.alloc_hint > alloc_size) {
			alloc_size = call->pkt.u.request.alloc_hint;
		}

		call->pkt.u.request.stub_and_verifier.data = 
			talloc_realloc(call->mem_ctx,
				       call->pkt.u.request.stub_and_verifier.data, alloc_size);
		if (!call->pkt.u.request.stub_and_verifier.data) {
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}
		memcpy(call->pkt.u.request.stub_and_verifier.data +
		       call->pkt.u.request.stub_and_verifier.length,
		       call2->pkt.u.request.stub_and_verifier.data,
		       call2->pkt.u.request.stub_and_verifier.length);
		call->pkt.u.request.stub_and_verifier.length += 
			call2->pkt.u.request.stub_and_verifier.length;

		call->pkt.pfc_flags |= (call2->pkt.pfc_flags & DCERPC_PFC_FLAG_LAST);
	}

	/* this may not be the last pdu in the chain - if its isn't then
	   just put it on the call_list and wait for the rest */
	if (!(call->pkt.pfc_flags & DCERPC_PFC_FLAG_LAST)) {
		DLIST_ADD_END(dce->call_list, call, struct dcesrv_call_state *);
		return NT_STATUS_OK;
	}

	switch (call->pkt.ptype) {
	case DCERPC_PKT_BIND:
		status = dcesrv_bind(call);
		break;
	case DCERPC_PKT_REQUEST:
		status = dcesrv_request(call);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	/* if we are going to be sending a reply then add
	   it to the list of pending calls. We add it to the end to keep the call
	   list in the order we will answer */
	if (NT_STATUS_IS_OK(status)) {
		DLIST_ADD_END(dce->call_list, call, struct dcesrv_call_state *);
	} else {
		talloc_destroy(mem_ctx);
	}

	return status;
}

/*
  retrieve some output from a dcerpc server. The amount of data that
  is wanted is in data->length and data->data is already allocated
  to hold that much data.
*/
NTSTATUS dcesrv_output(struct dcesrv_state *dce, DATA_BLOB *data)
{
	struct dcesrv_call_state *call;
	struct dcesrv_call_reply *rep;

	call = dce->call_list;
	if (!call || !call->replies) {
		return NT_STATUS_FOOBAR;
	}
	rep = call->replies;

	if (data->length >= rep->data.length) {
		data->length = rep->data.length;
	}

	memcpy(data->data, rep->data.data, data->length);
	rep->data.length -= data->length;
	rep->data.data += data->length;

	if (rep->data.length == 0) {
		/* we're done with this section of the call */
		DLIST_REMOVE(call->replies, rep);
	}

	if (call->replies == NULL) {
		/* we're done with the whole call */
		DLIST_REMOVE(dce->call_list, call);
		talloc_destroy(call->mem_ctx);
	}

	return NT_STATUS_OK;
}


/*
  a useful function for implementing the query endpoint op
 */
BOOL dcesrv_table_query(const struct dcerpc_interface_table *table,
			const struct dcesrv_endpoint *ep)
{
	int i;
	const struct dcerpc_endpoint_list *endpoints = table->endpoints;

	if (ep->type != ENDPOINT_SMB) {
		return False;
	}

	for (i=0;i<endpoints->count;i++) {
		if (strcasecmp(ep->info.smb_pipe, endpoints->names[i]) == 0) {
			return True;
		}
	}
	return False;
}


/*
  a useful function for implementing the lookup_endpoints op
 */
int dcesrv_lookup_endpoints(const struct dcerpc_interface_table *table,
			    TALLOC_CTX *mem_ctx,
			    struct dcesrv_ep_iface **e)
{
	*e = talloc_p(mem_ctx, struct dcesrv_ep_iface);
	if (! *e) {
		return -1;
	}

	(*e)->uuid = table->uuid;
	(*e)->if_version = table->if_version;
	(*e)->endpoint.type = ENDPOINT_SMB;
	(*e)->endpoint.info.smb_pipe = table->endpoints->names[0];

	return 1;
}


/*
  initialise the dcerpc server subsystem
*/
BOOL dcesrv_init(struct server_context *smb)
{
	rpc_echo_init(smb);
	rpc_epmapper_init(smb);
	return True;
}
