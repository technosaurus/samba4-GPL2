/* 
   Unix SMB/CIFS implementation.

   dcerpc authentication operations

   Copyright (C) Andrew Tridgell 2003
   
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

/*
  do a non-athenticated dcerpc bind
*/
NTSTATUS dcerpc_bind_auth_none(struct dcerpc_pipe *p,
			       const char *uuid, uint_t version)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("dcerpc_bind_auth_ntlm");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	status = dcerpc_bind_byuuid(p, mem_ctx, uuid, version);
	talloc_destroy(mem_ctx);

	return status;
}

const struct dcesrv_security_ops *dcerpc_security_by_authtype(uint8_t auth_type)
{
	switch (auth_type) {
		case DCERPC_AUTH_TYPE_SCHANNEL:
			return dcerpc_schannel_security_get_ops();

		case DCERPC_AUTH_TYPE_NTLMSSP:
			return dcerpc_ntlmssp_security_get_ops();
	}

	return NULL;
}

NTSTATUS dcerpc_bind_auth(struct dcerpc_pipe *p, uint8_t auth_type,
				       const char *uuid, uint_t version,
				       const char *domain,
				       const char *username,
				       const char *password)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	DATA_BLOB credentials;

	mem_ctx = talloc_init("dcerpc_bind_auth");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	p->security_state.ops = dcerpc_security_by_authtype(auth_type);
	if (!p->security_state.ops) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	p->security_state.user.domain = domain;
	p->security_state.user.name = username;
	p->security_state.user.password = password;

	status = p->security_state.ops->start(p, &p->security_state);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	p->security_state.auth_info = talloc(p->mem_ctx, sizeof(*p->security_state.auth_info));
	if (!p->security_state.auth_info) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	p->security_state.auth_info->auth_type = auth_type;
	p->security_state.auth_info->auth_pad_length = 0;
	p->security_state.auth_info->auth_reserved = 0;
	p->security_state.auth_info->auth_context_id = random();
	p->security_state.auth_info->credentials = data_blob(NULL, 0);

	if (p->flags & DCERPC_SEAL) {
		p->security_state.auth_info->auth_level = DCERPC_AUTH_LEVEL_PRIVACY;
	} else if (p->flags & DCERPC_SIGN) {
		p->security_state.auth_info->auth_level = DCERPC_AUTH_LEVEL_INTEGRITY;
	} else {
		p->security_state.auth_info->auth_level = DCERPC_AUTH_LEVEL_NONE;
	}

	status = p->security_state.ops->update(&p->security_state, mem_ctx,
					p->security_state.auth_info->credentials,
					&credentials);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		goto done;
	}

	p->security_state.auth_info->credentials = credentials;

	status = dcerpc_bind_byuuid(p, mem_ctx, uuid, version);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = p->security_state.ops->update(&p->security_state, mem_ctx,
					p->security_state.auth_info->credentials,
					&credentials);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		goto done;
	}

	p->security_state.auth_info->credentials = credentials;

	status = dcerpc_auth3(p, mem_ctx);
done:
	talloc_destroy(mem_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		ZERO_STRUCT(p->security_state);
	}

	return status;
}
