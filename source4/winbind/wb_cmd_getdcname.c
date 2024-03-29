/* 
   Unix SMB/CIFS implementation.

   Command backend for wbinfo --getdcname

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
#include "winbind/wb_server.h"
#include "smbd/service_task.h"

#include "librpc/gen_ndr/ndr_netlogon_c.h"

struct cmd_getdcname_state {
	struct composite_context *ctx;
	const char *domain_name;

	struct netr_GetAnyDCName g;
};

static void getdcname_recv_domain(struct composite_context *ctx);
static void getdcname_recv_dcname(struct rpc_request *req);

struct composite_context *wb_cmd_getdcname_send(TALLOC_CTX *mem_ctx,
						struct wbsrv_service *service,
						const char *domain_name)
{
	struct composite_context *result, *ctx;
	struct cmd_getdcname_state *state;

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct cmd_getdcname_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->domain_name = talloc_strdup(state, domain_name);
	if (state->domain_name == NULL) goto failed;

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (ctx == NULL) goto failed;

	ctx->async.fn = getdcname_recv_domain;
	ctx->async.private_data = state;
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void getdcname_recv_domain(struct composite_context *ctx)
{
	struct cmd_getdcname_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getdcname_state);
	struct wbsrv_domain *domain;
	struct rpc_request *req;

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	state->g.in.logon_server = talloc_asprintf(
		state, "\\\\%s",
		dcerpc_server_name(domain->netlogon_pipe));
	state->g.in.domainname = state->domain_name;

	req = dcerpc_netr_GetAnyDCName_send(domain->netlogon_pipe, state,
					    &state->g);
	if (composite_nomem(req, state->ctx)) return;

	composite_continue_rpc(state->ctx, req, getdcname_recv_dcname, state);
}

static void getdcname_recv_dcname(struct rpc_request *req)
{
	struct cmd_getdcname_state *state =
		talloc_get_type(req->async.private_data,
				struct cmd_getdcname_state);

	state->ctx->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(state->ctx)) return;
	state->ctx->status = werror_to_ntstatus(state->g.out.result);
	if (!composite_is_ok(state->ctx)) return;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_getdcname_recv(struct composite_context *c,
			       TALLOC_CTX *mem_ctx,
			       const char **dcname)
{
	struct cmd_getdcname_state *state =
		talloc_get_type(c->private_data, struct cmd_getdcname_state);
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		const char *p = state->g.out.dcname;
		if (*p == '\\') p += 1;
		if (*p == '\\') p += 1;
		*dcname = talloc_strdup(mem_ctx, p);
		if (*dcname == NULL) {
			status = NT_STATUS_NO_MEMORY;
		}
	}
	talloc_free(state);
	return status;
}
