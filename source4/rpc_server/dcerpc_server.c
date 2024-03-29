/* 
   Unix SMB/CIFS implementation.

   server side dcerpc core code

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Stefan (metze) Metzmacher 2004-2005
   
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
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "auth/auth.h"
#include "auth/gensec/gensec.h"
#include "lib/util/dlinklist.h"
#include "rpc_server/dcerpc_server.h"
#include "lib/events/events.h"
#include "smbd/service_task.h"
#include "smbd/service_stream.h"
#include "smbd/service.h"
#include "system/filesys.h"
#include "libcli/security/security.h"
#include "build.h"

extern const struct dcesrv_interface dcesrv_mgmt_interface;

/*
  see if two endpoints match
*/
static BOOL endpoints_match(const struct dcerpc_binding *ep1,
			    const struct dcerpc_binding *ep2)
{
	if (ep1->transport != ep2->transport) {
		return False;
	}

	if (!ep1->endpoint || !ep2->endpoint) {
		return ep1->endpoint == ep2->endpoint;
	}

	if (strcasecmp(ep1->endpoint, ep2->endpoint) != 0) 
		return False;

	return True;
}

/*
  find an endpoint in the dcesrv_context
*/
static struct dcesrv_endpoint *find_endpoint(struct dcesrv_context *dce_ctx,
					     const struct dcerpc_binding *ep_description)
{
	struct dcesrv_endpoint *ep;
	for (ep=dce_ctx->endpoint_list; ep; ep=ep->next) {
		if (endpoints_match(ep->ep_description, ep_description)) {
			return ep;
		}
	}
	return NULL;
}

/*
  find a registered context_id from a bind or alter_context
*/
static struct dcesrv_connection_context *dcesrv_find_context(struct dcesrv_connection *conn, 
								   uint32_t context_id)
{
	struct dcesrv_connection_context *c;
	for (c=conn->contexts;c;c=c->next) {
		if (c->context_id == context_id) return c;
	}
	return NULL;
}

/*
  see if a uuid and if_version match to an interface
*/
static BOOL interface_match(const struct dcesrv_interface *if1,
							const struct dcesrv_interface *if2)
{
	return (if1->syntax_id.if_version == if2->syntax_id.if_version && 
			GUID_equal(&if1->syntax_id.uuid, &if2->syntax_id.uuid));
}

/*
  find the interface operations on an endpoint
*/
static const struct dcesrv_interface *find_interface(const struct dcesrv_endpoint *endpoint,
						     const struct dcesrv_interface *iface)
{
	struct dcesrv_if_list *ifl;
	for (ifl=endpoint->interface_list; ifl; ifl=ifl->next) {
		if (interface_match(&(ifl->iface), iface)) {
			return &(ifl->iface);
		}
	}
	return NULL;
}

/*
  see if a uuid and if_version match to an interface
*/
static BOOL interface_match_by_uuid(const struct dcesrv_interface *iface,
				    const struct GUID *uuid, uint32_t if_version)
{
	return (iface->syntax_id.if_version == if_version && 
			GUID_equal(&iface->syntax_id.uuid, uuid));
}

/*
  find the interface operations on an endpoint by uuid
*/
static const struct dcesrv_interface *find_interface_by_uuid(const struct dcesrv_endpoint *endpoint,
							     const struct GUID *uuid, uint32_t if_version)
{
	struct dcesrv_if_list *ifl;
	for (ifl=endpoint->interface_list; ifl; ifl=ifl->next) {
		if (interface_match_by_uuid(&(ifl->iface), uuid, if_version)) {
			return &(ifl->iface);
		}
	}
	return NULL;
}

/*
  find the earlier parts of a fragmented call awaiting reassembily
*/
static struct dcesrv_call_state *dcesrv_find_fragmented_call(struct dcesrv_connection *dce_conn, uint16_t call_id)
{
	struct dcesrv_call_state *c;
	for (c=dce_conn->incoming_fragmented_call_list;c;c=c->next) {
		if (c->pkt.call_id == call_id) {
			return c;
		}
	}
	return NULL;
}

/*
  register an interface on an endpoint
*/
_PUBLIC_ NTSTATUS dcesrv_interface_register(struct dcesrv_context *dce_ctx,
				   const char *ep_name,
				   const struct dcesrv_interface *iface,
				   const struct security_descriptor *sd)
{
	struct dcesrv_endpoint *ep;
	struct dcesrv_if_list *ifl;
	struct dcerpc_binding *binding;
	BOOL add_ep = False;
	NTSTATUS status;
	
	status = dcerpc_parse_binding(dce_ctx, ep_name, &binding);

	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0, ("Trouble parsing binding string '%s'\n", ep_name));
		return status;
	}

	/* check if this endpoint exists
	 */
	if ((ep=find_endpoint(dce_ctx, binding))==NULL) {
		ep = talloc(dce_ctx, struct dcesrv_endpoint);
		if (!ep) {
			return NT_STATUS_NO_MEMORY;
		}
		ZERO_STRUCTP(ep);
		ep->ep_description = talloc_reference(ep, binding);
		add_ep = True;

		/* add mgmt interface */
		ifl = talloc(dce_ctx, struct dcesrv_if_list);
		if (!ifl) {
			return NT_STATUS_NO_MEMORY;
		}

		memcpy(&(ifl->iface), &dcesrv_mgmt_interface, 
			   sizeof(struct dcesrv_interface));

		DLIST_ADD(ep->interface_list, ifl);
	}

	/* see if the interface is already registered on te endpoint */
	if (find_interface(ep, iface)!=NULL) {
		DEBUG(0,("dcesrv_interface_register: interface '%s' already registered on endpoint '%s'\n",
			iface->name, ep_name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	/* talloc a new interface list element */
	ifl = talloc(dce_ctx, struct dcesrv_if_list);
	if (!ifl) {
		return NT_STATUS_NO_MEMORY;
	}

	/* copy the given interface struct to the one on the endpoints interface list */
	memcpy(&(ifl->iface),iface, sizeof(struct dcesrv_interface));

	/* if we have a security descriptor given,
	 * we should see if we can set it up on the endpoint
	 */
	if (sd != NULL) {
		/* if there's currently no security descriptor given on the endpoint
		 * we try to set it
		 */
		if (ep->sd == NULL) {
			ep->sd = security_descriptor_copy(dce_ctx, sd);
		}

		/* if now there's no security descriptor given on the endpoint
		 * something goes wrong, either we failed to copy the security descriptor
		 * or there was already one on the endpoint
		 */
		if (ep->sd != NULL) {
			DEBUG(0,("dcesrv_interface_register: interface '%s' failed to setup a security descriptor\n"
			         "                           on endpoint '%s'\n",
				iface->name, ep_name));
			if (add_ep) free(ep);
			free(ifl);
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	}

	/* finally add the interface on the endpoint */
	DLIST_ADD(ep->interface_list, ifl);

	/* if it's a new endpoint add it to the dcesrv_context */
	if (add_ep) {
		DLIST_ADD(dce_ctx->endpoint_list, ep);
	}

	DEBUG(4,("dcesrv_interface_register: interface '%s' registered on endpoint '%s'\n",
		iface->name, ep_name));

	return NT_STATUS_OK;
}

static NTSTATUS dcesrv_inherited_session_key(struct dcesrv_connection *p,
					      DATA_BLOB *session_key)
{
	if (p->auth_state.session_info->session_key.length) {
		*session_key = p->auth_state.session_info->session_key;
		return NT_STATUS_OK;
	}
	return NT_STATUS_NO_USER_SESSION_KEY;
}

NTSTATUS dcesrv_generic_session_key(struct dcesrv_connection *p,
				    DATA_BLOB *session_key)
{
	/* this took quite a few CPU cycles to find ... */
	session_key->data = discard_const_p(uint8_t, "SystemLibraryDTC");
	session_key->length = 16;
	return NT_STATUS_OK;
}

/*
  fetch the user session key - may be default (above) or the SMB session key
*/
_PUBLIC_ NTSTATUS dcesrv_fetch_session_key(struct dcesrv_connection *p,
				  DATA_BLOB *session_key)
{
	return p->auth_state.session_key(p, session_key);
}


/*
  destroy a link to an endpoint
*/
static int dcesrv_endpoint_destructor(struct dcesrv_connection *p)
{
	while (p->contexts) {
		struct dcesrv_connection_context *c = p->contexts;

		DLIST_REMOVE(p->contexts, c);

		if (c->iface) {
			c->iface->unbind(c, c->iface);
		}
	}

	return 0;
}


/*
  connect to a dcerpc endpoint
*/
NTSTATUS dcesrv_endpoint_connect(struct dcesrv_context *dce_ctx,
				 TALLOC_CTX *mem_ctx,
				 const struct dcesrv_endpoint *ep,
				 struct auth_session_info *session_info,
				 struct event_context *event_ctx,
				 struct messaging_context *msg_ctx,
				 struct server_id server_id,
				 uint32_t state_flags,
				 struct dcesrv_connection **_p)
{
	struct dcesrv_connection *p;

	if (!session_info) {
		return NT_STATUS_ACCESS_DENIED;
	}

	p = talloc(mem_ctx, struct dcesrv_connection);
	NT_STATUS_HAVE_NO_MEMORY(p);

	if (!talloc_reference(p, session_info)) {
		talloc_free(p);
		return NT_STATUS_NO_MEMORY;
	}

	p->dce_ctx = dce_ctx;
	p->endpoint = ep;
	p->contexts = NULL;
	p->call_list = NULL;
	p->incoming_fragmented_call_list = NULL;
	p->pending_call_list = NULL;
	p->cli_max_recv_frag = 0;
	p->partial_input = data_blob(NULL, 0);
	p->auth_state.auth_info = NULL;
	p->auth_state.gensec_security = NULL;
	p->auth_state.session_info = session_info;
	p->auth_state.session_key = dcesrv_generic_session_key;
	p->event_ctx = event_ctx;
	p->msg_ctx = msg_ctx;
	p->server_id = server_id;
	p->processing = False;
	p->state_flags = state_flags;
	ZERO_STRUCT(p->transport);

	talloc_set_destructor(p, dcesrv_endpoint_destructor);

	*_p = p;
	return NT_STATUS_OK;
}

/*
  search and connect to a dcerpc endpoint
*/
_PUBLIC_ NTSTATUS dcesrv_endpoint_search_connect(struct dcesrv_context *dce_ctx,
					TALLOC_CTX *mem_ctx,
					const struct dcerpc_binding *ep_description,
					struct auth_session_info *session_info,
					struct event_context *event_ctx,
					struct messaging_context *msg_ctx,
					struct server_id server_id,
					uint32_t state_flags,
					struct dcesrv_connection **dce_conn_p)
{
	NTSTATUS status;
	const struct dcesrv_endpoint *ep;

	/* make sure this endpoint exists */
	ep = find_endpoint(dce_ctx, ep_description);
	if (!ep) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	status = dcesrv_endpoint_connect(dce_ctx, mem_ctx, ep, session_info,
					 event_ctx, msg_ctx, server_id,
					 state_flags, dce_conn_p);
	NT_STATUS_NOT_OK_RETURN(status);

	(*dce_conn_p)->auth_state.session_key = dcesrv_inherited_session_key;

	/* TODO: check security descriptor of the endpoint here 
	 *       if it's a smb named pipe
	 *	 if it's failed free dce_conn_p
	 */

	return NT_STATUS_OK;
}


static void dcesrv_init_hdr(struct ncacn_packet *pkt)
{
	pkt->rpc_vers = 5;
	pkt->rpc_vers_minor = 0;
	if (lp_rpc_big_endian()) {
		pkt->drep[0] = 0;
	} else {
		pkt->drep[0] = DCERPC_DREP_LE;
	}
	pkt->drep[1] = 0;
	pkt->drep[2] = 0;
	pkt->drep[3] = 0;
}

/*
  move a call from an existing linked list to the specified list. This
  prevents bugs where we forget to remove the call from a previous
  list when moving it.
 */
static void dcesrv_call_set_list(struct dcesrv_call_state *call, 
				 enum dcesrv_call_list list)
{
	switch (call->list) {
	case DCESRV_LIST_NONE:
		break;
	case DCESRV_LIST_CALL_LIST:
		DLIST_REMOVE(call->conn->call_list, call);
		break;
	case DCESRV_LIST_FRAGMENTED_CALL_LIST:
		DLIST_REMOVE(call->conn->incoming_fragmented_call_list, call);
		break;
	case DCESRV_LIST_PENDING_CALL_LIST:
		DLIST_REMOVE(call->conn->pending_call_list, call);
		break;
	}
	call->list = list;
	switch (list) {
	case DCESRV_LIST_NONE:
		break;
	case DCESRV_LIST_CALL_LIST:
		DLIST_ADD_END(call->conn->call_list, call, struct dcesrv_call_state *);
		break;
	case DCESRV_LIST_FRAGMENTED_CALL_LIST:
		DLIST_ADD_END(call->conn->incoming_fragmented_call_list, call, struct dcesrv_call_state *);
		break;
	case DCESRV_LIST_PENDING_CALL_LIST:
		DLIST_ADD_END(call->conn->pending_call_list, call, struct dcesrv_call_state *);
		break;
	}
}

/*
  return a dcerpc fault
*/
static NTSTATUS dcesrv_fault(struct dcesrv_call_state *call, uint32_t fault_code)
{
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	NTSTATUS status;

	/* setup a bind_ack */
	dcesrv_init_hdr(&pkt);
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_FAULT;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.fault.alloc_hint = 0;
	pkt.u.fault.context_id = 0;
	pkt.u.fault.cancel_count = 0;
	pkt.u.fault.status = fault_code;

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	return NT_STATUS_OK;	
}


/*
  return a dcerpc bind_nak
*/
static NTSTATUS dcesrv_bind_nak(struct dcesrv_call_state *call, uint32_t reason)
{
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	NTSTATUS status;

	/* setup a bind_nak */
	dcesrv_init_hdr(&pkt);
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_BIND_NAK;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.bind_nak.reject_reason = reason;
	if (pkt.u.bind_nak.reject_reason == DECRPC_BIND_PROTOCOL_VERSION_NOT_SUPPORTED) {
		pkt.u.bind_nak.versions.v.num_versions = 0;
	}

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	return NT_STATUS_OK;	
}


/*
  handle a bind request
*/
static NTSTATUS dcesrv_bind(struct dcesrv_call_state *call)
{
	uint32_t if_version, transfer_syntax_version;
	struct GUID uuid, *transfer_syntax_uuid;
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	NTSTATUS status;
	uint32_t result=0, reason=0;
	uint32_t context_id;
	const struct dcesrv_interface *iface;

	if (call->pkt.u.bind.assoc_group_id != 0) {
		return dcesrv_bind_nak(call, 0);	
	}

	if (call->pkt.u.bind.num_contexts < 1 ||
	    call->pkt.u.bind.ctx_list[0].num_transfer_syntaxes < 1) {
		return dcesrv_bind_nak(call, 0);
	}

	context_id = call->pkt.u.bind.ctx_list[0].context_id;

	/* you can't bind twice on one context */
	if (dcesrv_find_context(call->conn, context_id) != NULL) {
		return dcesrv_bind_nak(call, 0);
	}

	if_version = call->pkt.u.bind.ctx_list[0].abstract_syntax.if_version;
	uuid = call->pkt.u.bind.ctx_list[0].abstract_syntax.uuid;

	transfer_syntax_version = call->pkt.u.bind.ctx_list[0].transfer_syntaxes[0].if_version;
	transfer_syntax_uuid = &call->pkt.u.bind.ctx_list[0].transfer_syntaxes[0].uuid;
	if (!GUID_equal(&ndr_transfer_syntax.uuid, transfer_syntax_uuid) != 0 ||
	    ndr_transfer_syntax.if_version != transfer_syntax_version) {
		char *uuid_str = GUID_string(call, transfer_syntax_uuid);
		/* we only do NDR encoded dcerpc */
		DEBUG(0,("Non NDR transfer syntax requested - %s\n", uuid_str));
		talloc_free(uuid_str);
		return dcesrv_bind_nak(call, 0);
	}

	iface = find_interface_by_uuid(call->conn->endpoint, &uuid, if_version);
	if (iface == NULL) {
		char *uuid_str = GUID_string(call, &uuid);
		DEBUG(2,("Request for unknown dcerpc interface %s/%d\n", uuid_str, if_version));
		talloc_free(uuid_str);

		/* we don't know about that interface */
		result = DCERPC_BIND_PROVIDER_REJECT;
		reason = DCERPC_BIND_REASON_ASYNTAX;		
	}

	if (iface) {
		/* add this context to the list of available context_ids */
		struct dcesrv_connection_context *context = talloc(call->conn, 
								   struct dcesrv_connection_context);
		if (context == NULL) {
			return dcesrv_bind_nak(call, 0);
		}
		context->conn = call->conn;
		context->iface = iface;
		context->context_id = context_id;
		context->private = NULL;
		context->handles = NULL;
		DLIST_ADD(call->conn->contexts, context);
		call->context = context;
	}

	if (call->conn->cli_max_recv_frag == 0) {
		call->conn->cli_max_recv_frag = call->pkt.u.bind.max_recv_frag;
	}

	/* handle any authentication that is being requested */
	if (!dcesrv_auth_bind(call)) {
		return dcesrv_bind_nak(call, DCERPC_BIND_REASON_INVALID_AUTH_TYPE);
	}

	/* setup a bind_ack */
	dcesrv_init_hdr(&pkt);
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_BIND_ACK;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.bind_ack.max_xmit_frag = 0x2000;
	pkt.u.bind_ack.max_recv_frag = 0x2000;
	/* we need to send a non zero assoc_group_id here to make longhorn happy, it also matches samba3 */
	pkt.u.bind_ack.assoc_group_id = 0x12345678;
	if (iface) {
		/* FIXME: Use pipe name as specified by endpoint instead of interface name */
		pkt.u.bind_ack.secondary_address = talloc_asprintf(call, "\\PIPE\\%s", iface->name);
	} else {
		pkt.u.bind_ack.secondary_address = "";
	}
	pkt.u.bind_ack.num_results = 1;
	pkt.u.bind_ack.ctx_list = talloc(call, struct dcerpc_ack_ctx);
	if (!pkt.u.bind_ack.ctx_list) {
		return NT_STATUS_NO_MEMORY;
	}
	pkt.u.bind_ack.ctx_list[0].result = result;
	pkt.u.bind_ack.ctx_list[0].reason = reason;
	pkt.u.bind_ack.ctx_list[0].syntax = ndr_transfer_syntax;
	pkt.u.bind_ack.auth_info = data_blob(NULL, 0);

	if (!dcesrv_auth_bind_ack(call, &pkt)) {
		return dcesrv_bind_nak(call, 0);
	}

	if (iface) {
		status = iface->bind(call, iface);
		if (!NT_STATUS_IS_OK(status)) {
			char *uuid_str = GUID_string(call, &uuid);
			DEBUG(2,("Request for dcerpc interface %s/%d rejected: %s\n", 
				 uuid_str, if_version, nt_errstr(status)));
			talloc_free(uuid_str);
			return dcesrv_bind_nak(call, 0);
		}
	}

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt, 
				  call->conn->auth_state.auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	return NT_STATUS_OK;
}


/*
  handle a auth3 request
*/
static NTSTATUS dcesrv_auth3(struct dcesrv_call_state *call)
{
	/* handle the auth3 in the auth code */
	if (!dcesrv_auth_auth3(call)) {
		return dcesrv_fault(call, DCERPC_FAULT_OTHER);
	}

	talloc_free(call);

	/* we don't send a reply to a auth3 request, except by a
	   fault */
	return NT_STATUS_OK;
}


/*
  handle a bind request
*/
static NTSTATUS dcesrv_alter_new_context(struct dcesrv_call_state *call, uint32_t context_id)
{
	uint32_t if_version, transfer_syntax_version;
	struct dcesrv_connection_context *context;
	const struct dcesrv_interface *iface;
	struct GUID uuid, *transfer_syntax_uuid;

	if_version = call->pkt.u.alter.ctx_list[0].abstract_syntax.if_version;
	uuid = call->pkt.u.alter.ctx_list[0].abstract_syntax.uuid;

	transfer_syntax_version = call->pkt.u.alter.ctx_list[0].transfer_syntaxes[0].if_version;
	transfer_syntax_uuid = &call->pkt.u.alter.ctx_list[0].transfer_syntaxes[0].uuid;
	if (!GUID_equal(transfer_syntax_uuid, &ndr_transfer_syntax.uuid) ||
	    ndr_transfer_syntax.if_version != transfer_syntax_version) {
		/* we only do NDR encoded dcerpc */
		return NT_STATUS_RPC_PROTSEQ_NOT_SUPPORTED;
	}

	iface = find_interface_by_uuid(call->conn->endpoint, &uuid, if_version);
	if (iface == NULL) {
		char *uuid_str = GUID_string(call, &uuid);
		DEBUG(2,("Request for unknown dcerpc interface %s/%d\n", uuid_str, if_version));
		talloc_free(uuid_str);
		return NT_STATUS_RPC_PROTSEQ_NOT_SUPPORTED;
	}

	/* add this context to the list of available context_ids */
	context = talloc(call->conn, struct dcesrv_connection_context);
	if (context == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	context->conn = call->conn;
	context->iface = iface;
	context->context_id = context_id;
	context->private = NULL;
	context->handles = NULL;
	DLIST_ADD(call->conn->contexts, context);
	call->context = context;

	return NT_STATUS_OK;
}


/*
  handle a alter context request
*/
static NTSTATUS dcesrv_alter(struct dcesrv_call_state *call)
{
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	NTSTATUS status;
	uint32_t result=0, reason=0;
	uint32_t context_id;

	/* handle any authentication that is being requested */
	if (!dcesrv_auth_alter(call)) {
		/* TODO: work out the right reject code */
		result = DCERPC_BIND_PROVIDER_REJECT;
		reason = DCERPC_BIND_REASON_ASYNTAX;		
	}

	context_id = call->pkt.u.alter.ctx_list[0].context_id;

	/* see if they are asking for a new interface */
	if (result == 0 &&
	    dcesrv_find_context(call->conn, context_id) == NULL) {
		status = dcesrv_alter_new_context(call, context_id);
		if (!NT_STATUS_IS_OK(status)) {
			result = DCERPC_BIND_PROVIDER_REJECT;
			reason = DCERPC_BIND_REASON_ASYNTAX;		
		}
	}

	/* setup a alter_resp */
	dcesrv_init_hdr(&pkt);
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_ALTER_RESP;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.alter_resp.max_xmit_frag = 0x2000;
	pkt.u.alter_resp.max_recv_frag = 0x2000;
	pkt.u.alter_resp.assoc_group_id = call->pkt.u.alter.assoc_group_id;
	pkt.u.alter_resp.num_results = 1;
	pkt.u.alter_resp.ctx_list = talloc_array(call, struct dcerpc_ack_ctx, 1);
	if (!pkt.u.alter_resp.ctx_list) {
		return NT_STATUS_NO_MEMORY;
	}
	pkt.u.alter_resp.ctx_list[0].result = result;
	pkt.u.alter_resp.ctx_list[0].reason = reason;
	pkt.u.alter_resp.ctx_list[0].syntax = ndr_transfer_syntax;
	pkt.u.alter_resp.auth_info = data_blob(NULL, 0);
	pkt.u.alter_resp.secondary_address = "";

	if (!dcesrv_auth_alter_ack(call, &pkt)) {
		return dcesrv_bind_nak(call, 0);
	}

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt, 
				  call->conn->auth_state.auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	return NT_STATUS_OK;
}

/*
  handle a dcerpc request packet
*/
static NTSTATUS dcesrv_request(struct dcesrv_call_state *call)
{
	struct ndr_pull *pull;
	NTSTATUS status;
	struct dcesrv_connection_context *context;

	/* if authenticated, and the mech we use can't do async replies, don't use them... */
	if (call->conn->auth_state.gensec_security && 
	    !gensec_have_feature(call->conn->auth_state.gensec_security, GENSEC_FEATURE_ASYNC_REPLIES)) {
		call->state_flags &= ~DCESRV_CALL_STATE_FLAG_MAY_ASYNC;
	}

	context = dcesrv_find_context(call->conn, call->pkt.u.request.context_id);
	if (context == NULL) {
		return dcesrv_fault(call, DCERPC_FAULT_UNK_IF);
	}

	pull = ndr_pull_init_blob(&call->pkt.u.request.stub_and_verifier, call);
	NT_STATUS_HAVE_NO_MEMORY(pull);

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;

	call->context	= context;
	call->ndr_pull	= pull;

	if (call->pkt.pfc_flags & DCERPC_PFC_FLAG_OBJECT_UUID) {
		pull->flags |= LIBNDR_FLAG_OBJECT_PRESENT;
	}

	if (!(call->pkt.drep[0] & DCERPC_DREP_LE)) {
		pull->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	/* unravel the NDR for the packet */
	status = context->iface->ndr_pull(call, call, pull, &call->r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault(call, call->fault_code);
	}

	if (pull->offset != pull->data_size) {
		DEBUG(3,("Warning: %d extra bytes in incoming RPC request\n", 
			 pull->data_size - pull->offset));
		dump_data(10, pull->data+pull->offset, pull->data_size - pull->offset);
	}

	/* call the dispatch function */
	status = context->iface->dispatch(call, call, call->r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5,("dcerpc fault in call %s:%02x - %s\n",
			 context->iface->name, 
			 call->pkt.u.request.opnum,
			 dcerpc_errstr(pull, call->fault_code)));
		return dcesrv_fault(call, call->fault_code);
	}

	/* add the call to the pending list */
	dcesrv_call_set_list(call, DCESRV_LIST_PENDING_CALL_LIST);

	if (call->state_flags & DCESRV_CALL_STATE_FLAG_ASYNC) {
		return NT_STATUS_OK;
	}

	return dcesrv_reply(call);
}

_PUBLIC_ NTSTATUS dcesrv_reply(struct dcesrv_call_state *call)
{
	struct ndr_push *push;
	NTSTATUS status;
	DATA_BLOB stub;
	uint32_t total_length;
	struct dcesrv_connection_context *context = call->context;

	/* call the reply function */
	status = context->iface->reply(call, call, call->r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault(call, call->fault_code);
	}

	/* form the reply NDR */
	push = ndr_push_init_ctx(call);
	NT_STATUS_HAVE_NO_MEMORY(push);

	/* carry over the pointer count to the reply in case we are
	   using full pointer. See NDR specification for full
	   pointers */
	push->ptr_count = call->ndr_pull->ptr_count;

	if (lp_rpc_big_endian()) {
		push->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	status = context->iface->ndr_push(call, call, push, call->r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault(call, call->fault_code);
	}

	stub = ndr_push_blob(push);

	total_length = stub.length;

	do {
		uint32_t length;
		struct data_blob_list_item *rep;
		struct ncacn_packet pkt;

		rep = talloc(call, struct data_blob_list_item);
		NT_STATUS_HAVE_NO_MEMORY(rep);

		length = stub.length;
		if (length + DCERPC_RESPONSE_LENGTH > call->conn->cli_max_recv_frag) {
			/* the 32 is to cope with signing data */
			length = call->conn->cli_max_recv_frag - 
				(DCERPC_MAX_SIGN_SIZE+DCERPC_RESPONSE_LENGTH);
		}

		/* form the dcerpc response packet */
		dcesrv_init_hdr(&pkt);
		pkt.auth_length = 0;
		pkt.call_id = call->pkt.call_id;
		pkt.ptype = DCERPC_PKT_RESPONSE;
		pkt.pfc_flags = 0;
		if (stub.length == total_length) {
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

		if (!dcesrv_auth_response(call, &rep->blob, &pkt)) {
			return dcesrv_fault(call, DCERPC_FAULT_OTHER);		
		}

		dcerpc_set_frag_length(&rep->blob, rep->blob.length);

		DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
		
		stub.data += length;
		stub.length -= length;
	} while (stub.length != 0);

	/* move the call from the pending to the finished calls list */
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	if (call->conn->call_list && call->conn->call_list->replies) {
		if (call->conn->transport.report_output_data) {
			call->conn->transport.report_output_data(call->conn);
		}
	}

	return NT_STATUS_OK;
}

_PUBLIC_ struct socket_address *dcesrv_connection_get_my_addr(struct dcesrv_connection *conn, TALLOC_CTX *mem_ctx)
{
	if (!conn->transport.get_my_addr) {
		return NULL;
	}

	return conn->transport.get_my_addr(conn, mem_ctx);
}

_PUBLIC_ struct socket_address *dcesrv_connection_get_peer_addr(struct dcesrv_connection *conn, TALLOC_CTX *mem_ctx)
{
	if (!conn->transport.get_peer_addr) {
		return NULL;
	}

	return conn->transport.get_peer_addr(conn, mem_ctx);
}

/*
  work out if we have a full packet yet
*/
static BOOL dce_full_packet(const DATA_BLOB *data)
{
	if (data->length < DCERPC_FRAG_LEN_OFFSET+2) {
		return False;
	}
	if (dcerpc_get_frag_length(data) > data->length) {
		return False;
	}
	return True;
}

/*
  we might have consumed only part of our input - advance past that part
*/
static void dce_partial_advance(struct dcesrv_connection *dce_conn, uint32_t offset)
{
	DATA_BLOB blob;

	if (dce_conn->partial_input.length == offset) {
		data_blob_free(&dce_conn->partial_input);
		return;
	}

	blob = dce_conn->partial_input;
	dce_conn->partial_input = data_blob(blob.data + offset,
					    blob.length - offset);
	data_blob_free(&blob);
}

/*
  remove the call from the right list when freed
 */
static int dcesrv_call_dequeue(struct dcesrv_call_state *call)
{
	dcesrv_call_set_list(call, DCESRV_LIST_NONE);
	return 0;
}

/*
  process some input to a dcerpc endpoint server.
*/
NTSTATUS dcesrv_input_process(struct dcesrv_connection *dce_conn)
{
	struct ndr_pull *ndr;
	NTSTATUS status;
	struct dcesrv_call_state *call;
	DATA_BLOB blob;

	call = talloc_zero(dce_conn, struct dcesrv_call_state);
	if (!call) {
		talloc_free(dce_conn->partial_input.data);
		return NT_STATUS_NO_MEMORY;
	}
	call->conn		= dce_conn;
	call->event_ctx		= dce_conn->event_ctx;
	call->msg_ctx		= dce_conn->msg_ctx;
	call->state_flags	= call->conn->state_flags;
	call->time		= timeval_current();
	call->list              = DCESRV_LIST_NONE;

	talloc_set_destructor(call, dcesrv_call_dequeue);

	blob = dce_conn->partial_input;
	blob.length = dcerpc_get_frag_length(&blob);

	ndr = ndr_pull_init_blob(&blob, call);
	if (!ndr) {
		talloc_free(dce_conn->partial_input.data);
		talloc_free(call);
		return NT_STATUS_NO_MEMORY;
	}

	if (!(CVAL(blob.data, DCERPC_DREP_OFFSET) & DCERPC_DREP_LE)) {
		ndr->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	status = ndr_pull_ncacn_packet(ndr, NDR_SCALARS|NDR_BUFFERS, &call->pkt);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(dce_conn->partial_input.data);
		talloc_free(call);
		return status;
	}

	/* we have to check the signing here, before combining the
	   pdus */
	if (call->pkt.ptype == DCERPC_PKT_REQUEST &&
	    !dcesrv_auth_request(call, &blob)) {
		dce_partial_advance(dce_conn, blob.length);
		return dcesrv_fault(call, DCERPC_FAULT_ACCESS_DENIED);		
	}

	dce_partial_advance(dce_conn, blob.length);

	/* see if this is a continued packet */
	if (call->pkt.ptype == DCERPC_PKT_REQUEST &&
	    !(call->pkt.pfc_flags & DCERPC_PFC_FLAG_FIRST)) {
		struct dcesrv_call_state *call2 = call;
		uint32_t alloc_size;

		/* we only allow fragmented requests, no other packet types */
		if (call->pkt.ptype != DCERPC_PKT_REQUEST) {
			return dcesrv_fault(call2, DCERPC_FAULT_OTHER);
		}

		/* this is a continuation of an existing call - find the call then
		   tack it on the end */
		call = dcesrv_find_fragmented_call(dce_conn, call2->pkt.call_id);
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
			talloc_realloc(call, 
				       call->pkt.u.request.stub_and_verifier.data, 
				       uint8_t, alloc_size);
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

		talloc_free(call2);
	}

	/* this may not be the last pdu in the chain - if its isn't then
	   just put it on the incoming_fragmented_call_list and wait for the rest */
	if (call->pkt.ptype == DCERPC_PKT_REQUEST &&
	    !(call->pkt.pfc_flags & DCERPC_PFC_FLAG_LAST)) {
		dcesrv_call_set_list(call, DCESRV_LIST_FRAGMENTED_CALL_LIST);
		return NT_STATUS_OK;
	} 
	
	/* This removes any fragments we may have had stashed away */
	dcesrv_call_set_list(call, DCESRV_LIST_NONE);

	switch (call->pkt.ptype) {
	case DCERPC_PKT_BIND:
		status = dcesrv_bind(call);
		break;
	case DCERPC_PKT_AUTH3:
		status = dcesrv_auth3(call);
		break;
	case DCERPC_PKT_ALTER:
		status = dcesrv_alter(call);
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
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(call);
	}

	return status;
}


/*
  provide some input to a dcerpc endpoint server. This passes data
  from a dcerpc client into the server
*/
_PUBLIC_ NTSTATUS dcesrv_input(struct dcesrv_connection *dce_conn, const DATA_BLOB *data)
{
	NTSTATUS status;

	dce_conn->partial_input.data = talloc_realloc(dce_conn,
						      dce_conn->partial_input.data,
						      uint8_t,
						      dce_conn->partial_input.length + data->length);
	if (!dce_conn->partial_input.data) {
		return NT_STATUS_NO_MEMORY;
	}
	memcpy(dce_conn->partial_input.data + dce_conn->partial_input.length,
	       data->data, data->length);
	dce_conn->partial_input.length += data->length;

	while (dce_full_packet(&dce_conn->partial_input)) {
		status = dcesrv_input_process(dce_conn);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	return NT_STATUS_OK;
}

/*
  retrieve some output from a dcerpc server
  The caller supplies a function that will be called to do the
  actual output. 

  The first argument to write_fn() will be 'private', the second will
  be a pointer to a buffer containing the data to be sent and the 3rd
  will be a pointer to a size_t variable that will be set to the
  number of bytes that are consumed from the output.

  from the current fragment
*/
_PUBLIC_ NTSTATUS dcesrv_output(struct dcesrv_connection *dce_conn, 
		       void *private_data,
		       NTSTATUS (*write_fn)(void *private_data, DATA_BLOB *output, size_t *nwritten))
{
	NTSTATUS status;
	struct dcesrv_call_state *call;
	struct data_blob_list_item *rep;
	size_t nwritten;

	call = dce_conn->call_list;
	if (!call || !call->replies) {
		if (dce_conn->pending_call_list) {
			/* TODO: we need to say act async here
			 *       as we know we have pending requests
			 *	 which will be finished at a time
			 */
			return NT_STATUS_FOOBAR;
		}
		return NT_STATUS_FOOBAR;
	}
	rep = call->replies;

	status = write_fn(private_data, &rep->blob, &nwritten);
	NT_STATUS_IS_ERR_RETURN(status);

	rep->blob.length -= nwritten;
	rep->blob.data += nwritten;

	if (rep->blob.length == 0) {
		/* we're done with this section of the call */
		DLIST_REMOVE(call->replies, rep);
	}

	if (call->replies == NULL) {
		/* we're done with the whole call */
		dcesrv_call_set_list(call, DCESRV_LIST_NONE);
		talloc_free(call);
	}

	return status;
}

static NTSTATUS dcesrv_init_context(TALLOC_CTX *mem_ctx, const char **endpoint_servers, struct dcesrv_context **_dce_ctx)
{
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;
	int i;

	if (!endpoint_servers) {
		DEBUG(0,("dcesrv_init_context: no endpoint servers configured\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	dce_ctx = talloc(mem_ctx, struct dcesrv_context);
	NT_STATUS_HAVE_NO_MEMORY(dce_ctx);
	dce_ctx->endpoint_list	= NULL;

	for (i=0;endpoint_servers[i];i++) {
		const struct dcesrv_endpoint_server *ep_server;

		ep_server = dcesrv_ep_server_byname(endpoint_servers[i]);
		if (!ep_server) {
			DEBUG(0,("dcesrv_init_context: failed to find endpoint server = '%s'\n", endpoint_servers[i]));
			return NT_STATUS_INTERNAL_ERROR;
		}

		status = ep_server->init_server(dce_ctx, ep_server);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("dcesrv_init_context: failed to init endpoint server = '%s': %s\n", endpoint_servers[i],
				nt_errstr(status)));
			return status;
		}
	}

	*_dce_ctx = dce_ctx;
	return NT_STATUS_OK;
}

/*
  initialise the dcerpc server context for ncacn_np based services
*/
_PUBLIC_ NTSTATUS dcesrv_init_ipc_context(TALLOC_CTX *mem_ctx, struct dcesrv_context **_dce_ctx)
{
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;

	status = dcesrv_init_context(mem_ctx, lp_dcerpc_endpoint_servers(), &dce_ctx);
	NT_STATUS_NOT_OK_RETURN(status);

	*_dce_ctx = dce_ctx;
	return NT_STATUS_OK;
}

/* the list of currently registered DCERPC endpoint servers.
 */
static struct ep_server {
	struct dcesrv_endpoint_server *ep_server;
} *ep_servers = NULL;
static int num_ep_servers;

/*
  register a DCERPC endpoint server. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.  

  The 'type' is used to specify whether this is for a disk, printer or IPC$ share
*/
_PUBLIC_ NTSTATUS dcerpc_register_ep_server(const void *_ep_server)
{
	const struct dcesrv_endpoint_server *ep_server = _ep_server;
	
	if (dcesrv_ep_server_byname(ep_server->name) != NULL) {
		/* its already registered! */
		DEBUG(0,("DCERPC endpoint server '%s' already registered\n", 
			 ep_server->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	ep_servers = realloc_p(ep_servers, struct ep_server, num_ep_servers+1);
	if (!ep_servers) {
		smb_panic("out of memory in dcerpc_register");
	}

	ep_servers[num_ep_servers].ep_server = smb_xmemdup(ep_server, sizeof(*ep_server));
	ep_servers[num_ep_servers].ep_server->name = smb_xstrdup(ep_server->name);

	num_ep_servers++;

	DEBUG(3,("DCERPC endpoint server '%s' registered\n", 
		 ep_server->name));

	return NT_STATUS_OK;
}

/*
  return the operations structure for a named backend of the specified type
*/
const struct dcesrv_endpoint_server *dcesrv_ep_server_byname(const char *name)
{
	int i;

	for (i=0;i<num_ep_servers;i++) {
		if (strcmp(ep_servers[i].ep_server->name, name) == 0) {
			return ep_servers[i].ep_server;
		}
	}

	return NULL;
}

/*
  return the DCERPC module version, and the size of some critical types
  This can be used by endpoint server modules to either detect compilation errors, or provide
  multiple implementations for different smbd compilation options in one module
*/
const struct dcesrv_critical_sizes *dcerpc_module_version(void)
{
	static const struct dcesrv_critical_sizes critical_sizes = {
		DCERPC_MODULE_VERSION,
		sizeof(struct dcesrv_context),
		sizeof(struct dcesrv_endpoint),
		sizeof(struct dcesrv_endpoint_server),
		sizeof(struct dcesrv_interface),
		sizeof(struct dcesrv_if_list),
		sizeof(struct dcesrv_connection),
		sizeof(struct dcesrv_call_state),
		sizeof(struct dcesrv_auth),
		sizeof(struct dcesrv_handle)
	};

	return &critical_sizes;
}

/*
  open the dcerpc server sockets
*/
static void dcesrv_task_init(struct task_server *task)
{
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;
	struct dcesrv_endpoint *e;

	task_server_set_title(task, "task[dcesrv]");

	status = dcesrv_init_context(task->event_ctx,
				     lp_dcerpc_endpoint_servers(),
				     &dce_ctx);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	/* Make sure the directory for NCALRPC exists */
	if (!directory_exist(lp_ncalrpc_dir())) {
		mkdir(lp_ncalrpc_dir(), 0755);
	}

	for (e=dce_ctx->endpoint_list;e;e=e->next) {
		switch (e->ep_description->transport) {
		case NCACN_UNIX_STREAM:
			status = dcesrv_add_ep_unix(dce_ctx, e, task->event_ctx, task->model_ops);
			if (!NT_STATUS_IS_OK(status)) goto failed;
			break;
		
		case NCALRPC:
			status = dcesrv_add_ep_ncalrpc(dce_ctx, e, task->event_ctx, task->model_ops);
			if (!NT_STATUS_IS_OK(status)) goto failed;
			break;

		case NCACN_IP_TCP:
			status = dcesrv_add_ep_tcp(dce_ctx, e, task->event_ctx, task->model_ops);
			if (!NT_STATUS_IS_OK(status)) goto failed;
			break;
			
		case NCACN_NP:
			status = dcesrv_add_ep_np(dce_ctx, e, task->event_ctx, task->model_ops);
			if (!NT_STATUS_IS_OK(status)) goto failed;
			break;

		default:
			status = NT_STATUS_NOT_SUPPORTED;
			if (!NT_STATUS_IS_OK(status)) goto failed;
		}
	}

	return;
failed:
	task_server_terminate(task, "Failed to startup dcerpc server task");	
}

/*
  called on startup of the smb server service It's job is to start
  listening on all configured sockets
*/
static NTSTATUS dcesrv_init(struct event_context *event_context, 
			    const struct model_ops *model_ops)
{	
	return task_server_startup(event_context, model_ops, dcesrv_task_init);
}

NTSTATUS server_service_rpc_init(void)
{
	init_module_fn static_init[] = STATIC_dcerpc_server_MODULES;
	init_module_fn *shared_init = load_samba_modules(NULL, "dcerpc_server");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);
	
	return register_server_service("rpc", dcesrv_init);
}
