/* 
   Unix SMB/CIFS implementation.
   SMB client session context management functions
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
#include "libcli/raw/libcliraw.h"
#include "auth/auth.h"

#define SETUP_REQUEST_SESSION(cmd, wct, buflen) do { \
	req = smbcli_request_setup_session(session, cmd, wct, buflen); \
	if (!req) return NULL; \
} while (0)


/****************************************************************************
 Initialize the session context
****************************************************************************/
struct smbcli_session *smbcli_session_init(struct smbcli_transport *transport)
{
	struct smbcli_session *session;
	uint16_t flags2;
	uint32_t capabilities;

	session = talloc_zero(transport, struct smbcli_session);
	if (!session) {
		return NULL;
	}

	session->transport = talloc_reference(session, transport);
	session->pid = (uint16_t)getpid();
	session->vuid = UID_FIELD_INVALID;
	
	capabilities = transport->negotiate.capabilities;

	flags2 = FLAGS2_LONG_PATH_COMPONENTS | FLAGS2_EXTENDED_ATTRIBUTES;

	if (capabilities & CAP_UNICODE) {
		flags2 |= FLAGS2_UNICODE_STRINGS;
	}
	if (capabilities & CAP_STATUS32) {
		flags2 |= FLAGS2_32_BIT_ERROR_CODES;
	}
	if (capabilities & CAP_EXTENDED_SECURITY) {
		flags2 |= FLAGS2_EXTENDED_SECURITY;
	}
	if (session->transport->negotiate.sign_info.doing_signing) {
		flags2 |= FLAGS2_SMB_SECURITY_SIGNATURES;
	}

	session->flags2 = flags2;

	return session;
}

/****************************************************************************
 Perform a session setup (async send)
****************************************************************************/
struct smbcli_request *smb_raw_session_setup_send(struct smbcli_session *session, union smb_sesssetup *parms) 
{
	struct smbcli_request *req = NULL;

	switch (parms->generic.level) {
	case RAW_SESSSETUP_GENERIC:
		/* handled elsewhere */
		return NULL;

	case RAW_SESSSETUP_OLD:
		SETUP_REQUEST_SESSION(SMBsesssetupX, 10, 0);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv,VWV(2),parms->old.in.bufsize);
		SSVAL(req->out.vwv,VWV(3),parms->old.in.mpx_max);
		SSVAL(req->out.vwv,VWV(4),parms->old.in.vc_num);
		SIVAL(req->out.vwv,VWV(5),parms->old.in.sesskey);
		SSVAL(req->out.vwv,VWV(7),parms->old.in.password.length);
		SIVAL(req->out.vwv,VWV(8), 0); /* reserved */
		smbcli_req_append_blob(req, &parms->old.in.password);
		smbcli_req_append_string(req, parms->old.in.user, STR_TERMINATE);
		smbcli_req_append_string(req, parms->old.in.domain, STR_TERMINATE|STR_UPPER);
		smbcli_req_append_string(req, parms->old.in.os, STR_TERMINATE);
		smbcli_req_append_string(req, parms->old.in.lanman, STR_TERMINATE);
		break;

	case RAW_SESSSETUP_NT1:
		SETUP_REQUEST_SESSION(SMBsesssetupX, 13, 0);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->nt1.in.bufsize);
		SSVAL(req->out.vwv, VWV(3), parms->nt1.in.mpx_max);
		SSVAL(req->out.vwv, VWV(4), parms->nt1.in.vc_num);
		SIVAL(req->out.vwv, VWV(5), parms->nt1.in.sesskey);
		SSVAL(req->out.vwv, VWV(7), parms->nt1.in.password1.length);
		SSVAL(req->out.vwv, VWV(8), parms->nt1.in.password2.length);
		SIVAL(req->out.vwv, VWV(9), 0); /* reserved */
		SIVAL(req->out.vwv, VWV(11), parms->nt1.in.capabilities);
		smbcli_req_append_blob(req, &parms->nt1.in.password1);
		smbcli_req_append_blob(req, &parms->nt1.in.password2);
		smbcli_req_append_string(req, parms->nt1.in.user, STR_TERMINATE);
		smbcli_req_append_string(req, parms->nt1.in.domain, STR_TERMINATE|STR_UPPER);
		smbcli_req_append_string(req, parms->nt1.in.os, STR_TERMINATE);
		smbcli_req_append_string(req, parms->nt1.in.lanman, STR_TERMINATE);
		break;

	case RAW_SESSSETUP_SPNEGO:
		SETUP_REQUEST_SESSION(SMBsesssetupX, 12, 0);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->spnego.in.bufsize);
		SSVAL(req->out.vwv, VWV(3), parms->spnego.in.mpx_max);
		SSVAL(req->out.vwv, VWV(4), parms->spnego.in.vc_num);
		SIVAL(req->out.vwv, VWV(5), parms->spnego.in.sesskey);
		SSVAL(req->out.vwv, VWV(7), parms->spnego.in.secblob.length);
		SIVAL(req->out.vwv, VWV(8), 0); /* reserved */
		SIVAL(req->out.vwv, VWV(10), parms->spnego.in.capabilities);
		smbcli_req_append_blob(req, &parms->spnego.in.secblob);
		smbcli_req_append_string(req, parms->spnego.in.os, STR_TERMINATE);
		smbcli_req_append_string(req, parms->spnego.in.lanman, STR_TERMINATE);
		smbcli_req_append_string(req, parms->spnego.in.domain, STR_TERMINATE);
		break;
	}

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}


/****************************************************************************
 Perform a session setup (async recv)
****************************************************************************/
NTSTATUS smb_raw_session_setup_recv(struct smbcli_request *req, 
				    TALLOC_CTX *mem_ctx, 
				    union smb_sesssetup *parms) 
{
	uint16_t len;
	uint8_t *p;

	if (!smbcli_request_receive(req)) {
		return smbcli_request_destroy(req);
	}
	
	if (!NT_STATUS_IS_OK(req->status) &&
	    !NT_STATUS_EQUAL(req->status,NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		return smbcli_request_destroy(req);
	}

	switch (parms->generic.level) {
	case RAW_SESSSETUP_GENERIC:
		/* handled elsewhere */
		return NT_STATUS_INVALID_LEVEL;

	case RAW_SESSSETUP_OLD:
		SMBCLI_CHECK_WCT(req, 3);
		ZERO_STRUCT(parms->old.out);
		parms->old.out.vuid = SVAL(req->in.hdr, HDR_UID);
		parms->old.out.action = SVAL(req->in.vwv, VWV(2));
		p = req->in.data;
		if (p) {
			p += smbcli_req_pull_string(req, mem_ctx, &parms->old.out.os, p, -1, STR_TERMINATE);
			p += smbcli_req_pull_string(req, mem_ctx, &parms->old.out.lanman, p, -1, STR_TERMINATE);
			p += smbcli_req_pull_string(req, mem_ctx, &parms->old.out.domain, p, -1, STR_TERMINATE);
		}
		break;

	case RAW_SESSSETUP_NT1:
		SMBCLI_CHECK_WCT(req, 3);
		ZERO_STRUCT(parms->nt1.out);
		parms->nt1.out.vuid   = SVAL(req->in.hdr, HDR_UID);
		parms->nt1.out.action = SVAL(req->in.vwv, VWV(2));
		p = req->in.data;
		if (p) {
			p += smbcli_req_pull_string(req, mem_ctx, &parms->nt1.out.os, p, -1, STR_TERMINATE);
			p += smbcli_req_pull_string(req, mem_ctx, &parms->nt1.out.lanman, p, -1, STR_TERMINATE);
			if (p < (req->in.data + req->in.data_size)) {
				p += smbcli_req_pull_string(req, mem_ctx, &parms->nt1.out.domain, p, -1, STR_TERMINATE);
			}
		}
		break;

	case RAW_SESSSETUP_SPNEGO:
		SMBCLI_CHECK_WCT(req, 4);
		ZERO_STRUCT(parms->spnego.out);
		parms->spnego.out.vuid   = SVAL(req->in.hdr, HDR_UID);
		parms->spnego.out.action = SVAL(req->in.vwv, VWV(2));
		len                      = SVAL(req->in.vwv, VWV(3));
		p = req->in.data;
		if (!p) {
			break;
		}

		parms->spnego.out.secblob = smbcli_req_pull_blob(req, mem_ctx, p, len);
		p += parms->spnego.out.secblob.length;
		p += smbcli_req_pull_string(req, mem_ctx, &parms->spnego.out.os, p, -1, STR_TERMINATE);
		p += smbcli_req_pull_string(req, mem_ctx, &parms->spnego.out.lanman, p, -1, STR_TERMINATE);
		p += smbcli_req_pull_string(req, mem_ctx, &parms->spnego.out.domain, p, -1, STR_TERMINATE);
		break;
	}

failed:
	return smbcli_request_destroy(req);
}

/*
  form an encrypted lanman password from a plaintext password
  and the server supplied challenge
*/
static DATA_BLOB lanman_blob(const char *pass, DATA_BLOB challenge)
{
	DATA_BLOB blob = data_blob(NULL, 24);
	SMBencrypt(pass, challenge.data, blob.data);
	return blob;
}

/*
  form an encrypted NT password from a plaintext password
  and the server supplied challenge
*/
static DATA_BLOB nt_blob(const char *pass, DATA_BLOB challenge)
{
	DATA_BLOB blob = data_blob(NULL, 24);
	SMBNTencrypt(pass, challenge.data, blob.data);
	return blob;
}

/*
  store the user session key for a transport
*/
void smbcli_session_set_user_session_key(struct smbcli_session *session,
				   const DATA_BLOB *session_key)
{
	session->user_session_key = data_blob_talloc(session, 
						     session_key->data, 
						     session_key->length);
}

/*
  setup signing for a NT1 style session setup
*/
void smb_session_use_nt1_session_keys(struct smbcli_session *session, 
				      const char *password, const DATA_BLOB *nt_response)
{
	struct smbcli_transport *transport = session->transport; 
	uint8_t nt_hash[16];
	DATA_BLOB session_key = data_blob(NULL, 16);

	E_md4hash(password, nt_hash);
	SMBsesskeygen_ntv1(nt_hash, session_key.data);

	smbcli_transport_simple_set_signing(transport, session_key, *nt_response);

	smbcli_session_set_user_session_key(session, &session_key);
	data_blob_free(&session_key);
}

/****************************************************************************
 Perform a session setup (sync interface) using generic interface and the old
 style sesssetup call
****************************************************************************/
static NTSTATUS smb_raw_session_setup_generic_old(struct smbcli_session *session, 
						  TALLOC_CTX *mem_ctx, 
						  union smb_sesssetup *parms) 
{
	NTSTATUS status;
	union smb_sesssetup s2;

	/* use the old interface */
	s2.generic.level = RAW_SESSSETUP_OLD;
	s2.old.in.bufsize = session->transport->options.max_xmit;
	s2.old.in.mpx_max = session->transport->options.max_mux;
	s2.old.in.vc_num = 1;
	s2.old.in.sesskey = parms->generic.in.sesskey;
	s2.old.in.domain = parms->generic.in.domain;
	s2.old.in.user = parms->generic.in.user;
	s2.old.in.os = "Unix";
	s2.old.in.lanman = "Samba";
	
	if (!parms->generic.in.password) {
		s2.old.in.password = data_blob(NULL, 0);
	} else if (session->transport->negotiate.sec_mode & 
		   NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) {
		s2.old.in.password = lanman_blob(parms->generic.in.password, 
						 session->transport->negotiate.secblob);
	} else {
		s2.old.in.password = data_blob(parms->generic.in.password, 
					       strlen(parms->generic.in.password));
	}
	
	status = smb_raw_session_setup(session, mem_ctx, &s2);
	
	data_blob_free(&s2.old.in.password);
	
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	parms->generic.out.vuid = s2.old.out.vuid;
	parms->generic.out.os = s2.old.out.os;
	parms->generic.out.lanman = s2.old.out.lanman;
	parms->generic.out.domain = s2.old.out.domain;
	
	return NT_STATUS_OK;
}

/****************************************************************************
 Perform a session setup (sync interface) using generic interface and the NT1
 style sesssetup call
****************************************************************************/
static NTSTATUS smb_raw_session_setup_generic_nt1(struct smbcli_session *session, 
						  TALLOC_CTX *mem_ctx,
						  union smb_sesssetup *parms) 
{
	NTSTATUS status;
	union smb_sesssetup s2;

	s2.generic.level = RAW_SESSSETUP_NT1;
	s2.nt1.in.bufsize = session->transport->options.max_xmit;
	s2.nt1.in.mpx_max = session->transport->options.max_mux;
	s2.nt1.in.vc_num = 1;
	s2.nt1.in.sesskey = parms->generic.in.sesskey;
	s2.nt1.in.capabilities = parms->generic.in.capabilities;
	s2.nt1.in.domain = parms->generic.in.domain;
	s2.nt1.in.user = parms->generic.in.user;
	s2.nt1.in.os = "Unix";
	s2.nt1.in.lanman = "Samba";

	if (!parms->generic.in.password) {
		s2.nt1.in.password1 = data_blob(NULL, 0);
		s2.nt1.in.password2 = data_blob(NULL, 0);
	} else if (session->transport->negotiate.sec_mode & 
		   NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) {
		s2.nt1.in.password1 = lanman_blob(parms->generic.in.password, 
						  session->transport->negotiate.secblob);
		s2.nt1.in.password2 = nt_blob(parms->generic.in.password, 
					      session->transport->negotiate.secblob);
		smb_session_use_nt1_session_keys(session, parms->generic.in.password, &s2.nt1.in.password2);

	} else {
		s2.nt1.in.password1 = data_blob(parms->generic.in.password, 
						strlen(parms->generic.in.password));
		s2.nt1.in.password2 = data_blob(NULL, 0);
	}

	status = smb_raw_session_setup(session, mem_ctx, &s2);
		
	data_blob_free(&s2.nt1.in.password1);
	data_blob_free(&s2.nt1.in.password2);
		
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	parms->generic.out.vuid = s2.nt1.out.vuid;
	parms->generic.out.os = s2.nt1.out.os;
	parms->generic.out.lanman = s2.nt1.out.lanman;
	parms->generic.out.domain = s2.nt1.out.domain;

	return NT_STATUS_OK;
}

/****************************************************************************
 Perform a session setup (sync interface) using generic interface and the SPNEGO
 style sesssetup call
****************************************************************************/
static NTSTATUS smb_raw_session_setup_generic_spnego(struct smbcli_session *session, 
						  TALLOC_CTX *mem_ctx,
						  union smb_sesssetup *parms) 
{
	NTSTATUS status;
	NTSTATUS session_key_err = NT_STATUS_NO_USER_SESSION_KEY;
	union smb_sesssetup s2;
	DATA_BLOB session_key = data_blob(NULL, 0);
	DATA_BLOB null_data_blob = data_blob(NULL, 0);
	const char *chosen_oid = NULL;

	s2.generic.level = RAW_SESSSETUP_SPNEGO;
	s2.spnego.in.bufsize = session->transport->options.max_xmit;
	s2.spnego.in.mpx_max = session->transport->options.max_mux;
	s2.spnego.in.vc_num = 1;
	s2.spnego.in.sesskey = parms->generic.in.sesskey;
	s2.spnego.in.capabilities = parms->generic.in.capabilities;
	s2.spnego.in.domain = parms->generic.in.domain;
	s2.spnego.in.os = "Unix";
	s2.spnego.in.lanman = "Samba";
	s2.spnego.out.vuid = session->vuid;

	smbcli_temp_set_signing(session->transport);

	status = gensec_client_start(session, &session->gensec);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start GENSEC client mode: %s\n", nt_errstr(status)));
		return status;
	}

	gensec_want_feature(session->gensec, GENSEC_FEATURE_SESSION_KEY);

	status = gensec_set_domain(session->gensec, parms->generic.in.domain);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC client domain to %s: %s\n", 
			  parms->generic.in.domain, nt_errstr(status)));
		goto done;
	}

	status = gensec_set_username(session->gensec, parms->generic.in.user);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC client username to %s: %s\n", 
			  parms->generic.in.user, nt_errstr(status)));
		goto done;
	}

	status = gensec_set_password(session->gensec, parms->generic.in.password);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC client password: %s\n", 
			  nt_errstr(status)));
		goto done;
	}

	status = gensec_set_target_hostname(session->gensec, session->transport->socket->hostname);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC target hostname: %s\n", 
			  nt_errstr(status)));
		goto done;
	}

	if (session->transport->negotiate.secblob.length) {
		chosen_oid = GENSEC_OID_SPNEGO;
	} else {
		/* without a sec blob, means raw NTLMSSP */
		chosen_oid = GENSEC_OID_NTLMSSP;
	}

	status = gensec_start_mech_by_oid(session->gensec, chosen_oid);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start set GENSEC client SPNEGO mechanism %s: %s\n",
			  gensec_get_name_by_oid(chosen_oid), nt_errstr(status)));
		goto done;
	}
	
	status = gensec_update(session->gensec, mem_ctx,
			       session->transport->negotiate.secblob,
			       &s2.spnego.in.secblob);

	while(1) {
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED) && !NT_STATUS_IS_OK(status)) {
			break;
		}

		if (!NT_STATUS_IS_OK(session_key_err)) {
			session_key_err = gensec_session_key(session->gensec, &session_key);
		}
		if (NT_STATUS_IS_OK(session_key_err)) {
			smbcli_transport_simple_set_signing(session->transport, session_key, null_data_blob);
		}
		
		if (NT_STATUS_IS_OK(status) && s2.spnego.in.secblob.length == 0) {
			break;
		}

		session->vuid = s2.spnego.out.vuid;
		status = smb_raw_session_setup(session, mem_ctx, &s2);
		session->vuid = UID_FIELD_INVALID;
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			break;
		}

		status = gensec_update(session->gensec, mem_ctx,
				       s2.spnego.out.secblob,
				       &s2.spnego.in.secblob);

	}

done:
	if (NT_STATUS_IS_OK(status)) {
		if (!NT_STATUS_IS_OK(session_key_err)) {
			DEBUG(1, ("Failed to get user session key: %s\n", nt_errstr(session_key_err)));
			return session_key_err;
		}

		smbcli_session_set_user_session_key(session, &session_key);

		parms->generic.out.vuid = s2.spnego.out.vuid;
		parms->generic.out.os = s2.spnego.out.os;
		parms->generic.out.lanman = s2.spnego.out.lanman;
		parms->generic.out.domain = s2.spnego.out.domain;
	} else {
		talloc_free(session->gensec);
		session->gensec = NULL;
		DEBUG(1, ("Failed to login with %s: %s\n", gensec_get_name_by_oid(chosen_oid), nt_errstr(status)));
		return status;
	}

	return status;
}

/****************************************************************************
 Perform a session setup (sync interface) using generic interface
****************************************************************************/
static NTSTATUS smb_raw_session_setup_generic(struct smbcli_session *session, 
					      TALLOC_CTX *mem_ctx,
					      union smb_sesssetup *parms) 
{
	if (session->transport->negotiate.protocol < PROTOCOL_LANMAN1) {
		/* no session setup at all in earliest protocols */
		ZERO_STRUCT(parms->generic.out);
		return NT_STATUS_OK;
	}

	/* see if we need to use the original session setup interface */
	if (session->transport->negotiate.protocol < PROTOCOL_NT1) {
		return smb_raw_session_setup_generic_old(session, mem_ctx, parms);
	}

	/* see if we should use the NT1 interface */
	if (!session->transport->options.use_spnego ||
	    !(parms->generic.in.capabilities & CAP_EXTENDED_SECURITY)) {
		return smb_raw_session_setup_generic_nt1(session, mem_ctx, parms);
	}

	/* default to using SPNEGO/NTLMSSP */
	return smb_raw_session_setup_generic_spnego(session, mem_ctx, parms);
}


/****************************************************************************
 Perform a session setup (sync interface)
this interface allows for RAW_SESSSETUP_GENERIC to auto-select session
setup variant based on negotiated protocol options
****************************************************************************/
NTSTATUS smb_raw_session_setup(struct smbcli_session *session, TALLOC_CTX *mem_ctx, 
			       union smb_sesssetup *parms) 
{
	struct smbcli_request *req;

	if (parms->generic.level == RAW_SESSSETUP_GENERIC) {
		NTSTATUS ret = smb_raw_session_setup_generic(session, mem_ctx, parms);

		if (NT_STATUS_IS_OK(ret) 
		    && parms->generic.in.user 
		    && *parms->generic.in.user) {
			if (!session->transport->negotiate.sign_info.doing_signing 
			    && session->transport->negotiate.sign_info.mandatory_signing) {
				DEBUG(0, ("SMB signing required, but server does not support it\n"));
				return NT_STATUS_ACCESS_DENIED;
			}
		}
		return ret;
	}

	req = smb_raw_session_setup_send(session, parms);
	return smb_raw_session_setup_recv(req, mem_ctx, parms);
}


/****************************************************************************
 Send a uloggoff (async send)
*****************************************************************************/
struct smbcli_request *smb_raw_ulogoff_send(struct smbcli_session *session)
{
	struct smbcli_request *req;

	SETUP_REQUEST_SESSION(SMBulogoffX, 2, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a uloggoff (sync interface)
*****************************************************************************/
NTSTATUS smb_raw_ulogoff(struct smbcli_session *session)
{
	struct smbcli_request *req = smb_raw_ulogoff_send(session);
	return smbcli_request_simple_recv(req);
}


/****************************************************************************
 Send a exit (async send)
*****************************************************************************/
struct smbcli_request *smb_raw_exit_send(struct smbcli_session *session)
{
	struct smbcli_request *req;

	SETUP_REQUEST_SESSION(SMBexit, 0, 0);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a exit (sync interface)
*****************************************************************************/
NTSTATUS smb_raw_exit(struct smbcli_session *session)
{
	struct smbcli_request *req = smb_raw_exit_send(session);
	return smbcli_request_simple_recv(req);
}
