/* 
   Unix SMB/CIFS implementation.
   Main winbindd samba3 server routines

   Copyright (C) Stefan Metzmacher	2005
   Copyright (C) Volker Lendecke	2005

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
#include "smbd/service_stream.h"
#include "nsswitch/winbind_nss_config.h"
#include "nsswitch/winbindd_nss.h"
#include "winbind/wb_server.h"
#include "winbind/wb_samba3_protocol.h"
#include "winbind/wb_async_helpers.h"
#include "librpc/gen_ndr/nbt.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "include/version.h"
#include "lib/events/events.h"
#include "librpc/gen_ndr/ndr_netlogon.h"

static void wbsrv_samba3_async_epilogue(NTSTATUS status,
					struct wbsrv_samba3_call *s3call)
{
	if (!NT_STATUS_IS_OK(status)) {
		struct winbindd_response *resp = &s3call->response;
		resp->result = WINBINDD_ERROR;
		WBSRV_SAMBA3_SET_STRING(resp->data.auth.nt_status_string,
					nt_errstr(status));
		WBSRV_SAMBA3_SET_STRING(resp->data.auth.error_string,
					nt_errstr(status));
		resp->data.auth.pam_error = nt_status_to_pam(status);
	}

	status = wbsrv_send_reply(s3call->call);
	if (!NT_STATUS_IS_OK(status)) {
		wbsrv_terminate_connection(s3call->call->wbconn,
					   "wbsrv_queue_reply() failed");
	}
}

NTSTATUS wbsrv_samba3_interface_version(struct wbsrv_samba3_call *s3call)
{
	s3call->response.result			= WINBINDD_OK;
	s3call->response.data.interface_version	= WINBIND_INTERFACE_VERSION;
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_info(struct wbsrv_samba3_call *s3call)
{
	s3call->response.result			= WINBINDD_OK;
	s3call->response.data.info.winbind_separator = *lp_winbind_separator();
	WBSRV_SAMBA3_SET_STRING(s3call->response.data.info.samba_version,
				SAMBA_VERSION_STRING);
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_domain_name(struct wbsrv_samba3_call *s3call)
{
	s3call->response.result			= WINBINDD_OK;
	WBSRV_SAMBA3_SET_STRING(s3call->response.data.domain_name,
				lp_workgroup());
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_netbios_name(struct wbsrv_samba3_call *s3call)
{
	s3call->response.result			= WINBINDD_OK;
	WBSRV_SAMBA3_SET_STRING(s3call->response.data.netbios_name,
				lp_netbios_name());
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_priv_pipe_dir(struct wbsrv_samba3_call *s3call)
{
	s3call->response.result			= WINBINDD_OK;
	s3call->response.extra_data =
		smbd_tmp_path(s3call, WINBINDD_SAMBA3_PRIVILEGED_SOCKET);
	NT_STATUS_HAVE_NO_MEMORY(s3call->response.extra_data);
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_ping(struct wbsrv_samba3_call *s3call)
{
	s3call->response.result			= WINBINDD_OK;
	return NT_STATUS_OK;
}

static void checkmachacc_recv_creds(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_check_machacc(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;

	DEBUG(5, ("wbsrv_samba3_check_machacc called\n"));

	ctx = wb_cmd_checkmachacc_send(s3call->call);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = checkmachacc_recv_creds;
	ctx->async.private_data = s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}
	
static void checkmachacc_recv_creds(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;

	status = wb_cmd_checkmachacc_recv(ctx);

	s3call->response.result = WINBINDD_OK;
	wbsrv_samba3_async_epilogue(status, s3call);
}

static void getdcname_recv_dc(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getdcname(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_getdcname called\n"));

	ctx = wb_cmd_getdcname_send(service, service->domains,
				    s3call->request.domain_name);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getdcname_recv_dc;
	ctx->async.private_data = s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getdcname_recv_dc(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	const char *dcname;
	NTSTATUS status;

	status = wb_cmd_getdcname_recv(ctx, s3call, &dcname);
	if (!NT_STATUS_IS_OK(status)) goto done;

	s3call->response.result = WINBINDD_OK;
	WBSRV_SAMBA3_SET_STRING(s3call->response.data.dc_name, dcname);

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

static void userdomgroups_recv_groups(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_userdomgroups(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_userdomgroups called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request.data.sid);
	if (sid == NULL) {
		DEBUG(5, ("Could not parse sid %s\n",
			  s3call->request.data.sid));
		return NT_STATUS_NO_MEMORY;
	}

	ctx = wb_cmd_userdomgroups_send(
		s3call->call->wbconn->listen_socket->service, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = userdomgroups_recv_groups;
	ctx->async.private_data = s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void userdomgroups_recv_groups(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	int i, num_sids;
	struct dom_sid **sids;
	char *sids_string;
	NTSTATUS status;

	status = wb_cmd_userdomgroups_recv(ctx, s3call, &num_sids, &sids);
	if (!NT_STATUS_IS_OK(status)) goto done;

	sids_string = talloc_strdup(s3call, "");
	if (sids_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<num_sids; i++) {
		sids_string = talloc_asprintf_append(
			sids_string, "%s\n", dom_sid_string(s3call, sids[i]));
	}

	if (sids_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	s3call->response.result = WINBINDD_OK;
	s3call->response.extra_data = sids_string;
	s3call->response.length += strlen(sids_string)+1;
	s3call->response.data.num_entries = num_sids;

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

static void usersids_recv_sids(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_usersids(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_usersids called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request.data.sid);
	if (sid == NULL) {
		DEBUG(5, ("Could not parse sid %s\n",
			  s3call->request.data.sid));
		return NT_STATUS_NO_MEMORY;
	}

	ctx = wb_cmd_usersids_send(
		s3call->call->wbconn->listen_socket->service, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = usersids_recv_sids;
	ctx->async.private_data = s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void usersids_recv_sids(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	int i, num_sids;
	struct dom_sid **sids;
	char *sids_string;
	NTSTATUS status;

	status = wb_cmd_usersids_recv(ctx, s3call, &num_sids, &sids);
	if (!NT_STATUS_IS_OK(status)) goto done;

	sids_string = talloc_strdup(s3call, "");
	if (sids_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<num_sids; i++) {
		sids_string = talloc_asprintf_append(
			sids_string, "%s\n", dom_sid_string(s3call, sids[i]));
		if (sids_string == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

	s3call->response.result = WINBINDD_OK;
	s3call->response.extra_data = sids_string;
	s3call->response.length += strlen(sids_string);
	s3call->response.data.num_entries = num_sids;

	/* Hmmmm. Nasty protocol -- who invented the zeros between the
	 * SIDs? Hmmm. Could have been me -- vl */

	while (*sids_string != '\0') {
		if ((*sids_string) == '\n') {
			*sids_string = '\0';
		}
		sids_string += 1;
	}

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

static void lookupname_recv_sid(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_lookupname(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_lookupname called\n"));

	ctx = wb_cmd_lookupname_send(service, service->domains,
				     s3call->request.data.name.dom_name,
				     s3call->request.data.name.name);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	/* setup the callbacks */
	ctx->async.fn = lookupname_recv_sid;
	ctx->async.private_data	= s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void lookupname_recv_sid(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	struct wb_sid_object *sid;
	NTSTATUS status;

	status = wb_cmd_lookupname_recv(ctx, s3call, &sid);
	if (!NT_STATUS_IS_OK(status)) goto done;

	s3call->response.result = WINBINDD_OK;
	s3call->response.data.sid.type = sid->type;
	WBSRV_SAMBA3_SET_STRING(s3call->response.data.sid.sid,
				dom_sid_string(s3call, sid->sid));

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

static void lookupsid_recv_name(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_lookupsid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->call->wbconn->listen_socket->service;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_lookupsid called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request.data.sid);
	if (sid == NULL) {
		DEBUG(5, ("Could not parse sid %s\n",
			  s3call->request.data.sid));
		return NT_STATUS_NO_MEMORY;
	}

	ctx = wb_cmd_lookupsid_send(service, service->domains, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	/* setup the callbacks */
	ctx->async.fn = lookupsid_recv_name;
	ctx->async.private_data	= s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void lookupsid_recv_name(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	struct wb_sid_object *sid;
	NTSTATUS status;

	status = wb_cmd_lookupsid_recv(ctx, s3call, &sid);
	if (!NT_STATUS_IS_OK(status)) goto done;

	s3call->response.result = WINBINDD_OK;
	s3call->response.data.name.type = sid->type;
	WBSRV_SAMBA3_SET_STRING(s3call->response.data.name.dom_name,
				sid->domain);
	WBSRV_SAMBA3_SET_STRING(s3call->response.data.name.name, sid->name);

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

NTSTATUS wbsrv_samba3_pam_auth(struct wbsrv_samba3_call *s3call)
{
	s3call->response.result			= WINBINDD_ERROR;
	return NT_STATUS_OK;
}

#if 0
static BOOL samba3_parse_domuser(TALLOC_CTX *mem_ctx, const char *domuser,
				 char **domain, char **user)
{
	char *p = strchr(domuser, *lp_winbind_separator());

	if (p == NULL) {
		*domain = talloc_strdup(mem_ctx, lp_workgroup());
	} else {
		*domain = talloc_strndup(mem_ctx, domuser,
					 PTR_DIFF(p, domuser));
		domuser = p+1;
	}

	*user = talloc_strdup(mem_ctx, domuser);

	return ((*domain != NULL) && (*user != NULL));
}
#endif

static void pam_auth_crap_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_pam_auth_crap(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;

	DATA_BLOB chal, nt_resp, lm_resp;

	DEBUG(5, ("wbsrv_samba3_pam_auth_crap called\n"));

	chal.data      = s3call->request.data.auth_crap.chal;
	chal.length    = sizeof(s3call->request.data.auth_crap.chal);
	nt_resp.data   = s3call->request.data.auth_crap.nt_resp;
	nt_resp.length = s3call->request.data.auth_crap.nt_resp_len;
	lm_resp.data   = s3call->request.data.auth_crap.lm_resp;
	lm_resp.length = s3call->request.data.auth_crap.lm_resp_len;

	ctx = wb_cmd_pam_auth_crap_send(
		s3call->call, 
		s3call->request.data.auth_crap.domain,
		s3call->request.data.auth_crap.user,
		s3call->request.data.auth_crap.workstation,
		chal, nt_resp, lm_resp);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = pam_auth_crap_recv;
	ctx->async.private_data = s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void pam_auth_crap_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	struct winbindd_response *resp = &s3call->response;
	NTSTATUS status;
	DATA_BLOB info3;
	struct netr_UserSessionKey user_session_key;
	struct netr_LMSessionKey lm_key;

	status = wb_cmd_pam_auth_crap_recv(ctx, s3call, &info3,
					   &user_session_key, &lm_key);
	if (!NT_STATUS_IS_OK(status)) goto done;

	if (s3call->request.flags & WBFLAG_PAM_USER_SESSION_KEY) {
		memcpy(s3call->response.data.auth.user_session_key, 
		       &user_session_key.key,
		       sizeof(s3call->response.data.auth.user_session_key));
	}

	if (s3call->request.flags & WBFLAG_PAM_INFO3_NDR) {
		s3call->response.extra_data = info3.data;
		s3call->response.length += info3.length;
	}

	if (s3call->request.flags & WBFLAG_PAM_LMKEY) {
		memcpy(s3call->response.data.auth.first_8_lm_hash, 
		       lm_key.key,
		       sizeof(s3call->response.data.auth.first_8_lm_hash));
	}
	
	resp->result = WINBINDD_OK;

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

static void list_trustdom_recv_doms(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_list_trustdom(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_list_trustdom called\n"));

	ctx = wb_cmd_list_trustdoms_send(service);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = list_trustdom_recv_doms;
	ctx->async.private_data = s3call;
	s3call->call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void list_trustdom_recv_doms(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	int i, num_domains;
	struct wb_dom_info **domains;
	NTSTATUS status;
	char *result;

	status = wb_cmd_list_trustdoms_recv(ctx, s3call, &num_domains,
					    &domains);
	if (!NT_STATUS_IS_OK(status)) goto done;

	result = talloc_strdup(s3call, "");
	if (result == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<num_domains; i++) {
		result = talloc_asprintf_append(
			result, "%s\\%s\\%s",
			domains[i]->name, domains[i]->name,
			dom_sid_string(s3call, domains[i]->sid));
	}

	if (result == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	s3call->response.result = WINBINDD_OK;
	if (num_domains > 0) {
		s3call->response.extra_data = result;
		s3call->response.length += strlen(result)+1;
	}

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}
