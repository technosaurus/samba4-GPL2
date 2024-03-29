/* 
   Unix SMB/CIFS implementation.

   lsa calls for file sharing connections

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
  when dealing with ACLs the file sharing client code needs to
  sometimes make LSA RPC calls. This code provides an easy interface
  for doing those calls.  
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"

struct smblsa_state {
	struct dcerpc_pipe *pipe;
	struct smbcli_tree *ipc_tree;
	struct policy_handle handle;
};

/*
  establish the lsa pipe connection
*/
static NTSTATUS smblsa_connect(struct smbcli_state *cli)
{
	struct smblsa_state *lsa;
	NTSTATUS status;
	struct lsa_OpenPolicy r;
	uint16_t system_name = '\\';
	union smb_tcon tcon;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;

	if (cli->lsa != NULL) {
		return NT_STATUS_OK;
	}

	lsa = talloc(cli, struct smblsa_state);
	if (lsa == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	lsa->ipc_tree = smbcli_tree_init(cli->session, lsa, False);
	if (lsa->ipc_tree == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* connect to IPC$ */
	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = "ipc$";
	tcon.tconx.in.device = "IPC";	
	status = smb_raw_tcon(lsa->ipc_tree, lsa, &tcon);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(lsa);
		return status;
	}
	lsa->ipc_tree->tid = tcon.tconx.out.tid;

	lsa->pipe = dcerpc_pipe_init(lsa, cli->transport->socket->event.ctx);
	if (lsa->pipe == NULL) {
		talloc_free(lsa);
		return NT_STATUS_NO_MEMORY;
	}

	/* open the LSA pipe */
	status = dcerpc_pipe_open_smb(lsa->pipe, lsa->ipc_tree, DCERPC_LSARPC_NAME);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(lsa);
		return status;
	}

	/* bind to the LSA pipe */
	status = dcerpc_bind_auth_none(lsa->pipe, &dcerpc_table_lsarpc);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(lsa);
                return status;
        }


	/* open a lsa policy handle */
	qos.len = 0;
	qos.impersonation_level = 2;
	qos.context_mode = 1;
	qos.effective_only = 0;

	attr.len = 0;
	attr.root_dir = NULL;
	attr.object_name = NULL;
	attr.attributes = 0;
	attr.sec_desc = NULL;
	attr.sec_qos = &qos;

	r.in.system_name = &system_name;
	r.in.attr = &attr;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &lsa->handle;

	status = dcerpc_lsa_OpenPolicy(lsa->pipe, lsa, &r);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(lsa);
		return status;
	}

	cli->lsa = lsa;
	
	return NT_STATUS_OK;
}


/*
  return the set of privileges for the given sid
*/
NTSTATUS smblsa_sid_privileges(struct smbcli_state *cli, struct dom_sid *sid, 
			       TALLOC_CTX *mem_ctx,
			       struct lsa_RightSet *rights)
{
	NTSTATUS status;
	struct lsa_EnumAccountRights r;

	status = smblsa_connect(cli);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r.in.handle = &cli->lsa->handle;
	r.in.sid = sid;
	r.out.rights = rights;

	return dcerpc_lsa_EnumAccountRights(cli->lsa->pipe, mem_ctx, &r);
}


/*
  check if a named sid has a particular named privilege
*/
NTSTATUS smblsa_sid_check_privilege(struct smbcli_state *cli, 
				    const char *sid_str,
				    const char *privilege)
{
	struct lsa_RightSet rights;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = talloc_new(cli);
	struct dom_sid *sid;
	unsigned i;

	sid = dom_sid_parse_talloc(mem_ctx, sid_str);
	if (sid == NULL) {
		talloc_free(mem_ctx);
		return NT_STATUS_INVALID_SID;
	}

	status = smblsa_sid_privileges(cli, sid, mem_ctx, &rights);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return status;
	}

	for (i=0;i<rights.count;i++) {
		if (strcmp(rights.names[i].string, privilege) == 0) {
			talloc_free(mem_ctx);
			return NT_STATUS_OK;
		}
	}

	talloc_free(mem_ctx);
	return NT_STATUS_NOT_FOUND;
}


/*
  lookup a SID, returning its name
*/
NTSTATUS smblsa_lookup_sid(struct smbcli_state *cli, 
			   const char *sid_str,
			   TALLOC_CTX *mem_ctx,
			   const char **name)
{
	struct lsa_LookupSids r;
	struct lsa_TransNameArray names;
	struct lsa_SidArray sids;
	uint32_t count = 1;
	NTSTATUS status;
	struct dom_sid *sid;
	TALLOC_CTX *mem_ctx2 = talloc_new(mem_ctx);

	status = smblsa_connect(cli);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	sid = dom_sid_parse_talloc(mem_ctx2, sid_str);
	if (sid == NULL) {
		return NT_STATUS_INVALID_SID;
	}

	names.count = 0;
	names.names = NULL;

	sids.num_sids = 1;
	sids.sids = talloc(mem_ctx2, struct lsa_SidPtr);
	sids.sids[0].sid = sid;

	r.in.handle = &cli->lsa->handle;
	r.in.sids = &sids;
	r.in.names = &names;
	r.in.level = 1;
	r.in.count = &count;
	r.out.count = &count;
	r.out.names = &names;

	status = dcerpc_lsa_LookupSids(cli->lsa->pipe, mem_ctx2, &r);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx2);
		return status;
	}
	if (names.count != 1) {
		talloc_free(mem_ctx2);
		return NT_STATUS_UNSUCCESSFUL;
	}

	(*name) = talloc_asprintf(mem_ctx, "%s\\%s", 
				  r.out.domains->domains[0].name.string,
				  names.names[0].name.string);

	talloc_free(mem_ctx2);

	return NT_STATUS_OK;	
}

/*
  lookup a name, returning its sid
*/
NTSTATUS smblsa_lookup_name(struct smbcli_state *cli, 
			    const char *name,
			    TALLOC_CTX *mem_ctx,
			    const char **sid_str)
{
	struct lsa_LookupNames r;
	struct lsa_TransSidArray sids;
	struct lsa_String names;
	uint32_t count = 1;
	NTSTATUS status;
	struct dom_sid *sid;
	TALLOC_CTX *mem_ctx2 = talloc_new(mem_ctx);
	uint32_t rid;

	status = smblsa_connect(cli);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	sids.count = 0;
	sids.sids = NULL;

	names.string = name;

	r.in.handle = &cli->lsa->handle;
	r.in.num_names = 1;
	r.in.names = &names;
	r.in.sids = &sids;
	r.in.level = 1;
	r.in.count = &count;
	r.out.count = &count;
	r.out.sids = &sids;

	status = dcerpc_lsa_LookupNames(cli->lsa->pipe, mem_ctx2, &r);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx2);
		return status;
	}
	if (sids.count != 1) {
		talloc_free(mem_ctx2);
		return NT_STATUS_UNSUCCESSFUL;
	}

	sid = r.out.domains->domains[0].sid;
	rid = sids.sids[0].rid;
	
	(*sid_str) = talloc_asprintf(mem_ctx, "%s-%u", 
				     dom_sid_string(mem_ctx2, sid), rid);

	talloc_free(mem_ctx2);

	return NT_STATUS_OK;	
}


/*
  add a set of privileges to the given sid
*/
NTSTATUS smblsa_sid_add_privileges(struct smbcli_state *cli, struct dom_sid *sid, 
				   TALLOC_CTX *mem_ctx,
				   struct lsa_RightSet *rights)
{
	NTSTATUS status;
	struct lsa_AddAccountRights r;

	status = smblsa_connect(cli);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r.in.handle = &cli->lsa->handle;
	r.in.sid = sid;
	r.in.rights = rights;

	return dcerpc_lsa_AddAccountRights(cli->lsa->pipe, mem_ctx, &r);
}

/*
  remove a set of privileges from the given sid
*/
NTSTATUS smblsa_sid_del_privileges(struct smbcli_state *cli, struct dom_sid *sid, 
				   TALLOC_CTX *mem_ctx,
				   struct lsa_RightSet *rights)
{
	NTSTATUS status;
	struct lsa_RemoveAccountRights r;

	status = smblsa_connect(cli);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r.in.handle = &cli->lsa->handle;
	r.in.sid = sid;
	r.in.unknown = 0;
	r.in.rights = rights;

	return dcerpc_lsa_RemoveAccountRights(cli->lsa->pipe, mem_ctx, &r);
}
