/* 
   Unix SMB/CIFS implementation.

   dcerpc authentication operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004
   Copyright (C) Stefan Metzmacher 2005
   
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
#include "auth/auth.h"

struct gensec_ntlmssp_state {
	struct auth_context *auth_context;
	struct auth_serversupplied_info *server_info;
	struct ntlmssp_state *ntlmssp_state;
	uint32 have_features;
};


/**
 * Return the challenge as determined by the authentication subsystem 
 * @return an 8 byte random challenge
 */

static const uint8_t *auth_ntlmssp_get_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = ntlmssp_state->auth_context;
	NTSTATUS status;
	const uint8_t *chal;

	status = auth_get_challenge(gensec_ntlmssp_state->auth_context, &chal);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return chal;
}

/**
 * Some authentication methods 'fix' the challenge, so we may not be able to set it
 *
 * @return If the effective challenge used by the auth subsystem may be modified
 */
static BOOL auth_ntlmssp_may_set_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = ntlmssp_state->auth_context;

	return auth_challenge_may_be_modified(gensec_ntlmssp_state->auth_context);
}

/**
 * NTLM2 authentication modifies the effective challenge, 
 * @param challenge The new challenge value
 */
static NTSTATUS auth_ntlmssp_set_challenge(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *challenge)
{
	NTSTATUS nt_status;
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = ntlmssp_state->auth_context;
	struct auth_context *auth_context = gensec_ntlmssp_state->auth_context;
	const uint8_t *chal;

	if (challenge->length != 8) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	chal = challenge->data;

	nt_status = auth_context_set_challenge(auth_context, chal, "NTLMSSP callback (NTLM2)");

	return nt_status;
}

/**
 * Check the password on an NTLMSSP login.  
 *
 * Return the session keys used on the connection.
 */

static NTSTATUS auth_ntlmssp_check_password(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *user_session_key, DATA_BLOB *lm_session_key) 
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = ntlmssp_state->auth_context;
	struct auth_usersupplied_info *user_info = NULL;
	NTSTATUS nt_status;

	nt_status = make_user_info_map(ntlmssp_state, 
				       gensec_ntlmssp_state->ntlmssp_state->user, 
				       gensec_ntlmssp_state->ntlmssp_state->domain, 
				       gensec_ntlmssp_state->ntlmssp_state->workstation, 
	                               gensec_ntlmssp_state->ntlmssp_state->lm_resp.data ? &gensec_ntlmssp_state->ntlmssp_state->lm_resp : NULL, 
	                               gensec_ntlmssp_state->ntlmssp_state->nt_resp.data ? &gensec_ntlmssp_state->ntlmssp_state->nt_resp : NULL, 
				       NULL, NULL, NULL, True,
				       &user_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	nt_status = auth_check_password(gensec_ntlmssp_state->auth_context, gensec_ntlmssp_state,
						  user_info, &gensec_ntlmssp_state->server_info);
	talloc_free(user_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	if (gensec_ntlmssp_state->server_info->user_session_key.length) {
		DEBUG(10, ("Got NT session key of length %u\n", gensec_ntlmssp_state->server_info->user_session_key.length));
		*user_session_key = data_blob_talloc(ntlmssp_state, 
						   gensec_ntlmssp_state->server_info->user_session_key.data,
						   gensec_ntlmssp_state->server_info->user_session_key.length);
	}
	if (gensec_ntlmssp_state->server_info->lm_session_key.length) {
		DEBUG(10, ("Got LM session key of length %u\n", gensec_ntlmssp_state->server_info->lm_session_key.length));
		*lm_session_key = data_blob_talloc(ntlmssp_state, 
						   gensec_ntlmssp_state->server_info->lm_session_key.data,
						   gensec_ntlmssp_state->server_info->lm_session_key.length);
	}
	return nt_status;
}

static int gensec_ntlmssp_destroy(void *ptr)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = ptr;

	if (gensec_ntlmssp_state->ntlmssp_state) {
		ntlmssp_end(&gensec_ntlmssp_state->ntlmssp_state);
	}

	return 0;
}

static NTSTATUS gensec_ntlmssp_start(struct gensec_security *gensec_security)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state;
	
	gensec_ntlmssp_state = talloc(gensec_security, struct gensec_ntlmssp_state);
	if (!gensec_ntlmssp_state) {
		return NT_STATUS_NO_MEMORY;
	}

	gensec_ntlmssp_state->ntlmssp_state = NULL;
	gensec_ntlmssp_state->auth_context = NULL;
	gensec_ntlmssp_state->server_info = NULL;
	gensec_ntlmssp_state->have_features = 0;

	talloc_set_destructor(gensec_ntlmssp_state, gensec_ntlmssp_destroy); 

	gensec_security->private_data = gensec_ntlmssp_state;
	return NT_STATUS_OK;
}

static NTSTATUS gensec_ntlmssp_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status;
	struct ntlmssp_state *ntlmssp_state;
	struct gensec_ntlmssp_state *gensec_ntlmssp_state;

	nt_status = gensec_ntlmssp_start(gensec_security);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	gensec_ntlmssp_state = gensec_security->private_data;

	nt_status = ntlmssp_server_start(gensec_ntlmssp_state, &gensec_ntlmssp_state->ntlmssp_state);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
		gensec_ntlmssp_state->ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
		gensec_ntlmssp_state->ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SEAL;
	}

	nt_status = auth_context_create(gensec_ntlmssp_state, lp_auth_methods(), &gensec_ntlmssp_state->auth_context);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	ntlmssp_state = gensec_ntlmssp_state->ntlmssp_state;
	ntlmssp_state->auth_context = gensec_ntlmssp_state;
	ntlmssp_state->get_challenge = auth_ntlmssp_get_challenge;
	ntlmssp_state->may_set_challenge = auth_ntlmssp_may_set_challenge;
	ntlmssp_state->set_challenge = auth_ntlmssp_set_challenge;
	ntlmssp_state->check_password = auth_ntlmssp_check_password;
	ntlmssp_state->server_role = lp_server_role();

	return NT_STATUS_OK;
}

static NTSTATUS gensec_ntlmssp_client_start(struct gensec_security *gensec_security)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state;
	char *password = NULL;
	NTSTATUS nt_status;

	nt_status = gensec_ntlmssp_start(gensec_security);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	gensec_ntlmssp_state = gensec_security->private_data;
	nt_status = ntlmssp_client_start(gensec_ntlmssp_state,
					 &gensec_ntlmssp_state->ntlmssp_state);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	if (gensec_security->want_features & GENSEC_FEATURE_SESSION_KEY) {
		/*
		 * We need to set this to allow a later SetPassword
		 * via the SAMR pipe to succeed. Strange.... We could
		 * also add  NTLMSSP_NEGOTIATE_SEAL here. JRA.
		 * 
		 * Without this, Windows will not create the master key
		 * that it thinks is only used for NTLMSSP signing and 
		 * sealing.  (It is actually pulled out and used directly) 
		 */
		gensec_ntlmssp_state->ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
		gensec_ntlmssp_state->ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
		gensec_ntlmssp_state->ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SEAL;
	}

	nt_status = ntlmssp_set_domain(gensec_ntlmssp_state->ntlmssp_state, 
				       gensec_security->user.domain);
	NT_STATUS_NOT_OK_RETURN(nt_status);
	
	nt_status = ntlmssp_set_username(gensec_ntlmssp_state->ntlmssp_state, 
					 gensec_security->user.name);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	if (gensec_security->user.name) {
		nt_status = gensec_get_password(gensec_security, gensec_ntlmssp_state, &password);
		NT_STATUS_NOT_OK_RETURN(nt_status);
	}

	if (password) {
		nt_status = ntlmssp_set_password(gensec_ntlmssp_state->ntlmssp_state, password);
		NT_STATUS_NOT_OK_RETURN(nt_status);
	}

	gensec_security->private_data = gensec_ntlmssp_state;

	return NT_STATUS_OK;
}

/*
  wrappers for the ntlmssp_*() functions
*/
static NTSTATUS gensec_ntlmssp_unseal_packet(struct gensec_security *gensec_security, 
					     TALLOC_CTX *mem_ctx, 
					     uint8_t *data, size_t length, 
					     const uint8_t *whole_pdu, size_t pdu_length, 
					     DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;

	return ntlmssp_unseal_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

static NTSTATUS gensec_ntlmssp_check_packet(struct gensec_security *gensec_security, 
					    TALLOC_CTX *mem_ctx, 
					    const uint8_t *data, size_t length, 
					    const uint8_t *whole_pdu, size_t pdu_length, 
					    const DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;

	return ntlmssp_check_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

static NTSTATUS gensec_ntlmssp_seal_packet(struct gensec_security *gensec_security, 
					   TALLOC_CTX *mem_ctx, 
					   uint8_t *data, size_t length, 
					   const uint8_t *whole_pdu, size_t pdu_length, 
					   DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;

	return ntlmssp_seal_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

static NTSTATUS gensec_ntlmssp_sign_packet(struct gensec_security *gensec_security, 
					   TALLOC_CTX *mem_ctx, 
					   const uint8_t *data, size_t length, 
					   const uint8_t *whole_pdu, size_t pdu_length, 
					   DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;

	return ntlmssp_sign_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

static size_t gensec_ntlmssp_sig_size(struct gensec_security *gensec_security) 
{
	return NTLMSSP_SIG_SIZE;
}

static NTSTATUS gensec_ntlmssp_wrap(struct gensec_security *gensec_security, 
				    TALLOC_CTX *mem_ctx, 
				    const DATA_BLOB *in, 
				    DATA_BLOB *out)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;
	DATA_BLOB sig;
	NTSTATUS nt_status;

	if (gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {

		*out = data_blob_talloc(mem_ctx, NULL, in->length + NTLMSSP_SIG_SIZE);
		memcpy(out->data + NTLMSSP_SIG_SIZE, in->data, in->length);

	        nt_status = ntlmssp_seal_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, 
						out->data + NTLMSSP_SIG_SIZE, 
						out->length - NTLMSSP_SIG_SIZE, 
						out->data + NTLMSSP_SIG_SIZE, 
						out->length - NTLMSSP_SIG_SIZE, 
						&sig);

		if (NT_STATUS_IS_OK(nt_status)) {
			memcpy(out->data, sig.data, NTLMSSP_SIG_SIZE);
		}
		return nt_status;

	} else if ((gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN) 
		   || (gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_ALWAYS_SIGN)) {

		*out = data_blob_talloc(mem_ctx, NULL, in->length + NTLMSSP_SIG_SIZE);
		memcpy(out->data + NTLMSSP_SIG_SIZE, in->data, in->length);

	        nt_status = ntlmssp_sign_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, 
						out->data + NTLMSSP_SIG_SIZE, 
						out->length - NTLMSSP_SIG_SIZE, 
						out->data + NTLMSSP_SIG_SIZE, 
						out->length - NTLMSSP_SIG_SIZE, 
						&sig);

		if (NT_STATUS_IS_OK(nt_status)) {
			memcpy(out->data, sig.data, NTLMSSP_SIG_SIZE);
		}
		return nt_status;

	} else {
		*out = *in;
		return NT_STATUS_OK;
	}
}


static NTSTATUS gensec_ntlmssp_unwrap(struct gensec_security *gensec_security, 
				      TALLOC_CTX *mem_ctx, 
				      const DATA_BLOB *in, 
				      DATA_BLOB *out)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;
	DATA_BLOB sig;

	if (gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {
		if (in->length < NTLMSSP_SIG_SIZE) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		sig.data = in->data;
		sig.length = NTLMSSP_SIG_SIZE;

		*out = data_blob_talloc(mem_ctx, in->data + NTLMSSP_SIG_SIZE, in->length - NTLMSSP_SIG_SIZE);
		
	        return ntlmssp_unseal_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, 
					     out->data, out->length, 
					     out->data, out->length, 
					     &sig);
						  
	} else if ((gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN) 
		   || (gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_ALWAYS_SIGN)) {
		if (in->length < NTLMSSP_SIG_SIZE) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		sig.data = in->data;
		sig.length = NTLMSSP_SIG_SIZE;

		*out = data_blob_talloc(mem_ctx, in->data + NTLMSSP_SIG_SIZE, in->length - NTLMSSP_SIG_SIZE);
		
	        return ntlmssp_check_packet(gensec_ntlmssp_state->ntlmssp_state, mem_ctx, 
					    out->data, out->length, 
					    out->data, out->length, 
					    &sig);
	} else {
		*out = *in;
		return NT_STATUS_OK;
	}
}

static NTSTATUS gensec_ntlmssp_session_key(struct gensec_security *gensec_security, 
					   DATA_BLOB *session_key)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;

	return ntlmssp_session_key(gensec_ntlmssp_state->ntlmssp_state, session_key);
}

/**
 * Next state function for the wrapped NTLMSSP state machine
 * 
 * @param gensec_security GENSEC state, initialised to NTLMSSP
 * @param out_mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param in The request, as a DATA_BLOB
 * @param out The reply, as an talloc()ed DATA_BLOB, on *out_mem_ctx
 * @return Error, MORE_PROCESSING_REQUIRED if a reply is sent, 
 *                or NT_STATUS_OK if the user is authenticated. 
 */

static NTSTATUS gensec_ntlmssp_update(struct gensec_security *gensec_security, TALLOC_CTX *out_mem_ctx, 
				      const DATA_BLOB in, DATA_BLOB *out) 
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;
	NTSTATUS status;

	status = ntlmssp_update(gensec_ntlmssp_state->ntlmssp_state, out_mem_ctx, in, out);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED) && !NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	gensec_ntlmssp_state->have_features = 0;

	if (gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN) {
		gensec_ntlmssp_state->have_features |= GENSEC_FEATURE_SIGN;
	}

	if (gensec_ntlmssp_state->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {
		gensec_ntlmssp_state->have_features |= GENSEC_FEATURE_SEAL;
	}

	if (gensec_ntlmssp_state->ntlmssp_state->session_key.data) {
		gensec_ntlmssp_state->have_features |= GENSEC_FEATURE_SESSION_KEY;
	}

	return status;
}

/** 
 * Return the credentials of a logged on user, including session keys
 * etc.
 *
 * Only valid after a successful authentication
 *
 * May only be called once per authentication.
 *
 */

static NTSTATUS gensec_ntlmssp_session_info(struct gensec_security *gensec_security,
					    struct auth_session_info **session_info) 
{
	NTSTATUS nt_status;
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;

	nt_status = auth_generate_session_info(gensec_ntlmssp_state, gensec_ntlmssp_state->server_info, session_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	(*session_info)->session_key = data_blob_talloc(*session_info, 
							gensec_ntlmssp_state->ntlmssp_state->session_key.data,
							gensec_ntlmssp_state->ntlmssp_state->session_key.length);

	return NT_STATUS_OK;
}

static BOOL gensec_ntlmssp_have_feature(struct gensec_security *gensec_security,
					uint32 feature)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;
	if (gensec_ntlmssp_state->have_features & feature) {
		return True;
	}

	return False;
}

static const struct gensec_security_ops gensec_ntlmssp_security_ops = {
	.name		= "ntlmssp",
	.sasl_name	= "NTLM",
	.auth_type	= DCERPC_AUTH_TYPE_NTLMSSP,
	.oid            = GENSEC_OID_NTLMSSP,
	.client_start   = gensec_ntlmssp_client_start,
	.server_start   = gensec_ntlmssp_server_start,
	.update 	= gensec_ntlmssp_update,
	.sig_size	= gensec_ntlmssp_sig_size,
	.sign_packet	= gensec_ntlmssp_sign_packet,
	.check_packet	= gensec_ntlmssp_check_packet,
	.seal_packet	= gensec_ntlmssp_seal_packet,
	.unseal_packet	= gensec_ntlmssp_unseal_packet,
	.wrap           = gensec_ntlmssp_wrap,
	.unwrap         = gensec_ntlmssp_unwrap,
	.session_key	= gensec_ntlmssp_session_key,
	.session_info   = gensec_ntlmssp_session_info,
	.have_feature   = gensec_ntlmssp_have_feature,
	.enabled        = True
};


NTSTATUS gensec_ntlmssp_init(void)
{
	NTSTATUS ret;
	ret = gensec_register(&gensec_ntlmssp_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_ntlmssp_security_ops.name));
		return ret;
	}

	return ret;
}
