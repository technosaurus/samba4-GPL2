/* 
   Unix SMB/CIFS implementation.

   endpoint server for the samr pipe

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

/*
  this type allows us to distinguish handle types
*/
enum samr_handle {
	SAMR_HANDLE_CONNECT,
	SAMR_HANDLE_DOMAIN,
	SAMR_HANDLE_USER,
	SAMR_HANDLE_GROUP,
	SAMR_HANDLE_ALIAS
};


/*
  state asscoiated with a samr_Connect*() operation
*/
struct samr_connect_state {
	TALLOC_CTX *mem_ctx;
	uint32 access_mask;
};


/*
  destroy an open connection. This closes the database connection
*/
static void samr_Connect_destroy(struct dcesrv_connection *conn, struct dcesrv_handle *h)
{
	struct samr_connect_state *state = h->data;
	talloc_destroy(state->mem_ctx);
}

/* 
  samr_Connect 

  create a connection to the SAM database
*/
static NTSTATUS samr_Connect(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			     struct samr_Connect *r)
{
	struct samr_connect_state *state;
	struct dcesrv_handle *handle;
	TALLOC_CTX *connect_mem_ctx;

	ZERO_STRUCTP(r->out.handle);

	connect_mem_ctx = talloc_init("samr_Connect");
	if (!connect_mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	state = talloc_p(connect_mem_ctx, struct samr_connect_state);
	if (!state) {
		return NT_STATUS_NO_MEMORY;
	}
	state->mem_ctx = connect_mem_ctx;

	/* make sure the sam database is accessible */
	if (samdb_connect() != 0) {
		talloc_destroy(state->mem_ctx);
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	handle = dcesrv_handle_new(dce_call->conn, SAMR_HANDLE_CONNECT);
	if (!handle) {
		talloc_destroy(state->mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	handle->data = state;
	handle->destroy = samr_Connect_destroy;

	state->access_mask = r->in.access_mask;
	*r->out.handle = handle->wire_handle;

	return NT_STATUS_OK;
}


/* 
  samr_Close 
*/
static NTSTATUS samr_Close(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			   struct samr_Close *r)
{
	struct dcesrv_handle *h = dcesrv_handle_fetch(dce_call->conn, 
						      r->in.handle, 
						      DCESRV_HANDLE_ANY);
	DCESRV_CHECK_HANDLE(h);

	/* this causes the callback samr_XXX_destroy() to be called by
	   the handle destroy code which destroys the state associated
	   with the handle */
	dcesrv_handle_destroy(dce_call->conn, h);

	ZERO_STRUCTP(r->out.handle);

	return NT_STATUS_OK;
}


/* 
  samr_SetSecurity 
*/
static NTSTATUS samr_SetSecurity(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct samr_SetSecurity *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QuerySecurity 
*/
static NTSTATUS samr_QuerySecurity(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				   struct samr_QuerySecurity *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_Shutdown 

  we refuse this operation completely. If a admin wants to shutdown samr
  in Samba then they should use the samba admin tools to disable the samr pipe
*/
static NTSTATUS samr_Shutdown(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			      struct samr_Shutdown *r)
{
	return NT_STATUS_ACCESS_DENIED;
}


/* 
  samr_LookupDomain 

  this maps from a domain name to a SID
*/
static NTSTATUS samr_LookupDomain(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct samr_LookupDomain *r)
{
	struct dcesrv_handle *h;
	struct dom_sid2 *sid;
	const char *sidstr;
		
	h = dcesrv_handle_fetch(dce_call->conn, r->in.handle, SAMR_HANDLE_CONNECT);

	DCESRV_CHECK_HANDLE(h);

	r->out.sid = NULL;

	if (r->in.domain->name == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	sidstr = samdb_search_string(mem_ctx, "objectSid",
				     "(&(name=%s)(objectclass=domain))",
				     r->in.domain->name);
	if (sidstr == NULL) {
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	sid = dom_sid_parse_talloc(mem_ctx, sidstr);
	if (sid == NULL) {
		DEBUG(1,("samdb: Invalid sid '%s' for domain %s\n",
			 sidstr, r->in.domain->name));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	r->out.sid = sid;

	return NT_STATUS_OK;
}


/* 
  samr_EnumDomains 

  list the domains in the SAM
*/
static NTSTATUS samr_EnumDomains(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct samr_EnumDomains *r)
{
	struct dcesrv_handle *h;
	struct samr_SamArray *array;
	char **domains;
	int count, i, start_i;

	h = dcesrv_handle_fetch(dce_call->conn, r->in.handle, SAMR_HANDLE_CONNECT);

	DCESRV_CHECK_HANDLE(h);

	*r->out.resume_handle = 0;
	r->out.sam = NULL;
	r->out.num_entries = 0;

	count = samdb_search_string_multiple(mem_ctx, &domains, 
					     "name", "(objectclass=domain)");
	if (count == -1) {
		DEBUG(1,("samdb: no domains found in EnumDomains\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	*r->out.resume_handle = count;

	start_i = *r->in.resume_handle;

	if (start_i >= count) {
		/* search past end of list is not an error for this call */
		return NT_STATUS_OK;
	}

	array = talloc_p(mem_ctx, struct samr_SamArray);
	if (array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
		
	array->count = 0;
	array->entries = NULL;

	array->entries = talloc_array_p(mem_ctx, struct samr_SamEntry, count - start_i);
	if (array->entries == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0;i<count-start_i;i++) {
		array->entries[i].idx = start_i + i;
		array->entries[i].name.name = domains[start_i+i];
	}

	r->out.sam = array;
	r->out.num_entries = i - start_i;
	array->count = r->out.num_entries;

	return NT_STATUS_OK;
}


/* 
  samr_OpenDomain 
*/
static NTSTATUS samr_OpenDomain(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_OpenDomain *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryDomainInfo 
*/
static NTSTATUS samr_QueryDomainInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryDomainInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_SetDomainInfo 
*/
static NTSTATUS samr_SetDomainInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetDomainInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_CreateDomainGroup 
*/
static NTSTATUS samr_CreateDomainGroup(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_CreateDomainGroup *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_EnumDomainGroups 
*/
static NTSTATUS samr_EnumDomainGroups(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_EnumDomainGroups *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_CreateUser 
*/
static NTSTATUS samr_CreateUser(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_CreateUser *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_EnumDomainUsers 
*/
static NTSTATUS samr_EnumDomainUsers(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_EnumDomainUsers *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_CreateDomAlias 
*/
static NTSTATUS samr_CreateDomAlias(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_CreateDomAlias *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_EnumDomainAliases 
*/
static NTSTATUS samr_EnumDomainAliases(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_EnumDomainAliases *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetAliasMembership 
*/
static NTSTATUS samr_GetAliasMembership(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetAliasMembership *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_LookupNames 
*/
static NTSTATUS samr_LookupNames(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_LookupNames *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_LookupRids 
*/
static NTSTATUS samr_LookupRids(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_LookupRids *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_OpenGroup 
*/
static NTSTATUS samr_OpenGroup(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_OpenGroup *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryGroupInfo 
*/
static NTSTATUS samr_QueryGroupInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryGroupInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_SetGroupInfo 
*/
static NTSTATUS samr_SetGroupInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetGroupInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_AddGroupMember 
*/
static NTSTATUS samr_AddGroupMember(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_AddGroupMember *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_DeleteDomainGroup 
*/
static NTSTATUS samr_DeleteDomainGroup(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_DeleteDomainGroup *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_DeleteGroupMember 
*/
static NTSTATUS samr_DeleteGroupMember(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_DeleteGroupMember *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryGroupMember 
*/
static NTSTATUS samr_QueryGroupMember(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryGroupMember *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_SetMemberAttributesOfGroup 
*/
static NTSTATUS samr_SetMemberAttributesOfGroup(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetMemberAttributesOfGroup *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_OpenAlias 
*/
static NTSTATUS samr_OpenAlias(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_OpenAlias *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryAliasInfo 
*/
static NTSTATUS samr_QueryAliasInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryAliasInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_SetAliasInfo 
*/
static NTSTATUS samr_SetAliasInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetAliasInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_DeleteDomAlias 
*/
static NTSTATUS samr_DeleteDomAlias(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_DeleteDomAlias *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_AddAliasMember 
*/
static NTSTATUS samr_AddAliasMember(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_AddAliasMember *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_DeleteAliasMember 
*/
static NTSTATUS samr_DeleteAliasMember(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_DeleteAliasMember *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetMembersInAlias 
*/
static NTSTATUS samr_GetMembersInAlias(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetMembersInAlias *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_OpenUser 
*/
static NTSTATUS samr_OpenUser(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_OpenUser *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_DeleteUser 
*/
static NTSTATUS samr_DeleteUser(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_DeleteUser *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryUserInfo 
*/
static NTSTATUS samr_QueryUserInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryUserInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_SetUserInfo 
*/
static NTSTATUS samr_SetUserInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetUserInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_ChangePasswordUser 
*/
static NTSTATUS samr_ChangePasswordUser(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_ChangePasswordUser *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetGroupsForUser 
*/
static NTSTATUS samr_GetGroupsForUser(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetGroupsForUser *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryDisplayInfo 
*/
static NTSTATUS samr_QueryDisplayInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryDisplayInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetDisplayEnumerationIndex 
*/
static NTSTATUS samr_GetDisplayEnumerationIndex(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetDisplayEnumerationIndex *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_TestPrivateFunctionsDomain 
*/
static NTSTATUS samr_TestPrivateFunctionsDomain(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_TestPrivateFunctionsDomain *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_TestPrivateFunctionsUser 
*/
static NTSTATUS samr_TestPrivateFunctionsUser(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_TestPrivateFunctionsUser *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetUserPwInfo 
*/
static NTSTATUS samr_GetUserPwInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetUserPwInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_RemoveMemberFromForeignDomain 
*/
static NTSTATUS samr_RemoveMemberFromForeignDomain(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_RemoveMemberFromForeignDomain *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryDomainInfo2 
*/
static NTSTATUS samr_QueryDomainInfo2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryDomainInfo2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryUserInfo2 
*/
static NTSTATUS samr_QueryUserInfo2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryUserInfo2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryDisplayInfo2 
*/
static NTSTATUS samr_QueryDisplayInfo2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryDisplayInfo2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetDisplayEnumerationIndex2 
*/
static NTSTATUS samr_GetDisplayEnumerationIndex2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetDisplayEnumerationIndex2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_CreateUser2 
*/
static NTSTATUS samr_CreateUser2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_CreateUser2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_QueryDisplayInfo3 
*/
static NTSTATUS samr_QueryDisplayInfo3(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_QueryDisplayInfo3 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_AddMultipleMembersToAlias 
*/
static NTSTATUS samr_AddMultipleMembersToAlias(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_AddMultipleMembersToAlias *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_RemoveMultipleMembersFromAlias 
*/
static NTSTATUS samr_RemoveMultipleMembersFromAlias(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_RemoveMultipleMembersFromAlias *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_OemChangePasswordUser2 
*/
static NTSTATUS samr_OemChangePasswordUser2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_OemChangePasswordUser2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_ChangePasswordUser2 
*/
static NTSTATUS samr_ChangePasswordUser2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_ChangePasswordUser2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetDomPwInfo 
*/
static NTSTATUS samr_GetDomPwInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetDomPwInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_Connect2 
*/
static NTSTATUS samr_Connect2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			      struct samr_Connect2 *r)
{
	struct samr_Connect c;

	c.in.system_name = NULL;
	c.in.access_mask = r->in.access_mask;
	c.out.handle = r->out.handle;

	return samr_Connect(dce_call, mem_ctx, &c);
}


/* 
  samr_SetUserInfo2 
*/
static NTSTATUS samr_SetUserInfo2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetUserInfo2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_SetBootKeyInformation 
*/
static NTSTATUS samr_SetBootKeyInformation(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetBootKeyInformation *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_GetBootKeyInformation 
*/
static NTSTATUS samr_GetBootKeyInformation(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_GetBootKeyInformation *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_Connect3 
*/
static NTSTATUS samr_Connect3(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_Connect3 *r)
{
	struct samr_Connect c;

	c.in.system_name = NULL;
	c.in.access_mask = r->in.access_mask;
	c.out.handle = r->out.handle;

	return samr_Connect(dce_call, mem_ctx, &c);
}


/* 
  samr_Connect4 
*/
static NTSTATUS samr_Connect4(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_Connect4 *r)
{
	struct samr_Connect c;

	c.in.system_name = NULL;
	c.in.access_mask = r->in.access_mask;
	c.out.handle = r->out.handle;

	return samr_Connect(dce_call, mem_ctx, &c);
}


/* 
  samr_ChangePasswordUser3 
*/
static NTSTATUS samr_ChangePasswordUser3(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_ChangePasswordUser3 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_Connect5 
*/
static NTSTATUS samr_Connect5(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			      struct samr_Connect5 *r)
{
	struct samr_Connect c;
	NTSTATUS status;

	c.in.system_name = NULL;
	c.in.access_mask = r->in.access_mask;
	c.out.handle = r->out.handle;

	status = samr_Connect(dce_call, mem_ctx, &c);

	r->out.info->info1.unknown1 = 3;
	r->out.info->info1.unknown2 = 0;
	r->out.level = r->in.level;

	return status;
}


/* 
  samr_RidToSid 
*/
static NTSTATUS samr_RidToSid(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_RidToSid *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_SetDsrmPassword 
*/
static NTSTATUS samr_SetDsrmPassword(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_SetDsrmPassword *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  samr_ValidatePassword 
*/
static NTSTATUS samr_ValidatePassword(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct samr_ValidatePassword *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_samr_s.c"
