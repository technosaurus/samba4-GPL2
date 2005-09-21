/* 
   Unix SMB/CIFS implementation.

   Kerberos backend for GENSEC
   
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   Copyright (C) Stefan Metzmacher 2004-2005

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
#include "system/kerberos.h"
#include "system/time.h"
#include "system/network.h"
#include "auth/kerberos/kerberos.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "auth/auth.h"

enum GENSEC_KRB5_STATE {
	GENSEC_KRB5_SERVER_START,
	GENSEC_KRB5_CLIENT_START,
	GENSEC_KRB5_CLIENT_MUTUAL_AUTH,
	GENSEC_KRB5_DONE
};

struct gensec_krb5_state {
	DATA_BLOB session_key;
	DATA_BLOB pac;
	enum GENSEC_KRB5_STATE state_position;
	struct smb_krb5_context *smb_krb5_context;
	krb5_auth_context auth_context;
	krb5_data enc_ticket;
	krb5_keyblock *keyblock;
	krb5_ticket *ticket;
	BOOL gssapi;
};

static int gensec_krb5_destroy(void *ptr) 
{
	struct gensec_krb5_state *gensec_krb5_state = ptr;

	if (!gensec_krb5_state->smb_krb5_context) {
		/* We can't clean anything else up unless we started up this far */
		return 0;
	}
	if (gensec_krb5_state->enc_ticket.length) { 
		kerberos_free_data_contents(gensec_krb5_state->smb_krb5_context->krb5_context, 
					    &gensec_krb5_state->enc_ticket); 
	}

	if (gensec_krb5_state->ticket) {
		krb5_free_ticket(gensec_krb5_state->smb_krb5_context->krb5_context, 
				 gensec_krb5_state->ticket);
	}

	/* ccache freed in a child destructor */

	krb5_free_keyblock(gensec_krb5_state->smb_krb5_context->krb5_context, 
			   gensec_krb5_state->keyblock);
		
	if (gensec_krb5_state->auth_context) {
		krb5_auth_con_free(gensec_krb5_state->smb_krb5_context->krb5_context, 
				   gensec_krb5_state->auth_context);
	}

	return 0;
}

static NTSTATUS gensec_krb5_start(struct gensec_security *gensec_security)
{
	struct gensec_krb5_state *gensec_krb5_state;

	gensec_krb5_state = talloc(gensec_security, struct gensec_krb5_state);
	if (!gensec_krb5_state) {
		return NT_STATUS_NO_MEMORY;
	}

	gensec_security->private_data = gensec_krb5_state;

	gensec_krb5_state->smb_krb5_context = NULL;
	gensec_krb5_state->auth_context = NULL;
	gensec_krb5_state->ticket = NULL;
	ZERO_STRUCT(gensec_krb5_state->enc_ticket);
	gensec_krb5_state->keyblock = NULL;
	gensec_krb5_state->session_key = data_blob(NULL, 0);
	gensec_krb5_state->pac = data_blob(NULL, 0);
	gensec_krb5_state->gssapi = False;

	talloc_set_destructor(gensec_krb5_state, gensec_krb5_destroy); 

	return NT_STATUS_OK;
}

static NTSTATUS gensec_krb5_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status;
	krb5_error_code ret = 0;
	struct gensec_krb5_state *gensec_krb5_state;

	nt_status = gensec_krb5_start(gensec_security);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	
	gensec_krb5_state = gensec_security->private_data;

	ret = smb_krb5_init_context(gensec_krb5_state,
				    &gensec_krb5_state->smb_krb5_context);
	if (ret) {
		DEBUG(1,("gensec_krb5_start: krb5_init_context failed (%s)\n", 
			 error_message(ret)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	ret = krb5_auth_con_init(gensec_krb5_state->smb_krb5_context->krb5_context, &gensec_krb5_state->auth_context);
	if (ret) {
		DEBUG(1,("gensec_krb5_start: krb5_auth_con_init failed (%s)\n", 
			 smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, 
						    ret, gensec_krb5_state)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	gensec_krb5_state = gensec_security->private_data;
	gensec_krb5_state->state_position = GENSEC_KRB5_SERVER_START;

	return NT_STATUS_OK;
}

static NTSTATUS gensec_fake_gssapi_krb5_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status = gensec_krb5_server_start(gensec_security);

	if (NT_STATUS_IS_OK(nt_status)) {
		struct gensec_krb5_state *gensec_krb5_state;
		gensec_krb5_state = gensec_security->private_data;
		gensec_krb5_state->gssapi = True;
	}
	return nt_status;
}

static NTSTATUS gensec_krb5_client_start(struct gensec_security *gensec_security)
{
	struct gensec_krb5_state *gensec_krb5_state;
	krb5_error_code ret;
	NTSTATUS nt_status;
	struct ccache_container *ccache_container;
	const char *hostname;
	krb5_flags ap_req_options = AP_OPTS_USE_SUBKEY | AP_OPTS_MUTUAL_REQUIRED;

	hostname = gensec_get_target_hostname(gensec_security);
	if (!hostname) {
		DEBUG(1, ("Could not determine hostname for target computer, cannot use kerberos\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (is_ipaddress(hostname)) {
		DEBUG(2, ("Cannot do krb5 to an IP address"));
		return NT_STATUS_INVALID_PARAMETER;
	}

			
	nt_status = gensec_krb5_start(gensec_security);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	gensec_krb5_state = gensec_security->private_data;
	gensec_krb5_state->state_position = GENSEC_KRB5_CLIENT_START;

	ret = cli_credentials_get_ccache(gensec_security->credentials, &ccache_container);
	if (ret) {
		DEBUG(1,("gensec_krb5_start: cli_credentials_get_ccache failed: %s\n", 
			 error_message(ret)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	gensec_krb5_state->smb_krb5_context = talloc_reference(gensec_krb5_state, ccache_container->smb_krb5_context);

	ret = krb5_auth_con_init(gensec_krb5_state->smb_krb5_context->krb5_context, &gensec_krb5_state->auth_context);
	if (ret) {
		DEBUG(1,("gensec_krb5_start: krb5_auth_con_init failed (%s)\n", 
			 smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, 
						    ret, gensec_krb5_state)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (ret == 0) {
		char *principal;
		krb5_data in_data;
		in_data.length = 0;
		
		principal = gensec_get_target_principal(gensec_security);
		if (principal && lp_client_use_spnego_principal()) {
			krb5_principal target_principal;
			ret = krb5_parse_name(gensec_krb5_state->smb_krb5_context->krb5_context, principal,
					      &target_principal);
			if (ret == 0) {
				ret = krb5_mk_req_exact(gensec_krb5_state->smb_krb5_context->krb5_context, 
							&gensec_krb5_state->auth_context,
							ap_req_options, 
							target_principal,
							&in_data, ccache_container->ccache, 
							&gensec_krb5_state->enc_ticket);
				krb5_free_principal(gensec_krb5_state->smb_krb5_context->krb5_context, 
						    target_principal);
			}
		} else {
			ret = krb5_mk_req(gensec_krb5_state->smb_krb5_context->krb5_context, 
					  &gensec_krb5_state->auth_context,
					  ap_req_options,
					  gensec_get_target_service(gensec_security),
					  hostname,
					  &in_data, ccache_container->ccache, 
					  &gensec_krb5_state->enc_ticket);
		}
	}
	switch (ret) {
	case 0:
		return NT_STATUS_OK;
	case KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN:
		DEBUG(3, ("Server [%s] is not registered with our KDC: %s\n", 
			  hostname, smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, ret, gensec_krb5_state)));
		return NT_STATUS_ACCESS_DENIED;
	case KRB5KDC_ERR_PREAUTH_FAILED:
	case KRB5KRB_AP_ERR_TKT_EXPIRED:
	case KRB5_CC_END:
		/* Too much clock skew - we will need to kinit to re-skew the clock */
	case KRB5KRB_AP_ERR_SKEW:
	case KRB5_KDCREP_SKEW:
	{
		DEBUG(3, ("kerberos (mk_req) failed: %s\n", 
			  smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, ret, gensec_krb5_state)));
		/* fall down to remaining code */
	}
	
	
	/* just don't print a message for these really ordinary messages */
	case KRB5_FCC_NOFILE:
	case KRB5_CC_NOTFOUND:
	case ENOENT:
		
		return NT_STATUS_UNSUCCESSFUL;
		break;
		
	default:
		DEBUG(0, ("kerberos: %s\n", 
			  smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, ret, gensec_krb5_state)));
		return NT_STATUS_UNSUCCESSFUL;
	}
}

static NTSTATUS gensec_fake_gssapi_krb5_client_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status = gensec_krb5_client_start(gensec_security);

	if (NT_STATUS_IS_OK(nt_status)) {
		struct gensec_krb5_state *gensec_krb5_state;
		gensec_krb5_state = gensec_security->private_data;
		gensec_krb5_state->gssapi = True;
	}
	return nt_status;
}

/**
 * Check if the packet is one for this mechansim
 * 
 * @param gensec_security GENSEC state
 * @param in The request, as a DATA_BLOB
 * @return Error, INVALID_PARAMETER if it's not a packet for us
 *                or NT_STATUS_OK if the packet is ok. 
 */

static NTSTATUS gensec_fake_gssapi_krb5_magic(struct gensec_security *gensec_security, 
				  const DATA_BLOB *in) 
{
	if (gensec_gssapi_check_oid(in, GENSEC_OID_KERBEROS5)) {
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_INVALID_PARAMETER;
	}
}


/**
 * Next state function for the Krb5 GENSEC mechanism
 * 
 * @param gensec_krb5_state KRB5 State
 * @param out_mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param in The request, as a DATA_BLOB
 * @param out The reply, as an talloc()ed DATA_BLOB, on *out_mem_ctx
 * @return Error, MORE_PROCESSING_REQUIRED if a reply is sent, 
 *                or NT_STATUS_OK if the user is authenticated. 
 */

static NTSTATUS gensec_krb5_update(struct gensec_security *gensec_security, 
				   TALLOC_CTX *out_mem_ctx, 
				   const DATA_BLOB in, DATA_BLOB *out) 
{
	struct gensec_krb5_state *gensec_krb5_state = gensec_security->private_data;
	krb5_error_code ret = 0;
	NTSTATUS nt_status;

	switch (gensec_krb5_state->state_position) {
	case GENSEC_KRB5_CLIENT_START:
	{
		DATA_BLOB unwrapped_out;
		
		if (gensec_krb5_state->gssapi) {
			unwrapped_out = data_blob_talloc(out_mem_ctx, gensec_krb5_state->enc_ticket.data, gensec_krb5_state->enc_ticket.length);
			
			/* wrap that up in a nice GSS-API wrapping */
			*out = gensec_gssapi_gen_krb5_wrap(out_mem_ctx, &unwrapped_out, TOK_ID_KRB_AP_REQ);
		} else {
			*out = data_blob_talloc(out_mem_ctx, gensec_krb5_state->enc_ticket.data, gensec_krb5_state->enc_ticket.length);
		}
		gensec_krb5_state->state_position = GENSEC_KRB5_CLIENT_MUTUAL_AUTH;
		nt_status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		return nt_status;
	}
		
	case GENSEC_KRB5_CLIENT_MUTUAL_AUTH:
	{
		DATA_BLOB unwrapped_in;
		krb5_data inbuf;
		krb5_ap_rep_enc_part *repl = NULL;
		uint8_t tok_id[2];

		if (gensec_krb5_state->gssapi) {
			if (!gensec_gssapi_parse_krb5_wrap(out_mem_ctx, &in, &unwrapped_in, tok_id)) {
				DEBUG(1,("gensec_gssapi_parse_krb5_wrap(mutual authentication) failed to parse\n"));
				dump_data_pw("Mutual authentication message:\n", in.data, in.length);
				return NT_STATUS_INVALID_PARAMETER;
			}
		} else {
			unwrapped_in = in;
		}
		/* TODO: check the tok_id */

		inbuf.data = unwrapped_in.data;
		inbuf.length = unwrapped_in.length;
		ret = krb5_rd_rep(gensec_krb5_state->smb_krb5_context->krb5_context, 
				  gensec_krb5_state->auth_context,
				  &inbuf, &repl);
		if (ret) {
			DEBUG(1,("krb5_rd_rep (mutual authentication) failed (%s)\n",
				 smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, ret, out_mem_ctx)));
			dump_data_pw("Mutual authentication message:\n", inbuf.data, inbuf.length);
			nt_status = NT_STATUS_ACCESS_DENIED;
		} else {
			*out = data_blob(NULL, 0);
			nt_status = NT_STATUS_OK;
			gensec_krb5_state->state_position = GENSEC_KRB5_DONE;
		}
		if (repl) {
			krb5_free_ap_rep_enc_part(gensec_krb5_state->smb_krb5_context->krb5_context, repl);
		}
		return nt_status;
	}

	case GENSEC_KRB5_SERVER_START:
	{
		DATA_BLOB unwrapped_in;
		DATA_BLOB unwrapped_out = data_blob(NULL, 0);
		uint8_t tok_id[2];

		if (!in.data) {
			return NT_STATUS_INVALID_PARAMETER;
		}	

		/* Parse the GSSAPI wrapping, if it's there... (win2k3 allows it to be omited) */
		if (gensec_krb5_state->gssapi
		    && gensec_gssapi_parse_krb5_wrap(out_mem_ctx, &in, &unwrapped_in, tok_id)) {
			nt_status = ads_verify_ticket(out_mem_ctx, 
						      gensec_krb5_state->smb_krb5_context,
						      &gensec_krb5_state->auth_context, 
						      lp_realm(), 
						      gensec_get_target_service(gensec_security), &unwrapped_in, 
						      &gensec_krb5_state->ticket, &unwrapped_out,
						      &gensec_krb5_state->keyblock);
		} else {
			/* TODO: check the tok_id */
			nt_status = ads_verify_ticket(out_mem_ctx, 
						      gensec_krb5_state->smb_krb5_context,
						      &gensec_krb5_state->auth_context, 
						      lp_realm(), 
						      gensec_get_target_service(gensec_security), 
						      &in, 
						      &gensec_krb5_state->ticket, &unwrapped_out,
						      &gensec_krb5_state->keyblock);
		}

		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}

		if (NT_STATUS_IS_OK(nt_status)) {
			gensec_krb5_state->state_position = GENSEC_KRB5_DONE;
			/* wrap that up in a nice GSS-API wrapping */
			if (gensec_krb5_state->gssapi) {
				*out = gensec_gssapi_gen_krb5_wrap(out_mem_ctx, &unwrapped_out, TOK_ID_KRB_AP_REP);
			} else {
				*out = unwrapped_out;
			}
		}
		return nt_status;
	}
	case GENSEC_KRB5_DONE:
		return NT_STATUS_OK;
	}
	
	return NT_STATUS_INVALID_PARAMETER;
}

static NTSTATUS gensec_krb5_session_key(struct gensec_security *gensec_security, 
					DATA_BLOB *session_key) 
{
	struct gensec_krb5_state *gensec_krb5_state = gensec_security->private_data;
	krb5_context context = gensec_krb5_state->smb_krb5_context->krb5_context;
	krb5_auth_context auth_context = gensec_krb5_state->auth_context;
	krb5_keyblock *skey;
	krb5_error_code err = -1;

	if (gensec_krb5_state->session_key.data) {
		*session_key = gensec_krb5_state->session_key;
		return NT_STATUS_OK;
	}

	switch (gensec_security->gensec_role) {
	case GENSEC_CLIENT:
		err = krb5_auth_con_getlocalsubkey(context, auth_context, &skey);
		break;
	case GENSEC_SERVER:
		err = krb5_auth_con_getremotesubkey(context, auth_context, &skey);
		break;
	}
	if (err == 0 && skey != NULL) {
		DEBUG(10, ("Got KRB5 session key of length %d\n",  
			   (int)KRB5_KEY_LENGTH(skey)));
		gensec_krb5_state->session_key = data_blob_talloc(gensec_krb5_state, 
						KRB5_KEY_DATA(skey), KRB5_KEY_LENGTH(skey));
		*session_key = gensec_krb5_state->session_key;
		dump_data_pw("KRB5 Session Key:\n", session_key->data, session_key->length);

		krb5_free_keyblock(context, skey);
		return NT_STATUS_OK;
	} else {
		DEBUG(10, ("KRB5 error getting session key %d\n", err));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}
}

static NTSTATUS gensec_krb5_session_info(struct gensec_security *gensec_security,
					 struct auth_session_info **_session_info) 
{
	NTSTATUS nt_status;
	struct gensec_krb5_state *gensec_krb5_state = gensec_security->private_data;
	struct auth_serversupplied_info *server_info = NULL;
	struct auth_session_info *session_info = NULL;
	struct PAC_LOGON_INFO *logon_info;

	krb5_const_principal client_principal;
	
	DATA_BLOB pac_wrapped;
	DATA_BLOB pac;

	TALLOC_CTX *mem_ctx = talloc_new(gensec_security);
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	pac_wrapped = get_auth_data_from_tkt(mem_ctx, gensec_krb5_state->ticket);

	pac = unwrap_pac(mem_ctx, &pac_wrapped);

	client_principal = get_principal_from_tkt(gensec_krb5_state->ticket);

	/* decode and verify the pac */
	nt_status = kerberos_pac_logon_info(gensec_krb5_state, &logon_info, pac,
					    gensec_krb5_state->smb_krb5_context->krb5_context,
					    NULL, gensec_krb5_state->keyblock,
					    client_principal,
					    gensec_krb5_state->ticket->ticket.authtime);

	/* IF we have the PAC - otherwise we need to get this
	 * data from elsewere - local ldb, or (TODO) lookup of some
	 * kind... 
	 *
	 * when heimdal can generate the PAC, we should fail if there's
	 * no PAC present
	 */

	if (NT_STATUS_IS_OK(nt_status)) {
		union netr_Validation validation;
		validation.sam3 = &logon_info->info3;
		nt_status = make_server_info_netlogon_validation(gensec_krb5_state, 
								 NULL,
								 3, &validation,
								 &server_info); 
		talloc_free(mem_ctx);
		NT_STATUS_NOT_OK_RETURN(nt_status);
	} else {
		krb5_error_code ret;
		DATA_BLOB user_sess_key = data_blob(NULL, 0);
		DATA_BLOB lm_sess_key = data_blob(NULL, 0);

		char *account_name;
		const char *realm = krb5_principal_get_realm(gensec_krb5_state->smb_krb5_context->krb5_context, 
							     get_principal_from_tkt(gensec_krb5_state->ticket));
		ret = krb5_unparse_name_norealm(gensec_krb5_state->smb_krb5_context->krb5_context, 
						get_principal_from_tkt(gensec_krb5_state->ticket), &account_name);
		if (ret) {
			return NT_STATUS_NO_MEMORY;
		}

		/* TODO: should we pass the krb5 session key in here? */
		nt_status = sam_get_server_info(mem_ctx, account_name, realm,
						user_sess_key, lm_sess_key,
						&server_info);
		free(account_name);
		
		if (!NT_STATUS_IS_OK(nt_status)) {
			talloc_free(mem_ctx);
			return nt_status;
		}
	}

	/* references the server_info into the session_info */
	nt_status = auth_generate_session_info(gensec_krb5_state, server_info, &session_info);
	talloc_free(mem_ctx);

	NT_STATUS_NOT_OK_RETURN(nt_status);

	nt_status = gensec_krb5_session_key(gensec_security, &session_info->session_key);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	*_session_info = session_info;

	return NT_STATUS_OK;
}

static NTSTATUS gensec_krb5_wrap(struct gensec_security *gensec_security, 
				   TALLOC_CTX *mem_ctx, 
				   const DATA_BLOB *in, 
				   DATA_BLOB *out)
{
	struct gensec_krb5_state *gensec_krb5_state = gensec_security->private_data;
	krb5_context context = gensec_krb5_state->smb_krb5_context->krb5_context;
	krb5_auth_context auth_context = gensec_krb5_state->auth_context;
	krb5_error_code ret;
	krb5_data input, output;
	krb5_replay_data replay;
	input.length = in->length;
	input.data = in->data;
	
	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
		ret = krb5_mk_priv(context, auth_context, &input, &output, &replay);
		if (ret) {
			DEBUG(1, ("krb5_mk_priv failed: %s\n", 
				  smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, 
							     ret, mem_ctx)));
			return NT_STATUS_ACCESS_DENIED;
		}
		*out = data_blob_talloc(mem_ctx, output.data, output.length);
		
		krb5_data_free(&output);
	} else {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static NTSTATUS gensec_krb5_unwrap(struct gensec_security *gensec_security, 
				     TALLOC_CTX *mem_ctx, 
				     const DATA_BLOB *in, 
				     DATA_BLOB *out)
{
	struct gensec_krb5_state *gensec_krb5_state = gensec_security->private_data;
	krb5_context context = gensec_krb5_state->smb_krb5_context->krb5_context;
	krb5_auth_context auth_context = gensec_krb5_state->auth_context;
	krb5_error_code ret;
	krb5_data input, output;
	krb5_replay_data replay;
	input.length = in->length;
	input.data = in->data;
	
	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
		ret = krb5_rd_priv(context, auth_context, &input, &output, &replay);
		if (ret) {
			DEBUG(1, ("krb5_rd_priv failed: %s\n", 
				  smb_get_krb5_error_message(gensec_krb5_state->smb_krb5_context->krb5_context, 
							     ret, mem_ctx)));
			return NT_STATUS_ACCESS_DENIED;
		}
		*out = data_blob_talloc(mem_ctx, output.data, output.length);
		
		krb5_data_free(&output);
	} else {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static BOOL gensec_krb5_have_feature(struct gensec_security *gensec_security,
				     uint32_t feature)
{
	if (feature & GENSEC_FEATURE_SESSION_KEY) {
		return True;
	} 
	
	return False;
}

static const char *gensec_krb5_oids[] = { 
	GENSEC_OID_KERBEROS5,
	GENSEC_OID_KERBEROS5_OLD,
	NULL 
};

static const struct gensec_security_ops gensec_fake_gssapi_krb5_security_ops = {
	.name		= "fake_gssapi_krb5",
	.auth_type	= DCERPC_AUTH_TYPE_KRB5,
	.oid            = gensec_krb5_oids,
	.client_start   = gensec_fake_gssapi_krb5_client_start,
	.server_start   = gensec_fake_gssapi_krb5_server_start,
	.update 	= gensec_krb5_update,
	.magic   	= gensec_fake_gssapi_krb5_magic,
	.session_key	= gensec_krb5_session_key,
	.session_info	= gensec_krb5_session_info,
	.have_feature   = gensec_krb5_have_feature,
	.wrap           = gensec_krb5_wrap,
	.unwrap         = gensec_krb5_unwrap,
	.enabled        = False
};

static const struct gensec_security_ops gensec_krb5_security_ops = {
	.name		= "krb5",
	.client_start   = gensec_krb5_client_start,
	.server_start   = gensec_krb5_server_start,
	.update 	= gensec_krb5_update,
	.session_key	= gensec_krb5_session_key,
	.session_info	= gensec_krb5_session_info,
	.have_feature   = gensec_krb5_have_feature,
	.enabled        = True
};

NTSTATUS gensec_krb5_init(void)
{
	NTSTATUS ret;

	ret = gensec_register(&gensec_krb5_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_krb5_security_ops.name));
		return ret;
	}

	ret = gensec_register(&gensec_fake_gssapi_krb5_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_krb5_security_ops.name));
		return ret;
	}

	return ret;
}
