/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett         2001-2002
   
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/** List of various built-in authentication modules */

static const struct auth_init_function_entry builtin_auth_init_functions[] = {
	{ "guest", auth_init_guest },
/*	{ "rhosts", auth_init_rhosts }, */
/*	{ "hostsequiv", auth_init_hostsequiv }, */
	{ "sam", auth_init_sam },	
	{ "samstrict", auth_init_samstrict },
	{ "samstrict_dc", auth_init_samstrict_dc },
	{ "unix", auth_init_unix },
/*	{ "smbserver", auth_init_smbserver }, */
/*	{ "ntdomain", auth_init_ntdomain }, */
/*	{ "trustdomain", auth_init_trustdomain }, */
/*	{ "winbind", auth_init_winbind }, */
#ifdef DEVELOPER
	{ "name_to_ntstatus", auth_init_name_to_ntstatus },
	{ "fixed_challenge", auth_init_fixed_challenge },
#endif
	{ "plugin", auth_init_plugin },
	{ NULL, NULL}
};

/****************************************************************************
 Try to get a challenge out of the various authentication modules.
 Returns a const char of length 8 bytes.
****************************************************************************/

static const uint8 *get_ntlm_challenge(struct auth_context *auth_context) 
{
	DATA_BLOB challenge = data_blob(NULL, 0);
	const char *challenge_set_by = NULL;
	auth_methods *auth_method;
	TALLOC_CTX *mem_ctx;

	if (auth_context->challenge.length) {
		DEBUG(5, ("get_ntlm_challenge (auth subsystem): returning previous challenge by module %s (normal)\n", 
			  auth_context->challenge_set_by));
		return auth_context->challenge.data;
	}

	for (auth_method = auth_context->auth_method_list; auth_method; auth_method = auth_method->next) {
		if (auth_method->get_chal == NULL) {
			DEBUG(5, ("auth_get_challenge: module %s did not want to specify a challenge\n", auth_method->name));
			continue;
		}

		DEBUG(5, ("auth_get_challenge: getting challenge from module %s\n", auth_method->name));
		if (challenge_set_by != NULL) {
			DEBUG(1, ("auth_get_challenge: CONFIGURATION ERROR: authentication method %s has already specified a challenge.  Challenge by %s ignored.\n", 
				  challenge_set_by, auth_method->name));
			continue;
		}

		mem_ctx = talloc_init("auth_get_challenge for module %s", auth_method->name);
		if (!mem_ctx) {
			smb_panic("talloc_init() failed!");
		}
		
		challenge = auth_method->get_chal(auth_context, &auth_method->private_data, mem_ctx);
		if (!challenge.length) {
			DEBUG(3, ("auth_get_challenge: getting challenge from authentication method %s FAILED.\n", 
				  auth_method->name));
		} else {
			DEBUG(5, ("auth_get_challenge: sucessfully got challenge from module %s\n", auth_method->name));
			auth_context->challenge = challenge;
			challenge_set_by = auth_method->name;
			auth_context->challenge_set_method = auth_method;
		}
		talloc_destroy(mem_ctx);
	}
	
	if (!challenge_set_by) {
		uchar chal[8];
		
		generate_random_buffer(chal, sizeof(chal), False);
		auth_context->challenge = data_blob_talloc(auth_context->mem_ctx, 
							   chal, sizeof(chal));
		
		challenge_set_by = "random";
	} 
	
	DEBUG(5, ("auth_context challenge created by %s\n", challenge_set_by));
	DEBUG(5, ("challenge is: \n"));
	dump_data(5, (const char*)auth_context->challenge.data, auth_context->challenge.length);
	
	SMB_ASSERT(auth_context->challenge.length == 8);

	auth_context->challenge_set_by=challenge_set_by;

	return auth_context->challenge.data;
}


/**
 * Check user is in correct domain (if required)
 *
 * @param user Only used to fill in the debug message
 * 
 * @param domain The domain to be verified
 *
 * @return True if the user can connect with that domain, 
 *         False otherwise.
**/

static BOOL check_domain_match(const char *user, const char *domain) 
{
	/*
	 * If we aren't serving to trusted domains, we must make sure that
	 * the validation request comes from an account in the same domain
	 * as the Samba server
	 */

	if (!lp_allow_trusted_domains() &&
	    !(strequal("", domain) || 
	      strequal(lp_workgroup(), domain) || 
	      is_myname(domain))) {
		DEBUG(1, ("check_domain_match: Attempt to connect as user %s from domain %s denied.\n", user, domain));
		return False;
	} else {
		return True;
	}
}

/**
 * Check a user's Plaintext, LM or NTLM password.
 *
 * Check a user's password, as given in the user_info struct and return various
 * interesting details in the server_info struct.
 *
 * This function does NOT need to be in a become_root()/unbecome_root() pair
 * as it makes the calls itself when needed.
 *
 * The return value takes precedence over the contents of the server_info 
 * struct.  When the return is other than NT_STATUS_OK the contents 
 * of that structure is undefined.
 *
 * @param user_info Contains the user supplied components, including the passwords.
 *                  Must be created with make_user_info() or one of its wrappers.
 *
 * @param auth_context Supplies the challenges and some other data. 
 *                  Must be created with make_auth_context(), and the challenges should be 
 *                  filled in, either at creation or by calling the challenge geneation 
 *                  function auth_get_challenge().  
 *
 * @param server_info If successful, contains information about the authentication, 
 *                    including a SAM_ACCOUNT struct describing the user.
 *
 * @return An NTSTATUS with NT_STATUS_OK or an appropriate error.
 *
 **/

static NTSTATUS check_ntlm_password(const struct auth_context *auth_context,
				    const struct auth_usersupplied_info *user_info, 
				    struct auth_serversupplied_info **server_info)
{
	
	NTSTATUS nt_status = NT_STATUS_LOGON_FAILURE;
	const char *pdb_username;
	auth_methods *auth_method;
	TALLOC_CTX *mem_ctx;

	if (!user_info || !auth_context || !server_info)
		return NT_STATUS_LOGON_FAILURE;

	DEBUG(3, ("check_ntlm_password:  Checking password for unmapped user [%s]\\[%s]@[%s] with the new password interface\n", 
		  user_info->client_domain.str, user_info->smb_name.str, user_info->wksta_name.str));

	DEBUG(3, ("check_ntlm_password:  mapped user is: [%s]\\[%s]@[%s]\n", 
		  user_info->domain.str, user_info->internal_username.str, user_info->wksta_name.str));

	if (auth_context->challenge.length != 8) {
		DEBUG(0, ("check_ntlm_password:  Invalid challenge stored for this auth context - cannot continue\n"));
		return NT_STATUS_LOGON_FAILURE;
	}

	if (auth_context->challenge_set_by)
		DEBUG(10, ("check_ntlm_password: auth_context challenge created by %s\n",
					auth_context->challenge_set_by));

	DEBUG(10, ("challenge is: \n"));
	dump_data(5, (const char*)auth_context->challenge.data, auth_context->challenge.length);

#ifdef DEBUG_PASSWORD
	DEBUG(100, ("user_info has passwords of length %d and %d\n", 
		    user_info->lm_resp.length, user_info->nt_resp.length));
	DEBUG(100, ("lm:\n"));
	dump_data(100, user_info->lm_resp.data, user_info->lm_resp.length);
	DEBUG(100, ("nt:\n"));
	dump_data(100, user_info->nt_resp.data, user_info->nt_resp.length);
#endif

	/* This needs to be sorted:  If it doesn't match, what should we do? */
  	if (!check_domain_match(user_info->smb_name.str, user_info->domain.str))
		return NT_STATUS_LOGON_FAILURE;

	for (auth_method = auth_context->auth_method_list;auth_method; auth_method = auth_method->next) {
		mem_ctx = talloc_init("%s authentication for user %s\\%s", auth_method->name, 
					    user_info->domain.str, user_info->smb_name.str);

		nt_status = auth_method->auth(auth_context, auth_method->private_data, mem_ctx, user_info, server_info);
		if (NT_STATUS_IS_OK(nt_status)) {
			DEBUG(3, ("check_ntlm_password: %s authentication for user [%s] suceeded\n", 
				  auth_method->name, user_info->smb_name.str));
		} else {
			DEBUG(5, ("check_ntlm_password: %s authentication for user [%s] FAILED with error %s\n", 
				  auth_method->name, user_info->smb_name.str, nt_errstr(nt_status)));
		}

		talloc_destroy(mem_ctx);

		if (NT_STATUS_IS_OK(nt_status))
			break;
	}

	/* This is one of the few places the *relies* (rather than just sets defaults
	   on the value of lp_security().  This needs to change.  A new paramater 
	   perhaps? */
	if (lp_security() >= SEC_SERVER)
		smb_user_control(user_info, *server_info, nt_status);

	if (NT_STATUS_IS_OK(nt_status)) {
		pdb_username = pdb_get_username((*server_info)->sam_account);
		if (!(*server_info)->guest) {
			/* We might not be root if we are an RPC call */
			become_root();
			nt_status = smb_pam_accountcheck(pdb_username);
			unbecome_root();
			
			if (NT_STATUS_IS_OK(nt_status)) {
				DEBUG(5, ("check_ntlm_password:  PAM Account for user [%s] suceeded\n", 
					  pdb_username));
			} else {
				DEBUG(3, ("check_ntlm_password:  PAM Account for user [%s] FAILED with error %s\n", 
					  pdb_username, nt_errstr(nt_status)));
			} 
		}
		
		if (NT_STATUS_IS_OK(nt_status)) {
			DEBUG((*server_info)->guest ? 5 : 2, 
			      ("check_ntlm_password:  %sauthentication for user [%s] -> [%s] -> [%s] suceeded\n", 
			       (*server_info)->guest ? "guest " : "", 
			       user_info->smb_name.str, 
			       user_info->internal_username.str, 
			       pdb_username));
		}
	}

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(2, ("check_ntlm_password:  Authentication for user [%s] -> [%s] FAILED with error %s\n", 
			  user_info->smb_name.str, user_info->internal_username.str, 
			  nt_errstr(nt_status)));
		ZERO_STRUCTP(server_info);
	}
	return nt_status;
}

/***************************************************************************
 Clear out a auth_context, and destroy the attached TALLOC_CTX
***************************************************************************/

static void free_auth_context(struct auth_context **auth_context)
{
	if (*auth_context != NULL)
		talloc_destroy((*auth_context)->mem_ctx);
	*auth_context = NULL;
}

/***************************************************************************
 Make a auth_info struct
***************************************************************************/

static NTSTATUS make_auth_context(struct auth_context **auth_context) 
{
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("authentication context");
	
	*auth_context = talloc(mem_ctx, sizeof(**auth_context));
	if (!*auth_context) {
		DEBUG(0,("make_auth_context: talloc failed!\n"));
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	ZERO_STRUCTP(*auth_context);

	(*auth_context)->mem_ctx = mem_ctx;
	(*auth_context)->check_ntlm_password = check_ntlm_password;
	(*auth_context)->get_ntlm_challenge = get_ntlm_challenge;
	(*auth_context)->free = free_auth_context;
	
	return NT_STATUS_OK;
}

/***************************************************************************
 Make a auth_info struct for the auth subsystem
***************************************************************************/

static NTSTATUS make_auth_context_text_list(struct auth_context **auth_context, char **text_list) 
{
	auth_methods *list = NULL;
	auth_methods *t = NULL;
	auth_methods *tmp;
	int i;
	NTSTATUS nt_status;

	if (!text_list) {
		DEBUG(2,("make_auth_context_text_list: No auth method list!?\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	
	if (!NT_STATUS_IS_OK(nt_status = make_auth_context(auth_context)))
		return nt_status;
	
	for (;*text_list; text_list++) { 
		DEBUG(5,("make_auth_context_text_list: Attempting to find an auth method to match %s\n",
					*text_list));
		for (i = 0; builtin_auth_init_functions[i].name; i++) {
			char *module_name = smb_xstrdup(*text_list);
			char *module_params = NULL;
			char *p;

			p = strchr(module_name, ':');
			if (p) {
				*p = 0;
				module_params = p+1;
				trim_string(module_params, " ", " ");
			}

			trim_string(module_name, " ", " ");

			if (strequal(builtin_auth_init_functions[i].name, module_name)) {
				DEBUG(5,("make_auth_context_text_list: Found auth method %s (at pos %d)\n", *text_list, i));
				if (NT_STATUS_IS_OK(builtin_auth_init_functions[i].init(*auth_context, module_params, &t))) {
					DEBUG(5,("make_auth_context_text_list: auth method %s has a valid init\n",
								*text_list));
					DLIST_ADD_END(list, t, tmp);
				} else {
					DEBUG(0,("make_auth_context_text_list: auth method %s did not correctly init\n",
								*text_list));
				}
				break;
			}
			SAFE_FREE(module_name);
		}
	}
	
	(*auth_context)->auth_method_list = list;
	
	return nt_status;
}

/***************************************************************************
 Make a auth_context struct for the auth subsystem
***************************************************************************/

NTSTATUS make_auth_context_subsystem(struct auth_context **auth_context) 
{
	char **auth_method_list = NULL; 
	NTSTATUS nt_status;

	if (lp_auth_methods() && !str_list_copy(&auth_method_list, lp_auth_methods())) {
		return NT_STATUS_NO_MEMORY;
	}

	if (auth_method_list == NULL) {
		switch (lp_security()) 
		{
		case SEC_DOMAIN:
			DEBUG(5,("Making default auth method list for security=domain\n"));
			auth_method_list = str_list_make("guest sam winbind ntdomain", NULL);
			break;
		case SEC_SERVER:
			DEBUG(5,("Making default auth method list for security=server\n"));
			auth_method_list = str_list_make("guest sam smbserver", NULL);
			break;
		case SEC_USER:
			if (lp_encrypted_passwords()) {	
				DEBUG(5,("Making default auth method list for security=user, encrypt passwords = yes\n"));
				auth_method_list = str_list_make("guest sam", NULL);
			} else {
				DEBUG(5,("Making default auth method list for security=user, encrypt passwords = no\n"));
				auth_method_list = str_list_make("guest unix", NULL);
			}
			break;
		case SEC_SHARE:
			if (lp_encrypted_passwords()) {
				DEBUG(5,("Making default auth method list for security=share, encrypt passwords = yes\n"));
				auth_method_list = str_list_make("guest sam", NULL);
			} else {
				DEBUG(5,("Making default auth method list for security=share, encrypt passwords = no\n"));
				auth_method_list = str_list_make("guest unix", NULL);
			}
			break;
		case SEC_ADS:
			DEBUG(5,("Making default auth method list for security=ADS\n"));
			auth_method_list = str_list_make("guest sam ads winbind ntdomain", NULL);
			break;
		default:
			DEBUG(5,("Unknown auth method!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}
	} else {
		DEBUG(5,("Using specified auth order\n"));
	}
	
	if (!NT_STATUS_IS_OK(nt_status = make_auth_context_text_list(auth_context, auth_method_list))) {
		str_list_free(&auth_method_list);
		return nt_status;
	}
	
	str_list_free(&auth_method_list);
	return nt_status;
}

/***************************************************************************
 Make a auth_info struct with a fixed challenge
***************************************************************************/

NTSTATUS make_auth_context_fixed(struct auth_context **auth_context, uchar chal[8]) 
{
	NTSTATUS nt_status;
	if (!NT_STATUS_IS_OK(nt_status = make_auth_context_subsystem(auth_context))) {
		return nt_status;
	}
	
	(*auth_context)->challenge = data_blob(chal, 8);
	(*auth_context)->challenge_set_by = "fixed";
	return nt_status;
}


