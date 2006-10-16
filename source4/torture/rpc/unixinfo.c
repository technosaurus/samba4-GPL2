/* 
   Unix SMB/CIFS implementation.
   test suite for unixinfo rpc operations

   Copyright (C) Volker Lendecke 2005
   
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
#include "torture/torture.h"
#include "torture/rpc/rpc.h"
#include "librpc/gen_ndr/ndr_unixinfo_c.h"
#include "libcli/security/security.h"


/*
  test the SidToUid interface
*/
static BOOL test_sidtouid(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct unixinfo_SidToUid r;
	struct dom_sid *sid;
	
	sid = dom_sid_parse_talloc(mem_ctx, "S-1-5-32-1234-5432");
	r.in.sid = *sid;

	status = dcerpc_unixinfo_SidToUid(p, mem_ctx, &r);
	if (NT_STATUS_EQUAL(NT_STATUS_NONE_MAPPED, status)) {
	} else if (!NT_STATUS_IS_OK(status)) {
		printf("UidToSid failed == %s\n", nt_errstr(status));
		return False;
	}

	return True;
}

/*
  test the UidToSid interface
*/
static bool test_uidtosid(struct torture_context *tctx, 
						  struct dcerpc_pipe *p)
{
	struct unixinfo_UidToSid r;

	r.in.uid = 1000;

	torture_assert_ntstatus_ok(tctx, dcerpc_unixinfo_UidToSid(p, tctx, &r), 
							   "UidToSid failed");

	return true;
}

static bool test_getpwuid(struct torture_context *tctx, 
						  struct dcerpc_pipe *p)
{
	uint64_t uids[512];
	uint32_t num_uids = ARRAY_SIZE(uids);
	uint32_t i;
	struct unixinfo_GetPWUid r;
	NTSTATUS result;

	for (i=0; i<num_uids; i++) {
		uids[i] = i;
	}
	
	r.in.count = &num_uids;
	r.in.uids = uids;
	r.out.count = &num_uids;
	r.out.infos = talloc_array(tctx, struct unixinfo_GetPWUidInfo, num_uids);

	result = dcerpc_unixinfo_GetPWUid(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, result, "GetPWUid failed");
	
	return true;
}

/*
  test the SidToGid interface
*/
static BOOL test_sidtogid(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct unixinfo_SidToGid r;
	struct dom_sid *sid;
	
	sid = dom_sid_parse_talloc(mem_ctx, "S-1-5-32-1234-5432");
	r.in.sid = *sid;

	status = dcerpc_unixinfo_SidToGid(p, mem_ctx, &r);
	if (NT_STATUS_EQUAL(NT_STATUS_NONE_MAPPED, status)) {
	} else if (!NT_STATUS_IS_OK(status)) {
		printf("SidToGid failed == %s\n", nt_errstr(status));
		return False;
	}

	return True;
}

/*
  test the GidToSid interface
*/
static BOOL test_gidtosid(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct unixinfo_GidToSid r;

	r.in.gid = 1000;

	status = dcerpc_unixinfo_GidToSid(p, tctx, &r);
	if (NT_STATUS_EQUAL(NT_STATUS_NO_SUCH_GROUP, status)) {
	} else torture_assert_ntstatus_ok(tctx, status, "GidToSid failed");

	return true;
}

struct torture_suite *torture_rpc_unixinfo(void)
{
	struct torture_suite *suite;
	struct torture_tcase *tcase;

	suite = torture_suite_create(talloc_autofree_context(), "UNIXINFO");
	tcase = torture_suite_add_rpc_iface_tcase(suite, "unixinfo", 
											  &dcerpc_table_unixinfo);

	torture_rpc_tcase_add_test(tcase, "uidtosid", test_uidtosid);
	torture_rpc_tcase_add_test(tcase, "getpwuid", test_getpwuid);
	torture_rpc_tcase_add_test(tcase, "gidtosid", test_gidtosid);

	return suite;
}
