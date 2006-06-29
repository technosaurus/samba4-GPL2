/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher  2004
   Copyright (C) Rafal Szczesniak   2005
   
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
#include "libnet/libnet.h"
#include "libcli/libcli.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_samr.h"


struct rpc_connect_srv_state {
	struct libnet_RpcConnect r;
	const char *binding;
};


static void continue_pipe_connect(struct composite_context *ctx);


/**
 * Initiates connection to rpc pipe on remote server
 * 
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values
 * @return composite context of this call
 **/

static struct composite_context* libnet_RpcConnectSrv_send(struct libnet_context *ctx,
							   TALLOC_CTX *mem_ctx,
							   struct libnet_RpcConnect *r)
{
	struct composite_context *c;	
	struct rpc_connect_srv_state *s;
	struct composite_context *pipe_connect_req;

	/* composite context allocation and setup */
	c = talloc_zero(mem_ctx, struct composite_context);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct rpc_connect_srv_state);
	if (composite_nomem(s, c)) return c;

	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->private_data = s;
	c->event_ctx = ctx->event_ctx;

	s->r = *r;
	ZERO_STRUCT(s->r.out);

	/* prepare binding string */
	switch (r->level) {
	case LIBNET_RPC_CONNECT_DC:
	case LIBNET_RPC_CONNECT_PDC:
	case LIBNET_RPC_CONNECT_SERVER:
		s->binding = talloc_asprintf(s, "ncacn_np:%s", r->in.name);
		break;

	case LIBNET_RPC_CONNECT_BINDING:
		s->binding = talloc_strdup(s, r->in.binding);
		break;

	case LIBNET_RPC_CONNECT_DC_INFO:
		/* this should never happen - DC_INFO level has a separate
		   composite function */
		composite_error(c, NT_STATUS_INVALID_LEVEL);
		return c;
	}

	/* connect to remote dcerpc pipe */
	pipe_connect_req = dcerpc_pipe_connect_send(c, &s->r.out.dcerpc_pipe,
						    s->binding, r->in.dcerpc_iface,
						    ctx->cred, c->event_ctx);
	if (composite_nomem(pipe_connect_req, c)) return c;

	composite_continue(c, pipe_connect_req, continue_pipe_connect, c);
	return c;
}


/*
  Step 2 of RpcConnectSrv - get rpc connection
*/
static void continue_pipe_connect(struct composite_context *ctx)
{
	struct composite_context *c;
	struct rpc_connect_srv_state *s;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_srv_state);

	/* receive result of rpc pipe connection */
	c->status = dcerpc_pipe_connect_recv(ctx, c, &s->r.out.dcerpc_pipe);
	if (!composite_is_ok(c)) return;

	s->r.out.error_string = NULL;
	composite_done(c);
}


/**
 * Receives result of connection to rpc pipe on remote server
 *
 * @param c composite context
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values
 * @return nt status of rpc connection
 **/

static NTSTATUS libnet_RpcConnectSrv_recv(struct composite_context *c,
					  struct libnet_context *ctx,
					  TALLOC_CTX *mem_ctx,
					  struct libnet_RpcConnect *r)
{
	NTSTATUS status;
	struct rpc_connect_srv_state *s = talloc_get_type(c->private_data,
					  struct rpc_connect_srv_state);

	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		/* move the returned rpc pipe between memory contexts */
		s = talloc_get_type(c->private_data, struct rpc_connect_srv_state);
		r->out.dcerpc_pipe = talloc_steal(mem_ctx, s->r.out.dcerpc_pipe);

		/* reference created pipe structure to long-term libnet_context
		   so that it can be used by other api functions even after short-term
		   mem_ctx is freed */
		if (r->in.dcerpc_iface == &dcerpc_table_samr) {
			ctx->samr_pipe = talloc_reference(ctx, r->out.dcerpc_pipe);

		} else if (r->in.dcerpc_iface == &dcerpc_table_lsarpc) {
			ctx->lsa_pipe = talloc_reference(ctx, r->out.dcerpc_pipe);
		}
	} else {
		r->out.error_string = talloc_steal(mem_ctx, s->r.out.error_string);
	}

	talloc_free(c);
	return status;
}


struct rpc_connect_dc_state {
	struct libnet_context *ctx;
	struct libnet_RpcConnect r;
	struct libnet_RpcConnect r2;
	struct libnet_LookupDCs f;
	const char *connect_name;
};


static void continue_lookup_dc(struct composite_context *ctx);
static void continue_rpc_connect(struct composite_context *ctx);


/**
 * Initiates connection to rpc pipe on domain pdc
 * 
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values
 * @return composite context of this call
 **/

static struct composite_context* libnet_RpcConnectDC_send(struct libnet_context *ctx,
							  TALLOC_CTX *mem_ctx,
							  struct libnet_RpcConnect *r)
{
	struct composite_context *c;
	struct rpc_connect_dc_state *s;
	struct composite_context *lookup_dc_req;

	/* composite context allocation and setup */
	c = talloc_zero(mem_ctx, struct composite_context);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct rpc_connect_dc_state);
	if (composite_nomem(s, c)) return c;

	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->private_data = s;
	c->event_ctx = ctx->event_ctx;

	s->ctx = ctx;
	s->r   = *r;
	ZERO_STRUCT(s->r.out);

	switch (r->level) {
	case LIBNET_RPC_CONNECT_PDC:
		s->f.in.name_type = NBT_NAME_PDC;
		break;

	case LIBNET_RPC_CONNECT_DC:
		s->f.in.name_type = NBT_NAME_LOGON;
		break;

	default:
		break;
	}
	s->f.in.domain_name = r->in.name;
	s->f.out.num_dcs    = 0;
	s->f.out.dcs        = NULL;

	/* find the domain pdc first */
	lookup_dc_req = libnet_LookupDCs_send(ctx, c, &s->f);
	if (composite_nomem(lookup_dc_req, c)) return c;

	composite_continue(c, lookup_dc_req, continue_lookup_dc, c);
	return c;
}


/*
  Step 2 of RpcConnectDC: get domain controller name/address and
  initiate RpcConnect to it
*/
static void continue_lookup_dc(struct composite_context *ctx)
{
	struct composite_context *c;
	struct rpc_connect_dc_state *s;
	struct composite_context *rpc_connect_req;
	
	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dc_state);
	
	/* receive result of domain controller lookup */
	c->status = libnet_LookupDCs_recv(ctx, c, &s->f);
	if (!composite_is_ok(c)) return;

	/* we might not have got back a name.  Fall back to the IP */
	if (s->f.out.dcs[0].name) {
		s->connect_name = s->f.out.dcs[0].name;
	} else {
		s->connect_name = s->f.out.dcs[0].address;
	}

	/* ok, pdc has been found so do attempt to rpc connect */
	s->r2.level	       = LIBNET_RPC_CONNECT_SERVER;

	/* this will cause yet another name resolution, but at least
	 * we pass the right name down the stack now */
	s->r2.in.name          = talloc_strdup(c, s->connect_name);
	s->r2.in.dcerpc_iface  = s->r.in.dcerpc_iface;	

	/* send rpc connect request to the server */
	rpc_connect_req = libnet_RpcConnect_send(s->ctx, c, &s->r2);
	if (composite_nomem(rpc_connect_req, c)) return;

	composite_continue(c, rpc_connect_req, continue_rpc_connect, c);
}


/*
  Step 3 of RpcConnectDC: get rpc connection to the server
*/
static void continue_rpc_connect(struct composite_context *ctx)
{
	struct composite_context *c;
	struct rpc_connect_dc_state *s;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dc_state);

	c->status = libnet_RpcConnect_recv(ctx, s->ctx, c, &s->r2);

	/* error string is to be passed anyway */
	s->r.out.error_string  = s->r2.out.error_string;
	if (!composite_is_ok(c)) return;

	s->r.out.dcerpc_pipe = s->r2.out.dcerpc_pipe;

	composite_done(c);
}


/**
 * Receives result of connection to rpc pipe on domain pdc
 *
 * @param c composite context
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values
 * @return nt status of rpc connection
 **/

static NTSTATUS libnet_RpcConnectDC_recv(struct composite_context *c,
					 struct libnet_context *ctx,
					 TALLOC_CTX *mem_ctx,
					 struct libnet_RpcConnect *r)
{
	NTSTATUS status;
	struct rpc_connect_dc_state *s = talloc_get_type(c->private_data,
					 struct rpc_connect_dc_state);

	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		/* move connected rpc pipe between memory contexts */
		r->out.dcerpc_pipe = talloc_steal(mem_ctx, s->r.out.dcerpc_pipe);

		/* reference created pipe structure to long-term libnet_context
		   so that it can be used by other api functions even after short-term
		   mem_ctx is freed */
		if (r->in.dcerpc_iface == &dcerpc_table_samr) {
			ctx->samr_pipe = talloc_reference(ctx, r->out.dcerpc_pipe);

		} else if (r->in.dcerpc_iface == &dcerpc_table_lsarpc) {
			ctx->lsa_pipe = talloc_reference(ctx, r->out.dcerpc_pipe);
		}

	} else {
		r->out.error_string = talloc_steal(mem_ctx, s->r.out.error_string);
	}

	talloc_free(c);
	return status;
}



struct rpc_connect_dci_state {
	struct libnet_context *ctx;
	struct libnet_RpcConnect r;
	struct libnet_RpcConnect rpc_conn;
	struct policy_handle lsa_handle;
	struct lsa_QosInfo qos;
	struct lsa_ObjectAttribute attr;
	struct lsa_OpenPolicy2 lsa_open_policy;
	struct dcerpc_pipe *lsa_pipe;
	struct lsa_QueryInfoPolicy2 lsa_query_info2;
	struct lsa_QueryInfoPolicy lsa_query_info;
	struct dcerpc_binding *final_binding;
	struct dcerpc_pipe *final_pipe;
};


static void continue_dci_rpc_connect(struct composite_context *ctx);
static void continue_lsa_policy(struct rpc_request *req);
static void continue_lsa_query_info(struct rpc_request *req);
static void continue_lsa_query_info2(struct rpc_request *req);
static void continue_epm_map_binding(struct composite_context *ctx);
static void continue_secondary_conn(struct composite_context *ctx);


/**
 * Initiates connection to rpc pipe on remote server or pdc. Received result
 * contains info on the domain name, domain sid and realm.
 * 
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values. Must be a talloc context
 * @return composite context of this call
 **/

static struct composite_context* libnet_RpcConnectDCInfo_send(struct libnet_context *ctx,
							      TALLOC_CTX *mem_ctx,
							      struct libnet_RpcConnect *r)
{
	struct composite_context *c, *conn_req;
	struct rpc_connect_dci_state *s;

	/* composite context allocation and setup */
	c = talloc_zero(mem_ctx, struct composite_context);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct rpc_connect_dci_state);
	if (composite_nomem(s, c)) return c;

	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->private_data = s;
	c->event_ctx = ctx->event_ctx;

	s->ctx = ctx;
	s->r   = *r;
	ZERO_STRUCT(s->r.out);

	/* proceed to pure rpc connection if the binding string is provided,
	   otherwise try to connect domain controller */
	if (r->in.binding == NULL) {
		s->rpc_conn.in.name    = r->in.name;
		s->rpc_conn.level      = LIBNET_RPC_CONNECT_DC;
	} else {
		s->rpc_conn.in.binding = r->in.binding;
		s->rpc_conn.level      = LIBNET_RPC_CONNECT_BINDING;
	}

	/* we need to query information on lsarpc interface first */
	s->rpc_conn.in.dcerpc_iface    = &dcerpc_table_lsarpc;
	
	/* request connection to the lsa pipe on the pdc */
	conn_req = libnet_RpcConnect_send(ctx, c, &s->rpc_conn);
	if (composite_nomem(c, conn_req)) return c;

	composite_continue(c, conn_req, continue_dci_rpc_connect, c);
	return c;
}


/*
  Step 2 of RpcConnectDCInfo: receive opened rpc pipe and open
  lsa policy handle
*/
static void continue_dci_rpc_connect(struct composite_context *ctx)
{
	struct composite_context *c;
	struct rpc_connect_dci_state *s;
	struct rpc_request *open_pol_req;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dci_state);

	c->status = libnet_RpcConnect_recv(ctx, s->ctx, c, &s->rpc_conn);
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	/* prepare to open a policy handle on lsa pipe */
	s->lsa_pipe = s->ctx->lsa_pipe;
	
	s->qos.len                 = 0;
	s->qos.impersonation_level = 2;
	s->qos.context_mode        = 1;
	s->qos.effective_only      = 0;

	s->attr.sec_qos = &s->qos;

	s->lsa_open_policy.in.attr        = &s->attr;
	s->lsa_open_policy.in.system_name = talloc_asprintf(c, "\\");
	if (composite_nomem(s->lsa_open_policy.in.system_name, c)) return;

	s->lsa_open_policy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	s->lsa_open_policy.out.handle     = &s->lsa_handle;

	open_pol_req = dcerpc_lsa_OpenPolicy2_send(s->lsa_pipe, c, &s->lsa_open_policy);
	if (composite_nomem(open_pol_req, c)) return;

	composite_continue_rpc(c, open_pol_req, continue_lsa_policy, c);
}


/*
  Step 3 of RpcConnectDCInfo: Get policy handle and query lsa info
  for kerberos realm (dns name) and guid. The query may fail.
*/
static void continue_lsa_policy(struct rpc_request *req)
{
	struct composite_context *c;
	struct rpc_connect_dci_state *s;
	struct rpc_request *query_info_req;

	c = talloc_get_type(req->async.private, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dci_state);

	c->status = dcerpc_ndr_request_recv(req);
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	if (!NT_STATUS_IS_OK(s->lsa_query_info2.out.result)) {
		composite_error(c, s->lsa_query_info2.out.result);
		return;
	}

	/* query lsa info for dns domain name and guid */
	s->lsa_query_info2.in.handle = &s->lsa_handle;
	s->lsa_query_info2.in.level  = LSA_POLICY_INFO_DNS;

	query_info_req = dcerpc_lsa_QueryInfoPolicy2_send(s->lsa_pipe, c, &s->lsa_query_info2);
	if (composite_nomem(query_info_req, c)) return;

	composite_continue_rpc(c, query_info_req, continue_lsa_query_info2, c);
}


/*
  Step 4 of RpcConnectDCInfo: Get realm and guid if provided (rpc call
  may result in failure) and query lsa info for domain name and sid.
*/
static void continue_lsa_query_info2(struct rpc_request *req)
{	
	struct composite_context *c;
	struct rpc_connect_dci_state *s;
	struct rpc_request *query_info_req;

	c = talloc_get_type(req->async.private, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dci_state);

	c->status = dcerpc_ndr_request_recv(req);
	
	/* In case of error just null the realm and guid and proceed
	   to the next step. After all, it doesn't have to be AD domain
	   controller we talking to - NT-style PDC also counts */

	if (NT_STATUS_EQUAL(c->status, NT_STATUS_NET_WRITE_FAULT)) {
		s->r.out.realm = NULL;
		s->r.out.guid  = NULL;

	} else {
		if (!NT_STATUS_IS_OK(c->status)) {
			s->r.out.error_string = talloc_asprintf(c,
								"lsa_QueryInfoPolicy2 failed: %s",
								nt_errstr(c->status));
			composite_error(c, c->status);
			return;
		}

		if (!NT_STATUS_IS_OK(s->lsa_query_info2.out.result)) {
			s->r.out.error_string = talloc_asprintf(c,
								"lsa_QueryInfoPolicy2 failed: %s",
								nt_errstr(s->lsa_query_info2.out.result));
			composite_error(c, s->lsa_query_info2.out.result);
			return;
		}

		/* Copy the dns domain name and guid from the query result */

		/* this should actually be a conversion from lsa_StringLarge */
		s->r.out.realm = s->lsa_query_info2.out.info->dns.dns_domain.string;
		s->r.out.guid  = talloc(c, struct GUID);
		if (composite_nomem(s->r.out.guid, c)) {
			s->r.out.error_string = NULL;
			return;
		}
		*s->r.out.guid = s->lsa_query_info2.out.info->dns.domain_guid;
	}

	/* query lsa info for domain name and sid */
	s->lsa_query_info.in.handle = &s->lsa_handle;
	s->lsa_query_info.in.level  = LSA_POLICY_INFO_DOMAIN;

	query_info_req = dcerpc_lsa_QueryInfoPolicy_send(s->lsa_pipe, c, &s->lsa_query_info);
	if (composite_nomem(query_info_req, c)) return;

	composite_continue_rpc(c, query_info_req, continue_lsa_query_info, c);
}


/*
  Step 5 of RpcConnectDCInfo: Get domain name and sid and request endpoint
  map binding
*/
static void continue_lsa_query_info(struct rpc_request *req)
{
	struct composite_context *c, *epm_map_req;
	struct rpc_connect_dci_state *s;

	c = talloc_get_type(req->async.private, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dci_state);

	c->status = dcerpc_ndr_request_recv(req);
	if (!NT_STATUS_IS_OK(c->status)) {
		s->r.out.error_string = talloc_asprintf(c,
							"lsa_QueryInfoPolicy failed: %s",
							nt_errstr(c->status));
		composite_error(c, c->status);
		return;
	}

	/* Copy the domain name and sid from the query result */
	s->r.out.domain_sid  = s->lsa_query_info.out.info->domain.sid;
	s->r.out.domain_name = s->lsa_query_info.out.info->domain.name.string;


	/* prepare to get endpoint mapping for the requested interface */
	s->final_binding = talloc(s, struct dcerpc_binding);
	if (composite_nomem(s->final_binding, c)) return;
	
	*s->final_binding = *s->lsa_pipe->binding;
	/* Ensure we keep hold of the member elements */
	talloc_reference(s->final_binding, s->lsa_pipe->binding);

	epm_map_req = dcerpc_epm_map_binding_send(c, s->final_binding, s->r.in.dcerpc_iface,
						  s->lsa_pipe->conn->event_ctx);
	if (composite_nomem(epm_map_req, c)) return;

	composite_continue(c, epm_map_req, continue_epm_map_binding, c);
}


/*
  Step 6 of RpcConnectDCInfo: Receive endpoint mapping and create secondary
  rpc connection derived from already used pipe but connected to the requested
  one (as specified in libnet_RpcConnect structure)
*/
static void continue_epm_map_binding(struct composite_context *ctx)
{
	struct composite_context *c, *sec_conn_req;
	struct rpc_connect_dci_state *s;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dci_state);

	c->status = dcerpc_epm_map_binding_recv(ctx);
	if (!NT_STATUS_IS_OK(c->status)) {
		s->r.out.error_string = talloc_asprintf(c,
							"failed to map pipe with endpoint mapper - %s",
							nt_errstr(c->status));
		composite_error(c, c->status);
		return;
	}

	/* create secondary connection derived from lsa pipe */
	sec_conn_req = dcerpc_secondary_connection_send(s->lsa_pipe, s->final_binding);
	if (composite_nomem(sec_conn_req, c)) return;

	composite_continue(c, sec_conn_req, continue_secondary_conn, c);
}


/*
  Step 7 of RpcConnectDCInfo: Get actual lsa pipe to be returned
  and complete this composite call
*/
static void continue_secondary_conn(struct composite_context *ctx)
{
	struct composite_context *c;
	struct rpc_connect_dci_state *s;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct rpc_connect_dci_state);

	c->status = dcerpc_secondary_connection_recv(ctx, &s->final_pipe);
	if (!NT_STATUS_IS_OK(c->status)) {
		s->r.out.error_string = talloc_asprintf(c,
							"secondary connection failed: %s",
							nt_errstr(c->status));
		
		composite_error(c, c->status);
		return;
	}

	s->r.out.dcerpc_pipe = s->final_pipe;
	composite_done(c);
}


/**
 * Receives result of connection to rpc pipe and gets basic
 * domain info (name, sid, realm, guid)
 *
 * @param c composite context
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing return values
 * @return nt status of rpc connection
 **/

static NTSTATUS libnet_RpcConnectDCInfo_recv(struct composite_context *c, struct libnet_context *ctx,
					     TALLOC_CTX *mem_ctx, struct libnet_RpcConnect *r)
{
	NTSTATUS status;
	struct rpc_connect_dci_state *s = talloc_get_type(c->private_data,
					  struct rpc_connect_dci_state);

	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		r->out.realm        = talloc_steal(mem_ctx, s->r.out.realm);
		r->out.guid         = talloc_steal(mem_ctx, s->r.out.guid);
		r->out.domain_name  = talloc_steal(mem_ctx, s->r.out.domain_name);
		r->out.domain_sid   = talloc_steal(mem_ctx, s->r.out.domain_sid);

		r->out.dcerpc_pipe  = talloc_steal(mem_ctx, s->r.out.dcerpc_pipe);

		/* reference created pipe structure to long-term libnet_context
		   so that it can be used by other api functions even after short-term
		   mem_ctx is freed */
		if (r->in.dcerpc_iface == &dcerpc_table_samr) {
			ctx->samr_pipe = talloc_reference(ctx, r->out.dcerpc_pipe);

		} else if (r->in.dcerpc_iface == &dcerpc_table_lsarpc) {
			ctx->lsa_pipe = talloc_reference(ctx, r->out.dcerpc_pipe);
		}

	} else {
		r->out.error_string = talloc_steal(mem_ctx, s->r.out.error_string);
	}

	talloc_free(c);
	return status;
}


/**
 * Initiates connection to rpc pipe on remote server or pdc, optionally
 * providing domain info
 * 
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values
 * @return composite context of this call
 **/

struct composite_context* libnet_RpcConnect_send(struct libnet_context *ctx,
						 TALLOC_CTX *mem_ctx,
						 struct libnet_RpcConnect *r)
{
	struct composite_context *c;

	switch (r->level) {
	case LIBNET_RPC_CONNECT_SERVER:
	case LIBNET_RPC_CONNECT_BINDING:
		c = libnet_RpcConnectSrv_send(ctx, mem_ctx, r);
		break;

	case LIBNET_RPC_CONNECT_PDC:
	case LIBNET_RPC_CONNECT_DC:
		c = libnet_RpcConnectDC_send(ctx, mem_ctx, r);
		break;

	case LIBNET_RPC_CONNECT_DC_INFO:
		c = libnet_RpcConnectDCInfo_send(ctx, mem_ctx, r);
		break;

	default:
		c = talloc_zero(mem_ctx, struct composite_context);
		composite_error(c, NT_STATUS_INVALID_LEVEL);
	}

	return c;
}


/**
 * Receives result of connection to rpc pipe on remote server or pdc
 *
 * @param c composite context
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values
 * @return nt status of rpc connection
 **/

NTSTATUS libnet_RpcConnect_recv(struct composite_context *c, struct libnet_context *ctx,
				TALLOC_CTX *mem_ctx, struct libnet_RpcConnect *r)
{
	switch (r->level) {
	case LIBNET_RPC_CONNECT_SERVER:
	case LIBNET_RPC_CONNECT_BINDING:
		return libnet_RpcConnectSrv_recv(c, ctx, mem_ctx, r);

	case LIBNET_RPC_CONNECT_PDC:
	case LIBNET_RPC_CONNECT_DC:
		return libnet_RpcConnectDC_recv(c, ctx, mem_ctx, r);

	case LIBNET_RPC_CONNECT_DC_INFO:
		return libnet_RpcConnectDCInfo_recv(c, ctx, mem_ctx, r);

	default:
		ZERO_STRUCT(r->out);
		return NT_STATUS_INVALID_LEVEL;
	}
}


/**
 * Connect to a rpc pipe on a remote server - sync version
 *
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param r data structure containing necessary parameters and return values
 * @return nt status of rpc connection
 **/

NTSTATUS libnet_RpcConnect(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			   struct libnet_RpcConnect *r)
{
	struct composite_context *c;
	
	c = libnet_RpcConnect_send(ctx, mem_ctx, r);
	return libnet_RpcConnect_recv(c, ctx, mem_ctx, r);
}
