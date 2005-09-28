/* 
   Unix SMB/CIFS implementation.

   utility code to join/leave a domain

   Copyright (C) Andrew Tridgell 2004
   
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
  this code is used by other torture modules to join/leave a domain
  as either a member, bdc or thru a trust relationship
*/

#include "includes.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "system/time.h"
#include "lib/crypto/crypto.h"
#include "libnet/libnet.h"
#include "lib/cmdline/popt_common.h"
#include "lib/ldb/include/ldb.h"


struct test_join {
	struct dcerpc_pipe *p;
	struct policy_handle user_handle;
	struct libnet_JoinDomain *libnet_r;
	const char *dom_sid;
};


static NTSTATUS DeleteUser_byname(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
				  struct policy_handle *handle, const char *name)
{
	NTSTATUS status;
	struct samr_DeleteUser d;
	struct policy_handle user_handle;
	uint32_t rid;
	struct samr_LookupNames n;
	struct lsa_String sname;
	struct samr_OpenUser r;

	sname.string = name;

	n.in.domain_handle = handle;
	n.in.num_names = 1;
	n.in.names = &sname;

	status = dcerpc_samr_LookupNames(p, mem_ctx, &n);
	if (NT_STATUS_IS_OK(status)) {
		rid = n.out.rids.ids[0];
	} else {
		return status;
	}

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = &user_handle;

	status = dcerpc_samr_OpenUser(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenUser(%s) failed - %s\n", name, nt_errstr(status));
		return status;
	}

	d.in.user_handle = &user_handle;
	d.out.user_handle = &user_handle;
	status = dcerpc_samr_DeleteUser(p, mem_ctx, &d);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/*
  create a test user in the domain
  an opaque pointer is returned. Pass it to torture_leave_domain() 
  when finished
*/

struct test_join *torture_create_testuser(const char *username, 
					  const char *domain,
					  uint16_t acct_type,
					  const char **random_password)
{
	NTSTATUS status;
	struct samr_Connect c;
	struct samr_CreateUser2 r;
	struct samr_OpenDomain o;
	struct samr_LookupDomain l;
	struct samr_GetUserPwInfo pwp;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	struct policy_handle handle;
	struct policy_handle domain_handle;
	uint32_t access_granted;
	uint32_t rid;
	DATA_BLOB session_key;
	struct lsa_String name;
	struct lsa_String comment;
	struct lsa_String full_name;
	
	int policy_min_pw_len = 0;
	struct test_join *join;
	char *random_pw;

	join = talloc(NULL, struct test_join);
	if (join == NULL) {
		return NULL;
	}

	ZERO_STRUCTP(join);

	printf("Connecting to SAMR\n");

	status = torture_rpc_connection(join, 
					&join->p, 
					DCERPC_SAMR_NAME,
					DCERPC_SAMR_UUID,
					DCERPC_SAMR_VERSION);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	c.in.system_name = NULL;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c.out.connect_handle = &handle;

	status = dcerpc_samr_Connect(join->p, join, &c);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		if (NT_STATUS_EQUAL(status, NT_STATUS_NET_WRITE_FAULT)) {
			errstr = dcerpc_errstr(join, join->p->last_fault_code);
		}
		printf("samr_Connect failed - %s\n", errstr);
		return NULL;
	}

	printf("Opening domain %s\n", domain);

	name.string = domain;
	l.in.connect_handle = &handle;
	l.in.domain_name = &name;

	status = dcerpc_samr_LookupDomain(join->p, join, &l);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupDomain failed - %s\n", nt_errstr(status));
		goto failed;
	}

	join->dom_sid = dom_sid_string(join, l.out.sid);

	o.in.connect_handle = &handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.sid = l.out.sid;
	o.out.domain_handle = &domain_handle;

	status = dcerpc_samr_OpenDomain(join->p, join, &o);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenDomain failed - %s\n", nt_errstr(status));
		goto failed;
	}

	printf("Creating account %s\n", username);

again:
	name.string = username;
	r.in.domain_handle = &domain_handle;
	r.in.account_name = &name;
	r.in.acct_flags = acct_type;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.user_handle = &join->user_handle;
	r.out.access_granted = &access_granted;
	r.out.rid = &rid;

	status = dcerpc_samr_CreateUser2(join->p, join, &r);

	if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
		status = DeleteUser_byname(join->p, join, &domain_handle, name.string);
		if (NT_STATUS_IS_OK(status)) {
			goto again;
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		printf("CreateUser2 failed - %s\n", nt_errstr(status));
		goto failed;
	}

	pwp.in.user_handle = &join->user_handle;

	status = dcerpc_samr_GetUserPwInfo(join->p, join, &pwp);
	if (NT_STATUS_IS_OK(status)) {
		policy_min_pw_len = pwp.out.info.min_password_length;
	}

	random_pw = generate_random_str(join, MAX(8, policy_min_pw_len));

	printf("Setting account password '%s'\n", random_pw);

	s.in.user_handle = &join->user_handle;
	s.in.info = &u;
	s.in.level = 24;

	encode_pw_buffer(u.info24.password.data, random_pw, STR_UNICODE);
	u.info24.pw_len = strlen(random_pw);

	status = dcerpc_fetch_session_key(join->p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		torture_leave_domain(join);
		goto failed;
	}

	arcfour_crypt_blob(u.info24.password.data, 516, &session_key);

	status = dcerpc_samr_SetUserInfo(join->p, join, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(status));
		goto failed;
	}

	ZERO_STRUCT(u);
	s.in.user_handle = &join->user_handle;
	s.in.info = &u;
	s.in.level = 21;

	u.info21.acct_flags = acct_type;
	u.info21.fields_present = SAMR_FIELD_ACCT_FLAGS | SAMR_FIELD_DESCRIPTION | SAMR_FIELD_COMMENT | SAMR_FIELD_FULL_NAME;
	comment.string = talloc_asprintf(join, 
					 "Tortured by Samba4: %s", 
					 timestring(join, time(NULL)));
	u.info21.comment = comment;
	full_name.string = talloc_asprintf(join, 
					 "Torture account for Samba4: %s", 
					 timestring(join, time(NULL)));
	u.info21.full_name = full_name;

	u.info21.description.string = talloc_asprintf(join, 
					 "Samba4 torture account created by host %s: %s", 
					 lp_netbios_name(), timestring(join, time(NULL)));

	printf("Resetting ACB flags, force pw change time\n");

	status = dcerpc_samr_SetUserInfo(join->p, join, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(status));
		goto failed;
	}

	if (random_password) {
		*random_password = random_pw;
	}

	return join;

failed:
	torture_leave_domain(join);
	return NULL;
}


struct test_join *torture_join_domain(const char *machine_name, 
				      uint32_t acct_flags,
				      const char **machine_password)
{
	NTSTATUS status;
	struct libnet_context *libnet_ctx;
	struct libnet_JoinDomain *libnet_r;
	struct test_join *tj;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	struct lsa_String comment;
	struct lsa_String full_name;
	
	tj = talloc(NULL, struct test_join);
	if (!tj) return NULL;

	libnet_r = talloc(tj, struct libnet_JoinDomain);
	if (!libnet_r) {
		talloc_free(tj);
		return NULL;
	}
	
	libnet_ctx = libnet_context_init(NULL);	
	if (!libnet_ctx) {
		talloc_free(tj);
		return NULL;
	}
	
	tj->libnet_r = libnet_r;
		
	libnet_ctx->cred = cmdline_credentials;
	libnet_r->in.binding = lp_parm_string(-1, "torture", "binding");
	libnet_r->in.level = LIBNET_JOINDOMAIN_SPECIFIED;
	libnet_r->in.netbios_name = machine_name;
	libnet_r->in.account_name = talloc_asprintf(libnet_r, "%s$", machine_name);
	if (!libnet_r->in.account_name) {
		talloc_free(tj);
		return NULL;
	}
	
	libnet_r->in.acct_type = acct_flags;

	status = libnet_JoinDomain(libnet_ctx, libnet_r, libnet_r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Domain join failed - %s.\n", nt_errstr(status)));
		talloc_free(tj);
                return NULL;
	}
	tj->p = libnet_r->out.samr_pipe;
	tj->user_handle = *libnet_r->out.user_handle;
	tj->dom_sid = dom_sid_string(tj, libnet_r->out.domain_sid);
	*machine_password = libnet_r->out.join_password;

	ZERO_STRUCT(u);
	s.in.user_handle = &tj->user_handle;
	s.in.info = &u;
	s.in.level = 21;

	u.info21.fields_present = SAMR_FIELD_DESCRIPTION | SAMR_FIELD_COMMENT | SAMR_FIELD_FULL_NAME;
	comment.string = talloc_asprintf(tj, 
					 "Tortured by Samba4: %s", 
					 timestring(tj, time(NULL)));
	u.info21.comment = comment;
	full_name.string = talloc_asprintf(tj, 
					 "Torture account for Samba4: %s", 
					 timestring(tj, time(NULL)));
	u.info21.full_name = full_name;

	u.info21.description.string = talloc_asprintf(tj, 
						      "Samba4 torture account created by host %s: %s", 
						      lp_netbios_name(), timestring(tj, time(NULL)));

	status = dcerpc_samr_SetUserInfo(tj->p, tj, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo (non-critical) failed - %s\n", nt_errstr(status));
	}

	DEBUG(0, ("%s joined domain %s (%s).\n", 
		  libnet_r->in.netbios_name, 
		  libnet_r->out.domain_name, 
		  tj->dom_sid));

	return tj;
}

struct dcerpc_pipe *torture_join_samr_pipe(struct test_join *join) 
{
	return join->p;
}

struct policy_handle *torture_join_samr_user_policy(struct test_join *join) 
{
	return &join->user_handle;
}

NTSTATUS torture_leave_ads_domain(TALLOC_CTX *mem_ctx, struct libnet_JoinDomain *libnet_r)
{
	NTSTATUS status;
	int rtn;
	TALLOC_CTX *tmp_ctx;

	struct ldb_dn *server_dn;
	struct ldb_context *ldb_ctx;

	char *remote_ldb_url; 
	 
	/* Check if we are a domain controller. If not, exit. */
	if (!libnet_r->out.server_dn_str) {
		return NT_STATUS_OK;
	}

	tmp_ctx = talloc_named(mem_ctx, 0, "torture_leave temporary context");
	if (!tmp_ctx) {
		libnet_r->out.error_string = NULL;
		return NT_STATUS_NO_MEMORY;
	}

	ldb_ctx = ldb_init(tmp_ctx);
	if (!ldb_ctx) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* Remove CN=Servers,... entry from the AD. */ 
	server_dn = ldb_dn_explode(tmp_ctx, libnet_r->out.server_dn_str);
	if (!server_dn) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	remote_ldb_url = talloc_asprintf(tmp_ctx, "ldap://%s", libnet_r->out.samr_binding->host);
	if (!remote_ldb_url) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	rtn = ldb_connect(ldb_ctx, remote_ldb_url, 0, NULL);
	if (rtn != 0) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	rtn = ldb_delete(ldb_ctx, server_dn);
	if (rtn != 0) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(0, ("%s removed successfully.\n", libnet_r->out.server_dn_str));

	talloc_free(tmp_ctx); 
	return status;
}

/*
  leave the domain, deleting the machine acct
*/

void torture_leave_domain(struct test_join *join)
{
	struct samr_DeleteUser d;
	NTSTATUS status;

	if (!join) {
		return;
	}
	d.in.user_handle = &join->user_handle;
	d.out.user_handle = &join->user_handle;
					
	/* Delete machine account */	                                                                                                                                                                                                                                                                                                                
	status = dcerpc_samr_DeleteUser(join->p, join, &d);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Delete of machine account failed\n");
	} else {
		printf("Delete of machine account was successful.\n");
	}

	if (join->libnet_r) {
		status = torture_leave_ads_domain(join, join->libnet_r);
	}
	
	talloc_free(join);
}

/*
  return the dom sid for a test join
*/
const char *torture_join_sid(struct test_join *join)
{
	return join->dom_sid;
}


struct test_join_ads_dc {
	struct test_join *join;
};

struct test_join_ads_dc *torture_join_domain_ads_dc(const char *machine_name, 
						    const char *domain,
						    const char **machine_password)
{
	struct test_join_ads_dc *join;

	join = talloc(NULL, struct test_join_ads_dc);
	if (join == NULL) {
		return NULL;
	}

	join->join = torture_join_domain(machine_name, 
					ACB_SVRTRUST,
					machine_password);

	if (!join->join) {
		return NULL;
	}

	/* do netlogon DrsEnumerateDomainTrusts */

	/* modify userAccountControl from 4096 to 532480 */
	
	/* modify RDN to OU=Domain Controllers and skip the $ from server name */

	/* ask objectVersion of Schema Partition */

	/* ask rIDManagerReferenz of the Domain Partition */

	/* ask fsMORoleOwner of the RID-Manager$ object
	 * returns CN=NTDS Settings,CN=<DC>,CN=Servers,CN=Default-First-Site-Name, ...
	 */

	/* ask for dnsHostName of CN=<DC>,CN=Servers,CN=Default-First-Site-Name, ... */

	/* ask for objectGUID of CN=NTDS Settings,CN=<DC>,CN=Servers,CN=Default-First-Site-Name, ... */

	/* ask for * of CN=Default-First-Site-Name, ... */

	/* search (&(|(objectClass=user)(objectClass=computer))(sAMAccountName=<machine_name>$)) in Domain Partition 
	 * attributes : distinguishedName, userAccountControl
	 */

	/* ask * for CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,... 
	 * should fail with noSuchObject
	 */

	/* add CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,... 
	 *
	 * objectClass = server
	 * systemFlags = 50000000
	 * serverReferenz = CN=<machine_name>,OU=Domain Controllers,...
	 */

	/* ask for * of CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name, ...
	 * should fail with noSuchObject
	 */

	/* search for (ncname=<domain_nc>) in CN=Partitions,CN=Configuration,... 
	 * attributes: ncName, dnsRoot
	 */

	/* modify add CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,...
	 * serverReferenz = CN=<machine_name>,OU=Domain Controllers,...
	 * should fail with attributeOrValueExists
	 */

	/* modify replace CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,...
	 * serverReferenz = CN=<machine_name>,OU=Domain Controllers,...
	 */

	/* DsReplicaAdd to create the CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name, ...
	 * needs to be tested
	 */

	return join;
}
		
void torture_leave_domain_ads_dc(struct test_join_ads_dc *join)
{

	if (join->join) {
		torture_leave_domain(join->join);
	}

	talloc_free(join);
}
