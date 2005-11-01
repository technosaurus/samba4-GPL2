/* 
   Unix SMB/CIFS implementation.

   kpasswd Server implementation

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell	2005

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
#include "smbd/service_task.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "kdc/kdc.h"
#include "system/network.h"
#include "dlinklist.h"
#include "lib/ldb/include/ldb.h"
#include "heimdal/lib/krb5/krb5-private.h"
#include "auth/auth.h"

/* hold information about one kdc socket */
struct kpasswd_socket {
	struct socket_context *sock;
	struct kdc_server *kdc;
	struct fd_event *fde;

	/* a queue of outgoing replies that have been deferred */
	struct kdc_reply *send_queue;
};

/* Return true if there is a valid error packet formed in the error_blob */
static BOOL kpasswdd_make_error_reply(struct kdc_server *kdc, 
				     TALLOC_CTX *mem_ctx, 
				     uint16_t result_code, 
				     const char *error_string, 
				     DATA_BLOB *error_blob) 
{
	char *error_string_utf8;
	ssize_t len;
	
	DEBUG(result_code ? 3 : 10, ("kpasswdd: %s\n", error_string));

	len = push_utf8_talloc(mem_ctx, &error_string_utf8, error_string);
	if (len == -1) {
		return False;
	}

	*error_blob = data_blob_talloc(mem_ctx, NULL, 2 + len + 1);
	if (!error_blob->data) {
		return False;
	}
	RSSVAL(error_blob->data, 0, result_code);
	memcpy(error_blob->data + 2, error_string_utf8, len + 1);
	return True;
}

/* Return true if there is a valid error packet formed in the error_blob */
static BOOL kpasswdd_make_unauth_error_reply(struct kdc_server *kdc, 
					    TALLOC_CTX *mem_ctx, 
					    uint16_t result_code, 
					    const char *error_string, 
					    DATA_BLOB *error_blob) 
{
	BOOL ret;
	int kret;
	DATA_BLOB error_bytes;
	krb5_data k5_error_bytes, k5_error_blob;
	ret = kpasswdd_make_error_reply(kdc, mem_ctx, result_code, error_string, 
				       &error_bytes);
	if (!ret) {
		return False;
	}
	k5_error_bytes.data = error_bytes.data;
	k5_error_bytes.length = error_bytes.length;
	kret = krb5_mk_error(kdc->smb_krb5_context->krb5_context,
			     result_code, NULL, &k5_error_bytes, 
			     NULL, NULL, NULL, NULL, &k5_error_blob);
	if (kret) {
		return False;
	}
	*error_blob = data_blob_talloc(mem_ctx, k5_error_blob.data, k5_error_blob.length);
	krb5_data_free(&k5_error_blob);
	if (!error_blob->data) {
		return False;
	}
	return True;
}

static BOOL kpasswd_make_pwchange_reply(struct kdc_server *kdc, 
					TALLOC_CTX *mem_ctx, 
					NTSTATUS status, 
					enum samr_RejectReason reject_reason,
					struct samr_DomInfo1 *dominfo,
					DATA_BLOB *error_blob) 
{
	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_ACCESSDENIED,
						"No such user when changing password",
						error_blob);
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_ACCESSDENIED,
						"Not permitted to change password",
						error_blob);
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_PASSWORD_RESTRICTION)) {
		const char *reject_string;
		switch (reject_reason) {
		case SAMR_REJECT_TOO_SHORT:
			reject_string = talloc_asprintf(mem_ctx, "Password too short, password must be at least %d characters long",
							dominfo->min_password_length);
			break;
		case SAMR_REJECT_COMPLEXITY:
			reject_string = "Password does not meet complexity requirements";
			break;
		case SAMR_REJECT_OTHER:
		default:
			reject_string = talloc_asprintf(mem_ctx, "Password must be at least %d characters long, and cannot match any of your %d previous passwords",
							dominfo->min_password_length, dominfo->password_history_length);
			break;
		}
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_SOFTERROR,
						reject_string,
						error_blob);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						 KRB5_KPASSWD_HARDERROR,
						 talloc_asprintf(mem_ctx, "failed to set password: %s", nt_errstr(status)),
						 error_blob);
		
	}
	return kpasswdd_make_error_reply(kdc, mem_ctx, KRB5_KPASSWD_SUCCESS,
					"Password changed",
					error_blob);
}

/* 
   A user password change
   
   Return true if there is a valid error packet (or sucess) formed in
   the error_blob
*/
static BOOL kpasswdd_change_password(struct kdc_server *kdc,
				     TALLOC_CTX *mem_ctx, 
				     struct auth_session_info *session_info,
				     const char *password,
				     DATA_BLOB *reply)
{
	NTSTATUS status;
	enum samr_RejectReason reject_reason;
	struct samr_DomInfo1 *dominfo;
	struct ldb_context *samdb;

	samdb = samdb_connect(mem_ctx, system_session(mem_ctx));
	if (!samdb) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_HARDERROR,
						"Failed to open samdb",
						reply);
	}
	
	DEBUG(3, ("Changing password of %s\n", dom_sid_string(mem_ctx, session_info->security_token->user_sid)));

	/* User password change */
	status = samdb_set_password_sid(samdb, mem_ctx, 
					session_info->security_token->user_sid,
					password, NULL, NULL, 
					True, /* this is a user password change */
					True, /* run restriction tests */
					&reject_reason,
					&dominfo);
	return kpasswd_make_pwchange_reply(kdc, mem_ctx, 
					   status, 
					   reject_reason,
					   dominfo, 
					   reply);

}

static BOOL kpasswd_process_request(struct kdc_server *kdc,
				    TALLOC_CTX *mem_ctx, 
				    struct gensec_security *gensec_security,
				    uint16_t version,
				    DATA_BLOB *input, 
				    DATA_BLOB *reply)
{
	NTSTATUS status;
	enum samr_RejectReason reject_reason;
	struct samr_DomInfo1 *dominfo;
	struct ldb_context *samdb;
	struct auth_session_info *session_info;
	struct ldb_message *msg = ldb_msg_new(gensec_security);
	krb5_context context = kdc->smb_krb5_context->krb5_context;
	int ret;
	if (!msg) {
		return False;
	}

	if (!NT_STATUS_IS_OK(gensec_session_info(gensec_security, 
						 &session_info))) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_HARDERROR,
						"gensec_session_info failed!",
						reply);
	}

	switch (version) {
	case KRB5_KPASSWD_VERS_CHANGEPW:
	{
		char *password = talloc_strndup(mem_ctx, (const char *)input->data, input->length);
		if (!password) {
			return False;
		}
		return kpasswdd_change_password(kdc, mem_ctx, session_info, 
						password, reply);
		break;
	}
	case KRB5_KPASSWD_VERS_SETPW:
	{
		size_t len;
		ChangePasswdDataMS chpw;
		char *password;
		krb5_principal principal;
		char *set_password_on_princ;
		struct ldb_dn *set_password_on_dn;

		samdb = samdb_connect(gensec_security, session_info);

		ret = decode_ChangePasswdDataMS(input->data, input->length,
						&chpw, &len);
		if (ret) {
			return kpasswdd_make_error_reply(kdc, mem_ctx, 
							KRB5_KPASSWD_MALFORMED,
							"failed to decode password change structure",
							reply);
		}
		
		password = talloc_strndup(mem_ctx, chpw.newpasswd.data, 
					  chpw.newpasswd.length);
		if (!password) {
			free_ChangePasswdDataMS(&chpw);
			return False;
		}
		if ((chpw.targname && !chpw.targrealm) 
		    || (!chpw.targname && chpw.targrealm)) {
			return kpasswdd_make_error_reply(kdc, mem_ctx, 
							KRB5_KPASSWD_MALFORMED,
							"Realm and principal must be both present, or neither present",
							reply);
		}
		if (chpw.targname && chpw.targrealm) {
			if (_krb5_principalname2krb5_principal(&principal, *chpw.targname, 
							       *chpw.targrealm) != 0) {
				free_ChangePasswdDataMS(&chpw);
				return kpasswdd_make_error_reply(kdc, mem_ctx, 
								KRB5_KPASSWD_MALFORMED,
								"failed to extract principal to set",
								reply);
				
			}
		} else {
			free_ChangePasswdDataMS(&chpw);
			return kpasswdd_change_password(kdc, mem_ctx, session_info, 
							password, reply);
		}
		free_ChangePasswdDataMS(&chpw);

		if (krb5_unparse_name(context, principal, &set_password_on_princ) != 0) {
			krb5_free_principal(context, principal);
			return kpasswdd_make_error_reply(kdc, mem_ctx, 
							KRB5_KPASSWD_MALFORMED,
							"krb5_unparse_name failed!",
							reply);
		}
		
		krb5_free_principal(context, principal);
		
		status = crack_user_principal_name(samdb, mem_ctx, 
						   set_password_on_princ, 
						   &set_password_on_dn, NULL);
		free(set_password_on_princ);
		if (!NT_STATUS_IS_OK(status)) {
			return kpasswd_make_pwchange_reply(kdc, mem_ctx, 
							   status,
							   reject_reason, 
							   dominfo, 
							   reply);
		}

		/* Admin password set */
		status = samdb_set_password(samdb, mem_ctx,
					    set_password_on_dn, NULL,
					    msg, password, NULL, NULL, 
					    False, /* this is not a user password change */
					    True, /* run restriction tests */
					    &reject_reason, &dominfo);

		return kpasswd_make_pwchange_reply(kdc, mem_ctx, 
						   status,
						   reject_reason, 
						   dominfo, 
						   reply);
	}
	default:
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_BAD_VERSION,
						talloc_asprintf(mem_ctx, 
								"Protocol version %u not supported", 
								version),
						reply);
	}
	return True;
}

BOOL kpasswdd_process(struct kdc_server *kdc,
		      TALLOC_CTX *mem_ctx, 
		      DATA_BLOB *input, 
		      DATA_BLOB *reply,
		      const char *from,
		      int src_port)
{
	BOOL ret;
	const uint16_t header_len = 6;
	uint16_t len;
	uint16_t ap_req_len;
	uint16_t krb_priv_len;
	uint16_t version;
	NTSTATUS nt_status;
	DATA_BLOB ap_req, krb_priv_req, krb_priv_rep, ap_rep;
	DATA_BLOB kpasswd_req, kpasswd_rep;
	struct cli_credentials *server_credentials;
	struct gensec_security *gensec_security;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	
	if (!tmp_ctx) {
		return False;
	}

	if (input->length <= header_len) {
		talloc_free(tmp_ctx);
		return False;
	}

	len = RSVAL(input->data, 0);
	if (input->length != len) {
		talloc_free(tmp_ctx);
		return False;
	}

	version = RSVAL(input->data, 2);
	ap_req_len = RSVAL(input->data, 4);
	if ((ap_req_len >= len) || (ap_req_len + header_len) >= len) {
		talloc_free(tmp_ctx);
		return False;
	}
	
	krb_priv_len = len - ap_req_len;
	ap_req = data_blob_const(&input->data[header_len], ap_req_len);
	krb_priv_req = data_blob_const(&input->data[header_len + ap_req_len], krb_priv_len);
	
	nt_status = gensec_server_start(tmp_ctx, &gensec_security, kdc->task->event_ctx);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return False;
	}

	server_credentials 
		= cli_credentials_init(tmp_ctx);
	if (!server_credentials) {
		DEBUG(1, ("Failed to init server credentials\n"));
		return False;
	}
	
	cli_credentials_set_conf(server_credentials);
	nt_status = cli_credentials_set_stored_principal(server_credentials, "kadmin/changepw");
	if (!NT_STATUS_IS_OK(nt_status)) {
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "Failed to obtain server credentials for kadmin/changepw: %s\n", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}
	
	gensec_set_credentials(gensec_security, server_credentials);
	gensec_want_feature(gensec_security, GENSEC_FEATURE_SEAL);

	nt_status = gensec_start_mech_by_name(gensec_security, "krb5");
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return False;
	}

	nt_status = gensec_update(gensec_security, tmp_ctx, ap_req, &ap_rep);
	if (!NT_STATUS_IS_OK(nt_status) && !NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "gensec_update failed: %s", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}

	nt_status = gensec_unwrap(gensec_security, tmp_ctx, &krb_priv_req, &kpasswd_req);
	if (!NT_STATUS_IS_OK(nt_status)) {
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "gensec_unwrap failed: %s", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = kpasswd_process_request(kdc, tmp_ctx, 
				      gensec_security, 
				      version, 
				      &kpasswd_req, &kpasswd_rep); 
	if (!ret) {
		/* Argh! */
		return False;
	}
	
	nt_status = gensec_wrap(gensec_security, tmp_ctx, 
				&kpasswd_rep, &krb_priv_rep);
	if (!NT_STATUS_IS_OK(nt_status)) {
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "gensec_wrap failed: %s", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}
	
reply:
	*reply = data_blob_talloc(mem_ctx, NULL, krb_priv_rep.length + ap_rep.length + header_len);
	if (!reply->data) {
		return False;
	}

	RSSVAL(reply->data, 0, reply->length);
	RSSVAL(reply->data, 2, 1); /* This is a version 1 reply, MS change/set or otherwise */
	RSSVAL(reply->data, 4, ap_rep.length);
	memcpy(reply->data + header_len, 
	       ap_rep.data, 
	       ap_rep.length);
	memcpy(reply->data + header_len + ap_rep.length, 
	       krb_priv_rep.data, 
	       krb_priv_rep.length);

	talloc_free(tmp_ctx);
	return ret;
}

