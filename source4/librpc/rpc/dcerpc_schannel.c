/* 
   Unix SMB/CIFS implementation.

   dcerpc schannel operations

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

#include "includes.h"

#define DCERPC_SCHANNEL_STATE_START 0
#define DCERPC_SCHANNEL_STATE_UPDATE_1 1

struct dcerpc_schannel_state {
	TALLOC_CTX *mem_ctx;
	uint8_t state;
	struct schannel_bind bind_schannel;
	struct schannel_state *schannel_state;
};

/*
  wrappers for the schannel_*() functions
*/
static NTSTATUS dcerpc_schannel_unseal(struct dcerpc_security *dcerpc_security, 
				    TALLOC_CTX *mem_ctx, 
				    uint8_t *data, size_t length, DATA_BLOB *sig)
{
	struct dcerpc_schannel_state *dce_schan_state = dcerpc_security->private_data;

	return schannel_unseal_packet(dce_schan_state->schannel_state, mem_ctx, data, length, sig);
}

static NTSTATUS dcerpc_schannel_check_sig(struct dcerpc_security *dcerpc_security, 
				   TALLOC_CTX *mem_ctx, 
				   const uint8_t *data, size_t length, 
				   const DATA_BLOB *sig)
{
	struct dcerpc_schannel_state *dce_schan_state = dcerpc_security->private_data;

	return schannel_check_packet(dce_schan_state->schannel_state, data, length, sig);
}

static NTSTATUS dcerpc_schannel_seal(struct dcerpc_security *dcerpc_security, 
				  TALLOC_CTX *mem_ctx, 
				  uint8_t *data, size_t length, 
				  DATA_BLOB *sig)
{
	struct dcerpc_schannel_state *dce_schan_state = dcerpc_security->private_data;

	return schannel_seal_packet(dce_schan_state->schannel_state, mem_ctx, data, length, sig);
}

static NTSTATUS dcerpc_schannel_sign(struct dcerpc_security *dcerpc_security, 
				 TALLOC_CTX *mem_ctx, 
				 const uint8_t *data, size_t length, 
				 DATA_BLOB *sig)
{
	struct dcerpc_schannel_state *dce_schan_state = dcerpc_security->private_data;

	return schannel_sign_packet(dce_schan_state->schannel_state, mem_ctx, data, length, sig);
}

static NTSTATUS dcerpc_schannel_session_key(struct dcerpc_security *dcerpc_security, 
				  DATA_BLOB *session_key)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS dcerpc_schannel_start(struct dcerpc_pipe *p, struct dcerpc_security *dcerpc_security)
{
	struct dcerpc_schannel_state *dce_schan_state;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	uint8_t session_key[16];
	int chan_type = 0;

	if (p->flags & DCERPC_SCHANNEL_BDC) {
		chan_type = SEC_CHAN_BDC;
	} else if (p->flags & DCERPC_SCHANNEL_WORKSTATION) {
		chan_type = SEC_CHAN_WKSTA;
	} else if (p->flags & DCERPC_SCHANNEL_DOMAIN) {
		chan_type = SEC_CHAN_DOMAIN;
	}

	status = dcerpc_schannel_key(p, dcerpc_security->user.domain, 
					dcerpc_security->user.name,
					dcerpc_security->user.password, 
					chan_type, session_key);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	mem_ctx = talloc_init("dcerpc_schannel_start");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	dce_schan_state = talloc_p(mem_ctx, struct dcerpc_schannel_state);
	if (!dce_schan_state) {
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	dce_schan_state->mem_ctx = mem_ctx;

	status = schannel_start(&dce_schan_state->schannel_state, session_key, True);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dce_schan_state->state = DCERPC_SCHANNEL_STATE_START;

	dcerpc_security->private_data = dce_schan_state;

	dump_data_pw("session key:\n", dce_schan_state->schannel_state->session_key, 16);

	return status;
}

static NTSTATUS dcerpc_schannel_update(struct dcerpc_security *dcerpc_security, TALLOC_CTX *out_mem_ctx, 
						const DATA_BLOB in, DATA_BLOB *out) 
{
	struct dcerpc_schannel_state *dce_schan_state = dcerpc_security->private_data;
	NTSTATUS status;
	struct schannel_bind bind_schannel;

	if (dce_schan_state->state != DCERPC_SCHANNEL_STATE_START) {
		return NT_STATUS_OK;
	}

	dce_schan_state->state = DCERPC_SCHANNEL_STATE_UPDATE_1;

	bind_schannel.unknown1 = 0;
#if 0
	/* to support this we'd need to have access to the full domain name */
	bind_schannel.bind_type = 23;
	bind_schannel.u.info23.domain = dcerpc_security->user.domain;
	bind_schannel.u.info23.account_name = dcerpc_security->user.name;
	bind_schannel.u.info23.dnsdomain = str_format_nbt_domain(dce_schan_state->mem_ctx, fulldomainname);
	bind_schannel.u.info23.workstation = str_format_nbt_domain(dce_schan_state->mem_ctx, dcerpc_security->user.name);
#else
	bind_schannel.bind_type = 3;
	bind_schannel.u.info3.domain = dcerpc_security->user.domain;
	bind_schannel.u.info3.account_name = dcerpc_security->user.name;
#endif

	status = ndr_push_struct_blob(out, dce_schan_state->mem_ctx, &bind_schannel,
				      (ndr_push_flags_fn_t)ndr_push_schannel_bind);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}

static void dcerpc_schannel_end(struct dcerpc_security *dcerpc_security)
{
	struct dcerpc_schannel_state *dce_schan_state = dcerpc_security->private_data;

	schannel_end(&dce_schan_state->schannel_state);

	talloc_destroy(dce_schan_state->mem_ctx);

	dcerpc_security->private_data = NULL;
}


/*
  get a schannel key using a netlogon challenge on a secondary pipe
*/
NTSTATUS dcerpc_schannel_key(struct dcerpc_pipe *p,
			     const char *domain,
			     const char *username,
			     const char *password,
			     int chan_type,
			     uint8_t new_session_key[16])
{
	NTSTATUS status;
	struct dcerpc_pipe *p2;
	struct netr_ServerReqChallenge r;
	struct netr_ServerAuthenticate2 a;
	struct netr_Credential credentials1, credentials2, credentials3;
	struct samr_Password mach_pwd;
	struct creds_CredentialState creds;
	const char *workgroup, *workstation;
	uint32_t negotiate_flags;

	if (p->flags & DCERPC_SCHANNEL_128) {
		negotiate_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	} else {
		negotiate_flags = NETLOGON_NEG_AUTH2_FLAGS;
	}

	workstation = username;
	workgroup = domain;

	/*
	  step 1 - establish a netlogon connection, with no authentication
	*/
	status = dcerpc_secondary_connection(p, &p2, 
					     DCERPC_NETLOGON_NAME, 
					     DCERPC_NETLOGON_UUID, 
					     DCERPC_NETLOGON_VERSION);


	/*
	  step 2 - request a netlogon challenge
	*/
	r.in.server_name = talloc_asprintf(p->mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = workstation;
	r.in.credentials = &credentials1;
	r.out.credentials = &credentials2;

	generate_random_buffer(credentials1.data, sizeof(credentials1.data), False);

	status = dcerpc_netr_ServerReqChallenge(p2, p->mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/*
	  step 3 - authenticate on the netlogon pipe
	*/
	E_md4hash(password, mach_pwd.hash);
	creds_client_init(&creds, &credentials1, &credentials2, &mach_pwd, &credentials3,
			  negotiate_flags);

	a.in.server_name = r.in.server_name;
	a.in.account_name = talloc_asprintf(p->mem_ctx, "%s$", workstation);
	a.in.secure_channel_type = chan_type;
	a.in.computer_name = workstation;
	a.in.negotiate_flags = &negotiate_flags;
	a.out.negotiate_flags = &negotiate_flags;
	a.in.credentials = &credentials3;
	a.out.credentials = &credentials3;

	status = dcerpc_netr_ServerAuthenticate2(p2, p->mem_ctx, &a);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!creds_client_check(&creds, a.out.credentials)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/*
	  the schannel session key is now in creds.session_key

	  we no longer need the netlogon pipe open
	*/
	dcerpc_pipe_close(p2);

	memcpy(new_session_key, creds.session_key, 16);

	return NT_STATUS_OK;
}

const struct dcesrv_security_ops dcerpc_schannel_security_ops = {
	.name		= "schannel",
	.auth_type	= DCERPC_AUTH_TYPE_SCHANNEL,
	.start 		= dcerpc_schannel_start,
	.update 	= dcerpc_schannel_update,
	.seal 		= dcerpc_schannel_seal,
	.sign		= dcerpc_schannel_sign,
	.check_sig	= dcerpc_schannel_check_sig,
	.unseal		= dcerpc_schannel_unseal,
	.session_key	= dcerpc_schannel_session_key,
	.end		= dcerpc_schannel_end
};

const struct dcesrv_security_ops *dcerpc_schannel_security_get_ops(void)
{
	return &dcerpc_schannel_security_ops;
}

/*
  do a schannel style bind on a dcerpc pipe. The username is usually
  of the form HOSTNAME$ and the password is the domain trust password
*/
NTSTATUS dcerpc_bind_auth_schannel(struct dcerpc_pipe *p,
				   const char *uuid, uint_t version,
				   const char *domain,
				   const char *username,
				   const char *password)
{
	NTSTATUS status;

	status = dcerpc_bind_auth(p, DCERPC_AUTH_TYPE_SCHANNEL,
				uuid, version,
				domain, username, 
				password);

	return status;
}
