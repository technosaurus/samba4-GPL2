/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Volker Lendecke 2005
   
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
/*
  a composite API for finding a DC and its name
*/

#include "includes.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "winbind/wb_async_helpers.h"
#include "winbind/wb_server.h"
#include "smbd/service_stream.h"

#include "librpc/gen_ndr/nbt.h"
#include "librpc/gen_ndr/samr.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/irpc.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "libcli/raw/libcliraw.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "libcli/auth/credentials.h"

static BOOL comp_is_ok(struct composite_context *ctx)
{
	if (NT_STATUS_IS_OK(ctx->status)) {
		return True;
	}
	ctx->state = COMPOSITE_STATE_ERROR;
	if (ctx->async.fn != NULL) {
		ctx->async.fn(ctx);
	}
	return False;
}

static void comp_error(struct composite_context *ctx, NTSTATUS status)
{
	ctx->status = status;
	SMB_ASSERT(!comp_is_ok(ctx));
}

static BOOL comp_nomem(const void *p, struct composite_context *ctx)
{
	if (p != NULL) {
		return False;
	}
	comp_error(ctx, NT_STATUS_NO_MEMORY);
	return True;
}

static void comp_done(struct composite_context *ctx)
{
	ctx->state = COMPOSITE_STATE_DONE;
	if (ctx->async.fn != NULL) {
		ctx->async.fn(ctx);
	}
}

static void comp_cont(struct composite_context *ctx,
		      struct composite_context *new_ctx,
		      void (*continuation)(struct composite_context *),
		      void *private_data)
{
	if (comp_nomem(new_ctx, ctx)) return;
	new_ctx->async.fn = continuation;
	new_ctx->async.private_data = private_data;
}

static void rpc_cont(struct composite_context *ctx,
		     struct rpc_request *new_req,
		     void (*continuation)(struct rpc_request *),
		     void *private_data)
{
	if (comp_nomem(new_req, ctx)) return;
	new_req->async.callback = continuation;
	new_req->async.private = private_data;
}

static void irpc_cont(struct composite_context *ctx,
		      struct irpc_request *new_req,
		      void (*continuation)(struct irpc_request *),
		      void *private_data)
{
	if (comp_nomem(new_req, ctx)) return;
	new_req->async.fn = continuation;
	new_req->async.private = private_data;
}

struct finddcs_state {
	struct composite_context *ctx;
	struct messaging_context *msg_ctx;

	const char *domain_name;
	const struct dom_sid *domain_sid;

	struct nbtd_getdcname r;

	int num_dcs;
	struct nbt_dc_name *dcs;
};

static void finddcs_resolve(struct composite_context *ctx);
static void finddcs_getdc(struct irpc_request *ireq);

struct composite_context *wb_finddcs_send(const char *domain_name,
					  const struct dom_sid *domain_sid,
					  struct event_context *event_ctx,
					  struct messaging_context *msg_ctx)
{
	struct composite_context *result, *ctx;
	struct finddcs_state *state;
	struct nbt_name name;

	result = talloc_zero(NULL, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = event_ctx;

	state = talloc(result, struct finddcs_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->domain_name = talloc_strdup(state, domain_name);
	if (state->domain_name == NULL) goto failed;
	state->domain_sid = dom_sid_dup(state, domain_sid);
	if (state->domain_sid == NULL) goto failed;
	state->msg_ctx = msg_ctx;

	make_nbt_name(&name, state->domain_name, 0x1c);
	ctx = resolve_name_send(&name, result->event_ctx,
				lp_name_resolve_order());

	if (ctx == NULL) goto failed;
	ctx->async.fn = finddcs_resolve;
	ctx->async.private_data = state;

	return result;

failed:
	talloc_free(result);
	return NULL;
}

static void finddcs_resolve(struct composite_context *ctx)
{
	struct finddcs_state *state =
		talloc_get_type(ctx->async.private_data, struct finddcs_state);
	struct irpc_request *ireq;
	uint32_t *nbt_servers;
	const char *address;

	state->ctx->status = resolve_name_recv(ctx, state, &address);
	if (!comp_is_ok(state->ctx)) return;

	state->num_dcs = 1;
	state->dcs = talloc_array(state, struct nbt_dc_name, state->num_dcs);
	if (comp_nomem(state->dcs, state->ctx)) return;

	state->dcs[0].address = talloc_steal(state->dcs, address);

	nbt_servers = irpc_servers_byname(state->msg_ctx, "nbt_server");
	if ((nbt_servers == NULL) || (nbt_servers[0] == 0)) {
		comp_error(state->ctx, NT_STATUS_NO_LOGON_SERVERS);
		return;
	}

	state->r.in.domainname = state->domain_name;
	state->r.in.ip_address = state->dcs[0].address;
	state->r.in.my_computername = lp_netbios_name();
	state->r.in.my_accountname = talloc_asprintf(state, "%s$",
						     lp_netbios_name());
	if (comp_nomem(state->r.in.my_accountname, state->ctx)) return;
	state->r.in.account_control = ACB_WSTRUST;
	state->r.in.domain_sid = dom_sid_dup(state, state->domain_sid);
	if (comp_nomem(state->r.in.domain_sid, state->ctx)) return;

	ireq = irpc_call_send(state->msg_ctx, nbt_servers[0],
			      &dcerpc_table_irpc, DCERPC_NBTD_GETDCNAME,
			      &state->r, state);
	irpc_cont(state->ctx, ireq, finddcs_getdc, state);
}

static void finddcs_getdc(struct irpc_request *ireq)
{
	struct finddcs_state *state =
		talloc_get_type(ireq->async.private, struct finddcs_state);

	state->ctx->status = irpc_call_recv(ireq);
	if (!comp_is_ok(state->ctx)) return;

	state->dcs[0].name = talloc_steal(state->dcs, state->r.out.dcname);
	comp_done(state->ctx);
}

NTSTATUS wb_finddcs_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
			 int *num_dcs, struct nbt_dc_name **dcs)
{
	NTSTATUS status =composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct finddcs_state *state =
			talloc_get_type(c->private_data, struct finddcs_state);
		*num_dcs = state->num_dcs;
		*dcs = talloc_steal(mem_ctx, state->dcs);
	}
	talloc_free(c);
	return status;
}

NTSTATUS wb_finddcs(const char *domain_name, const struct dom_sid *domain_sid,
		    struct event_context *event_ctx,
		    struct messaging_context *msg_ctx,
		    TALLOC_CTX *mem_ctx,
		    int *num_dcs, struct nbt_dc_name **dcs)
{
	struct composite_context *c = wb_finddcs_send(domain_name, domain_sid,
						      event_ctx, msg_ctx);
	return wb_finddcs_recv(c, mem_ctx, num_dcs, dcs);
}

struct get_schannel_creds_state {
	struct composite_context *ctx;
	struct cli_credentials *wks_creds;
	struct dcerpc_pipe *p;
	struct netr_ServerReqChallenge r;

	struct creds_CredentialState *creds_state;
	struct netr_Credential netr_cred;
	uint32_t negotiate_flags;
	struct netr_ServerAuthenticate2 a;
};

static void get_schannel_creds_recv_auth(struct rpc_request *req);
static void get_schannel_creds_recv_chal(struct rpc_request *req);
static void get_schannel_creds_recv_pipe(struct composite_context *ctx);

struct composite_context *wb_get_schannel_creds_send(struct cli_credentials *wks_creds,
						     struct smbcli_tree *tree,
						     struct event_context *ev)
{
	struct composite_context *result, *ctx;
	struct get_schannel_creds_state *state;

	result = talloc_zero(NULL, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = ev;

	state = talloc(result, struct get_schannel_creds_state);
	if (state == NULL) goto failed;
	result->private_data = state;
	state->ctx = result;

	state->wks_creds = wks_creds;

	state->p = dcerpc_pipe_init(state, ev);
	if (state->p == NULL) goto failed;

	ctx = dcerpc_pipe_open_smb_send(state->p->conn, tree, "\\netlogon");
	if (ctx == NULL) goto failed;

	ctx->async.fn = get_schannel_creds_recv_pipe;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void get_schannel_creds_recv_pipe(struct composite_context *ctx)
{
	struct get_schannel_creds_state *state =
		talloc_get_type(ctx->async.private_data,
				struct get_schannel_creds_state);
	struct rpc_request *req;

	state->ctx->status = dcerpc_pipe_open_smb_recv(ctx);
	if (!comp_is_ok(state->ctx)) return;

	state->ctx->status = dcerpc_bind_auth_none(state->p,
						   DCERPC_NETLOGON_UUID,
						   DCERPC_NETLOGON_VERSION);
	if (!comp_is_ok(state->ctx)) return;

	state->r.in.computer_name =
		cli_credentials_get_workstation(state->wks_creds);
	state->r.in.server_name =
		talloc_asprintf(state, "\\\\%s",
				dcerpc_server_name(state->p));
	if (comp_nomem(state->r.in.server_name, state->ctx)) return;

	state->r.in.credentials = talloc(state, struct netr_Credential);
	if (comp_nomem(state->r.in.credentials, state->ctx)) return;

	state->r.out.credentials = talloc(state, struct netr_Credential);
	if (comp_nomem(state->r.out.credentials, state->ctx)) return;

	generate_random_buffer(state->r.in.credentials->data,
			       sizeof(state->r.in.credentials->data));

	req = dcerpc_netr_ServerReqChallenge_send(state->p, state, &state->r);
	rpc_cont(state->ctx, req, get_schannel_creds_recv_chal, state);
}

static void get_schannel_creds_recv_chal(struct rpc_request *req)
{
	struct get_schannel_creds_state *state =
		talloc_get_type(req->async.private,
				struct get_schannel_creds_state);
	const struct samr_Password *mach_pwd;

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!comp_is_ok(state->ctx)) return;
	state->ctx->status = state->r.out.result;
	if (!comp_is_ok(state->ctx)) return;

	state->creds_state = talloc(state, struct creds_CredentialState);
	if (comp_nomem(state->creds_state, state->ctx)) return;

	mach_pwd = cli_credentials_get_nt_hash(state->wks_creds, state);
	if (comp_nomem(mach_pwd, state->ctx)) return;

	state->negotiate_flags = NETLOGON_NEG_AUTH2_FLAGS;

	creds_client_init(state->creds_state, state->r.in.credentials,
			  state->r.out.credentials, mach_pwd,
			  &state->netr_cred, state->negotiate_flags);

	state->a.in.server_name =
		talloc_reference(state, state->r.in.server_name);
	state->a.in.account_name =
		cli_credentials_get_username(state->wks_creds);
	state->a.in.secure_channel_type =
		cli_credentials_get_secure_channel_type(state->wks_creds);
	state->a.in.computer_name =
		cli_credentials_get_workstation(state->wks_creds);
	state->a.in.negotiate_flags = &state->negotiate_flags;
	state->a.out.negotiate_flags = &state->negotiate_flags;
	state->a.in.credentials = &state->netr_cred;
	state->a.out.credentials = &state->netr_cred;

	req = dcerpc_netr_ServerAuthenticate2_send(state->p, state, &state->a);
	rpc_cont(state->ctx, req, get_schannel_creds_recv_auth, state);
}

static void get_schannel_creds_recv_auth(struct rpc_request *req)
{
	struct get_schannel_creds_state *state =
		talloc_get_type(req->async.private,
				struct get_schannel_creds_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!NT_STATUS_IS_OK(state->ctx->status)) goto done;
	state->ctx->status = state->a.out.result;
	if (!NT_STATUS_IS_OK(state->ctx->status)) goto done;

	if (!creds_client_check(state->creds_state,
				state->a.out.credentials)) {
		DEBUG(5, ("Server got us invalid creds\n"));
		state->ctx->status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	cli_credentials_set_netlogon_creds(state->wks_creds, state->creds_state);

	state->ctx->state = COMPOSITE_STATE_DONE;

 done:
	if (!NT_STATUS_IS_OK(state->ctx->status)) {
		state->ctx->state = COMPOSITE_STATE_ERROR;
	}
	if ((state->ctx->state >= COMPOSITE_STATE_DONE) &&
	    (state->ctx->async.fn != NULL)) {
		state->ctx->async.fn(state->ctx);
	}
}

NTSTATUS wb_get_schannel_creds_recv(struct composite_context *c,
				    TALLOC_CTX *mem_ctx,
				    struct dcerpc_pipe **netlogon_pipe)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct get_schannel_creds_state *state =
			talloc_get_type(c->private_data,
					struct get_schannel_creds_state);
		*netlogon_pipe = talloc_steal(mem_ctx, state->p);
	}
	talloc_free(c);
	return status;
}

NTSTATUS wb_get_schannel_creds(struct cli_credentials *wks_creds,
			       struct smbcli_tree *tree,
			       struct event_context *event_ctx,
			       TALLOC_CTX *mem_ctx,
			       struct dcerpc_pipe **netlogon_pipe)
{
	struct composite_context *c =
		wb_get_schannel_creds_send(wks_creds, tree, event_ctx);
	return wb_get_schannel_creds_recv(c, mem_ctx, netlogon_pipe);
}

struct get_lsa_pipe_state {
	struct composite_context *ctx;
	const char *domain_name;
	const struct dom_sid *domain_sid;

	struct smb_composite_connect conn;
	struct dcerpc_pipe *lsa_pipe;

	struct lsa_ObjectAttribute objectattr;
	struct lsa_OpenPolicy2 openpolicy;
	struct policy_handle policy_handle;

	struct lsa_QueryInfoPolicy queryinfo;

	struct lsa_Close close;
};

static void get_lsa_pipe_recv_dcs(struct composite_context *ctx);
static void get_lsa_pipe_recv_tree(struct composite_context *ctx);
static void get_lsa_pipe_recv_pipe(struct composite_context *ctx);
static void get_lsa_pipe_recv_openpol(struct rpc_request *req);
static void get_lsa_pipe_recv_queryinfo(struct rpc_request *req);
static void get_lsa_pipe_recv_close(struct rpc_request *req);

struct composite_context *wb_get_lsa_pipe_send(struct event_context *event_ctx,
					       struct messaging_context *msg_ctx,
					       const char *domain_name,
					       const struct dom_sid *domain_sid)
{
	struct composite_context *result, *ctx;
	struct get_lsa_pipe_state *state;

	result = talloc_zero(NULL, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = event_ctx;

	state = talloc(result, struct get_lsa_pipe_state);
	if (state == NULL) goto failed;
	result->private_data = state;
	state->ctx = result;

	state->domain_name = domain_name;
	state->domain_sid = domain_sid;

	ctx = wb_finddcs_send(domain_name, domain_sid, event_ctx, msg_ctx);
	if (ctx == NULL) goto failed;

	ctx->async.fn = get_lsa_pipe_recv_dcs;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void get_lsa_pipe_recv_dcs(struct composite_context *ctx)
{
	struct get_lsa_pipe_state *state =
		talloc_get_type(ctx->async.private_data,
				struct get_lsa_pipe_state);

	int num_dcs;
	struct nbt_dc_name *dcs;

	state->ctx->status = wb_finddcs_recv(ctx, state, &num_dcs, &dcs);
	if (!comp_is_ok(state->ctx)) return;

	if (num_dcs < 1) {
		comp_error(state->ctx, NT_STATUS_NO_LOGON_SERVERS);
		return;
	}

	state->conn.in.dest_host = dcs[0].address;
	state->conn.in.port = 0;
	state->conn.in.called_name = dcs[0].name;
	state->conn.in.service = "IPC$";
	state->conn.in.service_type = "IPC";
	state->conn.in.workgroup = state->domain_name;

	state->conn.in.credentials = cli_credentials_init(state);
	if (comp_nomem(state->conn.in.credentials, state->ctx)) return;
	cli_credentials_set_conf(state->conn.in.credentials);
	cli_credentials_set_anonymous(state->conn.in.credentials);

	ctx = smb_composite_connect_send(&state->conn, state, 
					 state->ctx->event_ctx);
	comp_cont(state->ctx, ctx, get_lsa_pipe_recv_tree, state);
}

static void get_lsa_pipe_recv_tree(struct composite_context *ctx)
{
	struct get_lsa_pipe_state *state =
		talloc_get_type(ctx->async.private_data,
				struct get_lsa_pipe_state);

	state->ctx->status = smb_composite_connect_recv(ctx, state);
	if (!comp_is_ok(state->ctx)) return;

	state->lsa_pipe = dcerpc_pipe_init(state, state->ctx->event_ctx);
	if (comp_nomem(state->lsa_pipe, state->ctx)) return;

	ctx = dcerpc_pipe_open_smb_send(state->lsa_pipe->conn,
					state->conn.out.tree, "\\lsarpc");
	comp_cont(state->ctx, ctx, get_lsa_pipe_recv_pipe, state);
}

static void get_lsa_pipe_recv_pipe(struct composite_context *ctx)
{
	struct get_lsa_pipe_state *state =
		talloc_get_type(ctx->async.private_data,
				struct get_lsa_pipe_state);
	struct rpc_request *req;

	state->ctx->status = dcerpc_pipe_open_smb_recv(ctx);
	if (!comp_is_ok(state->ctx)) return;

	talloc_unlink(state, state->conn.out.tree); /* The pipe owns it now */
	state->conn.out.tree = NULL;

	state->ctx->status = dcerpc_bind_auth_none(state->lsa_pipe,
						   DCERPC_LSARPC_UUID,
						   DCERPC_LSARPC_VERSION);
	if (!comp_is_ok(state->ctx)) return;

	state->openpolicy.in.system_name =
		talloc_asprintf(state, "\\\\%s",
				dcerpc_server_name(state->lsa_pipe));
	if (comp_nomem(state->openpolicy.in.system_name, state->ctx)) return;

	ZERO_STRUCT(state->objectattr);
	state->openpolicy.in.attr = &state->objectattr;
	state->openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->openpolicy.out.handle = &state->policy_handle;

	req = dcerpc_lsa_OpenPolicy2_send(state->lsa_pipe, state,
					  &state->openpolicy);
	rpc_cont(state->ctx, req, get_lsa_pipe_recv_openpol, state);
}

static void get_lsa_pipe_recv_openpol(struct rpc_request *req)
{
	struct get_lsa_pipe_state *state =
		talloc_get_type(req->async.private, struct get_lsa_pipe_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!comp_is_ok(state->ctx)) return;
	state->ctx->status = state->openpolicy.out.result;
	if (!comp_is_ok(state->ctx)) return;

	state->queryinfo.in.handle = &state->policy_handle;
	state->queryinfo.in.level = LSA_POLICY_INFO_ACCOUNT_DOMAIN;

	req = dcerpc_lsa_QueryInfoPolicy_send(state->lsa_pipe, state,
					      &state->queryinfo);
	rpc_cont(state->ctx, req, get_lsa_pipe_recv_queryinfo, state);
}

static void get_lsa_pipe_recv_queryinfo(struct rpc_request *req)
{
	struct get_lsa_pipe_state *state =
		talloc_get_type(req->async.private, struct get_lsa_pipe_state);
	struct lsa_DomainInfo *dominfo;

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!comp_is_ok(state->ctx)) return;
	state->ctx->status = state->queryinfo.out.result;
	if (!comp_is_ok(state->ctx)) return;

	dominfo = &state->queryinfo.out.info->account_domain;

	if (strcasecmp(state->domain_name, dominfo->name.string) != 0) {
		DEBUG(2, ("Expected domain name %s, DC %s said %s\n",
			  state->domain_name,
			  dcerpc_server_name(state->lsa_pipe),
			  dominfo->name.string));
		comp_error(state->ctx, NT_STATUS_INVALID_DOMAIN_STATE);
		return;
	}

	if (!dom_sid_equal(state->domain_sid, dominfo->sid)) {
		DEBUG(2, ("Expected domain sid %s, DC %s said %s\n",
			  dom_sid_string(state, state->domain_sid),
			  dcerpc_server_name(state->lsa_pipe),
			  dom_sid_string(state, dominfo->sid)));
		comp_error(state->ctx, NT_STATUS_INVALID_DOMAIN_STATE);
		return;
	}

	state->close.in.handle = &state->policy_handle;
	state->close.out.handle = &state->policy_handle;

	req = dcerpc_lsa_Close_send(state->lsa_pipe, state,
				    &state->close);
	rpc_cont(state->ctx, req, get_lsa_pipe_recv_close, state);
}

static void get_lsa_pipe_recv_close(struct rpc_request *req)
{
	struct get_lsa_pipe_state *state =
		talloc_get_type(req->async.private, struct get_lsa_pipe_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!comp_is_ok(state->ctx)) return;
	state->ctx->status = state->close.out.result;
	if (!comp_is_ok(state->ctx)) return;

	comp_done(state->ctx);
}

NTSTATUS wb_get_lsa_pipe_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
			      struct dcerpc_pipe **pipe)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct get_lsa_pipe_state *state =
			talloc_get_type(c->private_data,
					struct get_lsa_pipe_state);
		*pipe = talloc_steal(mem_ctx, state->lsa_pipe);
	}
	talloc_free(c);
	return status;
}

NTSTATUS wb_get_lsa_pipe(struct event_context *event_ctx,
			 struct messaging_context *msg_ctx,
			 const char *domain_name,
			 const struct dom_sid *domain_sid,
			 TALLOC_CTX *mem_ctx,
			 struct dcerpc_pipe **pipe)
{
	struct composite_context *c =
		wb_get_lsa_pipe_send(event_ctx, msg_ctx, domain_name,
				     domain_sid);
	return wb_get_lsa_pipe_recv(c, mem_ctx, pipe);
}

struct lsa_lookupnames_state {
	struct composite_context *ctx;
	uint32_t num_names;
	struct lsa_LookupNames r;
	struct lsa_TransSidArray sids;
	uint32_t count;
	struct wb_sid_object **result;
};

static void lsa_lookupnames_recv_sids(struct rpc_request *req);

struct composite_context *wb_lsa_lookupnames_send(struct dcerpc_pipe *lsa_pipe,
						  struct policy_handle *handle,
						  int num_names,
						  const char **names)
{
	struct composite_context *result;
	struct rpc_request *req;
	struct lsa_lookupnames_state *state;

	struct lsa_String *lsa_names;
	int i;

	result = talloc_zero(NULL, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = lsa_pipe->conn->event_ctx;

	state = talloc(result, struct lsa_lookupnames_state);
	if (state == NULL) goto failed;
	result->private_data = state;
	state->ctx = result;

	state->sids.count = 0;
	state->sids.sids = NULL;
	state->num_names = num_names;
	state->count = 0;

	lsa_names = talloc_array(state, struct lsa_String, num_names);
	if (lsa_names == NULL) goto failed;

	for (i=0; i<num_names; i++) {
		lsa_names[i].string = names[i];
	}

	state->r.in.handle = handle;
	state->r.in.num_names = num_names;
	state->r.in.names = lsa_names;
	state->r.in.sids = &state->sids;
	state->r.in.level = 1;
	state->r.in.count = &state->count;
	state->r.out.count = &state->count;
	state->r.out.sids = &state->sids;

	req = dcerpc_lsa_LookupNames_send(lsa_pipe, state, &state->r);
	if (req == NULL) goto failed;

	req->async.callback = lsa_lookupnames_recv_sids;
	req->async.private = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void lsa_lookupnames_recv_sids(struct rpc_request *req)
{
	struct lsa_lookupnames_state *state =
		talloc_get_type(req->async.private,
				struct lsa_lookupnames_state);
	int i;

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!comp_is_ok(state->ctx)) return;
	state->ctx->status = state->r.out.result;
	if (!NT_STATUS_IS_OK(state->ctx->status) &&
	    !NT_STATUS_EQUAL(state->ctx->status, STATUS_SOME_UNMAPPED)) {
		comp_error(state->ctx, state->ctx->status);
		return;
	}

	state->result = talloc_array(state, struct wb_sid_object *,
				     state->num_names);
	if (comp_nomem(state->result, state->ctx)) return;

	for (i=0; i<state->num_names; i++) {
		struct lsa_TranslatedSid *sid = &state->r.out.sids->sids[i];
		struct lsa_TrustInformation *dom;

		state->result[i] = talloc_zero(state->result,
					       struct wb_sid_object);
		if (comp_nomem(state->result[i], state->ctx)) return;

		state->result[i]->type = sid->sid_type;
		if (state->result[i]->type == SID_NAME_UNKNOWN) {
			continue;
		}

		if (sid->sid_index >= state->r.out.domains->count) {
			comp_error(state->ctx, NT_STATUS_INVALID_PARAMETER);
			return;
		}

		dom = &state->r.out.domains->domains[sid->sid_index];

		state->result[i]->sid = dom_sid_add_rid(state->result[i],
							dom->sid, sid->rid);
	}

	comp_done(state->ctx);
}

NTSTATUS wb_lsa_lookupnames_recv(struct composite_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct wb_sid_object ***sids)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct lsa_lookupnames_state *state =
			talloc_get_type(c->private_data,
					struct lsa_lookupnames_state);
		*sids = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(c);
	return status;
}

NTSTATUS wb_lsa_lookupnames(struct dcerpc_pipe *lsa_pipe, 
			    struct policy_handle *handle,
			    int num_names, const char **names,
			    TALLOC_CTX *mem_ctx,
			    struct wb_sid_object ***sids)
{
	struct composite_context *c =
		wb_lsa_lookupnames_send(lsa_pipe, handle, num_names, names);
	return wb_lsa_lookupnames_recv(c, mem_ctx, sids);
}

struct lsa_lookupname_state {
	struct composite_context *ctx;
	struct dcerpc_pipe *lsa_pipe;
	const char *name;
	struct wb_sid_object *sid;

	struct lsa_ObjectAttribute objectattr;
	struct lsa_OpenPolicy2 openpolicy;
	struct policy_handle policy_handle;
	struct lsa_Close close;
};

static void lsa_lookupname_recv_open(struct rpc_request *req);
static void lsa_lookupname_recv_sids(struct composite_context *ctx);

struct composite_context *wb_lsa_lookupname_send(struct dcerpc_pipe *lsa_pipe,
						 const char *name)
{
	struct composite_context *result;
	struct rpc_request *req;
	struct lsa_lookupname_state *state;

	result = talloc_zero(NULL, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = lsa_pipe->conn->event_ctx;

	state = talloc(result, struct lsa_lookupname_state);
	if (state == NULL) goto failed;
	result->private_data = state;

	state->lsa_pipe = lsa_pipe;
	state->name = talloc_strdup(state, name);
	if (state->name == NULL) goto failed;
	state->ctx = result;

	state->openpolicy.in.system_name =
		talloc_asprintf(state, "\\\\%s",
				dcerpc_server_name(state->lsa_pipe));
	ZERO_STRUCT(state->objectattr);
	state->openpolicy.in.attr = &state->objectattr;
	state->openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	state->openpolicy.out.handle = &state->policy_handle;

	req = dcerpc_lsa_OpenPolicy2_send(state->lsa_pipe, state,
					  &state->openpolicy);
	if (req == NULL) goto failed;

	req->async.callback = lsa_lookupname_recv_open;
	req->async.private = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void lsa_lookupname_recv_open(struct rpc_request *req)
{
	struct lsa_lookupname_state *state =
		talloc_get_type(req->async.private,
				struct lsa_lookupname_state);
	struct composite_context *ctx;

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!comp_is_ok(state->ctx)) return;
	state->ctx->status = state->openpolicy.out.result;
	if (!comp_is_ok(state->ctx)) return;

	ctx = wb_lsa_lookupnames_send(state->lsa_pipe, &state->policy_handle,
				      1, &state->name);
	comp_cont(state->ctx, ctx, lsa_lookupname_recv_sids, state);
}

static void lsa_lookupname_recv_sids(struct composite_context *ctx)
{
	struct lsa_lookupname_state *state =
		talloc_get_type(ctx->async.private_data,
				struct lsa_lookupname_state);
	struct rpc_request *req;
	struct wb_sid_object **sids;

	state->ctx->status = wb_lsa_lookupnames_recv(ctx, state, &sids);

	if (NT_STATUS_IS_OK(state->ctx->status)) {
		state->sid = NULL;
		if (sids != NULL) {
			state->sid = sids[0];
		}
	}

	state->close.in.handle = &state->policy_handle;
	state->close.out.handle = &state->policy_handle;

	req = dcerpc_lsa_Close_send(state->lsa_pipe, state,
				    &state->close);
	if (req != NULL) {
		req->async.callback =
			(void(*)(struct rpc_request *))talloc_free;
	}

	comp_done(state->ctx);
}

NTSTATUS wb_lsa_lookupname_recv(struct composite_context *c,
				TALLOC_CTX *mem_ctx,
				struct wb_sid_object **sid)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct lsa_lookupname_state *state =
			talloc_get_type(c->private_data,
					struct lsa_lookupname_state);
		*sid = talloc_steal(mem_ctx, state->sid);
	}
	talloc_free(c);
	return status;
}

NTSTATUS wb_lsa_lookupname(struct dcerpc_pipe *lsa_pipe, const char *name,
			   TALLOC_CTX *mem_ctx, struct wb_sid_object **sid)
{
	struct composite_context *c =
		wb_lsa_lookupname_send(lsa_pipe, name);
	return wb_lsa_lookupname_recv(c, mem_ctx, sid);
}

struct cmd_lookupname_state {
	struct composite_context *ctx;
	struct wbsrv_call *call;
	struct wbsrv_domain *domain;
	const char *name;
	struct wb_sid_object *result;
};

static void cmd_lookupname_recv_lsa(struct composite_context *ctx);
static void cmd_lookupname_recv_sid(struct composite_context *ctx);

struct composite_context *wb_cmd_lookupname_send(struct wbsrv_call *call,
						 const char *name)
{
	struct composite_context *result, *ctx;
	struct cmd_lookupname_state *state;
	struct wbsrv_service *service = call->wbconn->listen_socket->service;

	result = talloc_zero(call, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = call->event_ctx;

	state = talloc(result, struct cmd_lookupname_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->call = call;
	state->name = talloc_strdup(state, name);

	state->domain = service->domains;

	if (state->domain->lsa_pipe != NULL) {
		ctx = wb_lsa_lookupname_send(state->domain->lsa_pipe, name);
		if (ctx == NULL) goto failed;
		ctx->async.fn = cmd_lookupname_recv_sid;
		ctx->async.private_data = state;
		return result;
	}

	ctx = wb_get_lsa_pipe_send(result->event_ctx, 
				   call->wbconn->conn->msg_ctx,
				   state->domain->name,
				   state->domain->sid);
	if (ctx == NULL) goto failed;
	ctx->async.fn = cmd_lookupname_recv_lsa;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void cmd_lookupname_recv_lsa(struct composite_context *ctx)
{
	struct cmd_lookupname_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_lookupname_state);
	struct wbsrv_service *service =
		state->call->wbconn->listen_socket->service;

	struct dcerpc_pipe *pipe;

	state->ctx->status = wb_get_lsa_pipe_recv(ctx, state, &pipe);
	if (!comp_is_ok(state->ctx)) return;

	if (state->domain->lsa_pipe == NULL) {
		/* Only put the new pipe in if nobody else was faster. */
		state->domain->lsa_pipe = talloc_steal(service, pipe);
	}

	ctx = wb_lsa_lookupname_send(state->domain->lsa_pipe, state->name);
	comp_cont(state->ctx, ctx, cmd_lookupname_recv_sid, state);
}

static void cmd_lookupname_recv_sid(struct composite_context *ctx)
{
	struct cmd_lookupname_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_lookupname_state);

	state->ctx->status = wb_lsa_lookupname_recv(ctx, state,
						    &state->result);
	if (!comp_is_ok(state->ctx)) return;

	comp_done(state->ctx);
}

NTSTATUS wb_cmd_lookupname_recv(struct composite_context *c,
				TALLOC_CTX *mem_ctx,
				struct wb_sid_object **sid)
{
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		struct cmd_lookupname_state *state =
			talloc_get_type(c->private_data,
					struct cmd_lookupname_state);
		*sid = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(c);
	return status;
}

NTSTATUS wb_cmd_lookupname(struct wbsrv_call *call, const char *name,
			   TALLOC_CTX *mem_ctx, struct wb_sid_object **sid)
{
	struct composite_context *c =
		wb_cmd_lookupname_send(call, name);
	return wb_cmd_lookupname_recv(c, mem_ctx, sid);
}

struct cmd_checkmachacc_state {
	struct composite_context *ctx;
	struct wbsrv_call *call;
	struct wbsrv_domain *domain;
};

static void cmd_checkmachacc_recv_lsa(struct composite_context *ctx);
static void cmd_checkmachacc_recv_creds(struct composite_context *ctx);

struct composite_context *wb_cmd_checkmachacc_send(struct wbsrv_call *call)
{
	struct composite_context *result, *ctx;
	struct cmd_checkmachacc_state *state;
	struct wbsrv_service *service = call->wbconn->listen_socket->service;

	result = talloc(call, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = call->event_ctx;

	state = talloc(result, struct cmd_checkmachacc_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;
	state->call = call;

	state->domain = service->domains;

	if (state->domain->schannel_creds != NULL) {
		talloc_free(state->domain->schannel_creds);
	}

	state->domain->schannel_creds = cli_credentials_init(service);
	if (state->domain->schannel_creds == NULL) goto failed;

	cli_credentials_set_conf(state->domain->schannel_creds);

	state->ctx->status =
		cli_credentials_set_machine_account(state->domain->
						    schannel_creds);
	if (!NT_STATUS_IS_OK(state->ctx->status)) goto failed;

	if (state->domain->netlogon_auth2_pipe != NULL) {
		talloc_free(state->domain->netlogon_auth2_pipe);
		state->domain->netlogon_auth2_pipe = NULL;
	}

	if (state->domain->lsa_pipe != NULL) {
		struct smbcli_tree *tree =
			dcerpc_smb_tree(state->domain->lsa_pipe->conn);

		if (tree == NULL) goto failed;

		ctx = wb_get_schannel_creds_send(state->domain->schannel_creds,
						 tree, result->event_ctx);
		if (ctx == NULL) goto failed;

		ctx->async.fn = cmd_checkmachacc_recv_creds;
		ctx->async.private_data = state;
		return result;
	}

	ctx = wb_get_lsa_pipe_send(result->event_ctx, 
				   call->wbconn->conn->msg_ctx,
				   state->domain->name,
				   state->domain->sid);
	if (ctx == NULL) goto failed;
	ctx->async.fn = cmd_checkmachacc_recv_lsa;
	ctx->async.private_data = state;

	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void cmd_checkmachacc_recv_lsa(struct composite_context *ctx)
{
	struct cmd_checkmachacc_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_checkmachacc_state);
	struct wbsrv_service *service =
		state->call->wbconn->listen_socket->service;

	struct dcerpc_pipe *pipe;
	struct smbcli_tree *tree;

	state->ctx->status = wb_get_lsa_pipe_recv(ctx, state, &pipe);
	if (!comp_is_ok(state->ctx)) return;

	if (state->domain->lsa_pipe == NULL) {
		/* We gonna drop "our" pipe if someone else was faster */
		state->domain->lsa_pipe = talloc_steal(service, pipe);
	}

	tree = dcerpc_smb_tree(state->domain->lsa_pipe->conn);

	if (tree == NULL) {
		comp_error(state->ctx, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	ctx = wb_get_schannel_creds_send(state->domain->schannel_creds, tree,
					 state->ctx->event_ctx);
	comp_cont(state->ctx, ctx, cmd_checkmachacc_recv_creds, state);
}

static void cmd_checkmachacc_recv_creds(struct composite_context *ctx)
{
	struct cmd_checkmachacc_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_checkmachacc_state);
	struct wbsrv_service *service =
		state->call->wbconn->listen_socket->service;
	struct dcerpc_pipe *pipe;

	state->ctx->status = wb_get_schannel_creds_recv(ctx, state, &pipe);
	if (!comp_is_ok(state->ctx)) return;

	if (state->domain->netlogon_auth2_pipe != NULL) {
		/* Someone else was faster, we need to replace it with our
		 * pipe */
		talloc_free(state->domain->netlogon_auth2_pipe);
	}

	state->domain->netlogon_auth2_pipe = talloc_steal(service, pipe);

	comp_done(state->ctx);
}

NTSTATUS wb_cmd_checkmachacc_recv(struct composite_context *c)
{
	return composite_wait(c);
}

NTSTATUS wb_cmd_checkmachacc(struct wbsrv_call *call)
{
	struct composite_context *c = wb_cmd_checkmachacc_send(call);
	return wb_cmd_checkmachacc_recv(c);
}

struct get_netlogon_pipe_state {
	struct composite_context *ctx;
	struct wbsrv_call *call;
	struct wbsrv_domain *domain;
	struct dcerpc_pipe *p;
};

static void get_netlogon_pipe_recv_machacc(struct composite_context *ctx);
static void get_netlogon_pipe_recv_pipe(struct composite_context *ctx);

struct composite_context *wb_get_netlogon_pipe_send(struct wbsrv_call *call)
{
	struct composite_context *result, *ctx;
	struct get_netlogon_pipe_state *state;
	struct wbsrv_service *service = call->wbconn->listen_socket->service;

	result = talloc(call, struct composite_context);
	if (result == NULL) goto failed;
	result->state = COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx = call->event_ctx;

	state = talloc(result, struct get_netlogon_pipe_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;
	state->call = call;

	state->domain = service->domains;

	if (state->domain->netlogon_pipe != NULL) {
		talloc_free(state->domain->netlogon_pipe);
		state->domain->netlogon_pipe = NULL;
	}

	ctx = wb_cmd_checkmachacc_send(call);
	if (ctx == NULL) goto failed;
	ctx->async.fn = get_netlogon_pipe_recv_machacc;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void get_netlogon_pipe_recv_machacc(struct composite_context *ctx)
{
	struct get_netlogon_pipe_state *state =
		talloc_get_type(ctx->async.private_data,
				struct get_netlogon_pipe_state);
	
	struct smbcli_tree *tree = NULL;

	state->ctx->status = wb_cmd_checkmachacc_recv(ctx);
	if (!comp_is_ok(state->ctx)) return;

	state->p = dcerpc_pipe_init(state, state->ctx->event_ctx);
	if (comp_nomem(state->p, state->ctx)) return;

	if (state->domain->lsa_pipe != NULL) {
		tree = dcerpc_smb_tree(state->domain->lsa_pipe->conn);
	}

	if (tree == NULL) {
		comp_error(state->ctx, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	ctx = dcerpc_pipe_open_smb_send(state->p->conn, tree, "\\netlogon");
	comp_cont(state->ctx, ctx, get_netlogon_pipe_recv_pipe, state);
}

static void get_netlogon_pipe_recv_pipe(struct composite_context *ctx)
{
	struct get_netlogon_pipe_state *state =
		talloc_get_type(ctx->async.private_data,
				struct get_netlogon_pipe_state);
	struct wbsrv_service *service =
		state->call->wbconn->listen_socket->service;
	
	state->ctx->status = dcerpc_pipe_open_smb_recv(ctx);
	if (!comp_is_ok(state->ctx)) return;

	state->p->conn->flags |= (DCERPC_SIGN | DCERPC_SEAL);
	state->ctx->status =
		dcerpc_bind_auth_password(state->p,
					  DCERPC_NETLOGON_UUID,
					  DCERPC_NETLOGON_VERSION, 
					  state->domain->schannel_creds,
					  DCERPC_AUTH_TYPE_SCHANNEL,
					  NULL);
	if (!comp_is_ok(state->ctx)) return;

	state->domain->netlogon_pipe = talloc_steal(service, state->p);
	comp_done(state->ctx);
}

NTSTATUS wb_get_netlogon_pipe_recv(struct composite_context *c)
{
	return composite_wait(c);
}

NTSTATUS wb_get_netlogon_pipe(struct wbsrv_call *call)
{
	struct composite_context *c = wb_get_netlogon_pipe_send(call);
	return wb_get_netlogon_pipe_recv(c);
}
