/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2005
   
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
  a composite API for making handling a generic async session setup
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/auth/libcli_auth.h"
#include "auth/auth.h"
#include "auth/gensec/gensec.h"
#include "auth/credentials/credentials.h"
#include "version.h"

struct sesssetup_state {
	union smb_sesssetup setup;
	NTSTATUS gensec_status;
	struct smb_composite_sesssetup *io;
	struct smbcli_request *req;
};

static NTSTATUS session_setup_old(struct composite_context *c,
				  struct smbcli_session *session, 
				  struct smb_composite_sesssetup *io,
				  struct smbcli_request **req); 
static NTSTATUS session_setup_nt1(struct composite_context *c,
				  struct smbcli_session *session, 
				  struct smb_composite_sesssetup *io,
				  struct smbcli_request **req); 
static NTSTATUS session_setup_spnego(struct composite_context *c,
				     struct smbcli_session *session, 
				     struct smb_composite_sesssetup *io,
				     struct smbcli_request **req);

/*
  store the user session key for a transport
*/
static void set_user_session_key(struct smbcli_session *session,
				 const DATA_BLOB *session_key)
{
	session->user_session_key = data_blob_talloc(session, 
						     session_key->data, 
						     session_key->length);
}

/*
  handler for completion of a smbcli_request sub-request
*/
static void request_handler(struct smbcli_request *req)
{
	struct composite_context *c = req->async.private;
	struct sesssetup_state *state = talloc_get_type(c->private_data, struct sesssetup_state);
	struct smbcli_session *session = req->session;
	DATA_BLOB session_key = data_blob(NULL, 0);
	DATA_BLOB null_data_blob = data_blob(NULL, 0);
	NTSTATUS session_key_err, nt_status;

	c->status = smb_raw_sesssetup_recv(req, state, &state->setup);

	switch (state->setup.old.level) {
	case RAW_SESSSETUP_OLD:
		state->io->out.vuid = state->setup.old.out.vuid;
		/* This doesn't work, as this only happens on old
		 * protocols, where this comparison won't match. */
		if (NT_STATUS_EQUAL(c->status, NT_STATUS_LOGON_FAILURE)) {
			/* we neet to reset the vuid for a new try */
			session->vuid = 0;
			if (cli_credentials_wrong_password(state->io->in.credentials)) {
				nt_status = session_setup_old(c, session, 
							      state->io, 
							      &state->req);
				if (NT_STATUS_IS_OK(nt_status)) {
					c->status = nt_status;
					state->req->async.fn = request_handler;
					state->req->async.private = c;
					return;
				}
			}
		}
		break;

	case RAW_SESSSETUP_NT1:
		state->io->out.vuid = state->setup.nt1.out.vuid;
		if (NT_STATUS_EQUAL(c->status, NT_STATUS_LOGON_FAILURE)) {
			/* we neet to reset the vuid for a new try */
			session->vuid = 0;
			if (cli_credentials_wrong_password(state->io->in.credentials)) {
				nt_status = session_setup_nt1(c, session, 
							      state->io, 
							      &state->req);
				if (NT_STATUS_IS_OK(nt_status)) {
					c->status = nt_status;
					state->req->async.fn = request_handler;
					state->req->async.private = c;
					return;
				}
			}
		}
		break;

	case RAW_SESSSETUP_SPNEGO:
		state->io->out.vuid = state->setup.spnego.out.vuid;
		if (NT_STATUS_EQUAL(c->status, NT_STATUS_LOGON_FAILURE)) {
			/* we neet to reset the vuid for a new try */
			session->vuid = 0;
			if (cli_credentials_wrong_password(state->io->in.credentials)) {
				nt_status = session_setup_spnego(c, session, 
								      state->io, 
								      &state->req);
				if (NT_STATUS_IS_OK(nt_status)) {
					c->status = nt_status;
					state->req->async.fn = request_handler;
					state->req->async.private = c;
					return;
				}
			}
		}
		if (!NT_STATUS_EQUAL(c->status, NT_STATUS_MORE_PROCESSING_REQUIRED) && 
		    !NT_STATUS_IS_OK(c->status)) {
			break;
		}
		if (NT_STATUS_EQUAL(state->gensec_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {

			/* The status value here, from the earlier pass at GENSEC is
			 * vital to the security of the system.  Even if the other end
			 * accepts, if GENSEC claims 'MORE_PROCESSING_REQUIRED' then
			 * you must keep feeding it blobs, or else the remote
			 * host/attacker might avoid mutal authentication
			 * requirements */
			
			state->gensec_status = gensec_update(session->gensec, state,
							 state->setup.spnego.out.secblob,
							 &state->setup.spnego.in.secblob);
			c->status = state->gensec_status;
			if (!NT_STATUS_EQUAL(c->status, NT_STATUS_MORE_PROCESSING_REQUIRED) && 
			    !NT_STATUS_IS_OK(c->status)) {
				break;
			}
		} else {
			state->setup.spnego.in.secblob = data_blob(NULL, 0);
		}
			
		/* we need to do another round of session setup. We keep going until both sides
		   are happy */
		session_key_err = gensec_session_key(session->gensec, &session_key);
		if (NT_STATUS_IS_OK(session_key_err)) {
			set_user_session_key(session, &session_key);
			smbcli_transport_simple_set_signing(session->transport, session_key, null_data_blob);
		}

		if (state->setup.spnego.in.secblob.length) {
			/* 
			 * set the session->vuid value only for calling
			 * smb_raw_sesssetup_send()
			 */
			uint16_t vuid = session->vuid;
			session->vuid = state->io->out.vuid;
			state->req = smb_raw_sesssetup_send(session, &state->setup);
			session->vuid = vuid;
			state->req->async.fn = request_handler;
			state->req->async.private = c;
			return;
		}
		break;

	case RAW_SESSSETUP_SMB2:
		c->status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

	/* enforce the local signing required flag */
	if (NT_STATUS_IS_OK(c->status) && !cli_credentials_is_anonymous(state->io->in.credentials)) {
		if (!session->transport->negotiate.sign_info.doing_signing 
		    && session->transport->negotiate.sign_info.mandatory_signing) {
			DEBUG(0, ("SMB signing required, but server does not support it\n"));
			c->status = NT_STATUS_ACCESS_DENIED;
		}
	}

	if (NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_DONE;
	} else {
		c->state = COMPOSITE_STATE_ERROR;
	}
	if (c->async.fn) {
		c->async.fn(c);
	}
}


/*
  send a nt1 style session setup
*/
static NTSTATUS session_setup_nt1(struct composite_context *c,
				  struct smbcli_session *session, 
				  struct smb_composite_sesssetup *io,
				  struct smbcli_request **req) 
{
	NTSTATUS nt_status;
	struct sesssetup_state *state = talloc_get_type(c->private_data, struct sesssetup_state);
	const char *password = cli_credentials_get_password(io->in.credentials);
	DATA_BLOB names_blob = NTLMv2_generate_names_blob(state, session->transport->socket->hostname, lp_workgroup());
	DATA_BLOB session_key;
	int flags = CLI_CRED_NTLM_AUTH;
	if (lp_client_lanman_auth()) {
		flags |= CLI_CRED_LANMAN_AUTH;
	}

	if (lp_client_ntlmv2_auth()) {
		flags |= CLI_CRED_NTLMv2_AUTH;
	}

	state->setup.nt1.level           = RAW_SESSSETUP_NT1;
	state->setup.nt1.in.bufsize      = session->transport->options.max_xmit;
	state->setup.nt1.in.mpx_max      = session->transport->options.max_mux;
	state->setup.nt1.in.vc_num       = 1;
	state->setup.nt1.in.sesskey      = io->in.sesskey;
	state->setup.nt1.in.capabilities = io->in.capabilities;
	state->setup.nt1.in.os           = "Unix";
	state->setup.nt1.in.lanman       = talloc_asprintf(state, "Samba %s", SAMBA_VERSION_STRING);

	cli_credentials_get_ntlm_username_domain(io->in.credentials, state, 
						 &state->setup.nt1.in.user,
						 &state->setup.nt1.in.domain);
	

	if (session->transport->negotiate.sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) {
		nt_status = cli_credentials_get_ntlm_response(io->in.credentials, state, 
							      &flags, 
							      session->transport->negotiate.secblob, 
							      names_blob,
							      &state->setup.nt1.in.password1,
							      &state->setup.nt1.in.password2,
							      NULL, &session_key);
		NT_STATUS_NOT_OK_RETURN(nt_status);

		smbcli_transport_simple_set_signing(session->transport, session_key, 
						    state->setup.nt1.in.password2);
		set_user_session_key(session, &session_key);
		
		data_blob_free(&session_key);
	} else if (lp_client_plaintext_auth()) {
		state->setup.nt1.in.password1 = data_blob_talloc(state, password, strlen(password));
		state->setup.nt1.in.password2 = data_blob(NULL, 0);
	} else {
		/* could match windows client and return 'cannot logon from this workstation', but it just confuses everybody */
		return NT_STATUS_INVALID_PARAMETER;
	}

	*req = smb_raw_sesssetup_send(session, &state->setup);
	if (!*req) {
		return NT_STATUS_NO_MEMORY;
	}
	return (*req)->status;
}


/*
  old style session setup (pre NT1 protocol level)
*/
static NTSTATUS session_setup_old(struct composite_context *c,
				  struct smbcli_session *session, 
				  struct smb_composite_sesssetup *io,
				  struct smbcli_request **req) 
{
	NTSTATUS nt_status;
	struct sesssetup_state *state = talloc_get_type(c->private_data, struct sesssetup_state);
	const char *password = cli_credentials_get_password(io->in.credentials);
	DATA_BLOB names_blob = NTLMv2_generate_names_blob(state, session->transport->socket->hostname, lp_workgroup());
	DATA_BLOB session_key;
	int flags = 0;
	if (lp_client_lanman_auth()) {
		flags |= CLI_CRED_LANMAN_AUTH;
	}

	if (lp_client_ntlmv2_auth()) {
		flags |= CLI_CRED_NTLMv2_AUTH;
	}

	state->setup.old.level      = RAW_SESSSETUP_OLD;
	state->setup.old.in.bufsize = session->transport->options.max_xmit;
	state->setup.old.in.mpx_max = session->transport->options.max_mux;
	state->setup.old.in.vc_num  = 1;
	state->setup.old.in.sesskey = io->in.sesskey;
	state->setup.old.in.os      = "Unix";
	state->setup.old.in.lanman  = talloc_asprintf(state, "Samba %s", SAMBA_VERSION_STRING);
	cli_credentials_get_ntlm_username_domain(io->in.credentials, state, 
						 &state->setup.old.in.user,
						 &state->setup.old.in.domain);
	
	if (session->transport->negotiate.sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) {
		nt_status = cli_credentials_get_ntlm_response(io->in.credentials, state, 
							      &flags, 
							      session->transport->negotiate.secblob, 
							      names_blob,
							      &state->setup.old.in.password,
							      NULL,
							      NULL, &session_key);
		NT_STATUS_NOT_OK_RETURN(nt_status);
		set_user_session_key(session, &session_key);
		
		data_blob_free(&session_key);
	} else if (lp_client_plaintext_auth()) {
		state->setup.old.in.password = data_blob_talloc(state, password, strlen(password));
	} else {
		/* could match windows client and return 'cannot logon from this workstation', but it just confuses everybody */
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	*req = smb_raw_sesssetup_send(session, &state->setup);
	if (!*req) {
		return NT_STATUS_NO_MEMORY;
	}
	return (*req)->status;
}


/*
  Modern, all singing, all dancing extended security (and possibly SPNEGO) request
*/
static NTSTATUS session_setup_spnego(struct composite_context *c,
				     struct smbcli_session *session, 
				     struct smb_composite_sesssetup *io,
				     struct smbcli_request **req) 
{
	struct sesssetup_state *state = talloc_get_type(c->private_data, struct sesssetup_state);
	NTSTATUS status, session_key_err;
	DATA_BLOB session_key = data_blob(NULL, 0);
	DATA_BLOB null_data_blob = data_blob(NULL, 0);
	const char *chosen_oid = NULL;

	state->setup.spnego.level           = RAW_SESSSETUP_SPNEGO;
	state->setup.spnego.in.bufsize      = session->transport->options.max_xmit;
	state->setup.spnego.in.mpx_max      = session->transport->options.max_mux;
	state->setup.spnego.in.vc_num       = 1;
	state->setup.spnego.in.sesskey      = io->in.sesskey;
	state->setup.spnego.in.capabilities = io->in.capabilities;
	state->setup.spnego.in.os           = "Unix";
	state->setup.spnego.in.lanman       = talloc_asprintf(state, "Samba %s", SAMBA_VERSION_STRING);
	state->setup.spnego.in.workgroup    = io->in.workgroup;

	smbcli_temp_set_signing(session->transport);

	status = gensec_client_start(session, &session->gensec, c->event_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start GENSEC client mode: %s\n", nt_errstr(status)));
		return status;
	}

	gensec_want_feature(session->gensec, GENSEC_FEATURE_SESSION_KEY);

	status = gensec_set_credentials(session->gensec, io->in.credentials);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC client credentails: %s\n", 
			  nt_errstr(status)));
		return status;
	}

	status = gensec_set_target_hostname(session->gensec, session->transport->socket->hostname);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC target hostname: %s\n", 
			  nt_errstr(status)));
		return status;
	}

	status = gensec_set_target_service(session->gensec, "cifs");
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC target service: %s\n", 
			  nt_errstr(status)));
		return status;
	}

	if (session->transport->negotiate.secblob.length) {
		chosen_oid = GENSEC_OID_SPNEGO;
		status = gensec_start_mech_by_oid(session->gensec, chosen_oid);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to start set GENSEC client mechanism %s: %s\n",
				  gensec_get_name_by_oid(chosen_oid), nt_errstr(status)));
			chosen_oid = GENSEC_OID_NTLMSSP;
			status = gensec_start_mech_by_oid(session->gensec, chosen_oid);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(1, ("Failed to start set (fallback) GENSEC client mechanism %s: %s\n",
					  gensec_get_name_by_oid(chosen_oid), nt_errstr(status)));
			return status;
			}
		}
	} else {
		/* without a sec blob, means raw NTLMSSP */
		chosen_oid = GENSEC_OID_NTLMSSP;
		status = gensec_start_mech_by_oid(session->gensec, chosen_oid);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to start set GENSEC client mechanism %s: %s\n",
				  gensec_get_name_by_oid(chosen_oid), nt_errstr(status)));
		}
	}

	if (chosen_oid == GENSEC_OID_SPNEGO) {
		status = gensec_update(session->gensec, state,
				       session->transport->negotiate.secblob,
				       &state->setup.spnego.in.secblob);
	} else {
		status = gensec_update(session->gensec, state,
				       data_blob(NULL, 0),
				       &state->setup.spnego.in.secblob);

	}

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED) && 
	    !NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed initial gensec_update with mechanism %s: %s\n",
			  gensec_get_name_by_oid(chosen_oid), nt_errstr(status)));
		return status;
	}
	state->gensec_status = status;

	session_key_err = gensec_session_key(session->gensec, &session_key);
	if (NT_STATUS_IS_OK(session_key_err)) {
		smbcli_transport_simple_set_signing(session->transport, session_key, null_data_blob);
	}

	*req = smb_raw_sesssetup_send(session, &state->setup);
	if (!*req) {
		return NT_STATUS_NO_MEMORY;
	}
	return (*req)->status;
}


/*
  composite session setup function that hides the details of all the
  different session setup varients, including the multi-pass nature of
  the spnego varient
*/
struct composite_context *smb_composite_sesssetup_send(struct smbcli_session *session, 
						       struct smb_composite_sesssetup *io)
{
	struct composite_context *c;
	struct sesssetup_state *state;
	NTSTATUS status;

	c = talloc_zero(session, struct composite_context);
	if (c == NULL) return NULL;

	state = talloc(c, struct sesssetup_state);
	if (state == NULL) {
		talloc_free(c);
		return NULL;
	}

	state->io = io;

	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->private_data = state;
	c->event_ctx = session->transport->socket->event.ctx;

	/* no session setup at all in earliest protocol varients */
	if (session->transport->negotiate.protocol < PROTOCOL_LANMAN1) {
		ZERO_STRUCT(io->out);
		composite_done(c);
		return c;
	}

	/* see what session setup interface we will use */
	if (session->transport->negotiate.protocol < PROTOCOL_NT1) {
		status = session_setup_old(c, session, io, &state->req);
	} else if (!session->transport->options.use_spnego ||
		   !(io->in.capabilities & CAP_EXTENDED_SECURITY)) {
		status = session_setup_nt1(c, session, io, &state->req);
	} else {
		status = session_setup_spnego(c, session, io, &state->req);
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED) || 
	    NT_STATUS_IS_OK(status)) {
		state->req->async.fn = request_handler;
		state->req->async.private = c;
		return c;
	}

	c->state = COMPOSITE_STATE_ERROR;
	c->status = status;
	return c;
}


/*
  receive a composite session setup reply
*/
NTSTATUS smb_composite_sesssetup_recv(struct composite_context *c)
{
	NTSTATUS status;
	status = composite_wait(c);
	talloc_free(c);
	return status;
}

/*
  sync version of smb_composite_sesssetup 
*/
NTSTATUS smb_composite_sesssetup(struct smbcli_session *session, struct smb_composite_sesssetup *io)
{
	struct composite_context *c = smb_composite_sesssetup_send(session, io);
	return smb_composite_sesssetup_recv(c);
}
