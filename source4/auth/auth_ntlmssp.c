/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   handle NLTMSSP, server side

   Copyright (C) Andrew Tridgell      2001
   Copyright (C) Andrew Bartlett 2001-2003

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

/**
 * Return the challenge as determined by the authentication subsystem 
 * @return an 8 byte random challenge
 */

static const uint8_t *auth_ntlmssp_get_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	struct auth_ntlmssp_state *auth_ntlmssp_state = ntlmssp_state->auth_context;

	return auth_ntlmssp_state->auth_context->get_ntlm_challenge(auth_ntlmssp_state->auth_context);
}

/**
 * Some authentication methods 'fix' the challenge, so we may not be able to set it
 *
 * @return If the effective challenge used by the auth subsystem may be modified
 */
static BOOL auth_ntlmssp_may_set_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	struct auth_ntlmssp_state *auth_ntlmssp_state = ntlmssp_state->auth_context;

	return auth_ntlmssp_state->auth_context->challenge_may_be_modified;
}

/**
 * NTLM2 authentication modifies the effective challange, 
 * @param challenge The new challenge value
 */
static NTSTATUS auth_ntlmssp_set_challenge(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *challenge)
{
	struct auth_ntlmssp_state *auth_ntlmssp_state = ntlmssp_state->auth_context;
	struct auth_context *auth_context = auth_ntlmssp_state->auth_context;

	SMB_ASSERT(challenge->length == 8);

	auth_context->challenge = data_blob_talloc(auth_context->mem_ctx, 
						   challenge->data, challenge->length);

	auth_context->challenge_set_by = "NTLMSSP callback (NTLM2)";

	DEBUG(5, ("auth_context challenge set by %s\n", auth_context->challenge_set_by));
	DEBUG(5, ("challenge is: \n"));
	dump_data(5, (const char *)auth_context->challenge.data, auth_context->challenge.length);
	return NT_STATUS_OK;
}

/**
 * Check the password on an NTLMSSP login.  
 *
 * Return the session keys used on the connection.
 */

static NTSTATUS auth_ntlmssp_check_password(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *user_session_key, DATA_BLOB *lm_session_key) 
{
	struct auth_ntlmssp_state *auth_ntlmssp_state = ntlmssp_state->auth_context;
	auth_usersupplied_info *user_info = NULL;
	NTSTATUS nt_status;

#if 0
	/* the client has given us its machine name (which we otherwise would not get on port 445).
	   we need to possibly reload smb.conf if smb.conf includes depend on the machine name */

	set_remote_machine_name(auth_ntlmssp_state->ntlmssp_state->workstation, True);

	/* setup the string used by %U */
	/* sub_set_smb_name checks for weird internally */
	sub_set_smb_name(auth_ntlmssp_state->ntlmssp_state->user);

	reload_services(True);

#endif
	nt_status = make_user_info_map(&user_info, 
				       auth_ntlmssp_state->ntlmssp_state->user, 
				       auth_ntlmssp_state->ntlmssp_state->domain, 
				       auth_ntlmssp_state->ntlmssp_state->workstation, 
	                               auth_ntlmssp_state->ntlmssp_state->lm_resp.data ? &auth_ntlmssp_state->ntlmssp_state->lm_resp : NULL, 
	                               auth_ntlmssp_state->ntlmssp_state->nt_resp.data ? &auth_ntlmssp_state->ntlmssp_state->nt_resp : NULL, 
				       NULL, NULL, NULL,
				       True);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	nt_status = auth_ntlmssp_state->auth_context->check_ntlm_password(auth_ntlmssp_state->auth_context, 
									  user_info, &auth_ntlmssp_state->server_info); 

	free_user_info(&user_info);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	if (auth_ntlmssp_state->server_info->user_session_key.length) {
		DEBUG(10, ("Got NT session key of length %u\n", auth_ntlmssp_state->server_info->user_session_key.length));
		*user_session_key = data_blob_talloc(ntlmssp_state->mem_ctx, 
						   auth_ntlmssp_state->server_info->user_session_key.data,
						   auth_ntlmssp_state->server_info->user_session_key.length);
	}
	if (auth_ntlmssp_state->server_info->lm_session_key.length) {
		DEBUG(10, ("Got LM session key of length %u\n", auth_ntlmssp_state->server_info->lm_session_key.length));
		*lm_session_key = data_blob_talloc(ntlmssp_state->mem_ctx, 
						   auth_ntlmssp_state->server_info->lm_session_key.data,
						   auth_ntlmssp_state->server_info->lm_session_key.length);
	}
	return nt_status;
}

NTSTATUS auth_ntlmssp_start(struct auth_ntlmssp_state **auth_ntlmssp_state)
{
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("AUTH NTLMSSP context");
	
	*auth_ntlmssp_state = talloc_zero(mem_ctx, sizeof(**auth_ntlmssp_state));
	if (!*auth_ntlmssp_state) {
		DEBUG(0,("auth_ntlmssp_start: talloc failed!\n"));
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCTP(*auth_ntlmssp_state);

	(*auth_ntlmssp_state)->mem_ctx = mem_ctx;

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_server_start(&(*auth_ntlmssp_state)->ntlmssp_state))) {
		return nt_status;
	}

	if (!NT_STATUS_IS_OK(nt_status = make_auth_context_subsystem(&(*auth_ntlmssp_state)->auth_context))) {
		return nt_status;
	}

	(*auth_ntlmssp_state)->ntlmssp_state->auth_context = (*auth_ntlmssp_state);
	(*auth_ntlmssp_state)->ntlmssp_state->get_challenge = auth_ntlmssp_get_challenge;
	(*auth_ntlmssp_state)->ntlmssp_state->may_set_challenge = auth_ntlmssp_may_set_challenge;
	(*auth_ntlmssp_state)->ntlmssp_state->set_challenge = auth_ntlmssp_set_challenge;
	(*auth_ntlmssp_state)->ntlmssp_state->check_password = auth_ntlmssp_check_password;
	(*auth_ntlmssp_state)->ntlmssp_state->server_role = lp_server_role();
	
	return NT_STATUS_OK;
}

void auth_ntlmssp_end(struct auth_ntlmssp_state **auth_ntlmssp_state)
{
	TALLOC_CTX *mem_ctx = (*auth_ntlmssp_state)->mem_ctx;

	if ((*auth_ntlmssp_state)->ntlmssp_state) {
		ntlmssp_end(&(*auth_ntlmssp_state)->ntlmssp_state);
	}
	if ((*auth_ntlmssp_state)->auth_context) {
		((*auth_ntlmssp_state)->auth_context->free)(&(*auth_ntlmssp_state)->auth_context);
	}
	if ((*auth_ntlmssp_state)->server_info) {
		free_server_info(&(*auth_ntlmssp_state)->server_info);
	}
	talloc_destroy(mem_ctx);
	*auth_ntlmssp_state = NULL;
}


/**
 * Next state function for the wrapped NTLMSSP state machine
 * 
 * @param auth_ntlmssp_state NTLMSSP State
 * @param out_mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param in The request, as a DATA_BLOB
 * @param out The reply, as an talloc()ed DATA_BLOB, on *out_mem_ctx
 * @return Error, MORE_PROCESSING_REQUIRED if a reply is sent, 
 *                or NT_STATUS_OK if the user is authenticated. 
 */

NTSTATUS auth_ntlmssp_update(struct auth_ntlmssp_state *auth_ntlmssp_state, 
			     TALLOC_CTX *out_mem_ctx,
			     const DATA_BLOB in, DATA_BLOB *out) 
{
	return ntlmssp_update(auth_ntlmssp_state->ntlmssp_state, 
			      out_mem_ctx, 
			      in, out);
}
