/* 
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Andrew Bartlett 2001
   Copyright (C) Jeremy Allison 2000-2001
   Copyright (C) Rafal Szczesniak 2002

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
#include "librpc/gen_ndr/ndr_samr.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "auth/auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/****************************************************************************
 Create an auth_usersupplied_data structure
****************************************************************************/
static NTSTATUS make_user_info(TALLOC_CTX *mem_ctx,
			       struct auth_usersupplied_info **user_info, 
                               const char *smb_name, 
                               const char *internal_username,
                               const char *client_domain, 
                               const char *domain,
                               const char *wksta_name, 
                               DATA_BLOB *lm_password, DATA_BLOB *nt_password,
                               DATA_BLOB *lm_interactive_password, DATA_BLOB *nt_interactive_password,
                               DATA_BLOB *plaintext, 
                               BOOL encrypted)
{

	DEBUG(5,("attempting to make a user_info for %s (%s)\n", internal_username, smb_name));

	*user_info = talloc_p(mem_ctx, struct auth_usersupplied_info);
	if (!user_info) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCTP(*user_info);

	DEBUG(5,("making strings for %s's user_info struct\n", internal_username));

	(*user_info)->smb_name.str = talloc_strdup(*user_info, smb_name);
	if ((*user_info)->smb_name.str) { 
		(*user_info)->smb_name.len = strlen(smb_name);
	} else {
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}
	
	(*user_info)->internal_username.str = talloc_strdup(*user_info, internal_username);
	if ((*user_info)->internal_username.str) { 
		(*user_info)->internal_username.len = strlen(internal_username);
	} else {
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	(*user_info)->domain.str = talloc_strdup(*user_info, domain);
	if ((*user_info)->domain.str) { 
		(*user_info)->domain.len = strlen(domain);
	} else {
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	(*user_info)->client_domain.str = talloc_strdup(*user_info, client_domain);
	if ((*user_info)->client_domain.str) { 
		(*user_info)->client_domain.len = strlen(client_domain);
	} else {
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	(*user_info)->wksta_name.str = talloc_strdup(*user_info, wksta_name);
	if ((*user_info)->wksta_name.str) { 
		(*user_info)->wksta_name.len = strlen(wksta_name);
	} else {
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5,("making blobs for %s's user_info struct\n", internal_username));

	if (lm_password)
		(*user_info)->lm_resp = data_blob_talloc(*user_info, 
							 lm_password->data, 
							 lm_password->length);
	if (nt_password)
		(*user_info)->nt_resp = data_blob_talloc(*user_info,
							 nt_password->data, 
							 nt_password->length);
	if (lm_interactive_password)
		(*user_info)->lm_interactive_password = 
			data_blob_talloc(*user_info,
					 lm_interactive_password->data, 
					 lm_interactive_password->length);
	if (nt_interactive_password)
		(*user_info)->nt_interactive_password = 
			data_blob_talloc(*user_info, 
					 nt_interactive_password->data, 
					 nt_interactive_password->length);

	if (plaintext)
		(*user_info)->plaintext_password = 
			data_blob_talloc(*user_info, 
					 plaintext->data, 
					 plaintext->length);

	(*user_info)->encrypted = encrypted;

	DEBUG(10,("made an %sencrypted user_info for %s (%s)\n", encrypted ? "":"un" , internal_username, smb_name));

	return NT_STATUS_OK;
}

/****************************************************************************
 Create an auth_usersupplied_data structure after appropriate mapping.
****************************************************************************/

NTSTATUS make_user_info_map(TALLOC_CTX *mem_ctx,
			    struct auth_usersupplied_info **user_info, 
			    const char *smb_name, 
			    const char *client_domain, 
			    const char *wksta_name, 
 			    DATA_BLOB *lm_password, DATA_BLOB *nt_password,
 			    DATA_BLOB *lm_interactive_password, DATA_BLOB *nt_interactive_password,
			    DATA_BLOB *plaintext, 
			    BOOL encrypted)
{
	const char *domain;
	
	DEBUG(5, ("make_user_info_map: Mapping user [%s]\\[%s] from workstation [%s]\n",
	      client_domain, smb_name, wksta_name));
	
	/* don't allow "" as a domain, fixes a Win9X bug 
	   where it doens't supply a domain for logon script
	   'net use' commands.                                 */

	if ( *client_domain )
		domain = client_domain;
	else
		domain = lp_workgroup();

	/* we know that it is a trusted domain (and we are allowing
	   them) or it is our domain */
	
	return make_user_info(mem_ctx, 
			      user_info, smb_name, smb_name, 
			      client_domain, domain, wksta_name, 
			      lm_password, nt_password,
			      lm_interactive_password, nt_interactive_password,
			      plaintext, encrypted);
}

/****************************************************************************
 Create an auth_usersupplied_data, making the DATA_BLOBs here. 
 Decrypt and encrypt the passwords.
****************************************************************************/

NTSTATUS make_user_info_netlogon_network(TALLOC_CTX *mem_ctx,
					 struct auth_usersupplied_info **user_info, 
					 const char *smb_name, 
					 const char *client_domain, 
					 const char *wksta_name, 
					 const uint8_t *lm_network_password, int lm_password_len,
					 const uint8_t *nt_network_password, int nt_password_len)
{
	NTSTATUS nt_status;
	DATA_BLOB lm_blob = data_blob(lm_network_password, lm_password_len);
	DATA_BLOB nt_blob = data_blob(nt_network_password, nt_password_len);

	nt_status = make_user_info_map(mem_ctx,
				       user_info,
				       smb_name, client_domain, 
				       wksta_name, 
				       lm_password_len ? &lm_blob : NULL, 
				       nt_password_len ? &nt_blob : NULL,
				       NULL, NULL, NULL,
				       True);
	
	data_blob_free(&lm_blob);
	data_blob_free(&nt_blob);
	return nt_status;
}

/****************************************************************************
 Create an auth_usersupplied_data, making the DATA_BLOBs here. 
 Decrypt and encrypt the passwords.
****************************************************************************/

NTSTATUS make_user_info_netlogon_interactive(TALLOC_CTX *mem_ctx,
					     struct auth_usersupplied_info **user_info, 
					     const char *smb_name, 
					     const char *client_domain, 
					     const char *wksta_name, 
					     const uint8_t chal[8], 
					     const struct samr_Password *lm_interactive_password, 
					     const struct samr_Password *nt_interactive_password)
{
	NTSTATUS nt_status;
	DATA_BLOB local_lm_blob;
	DATA_BLOB local_nt_blob;
	
	DATA_BLOB lm_interactive_blob;
	DATA_BLOB nt_interactive_blob;
	uint8_t local_lm_response[24];
	uint8_t local_nt_response[24];
	
	SMBOWFencrypt(lm_interactive_password->hash, chal, local_lm_response);
	SMBOWFencrypt(nt_interactive_password->hash, chal, local_nt_response);
	
	local_lm_blob = data_blob(local_lm_response, 
				  sizeof(local_lm_response));
	lm_interactive_blob = data_blob(lm_interactive_password->hash, 
					sizeof(lm_interactive_password->hash));
	
	local_nt_blob = data_blob(local_nt_response, 
				  sizeof(local_nt_response));
	nt_interactive_blob = data_blob(nt_interactive_password->hash, 
					sizeof(nt_interactive_password->hash));
	
	nt_status = make_user_info_map(mem_ctx,
				       user_info, 
				       smb_name, client_domain, 
				       wksta_name, 
				       &local_lm_blob,
				       &local_nt_blob,
				       &lm_interactive_blob,
				       &nt_interactive_blob,
				       NULL,
				       True);
	
	data_blob_free(&local_lm_blob);
	data_blob_free(&local_nt_blob);
	data_blob_free(&lm_interactive_blob);
	data_blob_free(&nt_interactive_blob);
	return nt_status;
}
/****************************************************************************
 Create an auth_usersupplied_data structure
****************************************************************************/

NTSTATUS make_user_info_for_reply_enc(TALLOC_CTX *mem_ctx,
				      struct auth_usersupplied_info **user_info, 
                                      const char *smb_name,
                                      const char *client_domain, 
				      const char *remote_machine,
                                      DATA_BLOB lm_resp, DATA_BLOB nt_resp)
{
	return make_user_info_map(mem_ctx,
				  user_info, smb_name, 
				  client_domain, 
				  remote_machine,
				  lm_resp.data ? &lm_resp : NULL, 
				  nt_resp.data ? &nt_resp : NULL, 
				  NULL, NULL, NULL,
				  True);
}

/****************************************************************************
 Create a guest user_info blob, for anonymous authenticaion.
****************************************************************************/

BOOL make_user_info_guest(TALLOC_CTX *mem_ctx,
			  struct auth_usersupplied_info **user_info) 
{
	NTSTATUS nt_status;

	nt_status = make_user_info(mem_ctx,
				   user_info, 
				   "","", 
				   "","", 
				   "", 
				   NULL, NULL, 
				   NULL, NULL, 
				   NULL,
				   True);
			      
	return NT_STATUS_IS_OK(nt_status) ? True : False;
}

/****************************************************************************
 prints a struct security_token to debug output.
****************************************************************************/
void debug_security_token(int dbg_class, int dbg_lev, const struct security_token *token)
{
	TALLOC_CTX *mem_ctx;

	size_t     i;
	
	if (!token) {
		DEBUGC(dbg_class, dbg_lev, ("Security token: (NULL)\n"));
		return;
	}
	
	mem_ctx = talloc_init("debug_security_token()");
	if (!mem_ctx) {
		return;
	}

	DEBUGC(dbg_class, dbg_lev, ("Security token of user %s\n",
				    dom_sid_string(mem_ctx, token->user_sid) ));
	DEBUGADDC(dbg_class, dbg_lev, ("contains %lu SIDs\n", 
				       (unsigned long)token->num_sids));
	for (i = 0; i < token->num_sids; i++) {
		DEBUGADDC(dbg_class, dbg_lev, 
			  ("SID[%3lu]: %s\n", (unsigned long)i, 
			   dom_sid_string(mem_ctx, token->sids[i])));
	}

	talloc_destroy(mem_ctx);
}

/****************************************************************************
 prints a struct auth_session_info security token to debug output.
****************************************************************************/
void debug_session_info(int dbg_class, int dbg_lev, 
			const struct auth_session_info *session_info)
{
	if (!session_info) {
		DEBUGC(dbg_class, dbg_lev, ("Session Info: (NULL)\n"));
		return;	
	}

	debug_security_token(dbg_class, dbg_lev, session_info->security_token);
}

/****************************************************************************
 Create the SID list for this user.
****************************************************************************/
NTSTATUS create_security_token(TALLOC_CTX *mem_ctx, 
			       struct dom_sid *user_sid, struct dom_sid *group_sid, 
			       int n_groupSIDs, struct dom_sid **groupSIDs, 
			       BOOL is_guest, struct security_token **token)
{
	struct security_token *ptoken;
	int i;
	NTSTATUS status;

	ptoken = security_token_initialise(mem_ctx);
	if (ptoken == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ptoken->sids = talloc_array_p(ptoken, struct dom_sid *, n_groupSIDs + 5);
	if (!ptoken->sids) {
		return NT_STATUS_NO_MEMORY;
	}

	ptoken->user_sid = user_sid;
	ptoken->group_sid = group_sid;
	ptoken->privilege_mask = 0;

	ptoken->sids[0] = user_sid;
	ptoken->sids[1] = group_sid;

	/*
	 * Finally add the "standard" SIDs.
	 * The only difference between guest and "anonymous" (which we
	 * don't really support) is the addition of Authenticated_Users.
	 */
	ptoken->sids[2] = dom_sid_parse_talloc(mem_ctx, SID_WORLD);
	ptoken->sids[3] = dom_sid_parse_talloc(mem_ctx, SID_NT_NETWORK);
	ptoken->sids[4] = dom_sid_parse_talloc(mem_ctx, 
					       is_guest?SID_BUILTIN_GUESTS:
					       SID_NT_AUTHENTICATED_USERS);
	ptoken->num_sids = 5;

	for (i = 0; i < n_groupSIDs; i++) {
		size_t check_sid_idx;
		for (check_sid_idx = 1; 
		     check_sid_idx < ptoken->num_sids; 
		     check_sid_idx++) {
			if (dom_sid_equal(ptoken->sids[check_sid_idx], groupSIDs[i])) {
				break;
			}
		}
		
		if (check_sid_idx == ptoken->num_sids) {
			ptoken->sids[ptoken->num_sids++] = groupSIDs[i];
		}
	}

	/* setup the privilege mask for this token */
	status = samdb_privilege_setup(ptoken);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(ptoken);
		return status;
	}
	
	debug_security_token(DBGC_AUTH, 10, ptoken);
	
	*token = ptoken;

	return NT_STATUS_OK;
}

/***************************************************************************
 Make a user_info struct
***************************************************************************/

NTSTATUS make_server_info(const TALLOC_CTX *mem_ctx,
			  struct auth_serversupplied_info **server_info, 
			  const char *username)
{
	*server_info = talloc_p(mem_ctx, struct auth_serversupplied_info);
	if (!*server_info) {
		return NT_STATUS_NO_MEMORY;
	}
	ZERO_STRUCTP(*server_info);
	
	return NT_STATUS_OK;
}

/***************************************************************************
 Make (and fill) a user_info struct for a guest login.
***************************************************************************/
NTSTATUS make_server_info_guest(TALLOC_CTX *mem_ctx, struct auth_serversupplied_info **server_info)
{
	NTSTATUS nt_status;

	nt_status = make_server_info(mem_ctx, server_info, "");

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	
	(*server_info)->guest = True;

	(*server_info)->user_sid = dom_sid_parse_talloc((*server_info), SID_NT_ANONYMOUS);
	(*server_info)->primary_group_sid = dom_sid_parse_talloc((*server_info), SID_BUILTIN_GUESTS);
	(*server_info)->n_domain_groups = 0;
	(*server_info)->domain_groups = NULL;
	
	/* annoying, but the Guest really does have a session key, 
	   and it is all zeros! */
	(*server_info)->user_session_key = data_blob_talloc(*server_info, NULL, 16);
	(*server_info)->lm_session_key = data_blob_talloc(*server_info, NULL, 16);

	data_blob_clear(&(*server_info)->user_session_key);
	data_blob_clear(&(*server_info)->lm_session_key);

	(*server_info)->account_name = "";
	(*server_info)->domain = "";
	(*server_info)->full_name = "Anonymous";
	(*server_info)->logon_script = "";
	(*server_info)->profile_path = "";
	(*server_info)->home_directory = "";
	(*server_info)->home_drive = "";

	(*server_info)->last_logon = 0;
	(*server_info)->last_logoff = 0;
	(*server_info)->acct_expiry = 0;
	(*server_info)->last_password_change = 0;
	(*server_info)->allow_password_change = 0;
	(*server_info)->force_password_change = 0;

	(*server_info)->logon_count = 0;
	(*server_info)->bad_password_count = 0;

	(*server_info)->acct_flags = ACB_NORMAL;

	return nt_status;
}

/***************************************************************************
 Make a server_info struct from the info3 returned by a domain logon 
***************************************************************************/

NTSTATUS make_server_info_netlogon_validation(TALLOC_CTX *mem_ctx, 
					      const char *internal_username,
					      struct auth_serversupplied_info **server_info, 
					      uint16 validation_level, 
					      union netr_Validation *validation) 
{
	NTSTATUS nt_status;
	struct netr_SamBaseInfo *base = NULL;
	switch (validation_level) {
	case 2:
		if (!validation || !validation->sam2) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		base = &validation->sam2->base;
		break;
	case 3:
		if (!validation || !validation->sam3) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		base = &validation->sam3->base;
		break;
	case 6:
		if (!validation || !validation->sam6) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		base = &validation->sam6->base;
		break;
	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	nt_status = make_server_info(mem_ctx, server_info, internal_username);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	
	(*server_info)->guest = False;

	/* 
	   Here is where we should check the list of
	   trusted domains, and verify that the SID 
	   matches.
	*/

	(*server_info)->user_sid = dom_sid_add_rid(*server_info, dom_sid_dup(*server_info, base->domain_sid), base->rid);
	(*server_info)->primary_group_sid = dom_sid_add_rid(*server_info, dom_sid_dup(*server_info, base->domain_sid), base->primary_gid);

	(*server_info)->domain_groups = talloc_array_p((*server_info), struct dom_sid*, base->group_count);
	if (!(*server_info)->domain_groups) {
		return NT_STATUS_NO_MEMORY;
	}
	
	for ((*server_info)->n_domain_groups = 0;
	     (*server_info)->n_domain_groups < base->group_count; 
	     (*server_info)->n_domain_groups++) {
		struct dom_sid *sid;
		sid = dom_sid_dup((*server_info)->domain_groups, base->domain_sid);
		if (!sid) {
			return NT_STATUS_NO_MEMORY;
		}
		(*server_info)->domain_groups[(*server_info)->n_domain_groups]
			= dom_sid_add_rid(*server_info, sid, 
					  base->groupids[(*server_info)->n_domain_groups].rid);
		if (!(*server_info)->domain_groups[(*server_info)->n_domain_groups]) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* Copy 'other' sids.  We need to do sid filtering here to
 	   prevent possible elevation of privileges.  See:

           http://www.microsoft.com/windows2000/techinfo/administration/security/sidfilter.asp
         */

	if (validation_level == 3) {
		int i;
		(*server_info)->domain_groups
			= talloc_realloc_p((*server_info), 
					   (*server_info)->domain_groups, 
					   struct dom_sid*, 
					   base->group_count + validation->sam3->sidcount);
		
		if (!(*server_info)->domain_groups) {
			return NT_STATUS_NO_MEMORY;
		}
	
		for (i = 0; i < validation->sam3->sidcount; i++) {
			(*server_info)->domain_groups[(*server_info)->n_domain_groups + i] = 
				dom_sid_dup((*server_info)->domain_groups, 
					    validation->sam3->sids[i].sid);
		}

		/* Where are the 'global' sids?... */
	}

	if (base->account_name.string) {
		(*server_info)->account_name = talloc_reference(*server_info, base->account_name.string);
	} else {
		(*server_info)->account_name = talloc_strdup(*server_info, internal_username);
	}
	
	(*server_info)->domain = talloc_reference(*server_info, base->domain.string);
	(*server_info)->full_name = talloc_reference(*server_info, base->full_name.string);
	(*server_info)->logon_script = talloc_reference(*server_info, base->logon_script.string);
	(*server_info)->profile_path = talloc_reference(*server_info, base->profile_path.string);
	(*server_info)->home_directory = talloc_reference(*server_info, base->home_directory.string);
	(*server_info)->home_drive = talloc_reference(*server_info, base->home_drive.string);
	(*server_info)->last_logon = base->last_logon;
	(*server_info)->last_logoff = base->last_logoff;
	(*server_info)->acct_expiry = base->acct_expiry;
	(*server_info)->last_password_change = base->last_password_change;
	(*server_info)->allow_password_change = base->allow_password_change;
	(*server_info)->force_password_change = base->force_password_change;

	(*server_info)->logon_count = base->logon_count;
	(*server_info)->bad_password_count = base->bad_password_count;

	(*server_info)->acct_flags = base->acct_flags;

	/* ensure we are never given NULL session keys */
	
	if (all_zero(base->key.key, sizeof(base->key.key))) {
		(*server_info)->user_session_key = data_blob(NULL, 0);
	} else {
		(*server_info)->user_session_key = data_blob_talloc((*server_info), base->key.key, sizeof(base->key.key));
	}

	if (all_zero(base->LMSessKey.key, sizeof(base->LMSessKey.key))) {
		(*server_info)->lm_session_key = data_blob(NULL, 0);
	} else {
		(*server_info)->lm_session_key = data_blob_talloc((*server_info), base->LMSessKey.key, sizeof(base->LMSessKey.key));
	}
	return NT_STATUS_OK;
}

/***************************************************************************
 Free a user_info struct
***************************************************************************/

void free_user_info(struct auth_usersupplied_info **user_info)
{
	DEBUG(5,("attempting to free (and zero) a user_info structure\n"));
	if (*user_info) {
		data_blob_clear(&(*user_info)->plaintext_password);
	}

	talloc_free(*user_info);
	*user_info = NULL;
}

/***************************************************************************
 Clear out a server_info struct that has been allocated
***************************************************************************/

void free_server_info(struct auth_serversupplied_info **server_info)
{
	DEBUG(5,("attempting to free a server_info structure\n"));
	talloc_free(*server_info);
	*server_info = NULL;
}

/***************************************************************************
 Make an auth_methods struct
***************************************************************************/

BOOL make_auth_methods(struct auth_context *auth_context, struct auth_methods **auth_method) 
{
	if (!auth_context) {
		smb_panic("no auth_context supplied to make_auth_methods()!\n");
	}

	if (!auth_method) {
		smb_panic("make_auth_methods: pointer to auth_method pointer is NULL!\n");
	}

	*auth_method = talloc_p(auth_context, struct auth_methods);
	if (!*auth_method) {
		return False;
	}
	ZERO_STRUCTP(*auth_method);
	
	return True;
}

NTSTATUS make_session_info(TALLOC_CTX *mem_ctx, 
			   struct auth_serversupplied_info *server_info, 
			   struct auth_session_info **session_info) 
{
	NTSTATUS nt_status;

	*session_info = talloc_p(mem_ctx, struct auth_session_info);
	if (!*session_info) {
		return NT_STATUS_NO_MEMORY;
	}
	
	(*session_info)->server_info = server_info;
	talloc_reference(*session_info, (*session_info)->server_info);

	/* unless set otherwise, the session key is the user session
	 * key from the auth subsystem */
 
	(*session_info)->session_key = server_info->user_session_key;

	/* we should search for local groups here */
	
	nt_status = create_security_token((*session_info), 
					  server_info->user_sid, 
					  server_info->primary_group_sid, 
					  server_info->n_domain_groups, 
					  server_info->domain_groups,
					  False, 
					  &(*session_info)->security_token);
	
	return nt_status;
}

/***************************************************************************
 Clear out a server_info struct that has been allocated
***************************************************************************/

void free_session_info(struct auth_session_info **session_info)
{
	DEBUG(5,("attempting to free a session_info structure\n"));
	talloc_free((*session_info));
	*session_info = NULL;
}

/**
 * Squash an NT_STATUS in line with security requirements.
 * In an attempt to avoid giving the whole game away when users
 * are authenticating, NT replaces both NT_STATUS_NO_SUCH_USER and 
 * NT_STATUS_WRONG_PASSWORD with NT_STATUS_LOGON_FAILURE in certain situations 
 * (session setups in particular).
 *
 * @param nt_status NTSTATUS input for squashing.
 * @return the 'squashed' nt_status
 **/

NTSTATUS nt_status_squash(NTSTATUS nt_status)
{
	if NT_STATUS_IS_OK(nt_status) {
		return nt_status;		
	} else if NT_STATUS_EQUAL(nt_status, NT_STATUS_NO_SUCH_USER) {
		/* Match WinXP and don't give the game away */
		return NT_STATUS_LOGON_FAILURE;
		
	} else if NT_STATUS_EQUAL(nt_status, NT_STATUS_WRONG_PASSWORD) {
		/* Match WinXP and don't give the game away */
		return NT_STATUS_LOGON_FAILURE;
	} else {
		return nt_status;
	}  
}



