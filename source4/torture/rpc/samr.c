/* 
   Unix SMB/CIFS implementation.
   test suite for samr rpc operations

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

static BOOL test_QueryUserInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
			       struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_QueryUserInfo r;
	uint16 levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
			   11, 12, 13, 14, 16, 17, 20, 21};
	int i;
	BOOL ret = True;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		printf("Testing QueryUserInfo level %u\n", levels[i]);

		r.in.handle = handle;
		r.in.level = levels[i];

		status = dcerpc_samr_QueryUserInfo(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("QueryUserInfo level %u failed - %s\n", 
			       levels[i], nt_errstr(status));
			ret = False;
		}

		NDR_PRINT_UNION_DEBUG(samr_UserInfo, r.in.level, r.out.info);
	}

	return ret;
}

static BOOL test_OpenUser(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
			  struct policy_handle *handle, uint32 rid)
{
	NTSTATUS status;
	struct samr_OpenUser r;
	struct policy_handle acct_handle;

	printf("Testing OpenUser(%u)\n", rid);

	r.in.handle = handle;
	r.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.acct_handle = &acct_handle;

	status = dcerpc_samr_OpenUser(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenUser(%u) failed - %s\n", rid, nt_errstr(status));
		return False;
	}

	if (!test_QueryUserInfo(p, mem_ctx, &acct_handle)) {
		return False;
	}
	
	return True;
}

static BOOL test_EnumDomainUsers(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
				 struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_EnumDomainUsers r;
	uint32 resume_handle=0;
	int i;
	BOOL ret = True;

	printf("Testing EnumDomainUsers\n");

	r.in.handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.acct_flags = 0;
	r.in.max_size = (uint32)-1;
	r.out.resume_handle = &resume_handle;

	status = dcerpc_samr_EnumDomainUsers(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("EnumDomainUsers failed - %s\n", nt_errstr(status));
		return False;
	}
	
	NDR_PRINT_DEBUG(samr_SamArray, r.out.sam);

	if (!r.out.sam) {
		return False;
	}

	for (i=0;i<r.out.sam->count;i++) {
		if (!test_OpenUser(p, mem_ctx, handle, r.out.sam->entries[i].idx)) {
			ret = False;
		}
	}

	return ret;	
}

static BOOL test_EnumDomainGroups(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
				  struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_EnumDomainGroups r;
	uint32 resume_handle=0;

	printf("Testing EnumDomainGroups\n");

	r.in.handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.max_size = (uint32)-1;
	r.out.resume_handle = &resume_handle;

	status = dcerpc_samr_EnumDomainGroups(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("EnumDomainGroups failed - %s\n", nt_errstr(status));
		return False;
	}
	
	NDR_PRINT_DEBUG(samr_SamArray, r.out.sam);

	return True;	
}

static BOOL test_EnumDomainAliases(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
				   struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_EnumDomainAliases r;
	uint32 resume_handle=0;

	printf("Testing EnumDomainAliases\n");

	r.in.handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.max_size = (uint32)-1;
	r.out.resume_handle = &resume_handle;

	status = dcerpc_samr_EnumDomainAliases(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("EnumDomainAliases failed - %s\n", nt_errstr(status));
		return False;
	}
	
	NDR_PRINT_DEBUG(samr_SamArray, r.out.sam);

	return True;	
}

static BOOL test_QueryDomainInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
				 struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_QueryDomainInfo r;
	uint16 levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13};
	int i;
	BOOL ret = True;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		printf("Testing QueryDomainInfo level %u\n", levels[i]);

		r.in.handle = handle;
		r.in.level = levels[i];

		status = dcerpc_samr_QueryDomainInfo(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("QueryDomainInfo level %u failed - %s\n", 
			       r.in.level, nt_errstr(status));
			ret = False;
			continue;
		}

		NDR_PRINT_UNION_DEBUG(samr_DomainInfo, r.in.level, r.out.info);
	}

	return True;	
}

static BOOL test_OpenDomain(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
			    struct policy_handle *handle, struct dom_sid2 *sid)
{
	NTSTATUS status;
	struct samr_OpenDomain r;
	struct policy_handle domain_handle;

	printf("Testing OpenDomain\n");

	r.in.handle = handle;
	r.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;
	r.in.sid = sid;
	r.out.domain_handle = &domain_handle;

	status = dcerpc_samr_OpenDomain(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenDomain failed - %s\n", nt_errstr(status));
		return False;
	}

	if (!test_QueryDomainInfo(p, mem_ctx, &domain_handle)) {
		return False;
	}

	if (!test_EnumDomainUsers(p, mem_ctx, &domain_handle)) {
		return False;
	}

	if (!test_EnumDomainGroups(p, mem_ctx, &domain_handle)) {
		return False;
	}

	if (!test_EnumDomainAliases(p, mem_ctx, &domain_handle)) {
		return False;
	}

	return True;	
}

static BOOL test_LookupDomain(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
			      struct policy_handle *handle, struct samr_Name *domain)
{
	NTSTATUS status;
	struct samr_LookupDomain r;

	printf("Testing LookupDomain(%s)\n", domain->name);

	r.in.handle = handle;
	r.in.domain = domain;

	status = dcerpc_samr_LookupDomain(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupDomain failed - %s\n", nt_errstr(status));
		return False;
	}

	NDR_PRINT_DEBUG(dom_sid2, r.out.sid);

	if (!test_OpenDomain(p, mem_ctx, handle, r.out.sid)) {
		return False;
	}

	return True;	
}


static BOOL test_EnumDomains(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
			     struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_EnumDomains r;
	uint32 resume_handle = 0;
	uint32 num_entries=0;
	int i;
	BOOL ret = True;

	r.in.handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.buf_size = (uint32)-1;
	r.out.resume_handle = &resume_handle;
	r.out.num_entries = &num_entries;

	status = dcerpc_samr_EnumDomains(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("EnumDomains failed - %s\n", nt_errstr(status));
		return False;
	}

	NDR_PRINT_DEBUG(samr_SamArray, r.out.sam);

	if (!r.out.sam) {
		return False;
	}

	for (i=0;i<r.out.sam->count;i++) {
		if (!test_LookupDomain(p, mem_ctx, handle, 
				       &r.out.sam->entries[i].name)) {
			ret = False;
		}
	}

	return ret;
}


static BOOL test_Connect(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
			 struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_Connect r;
	struct samr_Connect4 r4;

	r.in.system_name = 0;
	r.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;
	r.out.handle = handle;

	status = dcerpc_samr_Connect(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Connect failed - %s\n", nt_errstr(status));
		return False;
	}

	r4.in.system_name = "win2003";
	r4.in.unknown = 0;
	r4.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;
	r4.out.handle = handle;

	status = dcerpc_samr_Connect4(p, mem_ctx, &r4);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Connect4 failed - %s\n", nt_errstr(status));
		return False;
	}

	return True;
}


BOOL torture_rpc_samr(int dummy)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	BOOL ret = True;
	struct policy_handle handle;

	mem_ctx = talloc_init("torture_rpc_samr");

	status = torture_rpc_connection(&p, "samr");
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}
	
	if (!test_Connect(p, mem_ctx, &handle)) {
		ret = False;
	}

	if (!test_EnumDomains(p, mem_ctx, &handle)) {
		ret = False;
	}

        torture_rpc_close(p);

	return ret;
}
