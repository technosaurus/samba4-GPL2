/* 
   Unix SMB/CIFS implementation.

   test suite for netlogon rpc operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2004
   Copyright (C) Tim Potter      2003
   
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
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "auth/auth.h"


static const char *machine_password;

#define TEST_MACHINE_NAME "torturetest"

static BOOL test_LogonUasLogon(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_LogonUasLogon r;

	r.in.server_name = NULL;
	r.in.account_name = lp_parm_string(-1, "torture", "username");
	r.in.workstation = TEST_MACHINE_NAME;

	printf("Testing LogonUasLogon\n");

	status = dcerpc_netr_LogonUasLogon(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LogonUasLogon - %s\n", nt_errstr(status));
		return False;
	}

	return True;
	
}

static BOOL test_LogonUasLogoff(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_LogonUasLogoff r;

	r.in.server_name = NULL;
	r.in.account_name = lp_parm_string(-1, "torture", "username");
	r.in.workstation = TEST_MACHINE_NAME;

	printf("Testing LogonUasLogoff\n");

	status = dcerpc_netr_LogonUasLogoff(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LogonUasLogoff - %s\n", nt_errstr(status));
		return False;
	}

	return True;
	
}

static BOOL test_SetupCredentials(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				  struct creds_CredentialState *creds)
{
	NTSTATUS status;
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate a;
	struct netr_Credential credentials1, credentials2, credentials3;
	const char *plain_pass;
	struct samr_Password mach_password;

	printf("Testing ServerReqChallenge\n");

	r.in.server_name = NULL;
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credentials = &credentials1;
	r.out.credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	status = dcerpc_netr_ServerReqChallenge(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerReqChallenge - %s\n", nt_errstr(status));
		return False;
	}

	plain_pass = machine_password;
	if (!plain_pass) {
		printf("Unable to fetch machine password!\n");
		return False;
	}

	E_md4hash(plain_pass, mach_password.hash);

	a.in.server_name = NULL;
	a.in.account_name = talloc_asprintf(mem_ctx, "%s$", TEST_MACHINE_NAME);
	a.in.secure_channel_type = SEC_CHAN_BDC;
	a.in.computer_name = TEST_MACHINE_NAME;
	a.in.credentials = &credentials3;
	a.out.credentials = &credentials3;

	creds_client_init(creds, &credentials1, &credentials2, &mach_password, &credentials3, 
			  NETLOGON_NEG_AUTH2_FLAGS);

	printf("Testing ServerAuthenticate\n");

	status = dcerpc_netr_ServerAuthenticate(p, mem_ctx, &a);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerAuthenticate - %s\n", nt_errstr(status));
		return False;
	}

	if (!creds_client_check(creds, &credentials3)) {
		printf("Credential chaining failed\n");
		return False;
	}

	return True;
}

static BOOL test_SetupCredentials2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				   uint32_t negotiate_flags,
				   struct creds_CredentialState *creds)
{
	NTSTATUS status;
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate2 a;
	struct netr_Credential credentials1, credentials2, credentials3;
	const char *plain_pass;
	struct samr_Password mach_password;

	printf("Testing ServerReqChallenge\n");

	r.in.server_name = NULL;
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credentials = &credentials1;
	r.out.credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	status = dcerpc_netr_ServerReqChallenge(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerReqChallenge - %s\n", nt_errstr(status));
		return False;
	}

	plain_pass = machine_password;
	if (!plain_pass) {
		printf("Unable to fetch machine password!\n");
		return False;
	}

	E_md4hash(plain_pass, mach_password.hash);

	a.in.server_name = NULL;
	a.in.account_name = talloc_asprintf(mem_ctx, "%s$", TEST_MACHINE_NAME);
	a.in.secure_channel_type = SEC_CHAN_BDC;
	a.in.computer_name = TEST_MACHINE_NAME;
	a.in.negotiate_flags = &negotiate_flags;
	a.out.negotiate_flags = &negotiate_flags;
	a.in.credentials = &credentials3;
	a.out.credentials = &credentials3;

	creds_client_init(creds, &credentials1, &credentials2, &mach_password, &credentials3, 
			  negotiate_flags);

	printf("Testing ServerAuthenticate2\n");

	status = dcerpc_netr_ServerAuthenticate2(p, mem_ctx, &a);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerAuthenticate2 - %s\n", nt_errstr(status));
		return False;
	}

	if (!creds_client_check(creds, &credentials3)) {
		printf("Credential chaining failed\n");
		return False;
	}

	printf("negotiate_flags=0x%08x\n", negotiate_flags);

	return True;
}


static BOOL test_SetupCredentials3(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				   uint32_t negotiate_flags,
				   struct creds_CredentialState *creds)
{
	NTSTATUS status;
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate3 a;
	struct netr_Credential credentials1, credentials2, credentials3;
	const char *plain_pass;
	struct samr_Password mach_password;
	uint32 rid;

	printf("Testing ServerReqChallenge\n");

	r.in.server_name = NULL;
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.credentials = &credentials1;
	r.out.credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data));

	status = dcerpc_netr_ServerReqChallenge(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerReqChallenge - %s\n", nt_errstr(status));
		return False;
	}

	plain_pass = machine_password;
	if (!plain_pass) {
		printf("Unable to fetch machine password!\n");
		return False;
	}

	E_md4hash(plain_pass, mach_password.hash);

	a.in.server_name = NULL;
	a.in.account_name = talloc_asprintf(mem_ctx, "%s$", TEST_MACHINE_NAME);
	a.in.secure_channel_type = SEC_CHAN_BDC;
	a.in.computer_name = TEST_MACHINE_NAME;
	a.in.negotiate_flags = &negotiate_flags;
	a.in.credentials = &credentials3;
	a.out.credentials = &credentials3;
	a.out.negotiate_flags = &negotiate_flags;
	a.out.rid = &rid;

	creds_client_init(creds, &credentials1, &credentials2, &mach_password, &credentials3,
			  negotiate_flags);

	printf("Testing ServerAuthenticate3\n");

	status = dcerpc_netr_ServerAuthenticate3(p, mem_ctx, &a);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerAuthenticate3 - %s\n", nt_errstr(status));
		return False;
	}

	if (!creds_client_check(creds, &credentials3)) {
		printf("Credential chaining failed\n");
		return False;
	}

	printf("negotiate_flags=0x%08x\n", negotiate_flags);

	return True;
}

enum ntlm_break {
	BREAK_NONE,
	BREAK_LM,
	BREAK_NT,
	NO_LM,
	NO_NT
};

struct samlogon_state {
	TALLOC_CTX *mem_ctx;
	const char *account_name;
	const char *account_domain;
	const char *password;
	struct dcerpc_pipe *p;
	struct netr_LogonSamLogon r;
	struct netr_Authenticator auth, auth2;
	struct creds_CredentialState creds;

	DATA_BLOB chall;
};

/* 
   Authenticate a user with a challenge/response, checking session key
   and valid authentication types
*/
static NTSTATUS check_samlogon(struct samlogon_state *samlogon_state, 
			       enum ntlm_break break_which,
			       DATA_BLOB *chall, 
			       DATA_BLOB *lm_response, 
			       DATA_BLOB *nt_response, 
			       uint8_t lm_key[8], 
			       uint8_t user_session_key[16], 
			       char **error_string)
{
	NTSTATUS status;
	struct netr_LogonSamLogon *r = &samlogon_state->r;
	struct netr_NetworkInfo ninfo;

	struct netr_SamBaseInfo *base;
	
	printf("testing netr_LogonSamLogon\n");
	
	samlogon_state->r.in.logon.network = &ninfo;
	
	ninfo.identity_info.domain_name.string = samlogon_state->account_domain;
	ninfo.identity_info.parameter_control = 0;
	ninfo.identity_info.logon_id_low = 0;
	ninfo.identity_info.logon_id_high = 0;
	ninfo.identity_info.account_name.string = samlogon_state->account_name;
	ninfo.identity_info.workstation.string = TEST_MACHINE_NAME;
		
	memcpy(ninfo.challenge, chall->data, 8);
		
	switch (break_which) {
	case BREAK_NONE:
		break;
	case BREAK_LM:
		if (lm_response && lm_response->data) {
			lm_response->data[0]++;
		}
		break;
	case BREAK_NT:
		if (nt_response && nt_response->data) {
			nt_response->data[0]++;
		}
		break;
	case NO_LM:
		data_blob_free(lm_response);
		break;
	case NO_NT:
		data_blob_free(nt_response);
		break;
	}
		
	if (nt_response) {
		ninfo.nt.data = nt_response->data;
		ninfo.nt.length = nt_response->length;
	} else {
		ninfo.nt.data = NULL;
		ninfo.nt.length = 0;
	}
		
	if (lm_response) {
		ninfo.lm.data = lm_response->data;
		ninfo.lm.length = lm_response->length;
	} else {
		ninfo.lm.data = NULL;
		ninfo.lm.length = 0;
	}
		
	ZERO_STRUCT(samlogon_state->auth2);
	creds_client_authenticator(&samlogon_state->creds, &samlogon_state->auth);
		
	r->out.return_authenticator = NULL;
	status = dcerpc_netr_LogonSamLogon(samlogon_state->p, samlogon_state->mem_ctx, r);
	if (!NT_STATUS_IS_OK(status)) {
		if (error_string) {
			*error_string = strdup(nt_errstr(status));
		}
	}
		
	if (!r->out.return_authenticator || 
	    !creds_client_check(&samlogon_state->creds, &r->out.return_authenticator->cred)) {
		printf("Credential chaining failed\n");
	}

	if (!NT_STATUS_IS_OK(status)) {
		/* we cannot check the session key, if the logon failed... */
		return status;
	}
		
	/* find and decyrpt the session keys, return in parameters above */
	switch (r->in.validation_level) {
		case 2:
			base = &r->out.validation.sam2->base;
		break;
		case 3:
			base = &r->out.validation.sam3->base;
		break;
		case 6:
			base = &r->out.validation.sam6->base;
		break;
	}

	if (r->in.validation_level != 6) {
		static const char zeros[16];
			
		if (memcmp(base->key.key, zeros,  
			   sizeof(base->key.key)) != 0) {
			creds_arcfour_crypt(&samlogon_state->creds, 
					    base->key.key, 
					    sizeof(base->key.key));
		}
			
		if (user_session_key) {
			memcpy(user_session_key, base->key.key, 16);
		}
			
		if (memcmp(base->LMSessKey.key, zeros,  
			   sizeof(base->LMSessKey.key)) != 0) {
			creds_arcfour_crypt(&samlogon_state->creds, 
					    base->LMSessKey.key, 
					    sizeof(base->LMSessKey.key));
		}
			
		if (lm_key) {
			memcpy(lm_key, base->LMSessKey.key, 8);
		}
	} else {
		/* they aren't encrypted! */
		if (user_session_key) {
			memcpy(user_session_key, base->key.key, 16);
		}
		if (lm_key) {
			memcpy(lm_key, base->LMSessKey.key, 8);
		}
	}
	
	return status;
} 


/* 
 * Test the normal 'LM and NTLM' combination
 */

static BOOL test_lm_ntlm_broken(struct samlogon_state *samlogon_state, enum ntlm_break break_which, char **error_string) 
{
	BOOL pass = True;
	BOOL lm_good;
	NTSTATUS nt_status;
	DATA_BLOB lm_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB nt_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB session_key = data_blob_talloc(samlogon_state->mem_ctx, NULL, 16);

	uint8_t lm_key[8];
	uint8_t user_session_key[16];
	uint8_t lm_hash[16];
	uint8_t nt_hash[16];
	
	ZERO_STRUCT(lm_key);
	ZERO_STRUCT(user_session_key);

	lm_good = SMBencrypt(samlogon_state->password, samlogon_state->chall.data, lm_response.data);
	if (!lm_good) {
		ZERO_STRUCT(lm_hash);
	} else {
		E_deshash(samlogon_state->password, lm_hash); 
	}
		
	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data, nt_response.data);

	E_md4hash(samlogon_state->password, nt_hash);
	SMBsesskeygen_ntv1(nt_hash, session_key.data);

	nt_status = check_samlogon(samlogon_state,
				   break_which,
				   &samlogon_state->chall,
				   &lm_response,
				   &nt_response,
				   lm_key, 
				   user_session_key,
				   error_string);
	
	data_blob_free(&lm_response);

	if (NT_STATUS_EQUAL(NT_STATUS_WRONG_PASSWORD, nt_status)) {
		/* for 'long' passwords, the LM password is invalid */
		if (break_which == NO_NT && !lm_good) {
			return True;
		}
		return break_which == BREAK_NT;
	}

	if (!NT_STATUS_IS_OK(nt_status)) {
		return False;
	}

	if (break_which == NO_NT && !lm_good) {
		printf("LM password is 'long' (> 14 chars and therefore invalid) but login did not fail!");
		return False;
	}

	if (memcmp(lm_hash, lm_key, 
		   sizeof(lm_key)) != 0) {
		printf("LM Key does not match expectations!\n");
		printf("lm_key:\n");
		dump_data(1, (const char *)lm_key, 8);
		printf("expected:\n");
		dump_data(1, (const char *)lm_hash, 8);
		pass = False;
	}

	if (break_which == NO_NT) {
		char lm_key_expected[16];
		memcpy(lm_key_expected, lm_hash, 8);
		memset(lm_key_expected+8, '\0', 8);
		if (memcmp(lm_key_expected, user_session_key, 
			   16) != 0) {
			printf("NT Session Key does not match expectations (should be first-8 LM hash)!\n");
			printf("user_session_key:\n");
			dump_data(1, (const char *)user_session_key, sizeof(user_session_key));
			printf("expected:\n");
			dump_data(1, (const char *)lm_key_expected, sizeof(lm_key_expected));
			pass = False;
		}
	} else {		
		if (memcmp(session_key.data, user_session_key, 
			   sizeof(user_session_key)) != 0) {
			printf("NT Session Key does not match expectations!\n");
			printf("user_session_key:\n");
			dump_data(1, (const char *)user_session_key, 16);
			printf("expected:\n");
			dump_data(1, (const char *)session_key.data, session_key.length);
			pass = False;
		}
	}
        return pass;
}

/* 
 * Test LM authentication, no NT response supplied
 */

static BOOL test_lm(struct samlogon_state *samlogon_state, char **error_string) 
{

	return test_lm_ntlm_broken(samlogon_state, NO_NT, error_string);
}

/* 
 * Test the NTLM response only, no LM.
 */

static BOOL test_ntlm(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lm_ntlm_broken(samlogon_state, NO_LM, error_string);
}

/* 
 * Test the NTLM response only, but in the LM field.
 */

static BOOL test_ntlm_in_lm(struct samlogon_state *samlogon_state, char **error_string) 
{
	BOOL pass = True;
	NTSTATUS nt_status;
	DATA_BLOB nt_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);

	uint8_t lm_key[8];
	uint8_t lm_hash[16];
	uint8_t user_session_key[16];
	
	ZERO_STRUCT(user_session_key);

	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data, nt_response.data);

	E_deshash(samlogon_state->password, lm_hash); 

	nt_status = check_samlogon(samlogon_state,
				   BREAK_NONE,
				   &samlogon_state->chall,
				   &nt_response,
				   NULL,
				   lm_key, 
				   user_session_key,
				   error_string);
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		return False;
	}

	if (memcmp(lm_hash, lm_key, 
		   sizeof(lm_key)) != 0) {
		printf("LM Key does not match expectations!\n");
 		printf("lm_key:\n");
		dump_data(1, (const char *)lm_key, 8);
		printf("expected:\n");
		dump_data(1, (const char *)lm_hash, 8);
		pass = False;
	}
	if (memcmp(lm_hash, user_session_key, 8) != 0) {
		char lm_key_expected[16];
		memcpy(lm_key_expected, lm_hash, 8);
		memset(lm_key_expected+8, '\0', 8);
		if (memcmp(lm_key_expected, user_session_key, 
			   16) != 0) {
			printf("NT Session Key does not match expectations (should be first-8 LM hash)!\n");
			printf("user_session_key:\n");
			dump_data(1, (const char *)user_session_key, sizeof(user_session_key));
			printf("expected:\n");
			dump_data(1, (const char *)lm_key_expected, sizeof(lm_key_expected));
			pass = False;
		}
	}
        return pass;
}

/* 
 * Test the NTLM response only, but in the both the NT and LM fields.
 */

static BOOL test_ntlm_in_both(struct samlogon_state *samlogon_state, char **error_string) 
{
	BOOL pass = True;
	BOOL lm_good;
	NTSTATUS nt_status;
	DATA_BLOB nt_response = data_blob_talloc(samlogon_state->mem_ctx, NULL, 24);
	DATA_BLOB session_key = data_blob_talloc(samlogon_state->mem_ctx, NULL, 16);

	char lm_key[8];
	char lm_hash[16];
	char user_session_key[16];
	char nt_hash[16];
	
	ZERO_STRUCT(lm_key);
	ZERO_STRUCT(user_session_key);

	SMBNTencrypt(samlogon_state->password, samlogon_state->chall.data, 
		     nt_response.data);
	E_md4hash(samlogon_state->password, (uint8_t *)nt_hash);
	SMBsesskeygen_ntv1((const uint8_t *)nt_hash, 
			   session_key.data);

	lm_good = E_deshash(samlogon_state->password, (uint8_t *)lm_hash); 
	if (!lm_good) {
		ZERO_STRUCT(lm_hash);
	}

	nt_status = check_samlogon(samlogon_state,
				   BREAK_NONE,
				   &samlogon_state->chall,
				   NULL, 
				   &nt_response,
				   lm_key, 
				   user_session_key,
				   error_string);
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		return False;
	}

	if (memcmp(lm_hash, lm_key, 
		   sizeof(lm_key)) != 0) {
		printf("LM Key does not match expectations!\n");
 		printf("lm_key:\n");
		dump_data(1, lm_key, 8);
		printf("expected:\n");
		dump_data(1, lm_hash, 8);
		pass = False;
	}
	if (memcmp(session_key.data, user_session_key, 
		   sizeof(user_session_key)) != 0) {
		printf("NT Session Key does not match expectations!\n");
 		printf("user_session_key:\n");
		dump_data(1, user_session_key, 16);
 		printf("expected:\n");
		dump_data(1, (const char *)session_key.data, session_key.length);
		pass = False;
	}


        return pass;
}

/* 
 * Test the NTLMv2 and LMv2 responses
 */

static BOOL test_lmv2_ntlmv2_broken(struct samlogon_state *samlogon_state, enum ntlm_break break_which, char **error_string) 
{
	BOOL pass = True;
	NTSTATUS nt_status;
	DATA_BLOB ntlmv2_response = data_blob(NULL, 0);
	DATA_BLOB lmv2_response = data_blob(NULL, 0);
	DATA_BLOB ntlmv2_session_key = data_blob(NULL, 0);
	DATA_BLOB names_blob = NTLMv2_generate_names_blob(samlogon_state->mem_ctx, lp_netbios_name(), lp_workgroup());

	uint8_t user_session_key[16];

	ZERO_STRUCT(user_session_key);
	
	/* TODO - test with various domain cases, and without domain */
	if (!SMBNTLMv2encrypt(samlogon_state->account_name, samlogon_state->account_domain, 
			      samlogon_state->password, &samlogon_state->chall,
			      &names_blob,
			      &lmv2_response, &ntlmv2_response, 
			      &ntlmv2_session_key)) {
		data_blob_free(&names_blob);
		return False;
	}
	data_blob_free(&names_blob);

	nt_status = check_samlogon(samlogon_state,
				   break_which,
				   &samlogon_state->chall,
				   &lmv2_response,
				   &ntlmv2_response,
				   NULL, 
				   user_session_key,
				   error_string);
	
	data_blob_free(&lmv2_response);
	data_blob_free(&ntlmv2_response);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return break_which == BREAK_NT;
	}

	if (break_which != NO_NT && break_which != BREAK_NT && memcmp(ntlmv2_session_key.data, user_session_key, 
		   sizeof(user_session_key)) != 0) {
		printf("USER (NTLMv2) Session Key does not match expectations!\n");
 		printf("user_session_key:\n");
		dump_data(1, (const char *)user_session_key, 16);
 		printf("expected:\n");
		dump_data(1, (const char *)ntlmv2_session_key.data, ntlmv2_session_key.length);
		pass = False;
	}
        return pass;
}

/* 
 * Test the NTLMv2 and LMv2 responses
 */

static BOOL test_lmv2_ntlmv2(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_NONE, error_string);
}

/* 
 * Test the LMv2 response only
 */

static BOOL test_lmv2(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lmv2_ntlmv2_broken(samlogon_state, NO_NT, error_string);
}

/* 
 * Test the NTLMv2 response only
 */

static BOOL test_ntlmv2(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lmv2_ntlmv2_broken(samlogon_state, NO_LM, error_string);
}

static BOOL test_lm_ntlm(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lm_ntlm_broken(samlogon_state, BREAK_NONE, error_string);
}

static BOOL test_ntlm_lm_broken(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lm_ntlm_broken(samlogon_state, BREAK_LM, error_string);
}

static BOOL test_ntlm_ntlm_broken(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lm_ntlm_broken(samlogon_state, BREAK_NT, error_string);
}

static BOOL test_ntlmv2_lmv2_broken(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_LM, error_string);
}

static BOOL test_ntlmv2_ntlmv2_broken(struct samlogon_state *samlogon_state, char **error_string) 
{
	return test_lmv2_ntlmv2_broken(samlogon_state, BREAK_NT, error_string);
}

static BOOL test_plaintext(struct samlogon_state *samlogon_state, enum ntlm_break break_which, char **error_string)
{
	NTSTATUS nt_status;
	DATA_BLOB nt_response = data_blob(NULL, 0);
	DATA_BLOB lm_response = data_blob(NULL, 0);
	char *password;
	char *dospw;
	void *unicodepw;

	uint8_t user_session_key[16];
	uint8_t lm_key[16];
	static const uint8_t zeros[8];
	DATA_BLOB chall = data_blob_talloc(samlogon_state->mem_ctx, zeros, sizeof(zeros));

	ZERO_STRUCT(user_session_key);
	
	if ((push_ucs2_talloc(samlogon_state->mem_ctx, &unicodepw, 
			      samlogon_state->password)) == -1) {
		DEBUG(0, ("push_ucs2_allocate failed!\n"));
		exit(1);
	}

	nt_response = data_blob_talloc(samlogon_state->mem_ctx, unicodepw, utf16_len(unicodepw));

	password = strupper_talloc(samlogon_state->mem_ctx, samlogon_state->password);

	if ((convert_string_talloc(samlogon_state->mem_ctx, CH_UNIX, 
				   CH_DOS, password,
				   strlen(password)+1, 
				   (void**)&dospw)) == -1) {
		DEBUG(0, ("convert_string_talloc failed!\n"));
		exit(1);
	}

	lm_response = data_blob_talloc(samlogon_state->mem_ctx, dospw, strlen(dospw));

	nt_status = check_samlogon(samlogon_state,
				   break_which,
				   &chall,
				   &lm_response,
				   &nt_response,
				   lm_key, 
				   user_session_key,
				   error_string);
	
 	if (!NT_STATUS_IS_OK(nt_status)) {
		return break_which == BREAK_NT;
	}

	return True;
}

static BOOL test_plaintext_none_broken(struct samlogon_state *samlogon_state, 
				       char **error_string) {
	return test_plaintext(samlogon_state, BREAK_NONE, error_string);
}

static BOOL test_plaintext_lm_broken(struct samlogon_state *samlogon_state, 
				     char **error_string) {
	return test_plaintext(samlogon_state, BREAK_LM, error_string);
}

static BOOL test_plaintext_nt_broken(struct samlogon_state *samlogon_state, 
				     char **error_string) {
	return test_plaintext(samlogon_state, BREAK_NT, error_string);
}

static BOOL test_plaintext_nt_only(struct samlogon_state *samlogon_state, 
				   char **error_string) {
	return test_plaintext(samlogon_state, NO_LM, error_string);
}

static BOOL test_plaintext_lm_only(struct samlogon_state *samlogon_state, 
				   char **error_string) {
	return test_plaintext(samlogon_state, NO_NT, error_string);
}

/* 
   Tests:
   
   - LM only
   - NT and LM		   
   - NT
   - NT in LM field
   - NT in both fields
   - NTLMv2
   - NTLMv2 and LMv2
   - LMv2
   - plaintext tests (in challenge-response fields)
  
   check we get the correct session key in each case
   check what values we get for the LM session key
   
*/

static const struct ntlm_tests {
	BOOL (*fn)(struct samlogon_state *, char **);
	const char *name;
	BOOL expect_fail;
} test_table[] = {
	{test_lm, "LM", False},
	{test_lm_ntlm, "LM and NTLM", False},
	{test_ntlm, "NTLM", False},
	{test_ntlm_in_lm, "NTLM in LM", False},
	{test_ntlm_in_both, "NTLM in both", False},
	{test_ntlmv2, "NTLMv2", False},
	{test_lmv2_ntlmv2, "NTLMv2 and LMv2", False},
	{test_lmv2, "LMv2", False},
	{test_ntlmv2_lmv2_broken, "NTLMv2 and LMv2, LMv2 broken", False},
	{test_ntlmv2_ntlmv2_broken, "NTLMv2 and LMv2, NTLMv2 broken", False},
	{test_ntlm_lm_broken, "NTLM and LM, LM broken", False},
	{test_ntlm_ntlm_broken, "NTLM and LM, NTLM broken", False},
	{test_plaintext_none_broken, "Plaintext", True},
	{test_plaintext_lm_broken, "Plaintext LM broken", True},
	{test_plaintext_nt_broken, "Plaintext NT broken", True},
	{test_plaintext_nt_only, "Plaintext NT only", True},
	{test_plaintext_lm_only, "Plaintext LM only", True},
	{NULL, NULL}
};

/*
  try a netlogon SamLogon
*/
static BOOL test_SamLogon(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	int i, v, l;
	BOOL ret = True;
	int validation_levels[] = {2,3,6};
	int logon_levels[] = { 2, 6 };
	struct samlogon_state samlogon_state;
	
	samlogon_state.mem_ctx = mem_ctx;
	samlogon_state.account_name = lp_parm_string(-1, "torture", "username");
	samlogon_state.account_domain = lp_parm_string(-1, "torture", "userdomain");
	samlogon_state.password = lp_parm_string(-1, "torture", "password");
	samlogon_state.p = p;

	samlogon_state.chall = data_blob_talloc(mem_ctx, NULL, 8);

	generate_random_buffer(samlogon_state.chall.data, 8);

	if (!test_SetupCredentials2(p, mem_ctx, NETLOGON_NEG_AUTH2_FLAGS, &samlogon_state.creds)) {
		return False;
	}

	if (!test_SetupCredentials3(p, mem_ctx, NETLOGON_NEG_AUTH2_FLAGS, &samlogon_state.creds)) {
		return False;
	}

	if (!test_SetupCredentials3(p, mem_ctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, &samlogon_state.creds)) {
		return False;
	}

	samlogon_state.r.in.server_name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	samlogon_state.r.in.workstation = TEST_MACHINE_NAME;
	samlogon_state.r.in.credential = &samlogon_state.auth;
	samlogon_state.r.in.return_authenticator = &samlogon_state.auth2;

	for (i=0; test_table[i].fn; i++) {
		for (v=0;v<ARRAY_SIZE(validation_levels);v++) {
			for (l=0;l<ARRAY_SIZE(logon_levels);l++) {
				char *error_string = NULL;
				samlogon_state.r.in.validation_level = validation_levels[v];
				samlogon_state.r.in.logon_level = logon_levels[l];
				printf("Testing SamLogon with '%s' at validation level %d, logon level %d\n", 
				       test_table[i].name, validation_levels[v], logon_levels[l]);
	
				if (!test_table[i].fn(&samlogon_state, &error_string)) {
					if (test_table[i].expect_fail) {
						printf("Test %s failed (expected, test incomplete): %s\n", test_table[i].name, error_string);
					} else {
						printf("Test %s failed: %s\n", test_table[i].name, error_string);
						ret = False;
					}
					SAFE_FREE(error_string);
				}
			}
		}
	}

	return ret;
}


/*
  try a change password for our machine account
*/
static BOOL test_SetPassword(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_ServerPasswordSet r;
	const char *password;
	struct creds_CredentialState creds;

	if (!test_SetupCredentials(p, mem_ctx, &creds)) {
		return False;
	}

	r.in.server_name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.account_name = talloc_asprintf(mem_ctx, "%s$", TEST_MACHINE_NAME);
	r.in.secure_channel_type = SEC_CHAN_BDC;
	r.in.computer_name = TEST_MACHINE_NAME;

	password = generate_random_str(mem_ctx, 8);
	E_md4hash(password, r.in.new_password.hash);

	creds_des_encrypt(&creds, &r.in.new_password);

	printf("Testing ServerPasswordSet on machine account\n");
	printf("Changing machine account password to '%s'\n", password);

	creds_client_authenticator(&creds, &r.in.credential);

	status = dcerpc_netr_ServerPasswordSet(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerPasswordSet - %s\n", nt_errstr(status));
		return False;
	}

	if (!creds_client_check(&creds, &r.out.return_authenticator.cred)) {
		printf("Credential chaining failed\n");
	}

	/* by changing the machine password twice we test the
	   credentials chaining fully, and we verify that the server
	   allows the password to be set to the same value twice in a
	   row (match win2k3) */
	printf("Testing a second ServerPasswordSet on machine account\n");
	printf("Changing machine account password to '%s' (same as previous run)\n", password);

	creds_client_authenticator(&creds, &r.in.credential);

	status = dcerpc_netr_ServerPasswordSet(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ServerPasswordSet (2) - %s\n", nt_errstr(status));
		return False;
	}

	if (!creds_client_check(&creds, &r.out.return_authenticator.cred)) {
		printf("Credential chaining failed\n");
	}

	machine_password = password;

	if (!test_SetupCredentials(p, mem_ctx, &creds)) {
		printf("ServerPasswordSet failed to actually change the password\n");
		return False;
	}

	return True;
}


/* we remember the sequence numbers so we can easily do a DatabaseDelta */
static uint64_t sequence_nums[3];

/*
  try a netlogon DatabaseSync
*/
static BOOL test_DatabaseSync(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_DatabaseSync r;
	struct creds_CredentialState creds;
	const uint32_t database_ids[] = {0, 1, 2}; 
	int i;
	BOOL ret = True;

	if (!test_SetupCredentials(p, mem_ctx, &creds)) {
		return False;
	}

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	ZERO_STRUCT(r.in.return_authenticator);

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {
		r.in.sync_context = 0;
		r.in.database_id = database_ids[i];

		printf("Testing DatabaseSync of id %d\n", r.in.database_id);

		do {
			creds_client_authenticator(&creds, &r.in.credential);

			status = dcerpc_netr_DatabaseSync(p, mem_ctx, &r);
			if (!NT_STATUS_IS_OK(status) &&
			    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
				printf("DatabaseSync - %s\n", nt_errstr(status));
				ret = False;
				break;
			}

			if (!creds_client_check(&creds, &r.out.return_authenticator.cred)) {
				printf("Credential chaining failed\n");
			}

			r.in.sync_context = r.out.sync_context;

			if (r.out.delta_enum_array &&
			    r.out.delta_enum_array->num_deltas > 0 &&
			    r.out.delta_enum_array->delta_enum[0].delta_type == 1 &&
			    r.out.delta_enum_array->delta_enum[0].delta_union.domain) {
				sequence_nums[r.in.database_id] = 
					r.out.delta_enum_array->delta_enum[0].delta_union.domain->sequence_num;
				printf("\tsequence_nums[%d]=%llu\n",
				       r.in.database_id, 
				       sequence_nums[r.in.database_id]);
			}
		} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));
	}

	return ret;
}


/*
  try a netlogon DatabaseDeltas
*/
static BOOL test_DatabaseDeltas(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_DatabaseDeltas r;
	struct creds_CredentialState creds;
	const uint32_t database_ids[] = {0, 1, 2}; 
	int i;
	BOOL ret = True;

	if (!test_SetupCredentials(p, mem_ctx, &creds)) {
		return False;
	}

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	ZERO_STRUCT(r.in.return_authenticator);

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {
		r.in.database_id = database_ids[i];
		r.in.sequence_num = sequence_nums[r.in.database_id];

		if (r.in.sequence_num == 0) continue;

		r.in.sequence_num -= 1;


		printf("Testing DatabaseDeltas of id %d at %llu\n", 
		       r.in.database_id, r.in.sequence_num);

		do {
			creds_client_authenticator(&creds, &r.in.credential);

			status = dcerpc_netr_DatabaseDeltas(p, mem_ctx, &r);
			if (!NT_STATUS_IS_OK(status) &&
			    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
				printf("DatabaseDeltas - %s\n", nt_errstr(status));
				ret = False;
				break;
			}

			if (!creds_client_check(&creds, &r.out.return_authenticator.cred)) {
				printf("Credential chaining failed\n");
			}

			r.in.sequence_num++;
		} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));
	}

	return ret;
}


/*
  try a netlogon AccountDeltas
*/
static BOOL test_AccountDeltas(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_AccountDeltas r;
	struct creds_CredentialState creds;
	BOOL ret = True;

	if (!test_SetupCredentials(p, mem_ctx, &creds)) {
		return False;
	}

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	ZERO_STRUCT(r.in.return_authenticator);
	creds_client_authenticator(&creds, &r.in.credential);
	ZERO_STRUCT(r.in.uas);
	r.in.count=10;
	r.in.level=0;
	r.in.buffersize=100;

	printf("Testing AccountDeltas\n");

	/* w2k3 returns "NOT IMPLEMENTED" for this call */
	status = dcerpc_netr_AccountDeltas(p, mem_ctx, &r);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED)) {
		printf("AccountDeltas - %s\n", nt_errstr(status));
		ret = False;
	}

	return ret;
}

/*
  try a netlogon AccountSync
*/
static BOOL test_AccountSync(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_AccountSync r;
	struct creds_CredentialState creds;
	BOOL ret = True;

	if (!test_SetupCredentials(p, mem_ctx, &creds)) {
		return False;
	}

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	ZERO_STRUCT(r.in.return_authenticator);
	creds_client_authenticator(&creds, &r.in.credential);
	ZERO_STRUCT(r.in.recordid);
	r.in.reference=0;
	r.in.level=0;
	r.in.buffersize=100;

	printf("Testing AccountSync\n");

	/* w2k3 returns "NOT IMPLEMENTED" for this call */
	status = dcerpc_netr_AccountSync(p, mem_ctx, &r);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED)) {
		printf("AccountSync - %s\n", nt_errstr(status));
		ret = False;
	}

	return ret;
}

/*
  try a netlogon GetDcName
*/
static BOOL test_GetDcName(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_GetDcName r;

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.domainname = lp_workgroup();

	printf("Testing GetDcName\n");

	status = dcerpc_netr_GetDcName(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("GetDcName - %s\n", nt_errstr(status));
		return False;
	}

	printf("\tDC is at '%s'\n", r.out.dcname);

	return True;
}

/*
  try a netlogon LogonControl 
*/
static BOOL test_LogonControl(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_LogonControl r;
	BOOL ret = True;
	int i;

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.function_code = 1;

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl level %d\n", i);

		status = dcerpc_netr_LogonControl(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	return ret;
}


/*
  try a netlogon GetAnyDCName
*/
static BOOL test_GetAnyDCName(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_GetAnyDCName r;

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.domainname = lp_workgroup();

	printf("Testing GetAnyDCName\n");

	status = dcerpc_netr_GetAnyDCName(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("GetAnyDCName - %s\n", nt_errstr(status));
		return False;
	}

	if (r.out.dcname) {
		printf("\tDC is at '%s'\n", r.out.dcname);
	}

	return True;
}


/*
  try a netlogon LogonControl2
*/
static BOOL test_LogonControl2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_LogonControl2 r;
	BOOL ret = True;
	int i;

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));

	r.in.function_code = NETLOGON_CONTROL_REDISCOVER;
	r.in.data.domain = lp_workgroup();

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2 level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	r.in.function_code = NETLOGON_CONTROL_TC_QUERY;
	r.in.data.domain = lp_workgroup();

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2 level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	r.in.function_code = NETLOGON_CONTROL_TRANSPORT_NOTIFY;
	r.in.data.domain = lp_workgroup();

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2 level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	r.in.function_code = NETLOGON_CONTROL_SET_DBFLAG;
	r.in.data.debug_level = ~0;

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2 level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	return ret;
}

/*
  try a netlogon DatabaseSync2
*/
static BOOL test_DatabaseSync2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_DatabaseSync2 r;
	struct creds_CredentialState creds;
	const uint32_t database_ids[] = {0, 1, 2}; 
	int i;
	BOOL ret = True;

	if (!test_SetupCredentials2(p, mem_ctx, NETLOGON_NEG_AUTH2_FLAGS, &creds)) {
		return False;
	}

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computername = TEST_MACHINE_NAME;
	r.in.preferredmaximumlength = (uint32_t)-1;
	ZERO_STRUCT(r.in.return_authenticator);

	for (i=0;i<ARRAY_SIZE(database_ids);i++) {
		r.in.sync_context = 0;
		r.in.database_id = database_ids[i];
		r.in.restart_state = 0;

		printf("Testing DatabaseSync2 of id %d\n", r.in.database_id);

		do {
			creds_client_authenticator(&creds, &r.in.credential);

			status = dcerpc_netr_DatabaseSync2(p, mem_ctx, &r);
			if (!NT_STATUS_IS_OK(status) &&
			    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
				printf("DatabaseSync2 - %s\n", nt_errstr(status));
				ret = False;
				break;
			}

			if (!creds_client_check(&creds, &r.out.return_authenticator.cred)) {
				printf("Credential chaining failed\n");
			}

			r.in.sync_context = r.out.sync_context;
		} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));
	}

	return ret;
}


/*
  try a netlogon LogonControl2Ex
*/
static BOOL test_LogonControl2Ex(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_LogonControl2Ex r;
	BOOL ret = True;
	int i;

	r.in.logon_server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));

	r.in.function_code = NETLOGON_CONTROL_REDISCOVER;
	r.in.data.domain = lp_workgroup();

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	r.in.function_code = NETLOGON_CONTROL_TC_QUERY;
	r.in.data.domain = lp_workgroup();

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	r.in.function_code = NETLOGON_CONTROL_TRANSPORT_NOTIFY;
	r.in.data.domain = lp_workgroup();

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	r.in.function_code = NETLOGON_CONTROL_SET_DBFLAG;
	r.in.data.debug_level = ~0;

	for (i=1;i<4;i++) {
		r.in.level = i;

		printf("Testing LogonControl2Ex level %d function %d\n", 
		       i, r.in.function_code);

		status = dcerpc_netr_LogonControl2Ex(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LogonControl - %s\n", nt_errstr(status));
			ret = False;
		}
	}

	return ret;
}


/*
  try a netlogon netr_DsrEnumerateDomainTrusts
*/
static BOOL test_DsrEnumerateDomainTrusts(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_DsrEnumerateDomainTrusts r;

	r.in.server_name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.trust_flags = 0x3f;

	printf("Testing netr_DsrEnumerateDomainTrusts\n");

	status = dcerpc_netr_DsrEnumerateDomainTrusts(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		printf("netr_DsrEnumerateDomainTrusts - %s/%s\n", 
		       nt_errstr(status), win_errstr(r.out.result));
		return False;
	}

	return True;
}


/*
  test an ADS style interactive domain login
*/
static BOOL test_InteractiveLogin(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
				  struct creds_CredentialState *creds)
{
	NTSTATUS status;
	struct netr_LogonSamLogonWithFlags r;
	struct netr_Authenticator a, ra;
	struct netr_PasswordInfo pinfo;
	const char *plain_pass;

	ZERO_STRUCT(r);
	ZERO_STRUCT(ra);

	creds_client_authenticator(creds, &a);

	r.in.server_name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.workstation = TEST_MACHINE_NAME;
	r.in.credential = &a;
	r.in.return_authenticator = &ra;
	r.in.logon_level = 5;
	r.in.logon.password = &pinfo;
	r.in.validation_level = 6;
	r.in.flags = 0;

	pinfo.identity_info.domain_name.string = lp_parm_string(-1, "torture", "userdomain");
	pinfo.identity_info.parameter_control = 0;
	pinfo.identity_info.logon_id_low = 0;
	pinfo.identity_info.logon_id_high = 0;
	pinfo.identity_info.account_name.string = lp_parm_string(-1, "torture", "username");
	pinfo.identity_info.workstation.string = TEST_MACHINE_NAME;

	plain_pass = lp_parm_string(-1, "torture", "password");

	E_deshash(plain_pass, pinfo.lmpassword.hash);
	E_md4hash(plain_pass, pinfo.ntpassword.hash);

	creds_arcfour_crypt(creds, pinfo.lmpassword.hash, 16);
	creds_arcfour_crypt(creds, pinfo.ntpassword.hash, 16);

	printf("Testing netr_LogonSamLogonWithFlags\n");

	status = dcerpc_netr_LogonSamLogonWithFlags(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("netr_LogonSamLogonWithFlags - %s\n", nt_errstr(status));
		exit(1);
		return False;
	}

	if (!creds_client_check(creds, &r.out.return_authenticator->cred)) {
		printf("Credential chaining failed\n");
		return False;
	}

	return True;
}


static BOOL test_GetDomainInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_LogonGetDomainInfo r;
	struct netr_DomainQuery1 q1;
	struct netr_Authenticator a;
	struct creds_CredentialState creds;

	if (!test_SetupCredentials3(p, mem_ctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, &creds)) {
		return False;
	}

	ZERO_STRUCT(r);

	creds_client_authenticator(&creds, &a);

	r.in.server_name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.unknown1 = 512;
	r.in.level = 1;
	r.in.credential = &a;
	r.out.credential = &a;

	r.in.i1[0] = 0;
	r.in.i1[1] = 0;

	r.in.query.query1 = &q1;
	ZERO_STRUCT(q1);
	
	/* this should really be the fully qualified name */
	q1.workstation_domain = TEST_MACHINE_NAME;
	q1.workstation_site = "Default-First-Site-Name";
	q1.blob2.length = 0;
	q1.blob2.size = 0;
	q1.blob2.data = NULL;
	q1.product.string = "product string";

	printf("Testing netr_LogonGetDomainInfo\n");

	status = dcerpc_netr_LogonGetDomainInfo(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("netr_LogonGetDomainInfo - %s\n", nt_errstr(status));
		return False;
	}

	if (!creds_client_check(&creds, &a.cred)) {
		printf("Credential chaining failed\n");
		return False;
	}

	test_InteractiveLogin(p, mem_ctx, &creds);

	return True;
}


static void async_callback(struct rpc_request *req)
{
	int *counter = req->async.private;
	if (NT_STATUS_IS_OK(req->status)) {
		(*counter)++;
	}
}

static BOOL test_GetDomainInfo_async(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct netr_LogonGetDomainInfo r;
	struct netr_DomainQuery1 q1;
	struct netr_Authenticator a;
#define ASYNC_COUNT 100
	struct creds_CredentialState creds;
	struct creds_CredentialState creds_async[ASYNC_COUNT];
	struct rpc_request *req[ASYNC_COUNT];
	int i;
	int async_counter = 0;

	if (!test_SetupCredentials3(p, mem_ctx, NETLOGON_NEG_AUTH2_ADS_FLAGS, &creds)) {
		return False;
	}

	ZERO_STRUCT(r);
	r.in.server_name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = TEST_MACHINE_NAME;
	r.in.unknown1 = 512;
	r.in.level = 1;
	r.in.credential = &a;
	r.out.credential = &a;

	r.in.i1[0] = 0;
	r.in.i1[1] = 0;

	r.in.query.query1 = &q1;
	ZERO_STRUCT(q1);
	
	/* this should really be the fully qualified name */
	q1.workstation_domain = TEST_MACHINE_NAME;
	q1.workstation_site = "Default-First-Site-Name";
	q1.blob2.length = 0;
	q1.blob2.size = 0;
	q1.blob2.data = NULL;
	q1.product.string = "product string";

	printf("Testing netr_LogonGetDomainInfo - async count %d\n", ASYNC_COUNT);

	for (i=0;i<ASYNC_COUNT;i++) {
		creds_client_authenticator(&creds, &a);

		creds_async[i] = creds;
		req[i] = dcerpc_netr_LogonGetDomainInfo_send(p, mem_ctx, &r);

		req[i]->async.callback = async_callback;
		req[i]->async.private = &async_counter;

		/* even with this flush per request a w2k3 server seems to 
		   clag with multiple outstanding requests. bleergh. */
		if (event_loop_once(dcerpc_event_context(p)) != 0) {
			return False;
		}
	}

	for (i=0;i<ASYNC_COUNT;i++) {
		status = dcerpc_ndr_request_recv(req[i]);
		if (!NT_STATUS_IS_OK(status) || !NT_STATUS_IS_OK(r.out.result)) {
			printf("netr_LogonGetDomainInfo_async(%d) - %s/%s\n", 
			       i, nt_errstr(status), nt_errstr(r.out.result));
			break;
		}

		if (!creds_client_check(&creds_async[i], &a.cred)) {
			printf("Credential chaining failed at async %d\n", i);
			break;
		}
	}

	printf("Testing netr_LogonGetDomainInfo - async count %d OK\n", async_counter);

	return async_counter == ASYNC_COUNT;
}


BOOL torture_rpc_netlogon(void)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	BOOL ret = True;
	void *join_ctx;

	mem_ctx = talloc_init("torture_rpc_netlogon");

	join_ctx = torture_join_domain(TEST_MACHINE_NAME, lp_workgroup(), ACB_SVRTRUST, 
				       &machine_password);
	if (!join_ctx) {
		printf("Failed to join as BDC\n");
		return False;
	}

	status = torture_rpc_connection(&p, 
					DCERPC_NETLOGON_NAME,
					DCERPC_NETLOGON_UUID,
					DCERPC_NETLOGON_VERSION);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	if (!test_LogonUasLogon(p, mem_ctx)) {
		ret = False;
	}

	if (!test_LogonUasLogoff(p, mem_ctx)) {
		ret = False;
	}

	if (!test_SetPassword(p, mem_ctx)) {
		ret = False;
	}

	if (!test_SamLogon(p, mem_ctx)) {
		ret = False;
	}

	if (!test_GetDomainInfo(p, mem_ctx)) {
		ret = False;
	}

	if (!test_DatabaseSync(p, mem_ctx)) {
		ret = False;
	}

	if (!test_DatabaseDeltas(p, mem_ctx)) {
		ret = False;
	}

	if (!test_AccountDeltas(p, mem_ctx)) {
		ret = False;
	}

	if (!test_AccountSync(p, mem_ctx)) {
		ret = False;
	}

	if (!test_GetDcName(p, mem_ctx)) {
		ret = False;
	}

	if (!test_LogonControl(p, mem_ctx)) {
		ret = False;
	}

	if (!test_GetAnyDCName(p, mem_ctx)) {
		ret = False;
	}

	if (!test_LogonControl2(p, mem_ctx)) {
		ret = False;
	}

	if (!test_DatabaseSync2(p, mem_ctx)) {
		ret = False;
	}

	if (!test_LogonControl2Ex(p, mem_ctx)) {
		ret = False;
	}

	if (!test_DsrEnumerateDomainTrusts(p, mem_ctx)) {
		ret = False;
	}

	if (!test_GetDomainInfo_async(p, mem_ctx)) {
		ret = False;
	}

	talloc_destroy(mem_ctx);

	torture_rpc_close(p);

	torture_leave_domain(join_ctx);

	return ret;
}
