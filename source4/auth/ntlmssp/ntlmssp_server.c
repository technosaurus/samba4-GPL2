/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   handle NLTMSSP, client server side parsing

   Copyright (C) Andrew Tridgell      2001
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2005
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
#include "auth/ntlmssp/ntlmssp.h"
#include "lib/crypto/crypto.h"
#include "pstring.h"

/** 
 * Set a username on an NTLMSSP context - ensures it is talloc()ed 
 *
 */

static NTSTATUS ntlmssp_set_username(struct ntlmssp_state *ntlmssp_state, const char *user) 
{
	if (!user) {
		/* it should be at least "" */
		DEBUG(1, ("NTLMSSP failed to set username - cannot accept NULL username\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	ntlmssp_state->user = talloc_strdup(ntlmssp_state, user);
	if (!ntlmssp_state->user) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/** 
 * Set a domain on an NTLMSSP context - ensures it is talloc()ed 
 *
 */
static NTSTATUS ntlmssp_set_domain(struct ntlmssp_state *ntlmssp_state, const char *domain) 
{
	ntlmssp_state->domain = talloc_strdup(ntlmssp_state, domain);
	if (!ntlmssp_state->domain) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/** 
 * Set a workstation on an NTLMSSP context - ensures it is talloc()ed 
 *
 */
static NTSTATUS ntlmssp_set_workstation(struct ntlmssp_state *ntlmssp_state, const char *workstation) 
{
	ntlmssp_state->workstation = talloc_strdup(ntlmssp_state, workstation);
	if (!ntlmssp_state->workstation) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/**
 * Default challenge generation code.
 *
 */
   
static const uint8_t *get_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	uint8_t *chal = talloc_size(ntlmssp_state, 8);
	generate_random_buffer(chal, 8);

	return chal;
}

/**
 * Default 'we can set the challenge to anything we like' implementation
 *
 */
   
static BOOL may_set_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	return True;
}

/**
 * Default 'we can set the challenge to anything we like' implementation
 *
 * Does not actually do anything, as the value is always in the structure anyway.
 *
 */
   
static NTSTATUS set_challenge(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *challenge)
{
	SMB_ASSERT(challenge->length == 8);
	return NT_STATUS_OK;
}

/**
 * Determine correct target name flags for reply, given server role 
 * and negotiated flags
 * 
 * @param ntlmssp_state NTLMSSP State
 * @param neg_flags The flags from the packet
 * @param chal_flags The flags to be set in the reply packet
 * @return The 'target name' string.
 */

static const char *ntlmssp_target_name(struct ntlmssp_state *ntlmssp_state,
				       uint32_t neg_flags, uint32_t *chal_flags) 
{
	if (neg_flags & NTLMSSP_REQUEST_TARGET) {
		*chal_flags |= NTLMSSP_CHAL_TARGET_INFO;
		*chal_flags |= NTLMSSP_REQUEST_TARGET;
		if (ntlmssp_state->server_role == ROLE_STANDALONE) {
			*chal_flags |= NTLMSSP_TARGET_TYPE_SERVER;
			return ntlmssp_state->server_name;
		} else {
			*chal_flags |= NTLMSSP_TARGET_TYPE_DOMAIN;
			return ntlmssp_state->get_domain();
		};
	} else {
		return "";
	}
}

/**
 * Next state function for the Negotiate packet
 * 
 * @param ntlmssp_state NTLMSSP State
 * @param out_mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param in The request, as a DATA_BLOB
 * @param out The reply, as an talloc()ed DATA_BLOB, on *out_mem_ctx
 * @return Errors or MORE_PROCESSING_REQUIRED if a reply is sent. 
 */

NTSTATUS ntlmssp_server_negotiate(struct gensec_security *gensec_security, 
				  TALLOC_CTX *out_mem_ctx, 
				  const DATA_BLOB in, DATA_BLOB *out) 
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;
	struct ntlmssp_state *ntlmssp_state = gensec_ntlmssp_state->ntlmssp_state;
	DATA_BLOB struct_blob;
	fstring dnsname, dnsdomname;
	uint32_t neg_flags = 0;
	uint32_t ntlmssp_command, chal_flags;
	char *cliname=NULL, *domname=NULL;
	const uint8_t *cryptkey;
	const char *target_name;

	/* parse the NTLMSSP packet */
#if 0
	file_save("ntlmssp_negotiate.dat", request.data, request.length);
#endif

	if (in.length) {
		if (!msrpc_parse(ntlmssp_state, 
				 &in, "CddAA",
				 "NTLMSSP",
				 &ntlmssp_command,
				 &neg_flags,
				 &cliname,
				 &domname)) {
			DEBUG(1, ("ntlmssp_server_negotiate: failed to parse NTLMSSP:\n"));
			dump_data(2, in.data, in.length);
			return NT_STATUS_INVALID_PARAMETER;
		}
		
		debug_ntlmssp_flags(neg_flags);
	}
	
	ntlmssp_handle_neg_flags(ntlmssp_state, neg_flags, ntlmssp_state->allow_lm_key);

	/* Ask our caller what challenge they would like in the packet */
	cryptkey = ntlmssp_state->get_challenge(ntlmssp_state);

	/* Check if we may set the challenge */
	if (!ntlmssp_state->may_set_challenge(ntlmssp_state)) {
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_NTLM2;
	}

	/* The flags we send back are not just the negotiated flags,
	 * they are also 'what is in this packet'.  Therfore, we
	 * operate on 'chal_flags' from here on 
	 */

	chal_flags = ntlmssp_state->neg_flags;

	/* get the right name to fill in as 'target' */
	target_name = ntlmssp_target_name(ntlmssp_state, 
					  neg_flags, &chal_flags); 
	if (target_name == NULL) 
		return NT_STATUS_INVALID_PARAMETER;

	ntlmssp_state->chal = data_blob_talloc(ntlmssp_state, cryptkey, 8);
	ntlmssp_state->internal_chal = data_blob_talloc(ntlmssp_state, cryptkey, 8);

	/* This should be a 'netbios domain -> DNS domain' mapping */
	dnsdomname[0] = '\0';
	get_mydomname(dnsdomname);
	strlower_m(dnsdomname);
	
	dnsname[0] = '\0';
	get_myfullname(dnsname);
	
	/* This creates the 'blob' of names that appears at the end of the packet */
	if (chal_flags & NTLMSSP_CHAL_TARGET_INFO) 
	{
		const char *target_name_dns = "";
		if (chal_flags |= NTLMSSP_TARGET_TYPE_DOMAIN) {
			target_name_dns = dnsdomname;
		} else if (chal_flags |= NTLMSSP_TARGET_TYPE_SERVER) {
			target_name_dns = dnsname;
		}

		msrpc_gen(out_mem_ctx, 
			  &struct_blob, "aaaaa",
			  NTLMSSP_NAME_TYPE_DOMAIN, target_name,
			  NTLMSSP_NAME_TYPE_SERVER, ntlmssp_state->server_name,
			  NTLMSSP_NAME_TYPE_DOMAIN_DNS, dnsdomname,
			  NTLMSSP_NAME_TYPE_SERVER_DNS, dnsname,
			  0, "");
	} else {
		struct_blob = data_blob(NULL, 0);
	}

	{
		/* Marshel the packet in the right format, be it unicode or ASCII */
		const char *gen_string;
		if (ntlmssp_state->unicode) {
			gen_string = "CdUdbddB";
		} else {
			gen_string = "CdAdbddB";
		}
		
		msrpc_gen(out_mem_ctx, 
			  out, gen_string,
			  "NTLMSSP", 
			  NTLMSSP_CHALLENGE,
			  target_name,
			  chal_flags,
			  cryptkey, 8,
			  0, 0,
			  struct_blob.data, struct_blob.length);
	}
		
	ntlmssp_state->expected_state = NTLMSSP_AUTH;

	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}

/**
 * Next state function for the Authenticate packet
 * 
 * @param ntlmssp_state NTLMSSP State
 * @param request The request, as a DATA_BLOB
 * @return Errors or NT_STATUS_OK. 
 */

static NTSTATUS ntlmssp_server_preauth(struct ntlmssp_state *ntlmssp_state,
				       const DATA_BLOB request) 
{
	uint32_t ntlmssp_command, auth_flags;
	NTSTATUS nt_status;

	uint8_t session_nonce_hash[16];

	const char *parse_string;
	char *domain = NULL;
	char *user = NULL;
	char *workstation = NULL;

#if 0
	file_save("ntlmssp_auth.dat", request.data, request.length);
#endif

	if (ntlmssp_state->unicode) {
		parse_string = "CdBBUUUBd";
	} else {
		parse_string = "CdBBAAABd";
	}

	/* zero these out */
	data_blob_free(&ntlmssp_state->lm_resp);
	data_blob_free(&ntlmssp_state->nt_resp);

	ntlmssp_state->user = NULL;
	ntlmssp_state->domain = NULL;
	ntlmssp_state->workstation = NULL;

	/* now the NTLMSSP encoded auth hashes */
	if (!msrpc_parse(ntlmssp_state, 
			 &request, parse_string,
			 "NTLMSSP", 
			 &ntlmssp_command, 
			 &ntlmssp_state->lm_resp,
			 &ntlmssp_state->nt_resp,
			 &domain, 
			 &user, 
			 &workstation,
			 &ntlmssp_state->encrypted_session_key,
			 &auth_flags)) {
		DEBUG(10, ("ntlmssp_server_auth: failed to parse NTLMSSP (nonfatal):\n"));
		dump_data(10, request.data, request.length);

		/* zero this out */
		data_blob_free(&ntlmssp_state->encrypted_session_key);
		auth_flags = 0;
		
		/* Try again with a shorter string (Win9X truncates this packet) */
		if (ntlmssp_state->unicode) {
			parse_string = "CdBBUUU";
		} else {
			parse_string = "CdBBAAA";
		}

		/* now the NTLMSSP encoded auth hashes */
		if (!msrpc_parse(ntlmssp_state, 
				 &request, parse_string,
				 "NTLMSSP", 
				 &ntlmssp_command, 
				 &ntlmssp_state->lm_resp,
				 &ntlmssp_state->nt_resp,
				 &domain, 
				 &user, 
				 &workstation)) {
			DEBUG(1, ("ntlmssp_server_auth: failed to parse NTLMSSP:\n"));
			dump_data(2, request.data, request.length);

			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	if (auth_flags)
		ntlmssp_handle_neg_flags(ntlmssp_state, auth_flags, ntlmssp_state->allow_lm_key);

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_domain(ntlmssp_state, domain))) {
		/* zero this out */
		data_blob_free(&ntlmssp_state->encrypted_session_key);
		return nt_status;
	}

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_username(ntlmssp_state, user))) {
		/* zero this out */
		data_blob_free(&ntlmssp_state->encrypted_session_key);
		return nt_status;
	}

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_workstation(ntlmssp_state, workstation))) {
		/* zero this out */
		data_blob_free(&ntlmssp_state->encrypted_session_key);
		return nt_status;
	}

	DEBUG(3,("Got user=[%s] domain=[%s] workstation=[%s] len1=%lu len2=%lu\n",
		 ntlmssp_state->user, ntlmssp_state->domain, ntlmssp_state->workstation, (unsigned long)ntlmssp_state->lm_resp.length, (unsigned long)ntlmssp_state->nt_resp.length));

#if 0
	file_save("nthash1.dat",  &ntlmssp_state->nt_resp.data,  &ntlmssp_state->nt_resp.length);
	file_save("lmhash1.dat",  &ntlmssp_state->lm_resp.data,  &ntlmssp_state->lm_resp.length);
#endif

	/* NTLM2 uses a 'challenge' that is made of up both the server challenge, and a 
	   client challenge 
	
	   However, the NTLM2 flag may still be set for the real NTLMv2 logins, be careful.
	*/
	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		if (ntlmssp_state->nt_resp.length == 24 && ntlmssp_state->lm_resp.length == 24) {
			struct MD5Context md5_session_nonce_ctx;
			SMB_ASSERT(ntlmssp_state->internal_chal.data 
				   && ntlmssp_state->internal_chal.length == 8);
			
			ntlmssp_state->doing_ntlm2 = True;

			memcpy(ntlmssp_state->session_nonce, ntlmssp_state->internal_chal.data, 8);
			memcpy(&ntlmssp_state->session_nonce[8], ntlmssp_state->lm_resp.data, 8);
			
			MD5Init(&md5_session_nonce_ctx);
			MD5Update(&md5_session_nonce_ctx, ntlmssp_state->session_nonce, 16);
			MD5Final(session_nonce_hash, &md5_session_nonce_ctx);
			
			ntlmssp_state->chal = data_blob_talloc(ntlmssp_state, 
							       session_nonce_hash, 8);

			/* LM response is no longer useful, zero it out */
			data_blob_free(&ntlmssp_state->lm_resp);

			/* We changed the effective challenge - set it */
			if (!NT_STATUS_IS_OK(nt_status = 
					     ntlmssp_state->set_challenge(ntlmssp_state, 
									  &ntlmssp_state->chal))) {
				/* zero this out */
				data_blob_free(&ntlmssp_state->encrypted_session_key);
				return nt_status;
			}

			/* LM Key is incompatible... */
			ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;
		}
	}
	return NT_STATUS_OK;
}

/**
 * Next state function for the Authenticate packet 
 * (after authentication - figures out the session keys etc)
 * 
 * @param ntlmssp_state NTLMSSP State
 * @return Errors or NT_STATUS_OK. 
 */

static NTSTATUS ntlmssp_server_postauth(struct ntlmssp_state *ntlmssp_state,
					DATA_BLOB *user_session_key, 
					DATA_BLOB *lm_session_key) 
{
	NTSTATUS nt_status;
	DATA_BLOB session_key = data_blob(NULL, 0);

	if (user_session_key)
		dump_data_pw("USER session key:\n", user_session_key->data, user_session_key->length);

	if (lm_session_key) 
		dump_data_pw("LM first-8:\n", lm_session_key->data, lm_session_key->length);

	/* Handle the different session key derivation for NTLM2 */
	if (ntlmssp_state->doing_ntlm2) {
		if (user_session_key && user_session_key->data && user_session_key->length == 16) {
			session_key = data_blob_talloc(ntlmssp_state, NULL, 16);
			hmac_md5(user_session_key->data, ntlmssp_state->session_nonce, 
				 sizeof(ntlmssp_state->session_nonce), session_key.data);
			DEBUG(10,("ntlmssp_server_auth: Created NTLM2 session key.\n"));
			dump_data_pw("NTLM2 session key:\n", session_key.data, session_key.length);
			
		} else {
			DEBUG(10,("ntlmssp_server_auth: Failed to create NTLM2 session key.\n"));
			session_key = data_blob(NULL, 0);
		}
	} else if ((ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_LM_KEY) 
		/* Ensure we can never get here on NTLMv2 */
		&& (ntlmssp_state->nt_resp.length == 0 || ntlmssp_state->nt_resp.length == 24)) {

		if (lm_session_key && lm_session_key->data && lm_session_key->length >= 8) {
			if (ntlmssp_state->lm_resp.data && ntlmssp_state->lm_resp.length == 24) {
				session_key = data_blob_talloc(ntlmssp_state, NULL, 16);
				SMBsesskeygen_lm_sess_key(lm_session_key->data, ntlmssp_state->lm_resp.data, 
							  session_key.data);
				DEBUG(10,("ntlmssp_server_auth: Created NTLM session key.\n"));
				dump_data_pw("LM session key:\n", session_key.data, session_key.length);
  			} else {
				
				/* When there is no LM response, just use zeros */
 				static const uint8_t zeros[24];
 				session_key = data_blob_talloc(ntlmssp_state, NULL, 16);
 				SMBsesskeygen_lm_sess_key(zeros, zeros, 
 							  session_key.data);
 				DEBUG(10,("ntlmssp_server_auth: Created NTLM session key.\n"));
 				dump_data_pw("LM session key:\n", session_key.data, session_key.length);
			}
		} else {
 			/* LM Key not selected */
 			ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;

			DEBUG(10,("ntlmssp_server_auth: Failed to create NTLM session key.\n"));
			session_key = data_blob(NULL, 0);
		}

	} else if (user_session_key && user_session_key->data) {
		session_key = *user_session_key;
		DEBUG(10,("ntlmssp_server_auth: Using unmodified nt session key.\n"));
		dump_data_pw("unmodified session key:\n", session_key.data, session_key.length);

		/* LM Key not selected */
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;

	} else if (lm_session_key && lm_session_key->data) {
		/* Very weird to have LM key, but no user session key, but anyway.. */
		session_key = *lm_session_key;
		DEBUG(10,("ntlmssp_server_auth: Using unmodified lm session key.\n"));
		dump_data_pw("unmodified session key:\n", session_key.data, session_key.length);

		/* LM Key not selected */
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;

	} else {
		DEBUG(10,("ntlmssp_server_auth: Failed to create unmodified session key.\n"));
		session_key = data_blob(NULL, 0);

		/* LM Key not selected */
		ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;
	}

	/* With KEY_EXCH, the client supplies the proposed session key, 
	   but encrypts it with the long-term key */
	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_KEY_EXCH) {
		if (!ntlmssp_state->encrypted_session_key.data 
		    || ntlmssp_state->encrypted_session_key.length != 16) {
			data_blob_free(&ntlmssp_state->encrypted_session_key);
			DEBUG(1, ("Client-supplied KEY_EXCH session key was of invalid length (%u)!\n", 
				  ntlmssp_state->encrypted_session_key.length));
			return NT_STATUS_INVALID_PARAMETER;
		} else if (!session_key.data || session_key.length != 16) {
			DEBUG(5, ("server session key is invalid (len == %u), cannot do KEY_EXCH!\n", 
				  session_key.length));
			ntlmssp_state->session_key = session_key;
		} else {
			dump_data_pw("KEY_EXCH session key (enc):\n", 
				     ntlmssp_state->encrypted_session_key.data, 
				     ntlmssp_state->encrypted_session_key.length);
			arcfour_crypt(ntlmssp_state->encrypted_session_key.data, 
				      session_key.data, 
				      ntlmssp_state->encrypted_session_key.length);
			ntlmssp_state->session_key = data_blob_talloc(ntlmssp_state, 
								      ntlmssp_state->encrypted_session_key.data, 
								      ntlmssp_state->encrypted_session_key.length);
			dump_data_pw("KEY_EXCH session key:\n", ntlmssp_state->encrypted_session_key.data, 
				     ntlmssp_state->encrypted_session_key.length);
		}
	} else {
		ntlmssp_state->session_key = session_key;
	}

 	/* The server might need us to use a partial-strength session key */
 	ntlmssp_weaken_keys(ntlmssp_state);

	nt_status = ntlmssp_sign_init(ntlmssp_state);

	data_blob_free(&ntlmssp_state->encrypted_session_key);
	
	/* allow arbitarily many authentications, but watch that this will cause a 
	   memory leak, until the ntlmssp_state is shutdown 
	*/

	if (ntlmssp_state->server_multiple_authentications) {
		ntlmssp_state->expected_state = NTLMSSP_AUTH;
	} else {
		ntlmssp_state->expected_state = NTLMSSP_DONE;
	}

	return nt_status;
}


/**
 * Next state function for the Authenticate packet
 * 
 * @param ntlmssp_state NTLMSSP State
 * @param in The packet in from the NTLMSSP partner, as a DATA_BLOB
 * @param out The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors, NT_STATUS_MORE_PROCESSING_REQUIRED or NT_STATUS_OK. 
 */

NTSTATUS ntlmssp_server_auth(struct gensec_security *gensec_security, 
			     TALLOC_CTX *out_mem_ctx, 
			     const DATA_BLOB in, DATA_BLOB *out) 
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = gensec_security->private_data;
	struct ntlmssp_state *ntlmssp_state = gensec_ntlmssp_state->ntlmssp_state;
	DATA_BLOB user_session_key = data_blob(NULL, 0);
	DATA_BLOB lm_session_key = data_blob(NULL, 0);
	NTSTATUS nt_status;

	/* zero the outbound NTLMSSP packet */
	*out = data_blob_talloc(out_mem_ctx, NULL, 0);

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_server_preauth(ntlmssp_state, in))) {
		return nt_status;
	}

	/*
	 * Note we don't check here for NTLMv2 auth settings. If NTLMv2 auth
	 * is required (by "ntlm auth = no" and "lm auth = no" being set in the
	 * smb.conf file) and no NTLMv2 response was sent then the password check
	 * will fail here. JRA.
	 */

	/* Finally, actually ask if the password is OK */

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_state->check_password(ntlmssp_state, 
								       &user_session_key, &lm_session_key))) {
		return nt_status;
	}
	
	if (ntlmssp_state->server_use_session_keys) {
		return ntlmssp_server_postauth(ntlmssp_state, &user_session_key, &lm_session_key);
	} else {
		ntlmssp_state->session_key = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}
}

/**
 * Create an NTLMSSP state machine
 * 
 * @param ntlmssp_state NTLMSSP State, allocated by this function
 */

static NTSTATUS ntlmssp_server_start(TALLOC_CTX *mem_ctx, struct ntlmssp_state **ntlmssp_state)
{
	*ntlmssp_state = talloc(mem_ctx, struct ntlmssp_state);
	if (!*ntlmssp_state) {
		DEBUG(0,("ntlmssp_server_start: talloc failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	ZERO_STRUCTP(*ntlmssp_state);

	(*ntlmssp_state)->role = NTLMSSP_SERVER;

	(*ntlmssp_state)->get_challenge = get_challenge;
	(*ntlmssp_state)->set_challenge = set_challenge;
	(*ntlmssp_state)->may_set_challenge = may_set_challenge;

	(*ntlmssp_state)->workstation = NULL;
	(*ntlmssp_state)->server_name = lp_netbios_name();

	(*ntlmssp_state)->get_domain = lp_workgroup;
	(*ntlmssp_state)->server_role = ROLE_DOMAIN_MEMBER; /* a good default */

	(*ntlmssp_state)->expected_state = NTLMSSP_NEGOTIATE;

	(*ntlmssp_state)->allow_lm_key = (lp_lanman_auth() 
					  && lp_parm_bool(-1, "ntlmssp_server", "allow_lm_key", False));

	(*ntlmssp_state)->server_use_session_keys = True;
	(*ntlmssp_state)->server_multiple_authentications = False;
	
	(*ntlmssp_state)->ref_count = 1;

	(*ntlmssp_state)->neg_flags = 
		NTLMSSP_NEGOTIATE_NTLM;

	if (lp_parm_bool(-1, "ntlmssp_server", "128bit", True)) {
		(*ntlmssp_state)->neg_flags |= NTLMSSP_NEGOTIATE_128;		
	}

	if (lp_parm_bool(-1, "ntlmssp_server", "keyexchange", True)) {
		(*ntlmssp_state)->neg_flags |= NTLMSSP_NEGOTIATE_KEY_EXCH;		
	}

	if (lp_parm_bool(-1, "ntlmssp_server", "ntlm2", True)) {
		(*ntlmssp_state)->neg_flags |= NTLMSSP_NEGOTIATE_NTLM2;		
	}

	return NT_STATUS_OK;
}

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

NTSTATUS gensec_ntlmssp_server_start(struct gensec_security *gensec_security)
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

/** 
 * Return the credentials of a logged on user, including session keys
 * etc.
 *
 * Only valid after a successful authentication
 *
 * May only be called once per authentication.
 *
 */

NTSTATUS gensec_ntlmssp_session_info(struct gensec_security *gensec_security,
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
