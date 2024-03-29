/* 
   Unix SMB/CIFS implementation.

   Get a struct wb_dom_info for a trusted domain, relying on "our" DC.

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

#include "includes.h"
#include "libcli/composite/composite.h"
#include "libcli/resolve/resolve.h"
#include "libcli/security/security.h"
#include "winbind/wb_server.h"
#include "smbd/service_task.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "libcli/libcli.h"

struct trusted_dom_info_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct wbsrv_domain *my_domain;

	struct netr_DsRGetDCName d;
	struct netr_GetAnyDCName g;

	struct wb_dom_info *info;
};

static void trusted_dom_info_recv_domain(struct composite_context *ctx);
static void trusted_dom_info_recv_dsr(struct rpc_request *req);
static void trusted_dom_info_recv_dcname(struct rpc_request *req);
static void trusted_dom_info_recv_dcaddr(struct composite_context *ctx);

struct composite_context *wb_trusted_dom_info_send(TALLOC_CTX *mem_ctx,
						   struct wbsrv_service *service,
						   const char *domain_name,
						   const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct trusted_dom_info_state *state;

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct trusted_dom_info_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->info = talloc_zero(state, struct wb_dom_info);
	if (state->info == NULL) goto failed;

	state->service = service;

	state->info->sid = dom_sid_dup(state->info, sid);
	if (state->info->sid == NULL) goto failed;

	state->info->name = talloc_strdup(state->info, domain_name);
	if (state->info->name == NULL) goto failed;

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (ctx == NULL) goto failed;

	ctx->async.fn = trusted_dom_info_recv_domain;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void trusted_dom_info_recv_domain(struct composite_context *ctx)
{
	struct trusted_dom_info_state *state =
		talloc_get_type(ctx->async.private_data,
				struct trusted_dom_info_state);
	struct rpc_request *req;

	state->ctx->status = wb_sid2domain_recv(ctx, &state->my_domain);
	if (!composite_is_ok(state->ctx)) return;

	state->d.in.server_unc =
		talloc_asprintf(state, "\\\\%s",
				dcerpc_server_name(state->my_domain->netlogon_pipe));
	if (composite_nomem(state->d.in.server_unc,
			    state->ctx)) return;

	state->d.in.domain_name = state->info->name;
	state->d.in.domain_guid = NULL;
	state->d.in.site_guid = NULL;
	state->d.in.flags = DS_RETURN_DNS_NAME;

	req = dcerpc_netr_DsRGetDCName_send(state->my_domain->netlogon_pipe,
					    state, &state->d);
	composite_continue_rpc(state->ctx, req, trusted_dom_info_recv_dsr,
			       state);
}

/*
 * dcerpc_netr_DsRGetDCName has replied
 */

static void trusted_dom_info_recv_dsr(struct rpc_request *req)
{
	struct trusted_dom_info_state *state =
		talloc_get_type(req->async.private_data,
				struct trusted_dom_info_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!NT_STATUS_IS_OK(state->ctx->status)) {
		DEBUG(9, ("dcerpc_ndr_request_recv returned %s\n",
			  nt_errstr(state->ctx->status)));
		goto fallback;
	}

	state->ctx->status =
		werror_to_ntstatus(state->d.out.result);
	if (!NT_STATUS_IS_OK(state->ctx->status)) {
		DEBUG(9, ("dsrgetdcname returned %s\n",
			  nt_errstr(state->ctx->status)));
		goto fallback;
	}

	/* Hey, that was easy! */
	state->info->num_dcs = 1;
	state->info->dcs = talloc(state->info, struct nbt_dc_name);
	state->info->dcs[0].name = talloc_steal(state->info,
					    state->d.out.info->dc_unc);
	if (*state->info->dcs[0].name == '\\') state->info->dcs[0].name++;
	if (*state->info->dcs[0].name == '\\') state->info->dcs[0].name++;

	state->info->dcs[0].address = talloc_steal(state->info,
					       state->d.out.info->dc_address);
	if (*state->info->dcs[0].address == '\\') state->info->dcs[0].address++;
	if (*state->info->dcs[0].address == '\\') state->info->dcs[0].address++;

	state->info->dns_name = talloc_steal(state->info,
					     state->d.out.info->domain_name);

	composite_done(state->ctx);
	return;

 fallback:

	state->g.in.logon_server = talloc_asprintf(
		state, "\\\\%s",
		dcerpc_server_name(state->my_domain->netlogon_pipe));
	state->g.in.domainname = state->info->name;

	req = dcerpc_netr_GetAnyDCName_send(state->my_domain->netlogon_pipe,
					    state, &state->g);
	if (composite_nomem(req, state->ctx)) return;

	composite_continue_rpc(state->ctx, req, trusted_dom_info_recv_dcname,
			       state);
}

static void trusted_dom_info_recv_dcname(struct rpc_request *req)
{
	struct trusted_dom_info_state *state =
		talloc_get_type(req->async.private_data,
				struct trusted_dom_info_state);
	struct composite_context *ctx;
	struct nbt_name name;

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = werror_to_ntstatus(state->g.out.result);
	if (!composite_is_ok(state->ctx)) return;

	/* Hey, that was easy! */
	state->info->num_dcs = 1;
	state->info->dcs = talloc(state->info, struct nbt_dc_name);
	state->info->dcs[0].name = talloc_steal(state->info,
					    state->g.out.dcname);
	if (*state->info->dcs[0].name == '\\') state->info->dcs[0].name++;
	if (*state->info->dcs[0].name == '\\') state->info->dcs[0].name++;
	
	make_nbt_name(&name, state->info->dcs[0].name, 0x20);
	ctx = resolve_name_send(&name, state->service->task->event_ctx,
				lp_name_resolve_order());

	composite_continue(state->ctx, ctx, trusted_dom_info_recv_dcaddr,
			   state);
}

static void trusted_dom_info_recv_dcaddr(struct composite_context *ctx)
{
	struct trusted_dom_info_state *state =
		talloc_get_type(ctx->async.private_data,
				struct trusted_dom_info_state);

	state->ctx->status = resolve_name_recv(ctx, state->info,
					       &state->info->dcs[0].address);
	if (!composite_is_ok(state->ctx)) return;

	composite_done(state->ctx);
}

NTSTATUS wb_trusted_dom_info_recv(struct composite_context *ctx,
				  TALLOC_CTX *mem_ctx,
				  struct wb_dom_info **result)
{
	NTSTATUS status = composite_wait(ctx);
	if (NT_STATUS_IS_OK(status)) {
		struct trusted_dom_info_state *state =
			talloc_get_type(ctx->private_data,
					struct trusted_dom_info_state);
		*result = talloc_steal(mem_ctx, state->info);
	}
	talloc_free(ctx);
	return status;
}

NTSTATUS wb_trusted_dom_info(TALLOC_CTX *mem_ctx,
			     struct wbsrv_service *service,
			     const char *domain_name,
			     const struct dom_sid *sid,
			     struct wb_dom_info **result)
{
	struct composite_context *ctx =
		wb_trusted_dom_info_send(mem_ctx, service, domain_name, sid);
	return wb_trusted_dom_info_recv(ctx, mem_ctx, result);
}
