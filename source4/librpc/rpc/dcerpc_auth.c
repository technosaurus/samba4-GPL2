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
  do a simple ntlm style authentication on a dcerpc pipe
*/
NTSTATUS dcerpc_bind_auth_ntlm(struct dcerpc_pipe *p,
			       const char *uuid, unsigned version,
			       const char *domain,
			       const char *username,
			       const char *password)
{
	NTSTATUS status;
	struct ntlmssp_state *state;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("dcerpc_bind_auth_ntlm");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ntlmssp_client_start(&state);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = ntlmssp_set_domain(state, domain);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	
	status = ntlmssp_set_username(state, username);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = ntlmssp_set_password(state, password);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	p->auth_info = talloc(p->mem_ctx, sizeof(*p->auth_info));
	if (!p->auth_info) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	p->auth_info->auth_type = DCERPC_AUTH_TYPE_NTLMSSP;
	p->auth_info->auth_level = DCERPC_AUTH_LEVEL_INTEGRITY;
	p->auth_info->auth_pad_length = 0;
	p->auth_info->auth_reserved = 0;
	p->auth_info->auth_context_id = random();
	p->auth_info->credentials = data_blob(NULL, 0);
	p->ntlmssp_state = NULL;

	status = ntlmssp_update(state, 
				p->auth_info->credentials,
				&p->auth_info->credentials);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		goto done;
	}
	status = dcerpc_bind_byuuid(p, mem_ctx, uuid, version);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = ntlmssp_update(state, 
				p->auth_info->credentials, 
				&p->auth_info->credentials);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		goto done;
	}

	status = dcerpc_auth3(p, mem_ctx);
	p->ntlmssp_state = state;
	p->auth_info->credentials = data_blob(NULL, 0);

	ntlmssp_sign_init(state);

done:
	talloc_destroy(mem_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		p->ntlmssp_state = NULL;
	}

	return status;
}


/*
  do a non-athenticated dcerpc bind
*/
NTSTATUS dcerpc_bind_auth_none(struct dcerpc_pipe *p,
			       const char *uuid, unsigned version)
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
