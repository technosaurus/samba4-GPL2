/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   
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

/*
 * do a password change using DCERPC/SAMR calls
 * 1. connect to the SAMR pipe of users domain PDC (maybe a standalone server or workstation)
 * 2. try samr_ChangePasswordUser3
 * 3. try samr_ChangePasswordUser2
 * 4. try samr_OemChangePasswordUser2
 * (not yet: 5. try samr_ChangePasswordUser)
 */
static NTSTATUS libnet_ChangePassword_samr(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_ChangePassword *r)
{
        NTSTATUS status;
	union libnet_rpc_connect c;
#if 0
	struct policy_handle user_handle;
	struct samr_Password hash1, hash2, hash3, hash4, hash5, hash6;
	struct samr_ChangePasswordUser pw;
#endif
	struct samr_OemChangePasswordUser2 oe2;
	struct samr_ChangePasswordUser2 pw2;
	struct samr_ChangePasswordUser3 pw3;
	struct samr_Name server, account;
	struct samr_AsciiName a_server, a_account;
	struct samr_CryptPassword nt_pass, lm_pass;
	struct samr_Password nt_verifier, lm_verifier;
	uint8_t old_nt_hash[16], new_nt_hash[16];
	uint8_t old_lm_hash[16], new_lm_hash[16];

	/* prepare connect to the SAMR pipe of the users domain PDC */
	c.pdc.level			= LIBNET_RPC_CONNECT_PDC;
	c.pdc.in.domain_name		= r->samr.in.domain_name;
	c.pdc.in.dcerpc_iface_name	= DCERPC_SAMR_NAME;
	c.pdc.in.dcerpc_iface_uuid	= DCERPC_SAMR_UUID;
	c.pdc.in.dcerpc_iface_version	= DCERPC_SAMR_VERSION;

	/* 1. connect to the SAMR pipe of users domain PDC (maybe a standalone server or workstation) */
	status = libnet_rpc_connect(ctx, mem_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"Connection to SAMR pipe of PDC of domain '%s' failed: %s\n",
						r->samr.in.domain_name, nt_errstr(status));
		return status;
	}

	/* prepare password change for account */
	server.name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(c.pdc.out.dcerpc_pipe));
	account.name = r->samr.in.account_name;

	E_md4hash(r->samr.in.oldpassword, old_nt_hash);
	E_md4hash(r->samr.in.newpassword, new_nt_hash);

	E_deshash(r->samr.in.oldpassword, old_lm_hash);
	E_deshash(r->samr.in.newpassword, new_lm_hash);

	/* prepare samr_ChangePasswordUser3 */
	encode_pw_buffer(lm_pass.data, r->samr.in.newpassword, STR_UNICODE);
	arcfour_crypt(lm_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_lm_hash, old_lm_hash, lm_verifier.hash);

	encode_pw_buffer(nt_pass.data,  r->samr.in.newpassword, STR_UNICODE);
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	pw3.in.server = &server;
	pw3.in.account = &account;
	pw3.in.nt_password = &nt_pass;
	pw3.in.nt_verifier = &nt_verifier;
	pw3.in.lm_change = 1;
	pw3.in.lm_password = &lm_pass;
	pw3.in.lm_verifier = &lm_verifier;
	pw3.in.password3 = NULL;

	/* 2. try samr_ChangePasswordUser3 */
	status = dcerpc_samr_ChangePasswordUser3(c.pdc.out.dcerpc_pipe, mem_ctx, &pw3);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_ChangePasswordUser3 failed: %s\n",
						nt_errstr(status));
		goto ChangePasswordUser2;
	}

	/* check result of samr_ChangePasswordUser3 */
	if (!NT_STATUS_IS_OK(pw3.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_ChangePasswordUser3 for '%s\\%s' failed: %s\n",
						r->samr.in.domain_name, r->samr.in.account_name, 
						nt_errstr(pw3.out.result));
						/* TODO: give the reason of the reject */
		if (NT_STATUS_EQUAL(pw3.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
			status = pw3.out.result;
			goto disconnect;
		}
		goto ChangePasswordUser2;
	}

	goto disconnect;

ChangePasswordUser2:
	/* prepare samr_ChangePasswordUser2 */
	encode_pw_buffer(lm_pass.data, r->samr.in.newpassword, STR_ASCII|STR_TERMINATE);
	arcfour_crypt(lm_pass.data, old_lm_hash, 516);
	E_old_pw_hash(new_lm_hash, old_lm_hash, lm_verifier.hash);

	encode_pw_buffer(nt_pass.data, r->samr.in.newpassword, STR_UNICODE);
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	pw2.in.server = &server;
	pw2.in.account = &account;
	pw2.in.nt_password = &nt_pass;
	pw2.in.nt_verifier = &nt_verifier;
	pw2.in.lm_change = 1;
	pw2.in.lm_password = &lm_pass;
	pw2.in.lm_verifier = &lm_verifier;

	/* 3. try samr_ChangePasswordUser2 */
	status = dcerpc_samr_ChangePasswordUser2(c.pdc.out.dcerpc_pipe, mem_ctx, &pw2);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_ChangePasswordUser2 failed: %s\n",
						nt_errstr(status));
		goto OemChangePasswordUser2;
	}

	/* check result of samr_ChangePasswordUser2 */
	if (!NT_STATUS_IS_OK(pw2.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_ChangePasswordUser2 for '%s\\%s' failed: %s\n",
						r->samr.in.domain_name, r->samr.in.account_name, 
						nt_errstr(pw2.out.result));
		if (NT_STATUS_EQUAL(pw2.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
			status = pw2.out.result;
			goto disconnect;
		}
		goto OemChangePasswordUser2;
	}

	goto disconnect;

OemChangePasswordUser2:
	/* prepare samr_OemChangePasswordUser2 */
	a_server.name = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(c.pdc.out.dcerpc_pipe));
	a_account.name = r->samr.in.account_name;

	encode_pw_buffer(lm_pass.data, r->samr.in.newpassword, STR_ASCII);
	arcfour_crypt(lm_pass.data, old_lm_hash, 516);
	E_old_pw_hash(new_lm_hash, old_lm_hash, lm_verifier.hash);

	oe2.in.server = &a_server;
	oe2.in.account = &a_account;
	oe2.in.password = &lm_pass;
	oe2.in.hash = &lm_verifier;

	/* 4. try samr_OemChangePasswordUser2 */
	status = dcerpc_samr_OemChangePasswordUser2(c.pdc.out.dcerpc_pipe, mem_ctx, &oe2);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_OemChangePasswordUser2 failed: %s\n",
						nt_errstr(status));
		goto ChangePasswordUser;
	}

	/* check result of samr_OemChangePasswordUser2 */
	if (!NT_STATUS_IS_OK(oe2.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_OemChangePasswordUser2 for '%s\\%s' failed: %s\n",
						r->samr.in.domain_name, r->samr.in.account_name, 
						nt_errstr(oe2.out.result));
		if (NT_STATUS_EQUAL(oe2.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
			status = oe2.out.result;
			goto disconnect;
		}
		goto ChangePasswordUser;
	}

	goto disconnect;

ChangePasswordUser:
#if 0
	/* prepare samr_ChangePasswordUser */
	E_old_pw_hash(new_lm_hash, old_lm_hash, hash1.hash);
	E_old_pw_hash(old_lm_hash, new_lm_hash, hash2.hash);
	E_old_pw_hash(new_nt_hash, old_nt_hash, hash3.hash);
	E_old_pw_hash(old_nt_hash, new_nt_hash, hash4.hash);
	E_old_pw_hash(old_lm_hash, new_nt_hash, hash5.hash);
	E_old_pw_hash(old_nt_hash, new_lm_hash, hash6.hash);

	/* TODO: ask for a user_handle */
	pw.in.handle = &user_handle;
	pw.in.lm_present = 1;
	pw.in.old_lm_crypted = &hash1;
	pw.in.new_lm_crypted = &hash2;
	pw.in.nt_present = 1;
	pw.in.old_nt_crypted = &hash3;
	pw.in.new_nt_crypted = &hash4;
	pw.in.cross1_present = 1;
	pw.in.nt_cross = &hash5;
	pw.in.cross2_present = 1;
	pw.in.lm_cross = &hash6;

	/* 5. try samr_ChangePasswordUser */
	status = dcerpc_samr_ChangePasswordUser(c.pdc.out.dcerpc_pipe, mem_ctx, &pw);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_ChangePasswordUser failed: %s\n",
						nt_errstr(status));
		goto disconnect;
	}

	/* check result of samr_ChangePasswordUser */
	if (!NT_STATUS_IS_OK(pw.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_ChangePasswordUser for '%s\\%s' failed: %s\n",
						r->samr.in.domain_name, r->samr.in.account_name, 
						nt_errstr(pw.out.result));
		if (NT_STATUS_EQUAL(pw.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
			status = pw.out.result;
			goto disconnect;
		}
		goto disconnect;
	}
#endif
disconnect:
	/* close connection */
	dcerpc_pipe_close(c.pdc.out.dcerpc_pipe);

	return status;
}

static NTSTATUS libnet_ChangePassword_generic(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_ChangePassword *r)
{
	NTSTATUS status;
	union libnet_ChangePassword r2;

	r2.samr.level		= LIBNET_CHANGE_PASSWORD_SAMR;
	r2.samr.in.account_name	= r->generic.in.account_name;
	r2.samr.in.domain_name	= r->generic.in.domain_name;
	r2.samr.in.oldpassword	= r->generic.in.oldpassword;
	r2.samr.in.newpassword	= r->generic.in.newpassword;

	status = libnet_ChangePassword(ctx, mem_ctx, &r2);

	r->generic.out.error_string = r2.samr.out.error_string;

	return status;
}

NTSTATUS libnet_ChangePassword(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_ChangePassword *r)
{
	switch (r->generic.level) {
		case LIBNET_CHANGE_PASSWORD_GENERIC:
			return libnet_ChangePassword_generic(ctx, mem_ctx, r);
		case LIBNET_CHANGE_PASSWORD_SAMR:
			return libnet_ChangePassword_samr(ctx, mem_ctx, r);
		case LIBNET_CHANGE_PASSWORD_KRB5:
			return NT_STATUS_NOT_IMPLEMENTED;
		case LIBNET_CHANGE_PASSWORD_LDAP:
			return NT_STATUS_NOT_IMPLEMENTED;
		case LIBNET_CHANGE_PASSWORD_RAP:
			return NT_STATUS_NOT_IMPLEMENTED;
	}

	return NT_STATUS_INVALID_LEVEL;
}

/*
 * set a password with DCERPC/SAMR calls
 * 1. connect to the SAMR pipe of users domain PDC (maybe a standalone server or workstation)
 *    is it correct to contact the the pdc of the domain of the user who's password should be set?
 * 2. do a samr_Connect to get a policy handle
 * 3. do a samr_LookupDomain to get the domain sid
 * 4. do a samr_OpenDomain to get a domain handle
 * 5. do a samr_LookupNames to get the users rid
 * 6. do a samr_OpenUser to get a user handle
 * 7. try samr_SetUserInfo level 26 to set the password
 * 8. try samr_SetUserInfo level 25 to set the password
 * 8. try samr_SetUserInfo level 24 to set the password
 *10. try samr_SetUserInfo level 23 to set the password
 */
static NTSTATUS libnet_SetPassword_samr(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_SetPassword *r)
{
	NTSTATUS status;
	union libnet_rpc_connect c;
	struct samr_Connect sc;
	struct policy_handle p_handle;
	struct samr_LookupDomain ld;
	struct samr_Name d_name;
	struct samr_OpenDomain od;
	struct policy_handle d_handle;
	struct samr_LookupNames ln;
	struct samr_OpenUser ou;
	struct policy_handle u_handle;
	struct samr_SetUserInfo sui;
	union samr_UserInfo u_info;
	DATA_BLOB session_key;
	DATA_BLOB confounded_session_key = data_blob_talloc(mem_ctx, NULL, 16);
	uint8_t confounder[16];	
	struct MD5Context md5;

	/* prepare connect to the SAMR pipe of users domain PDC */
	c.pdc.level			= LIBNET_RPC_CONNECT_PDC;
	c.pdc.in.domain_name		= r->samr.in.domain_name;
	c.pdc.in.dcerpc_iface_name	= DCERPC_SAMR_NAME;
	c.pdc.in.dcerpc_iface_uuid	= DCERPC_SAMR_UUID;
	c.pdc.in.dcerpc_iface_version	= DCERPC_SAMR_VERSION;

	/* 1. connect to the SAMR pipe of users domain PDC (maybe a standalone server or workstation) */
	status = libnet_rpc_connect(ctx, mem_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"Connection to SAMR pipe of PDC of domain '%s' failed: %s\n",
						r->samr.in.domain_name, nt_errstr(status));
		return status;
	}

	/* prepare samr_Connect */
	ZERO_STRUCT(p_handle);
	sc.in.system_name = NULL;
	sc.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;
	sc.out.connect_handle = &p_handle;

	/* 2. do a samr_Connect to get a policy handle */
	status = dcerpc_samr_Connect(c.pdc.out.dcerpc_pipe, mem_ctx, &sc);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_Connect failed: %s\n",
						nt_errstr(status));
		goto disconnect;
	}

	/* check result of samr_Connect */
	if (!NT_STATUS_IS_OK(sc.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_Connect failed: %s\n", 
						nt_errstr(sc.out.result));
		status = sc.out.result;
		goto disconnect;
	}

	/* prepare samr_LookupDomain */
	d_name.name = r->samr.in.domain_name;
	ld.in.connect_handle = &p_handle;
	ld.in.domain = &d_name;

	/* 3. do a samr_LookupDomain to get the domain sid */
	status = dcerpc_samr_LookupDomain(c.pdc.out.dcerpc_pipe, mem_ctx, &ld);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_LookupDomain for [%s] failed: %s\n",
						r->samr.in.domain_name, nt_errstr(status));
		goto disconnect;
	}

	/* check result of samr_LookupDomain */
	if (!NT_STATUS_IS_OK(ld.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_LookupDomain for [%s] failed: %s\n",
						r->samr.in.domain_name, nt_errstr(ld.out.result));
		status = ld.out.result;
		goto disconnect;
	}

	/* prepare samr_OpenDomain */
	ZERO_STRUCT(d_handle);
	od.in.connect_handle = &p_handle;
	od.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;
	od.in.sid = ld.out.sid;
	od.out.domain_handle = &d_handle;

	/* 4. do a samr_OpenDomain to get a domain handle */
	status = dcerpc_samr_OpenDomain(c.pdc.out.dcerpc_pipe, mem_ctx, &od);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_OpenDomain for [%s] failed: %s\n",
						r->samr.in.domain_name, nt_errstr(status));
		goto disconnect;
	}

	/* check result of samr_OpenDomain */
	if (!NT_STATUS_IS_OK(od.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_OpenDomain for [%s] failed: %s\n",
						r->samr.in.domain_name, nt_errstr(od.out.result));
		status = od.out.result;
		goto disconnect;
	}

	/* prepare samr_LookupNames */
	ln.in.domain_handle = &d_handle;
	ln.in.num_names = 1;
	ln.in.names = talloc_array_p(mem_ctx, struct samr_Name, 1);
	if (!ln.in.names) {
		r->samr.out.error_string = "Out of Memory";
		return NT_STATUS_NO_MEMORY;
	}
	ln.in.names[0].name = r->samr.in.account_name;

	/* 5. do a samr_LookupNames to get the users rid */
	status = dcerpc_samr_LookupNames(c.pdc.out.dcerpc_pipe, mem_ctx, &ln);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_LookupNames for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(status));
		goto disconnect;
	}

	/* check result of samr_LookupNames */
	if (!NT_STATUS_IS_OK(ln.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_LookupNames for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(ln.out.result));
		status = ln.out.result;
		goto disconnect;
	}

	/* check if we got one RID for the user */
	if (ln.out.rids.count != 1) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_LookupNames for [%s] returns %d RIDs\n",
						r->samr.in.account_name, ln.out.rids.count);
		status = NT_STATUS_INVALID_PARAMETER;
		goto disconnect;	
	}

	/* prepare samr_OpenUser */
	ZERO_STRUCT(u_handle);
	ou.in.domain_handle = &d_handle;
	ou.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;
	ou.in.rid = ln.out.rids.ids[0];
	ou.out.user_handle = &u_handle;

	/* 6. do a samr_OpenUser to get a user handle */
	status = dcerpc_samr_OpenUser(c.pdc.out.dcerpc_pipe, mem_ctx, &ou);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_OpenUser for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(status));
		goto disconnect;
	}

	/* check result of samr_OpenUser */
	if (!NT_STATUS_IS_OK(ou.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"samr_OpenUser for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(ou.out.result));
		status = ou.out.result;
		goto disconnect;
	}

	/* prepare samr_SetUserInfo level 26 */
	ZERO_STRUCT(u_info);
	encode_pw_buffer(u_info.info26.password.data, r->samr.in.newpassword, STR_UNICODE);
	u_info.info26.pw_len = strlen(r->samr.in.newpassword);

	status = dcerpc_fetch_session_key(c.pdc.out.dcerpc_pipe, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"dcerpc_fetch_session_key failed: %s\n",
						nt_errstr(status));
		goto disconnect;
	}

	generate_random_buffer((uint8_t *)confounder, 16);

	MD5Init(&md5);
	MD5Update(&md5, confounder, 16);
	MD5Update(&md5, session_key.data, session_key.length);
	MD5Final(confounded_session_key.data, &md5);

	arcfour_crypt_blob(u_info.info26.password.data, 516, &confounded_session_key);
	memcpy(&u_info.info26.password.data[516], confounder, 16);

	sui.in.user_handle = &u_handle;
	sui.in.info = &u_info;
	sui.in.level = 26;

	/* 7. try samr_SetUserInfo level 26 to set the password */
	status = dcerpc_samr_SetUserInfo(c.pdc.out.dcerpc_pipe, mem_ctx, &sui);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 26 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(status));
		goto UserInfo25;
	}

	/* check result of samr_SetUserInfo level 26 */
	if (!NT_STATUS_IS_OK(sui.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 26 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(sui.out.result));
		if (NT_STATUS_EQUAL(sui.out.result, NT_STATUS_WRONG_PASSWORD)) {
			status = sui.out.result;
			goto disconnect;
		}
		goto UserInfo25;
	}

	goto disconnect;

UserInfo25:
	/* prepare samr_SetUserInfo level 25 */
	ZERO_STRUCT(u_info);
	u_info.info25.info.fields_present = SAMR_FIELD_PASSWORD;
	encode_pw_buffer(u_info.info25.password.data, r->samr.in.newpassword, STR_UNICODE);

	status = dcerpc_fetch_session_key(c.pdc.out.dcerpc_pipe, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"dcerpc_fetch_session_key failed: %s\n",
						nt_errstr(status));
		goto disconnect;
	}

	generate_random_buffer((uint8_t *)confounder, 16);

	MD5Init(&md5);
	MD5Update(&md5, confounder, 16);
	MD5Update(&md5, session_key.data, session_key.length);
	MD5Final(confounded_session_key.data, &md5);

	arcfour_crypt_blob(u_info.info25.password.data, 516, &confounded_session_key);
	memcpy(&u_info.info25.password.data[516], confounder, 16);

	sui.in.user_handle = &u_handle;
	sui.in.info = &u_info;
	sui.in.level = 25;

	/* 8. try samr_SetUserInfo level 25 to set the password */
	status = dcerpc_samr_SetUserInfo(c.pdc.out.dcerpc_pipe, mem_ctx, &sui);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 25 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(status));
		goto UserInfo24;
	}

	/* check result of samr_SetUserInfo level 25 */
	if (!NT_STATUS_IS_OK(sui.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 25 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(sui.out.result));
		if (NT_STATUS_EQUAL(sui.out.result, NT_STATUS_WRONG_PASSWORD)) {
			status = sui.out.result;
			goto disconnect;
		}
		goto UserInfo24;
	}

	goto disconnect;

UserInfo24:
	/* prepare samr_SetUserInfo level 24 */
	ZERO_STRUCT(u_info);
	encode_pw_buffer(u_info.info24.password.data, r->samr.in.newpassword, STR_UNICODE);
	/* w2k3 ignores this length */
	u_info.info24.pw_len = strlen_m(r->samr.in.newpassword)*2;

	status = dcerpc_fetch_session_key(c.pdc.out.dcerpc_pipe, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"dcerpc_fetch_session_key failed: %s\n",
						nt_errstr(status));
		goto disconnect;
	}

	arcfour_crypt_blob(u_info.info24.password.data, 516, &session_key);

	sui.in.user_handle = &u_handle;
	sui.in.info = &u_info;
	sui.in.level = 24;

	/* 9. try samr_SetUserInfo level 24 to set the password */
	status = dcerpc_samr_SetUserInfo(c.pdc.out.dcerpc_pipe, mem_ctx, &sui);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 24 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(status));
		goto UserInfo23;
	}

	/* check result of samr_SetUserInfo level 24 */
	if (!NT_STATUS_IS_OK(sui.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 24 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(sui.out.result));
		if (NT_STATUS_EQUAL(sui.out.result, NT_STATUS_WRONG_PASSWORD)) {
			status = sui.out.result;
			goto disconnect;
		}
		goto UserInfo23;
	}

	goto disconnect;

UserInfo23:
	/* prepare samr_SetUserInfo level 23 */
	ZERO_STRUCT(u_info);
	u_info.info23.info.fields_present = SAMR_FIELD_PASSWORD;
	encode_pw_buffer(u_info.info23.password.data, r->samr.in.newpassword, STR_UNICODE);

	status = dcerpc_fetch_session_key(c.pdc.out.dcerpc_pipe, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"dcerpc_fetch_session_key failed: %s\n",
						nt_errstr(status));
		goto disconnect;
	}

	arcfour_crypt_blob(u_info.info23.password.data, 516, &session_key);

	sui.in.user_handle = &u_handle;
	sui.in.info = &u_info;
	sui.in.level = 23;

	/* 10. try samr_SetUserInfo level 23 to set the password */
	status = dcerpc_samr_SetUserInfo(c.pdc.out.dcerpc_pipe, mem_ctx, &sui);
	if (!NT_STATUS_IS_OK(status)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 23 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(status));
		goto disconnect;
	}

	/* check result of samr_SetUserInfo level 23 */
	if (!NT_STATUS_IS_OK(sui.out.result)) {
		r->samr.out.error_string = talloc_asprintf(mem_ctx,
						"SetUserInfo level 23 for [%s] failed: %s\n",
						r->samr.in.account_name, nt_errstr(sui.out.result));
		if (NT_STATUS_EQUAL(sui.out.result, NT_STATUS_WRONG_PASSWORD)) {
			status = sui.out.result;
			goto disconnect;
		}
		goto disconnect;
	}

	goto disconnect;

disconnect:
	/* close connection */
	dcerpc_pipe_close(c.pdc.out.dcerpc_pipe);

	return status;
}

static NTSTATUS libnet_SetPassword_generic(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_SetPassword *r)
{
	NTSTATUS status;
	union libnet_SetPassword r2;

	r2.samr.level		= LIBNET_SET_PASSWORD_SAMR;
	r2.samr.in.account_name	= r->generic.in.account_name;
	r2.samr.in.domain_name	= r->generic.in.domain_name;
	r2.samr.in.newpassword	= r->generic.in.newpassword;

	status = libnet_SetPassword(ctx, mem_ctx, &r2);

	r->generic.out.error_string = r2.samr.out.error_string;

	return status;
}

NTSTATUS libnet_SetPassword(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, union libnet_SetPassword *r)
{
	switch (r->generic.level) {
		case LIBNET_SET_PASSWORD_GENERIC:
			return libnet_SetPassword_generic(ctx, mem_ctx, r);
		case LIBNET_SET_PASSWORD_SAMR:
			return libnet_SetPassword_samr(ctx, mem_ctx, r);
		case LIBNET_SET_PASSWORD_KRB5:
			return NT_STATUS_NOT_IMPLEMENTED;
		case LIBNET_SET_PASSWORD_LDAP:
			return NT_STATUS_NOT_IMPLEMENTED;
		case LIBNET_SET_PASSWORD_RAP:
			return NT_STATUS_NOT_IMPLEMENTED;
	}

	return NT_STATUS_INVALID_LEVEL;
}
