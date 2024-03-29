/* 
   Unix SMB/CIFS implementation.

   test suite for behaviour of rpc policy handles

   Copyright (C) Andrew Tridgell 2007
   
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
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "torture/rpc/rpc.h"

/*
  this tests the use of policy handles between connections
*/

static bool test_handles_lsa(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy r;
	struct lsa_Close c;
	uint16_t system_name = '\\';
	TALLOC_CTX *mem_ctx = talloc_new(torture);

	torture_comment(torture, "RPC-HANDLE-LSARPC\n");

	status = torture_rpc_connection(mem_ctx, &p1, &dcerpc_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe1");

	status = torture_rpc_connection(mem_ctx, &p2, &dcerpc_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe1");

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
	r.out.handle = &handle;

	status = dcerpc_lsa_OpenPolicy(p1, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "lsa_OpenPolicy not supported - skipping\n");
		talloc_free(mem_ctx);
		return true;
	}

	c.in.handle = &handle;
	c.out.handle = &handle2;

	status = dcerpc_lsa_Close(p2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p2");
	torture_assert_int_equal(torture, p2->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p2");

	status = dcerpc_lsa_Close(p1, mem_ctx, &c);
	torture_assert_ntstatus_ok(torture, status, "closing policy handle on p1");

	status = dcerpc_lsa_Close(p1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p1 again");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p1 again");
	
	talloc_free(mem_ctx);

	return true;
}

static bool test_handles_lsa_shared(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2, *p3, *p4, *p5;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy r;
	struct lsa_Close c;
	struct lsa_QuerySecurity qsec;
	uint16_t system_name = '\\';
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	enum dcerpc_transport_t transport;
	uint32_t assoc_group_id;

	torture_comment(torture, "RPC-HANDLE-LSARPC-SHARED\n");

	if (lp_parm_bool(-1, "torture", "samba4", False)) {
		torture_comment(torture, "LSA shared-policy-handle test against Samba4 - skipping\n");
		return true;
	}

	torture_comment(torture, "connect lsa pipe1\n");
	status = torture_rpc_connection(mem_ctx, &p1, &dcerpc_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe1");

	transport	= p1->conn->transport.transport,
	assoc_group_id	= p1->assoc_group_id;

	torture_comment(torture, "use assoc_group_id[0x%08X] for new connections\n", assoc_group_id);

	torture_comment(torture, "connect lsa pipe2\n");
	status = torture_rpc_connection_transport(mem_ctx, &p2, &dcerpc_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe2");

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
	r.out.handle = &handle;

	torture_comment(torture, "open lsa policy handle\n");
	status = dcerpc_lsa_OpenPolicy(p1, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "lsa_OpenPolicy not supported - skipping\n");
		talloc_free(mem_ctx);
		return true;
	}

	/*
	 * connect p3 after the policy handle is opened
	 */
	torture_comment(torture, "connect lsa pipe3 after the policy handle is opened\n");
	status = torture_rpc_connection_transport(mem_ctx, &p3, &dcerpc_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe3");

	qsec.in.handle 		= &handle;
	qsec.in.sec_info	= 0;
	c.in.handle = &handle;
	c.out.handle = &handle2;

	/*
	 * use policy handle on all 3 connections
	 */
	torture_comment(torture, "use the policy handle on p1,p2,p3\n");
	status = dcerpc_lsa_QuerySecurity(p1, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p1");

	status = dcerpc_lsa_QuerySecurity(p2, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p2");

	status = dcerpc_lsa_QuerySecurity(p3, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p3");

	/*
	 * close policy handle on connection 2 and the others get a fault
	 */
	torture_comment(torture, "close the policy handle on p2 others get a fault\n");
	status = dcerpc_lsa_Close(p2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "closing policy handle on p2");

	status = dcerpc_lsa_Close(p1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p1 again");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p1 again");

	status = dcerpc_lsa_Close(p3, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p3");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p3");

	status = dcerpc_lsa_Close(p2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p2 again");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p2 again");

	/*
	 * open a new policy handle on p3
	 */
	torture_comment(torture, "open a new policy handle on p3\n");
	status = dcerpc_lsa_OpenPolicy(p3, mem_ctx, &r);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "open policy handle on p3");

	/*
	 * use policy handle on all 3 connections
	 */
	torture_comment(torture, "use the policy handle on p1,p2,p3\n");
	status = dcerpc_lsa_QuerySecurity(p1, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p1");

	status = dcerpc_lsa_QuerySecurity(p2, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p2");

	status = dcerpc_lsa_QuerySecurity(p3, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "use policy handle on p3");

	/*
	 * close policy handle on connection 2 and the others get a fault
	 */
	torture_comment(torture, "close the policy handle on p2 others get a fault\n");
	status = dcerpc_lsa_Close(p2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "closing policy handle on p2");

	status = dcerpc_lsa_Close(p1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p1 again");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p1 again");

	status = dcerpc_lsa_Close(p3, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p3");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p3");

	status = dcerpc_lsa_Close(p2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p2 again");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p2 again");

	/*
	 * open a new policy handle
	 */
	torture_comment(torture, "open a new policy handle on p1 and use it\n");
	status = dcerpc_lsa_OpenPolicy(p1, mem_ctx, &r);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "open 2nd policy handle on p1");

	status = dcerpc_lsa_QuerySecurity(p1, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "QuerySecurity handle on p1");

	/* close first connection */
	torture_comment(torture, "disconnect p1\n");
	talloc_free(p1);
	msleep(5);

	/*
	 * and it's still available on p2,p3
	 */
	torture_comment(torture, "use policy handle on p2,p3\n");
	status = dcerpc_lsa_QuerySecurity(p2, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "QuerySecurity handle on p2 after p1 was disconnected");

	status = dcerpc_lsa_QuerySecurity(p3, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "QuerySecurity handle on p3 after p1 was disconnected");

	/*
	 * now open p4
	 * and use the handle on it
	 */
	torture_comment(torture, "connect lsa pipe4 and use policy handle\n");
	status = torture_rpc_connection_transport(mem_ctx, &p4, &dcerpc_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe4");

	status = dcerpc_lsa_QuerySecurity(p4, mem_ctx, &qsec);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK, 
				      "using policy handle on p4");

	/*
	 * now close p2,p3,p4
	 * without closing the policy handle
	 */
	torture_comment(torture, "disconnect p2,p3,p4\n");
	talloc_free(p2);
	talloc_free(p3);
	talloc_free(p4);
	msleep(10);

	/*
	 * now open p5
	 */
	torture_comment(torture, "connect lsa pipe5 - should fail\n");
	status = torture_rpc_connection_transport(mem_ctx, &p5, &dcerpc_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening lsa pipe5");

	talloc_free(mem_ctx);

	return true;
}


static bool test_handles_samr(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct samr_Connect r;
	struct samr_Close c;
	TALLOC_CTX *mem_ctx = talloc_new(torture);

	torture_comment(torture, "RPC-HANDLE-SAMR\n");

	status = torture_rpc_connection(mem_ctx, &p1, &dcerpc_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe1");

	status = torture_rpc_connection(mem_ctx, &p2, &dcerpc_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe1");

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.connect_handle = &handle;

	status = dcerpc_samr_Connect(p1, mem_ctx, &r);
	torture_assert_ntstatus_ok(torture, status, "opening policy handle on p1");

	c.in.handle = &handle;
	c.out.handle = &handle2;

	status = dcerpc_samr_Close(p2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p2");
	torture_assert_int_equal(torture, p2->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p2");

	status = dcerpc_samr_Close(p1, mem_ctx, &c);
	torture_assert_ntstatus_ok(torture, status, "closing policy handle on p1");

	status = dcerpc_samr_Close(p1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p1 again");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p1 again");
	
	talloc_free(mem_ctx);

	return true;
}

static bool test_handles_mixed_shared(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2, *p3, *p4, *p5, *p6;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct samr_Connect r;
	struct lsa_Close lc;
	struct samr_Close sc;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	enum dcerpc_transport_t transport;
	uint32_t assoc_group_id;

	torture_comment(torture, "RPC-HANDLE-MIXED-SHARED\n");

	if (lp_parm_bool(-1, "torture", "samba4", False)) {
		torture_comment(torture, "Mixed shared-policy-handle test against Samba4 - skipping\n");
		return true;
	}

	torture_comment(torture, "connect samr pipe1\n");
	status = torture_rpc_connection(mem_ctx, &p1, &dcerpc_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe1");

	transport	= p1->conn->transport.transport,
	assoc_group_id	= p1->assoc_group_id;

	torture_comment(torture, "use assoc_group_id[0x%08X] for new connections\n", assoc_group_id);

	torture_comment(torture, "connect lsa pipe2\n");
	status = torture_rpc_connection_transport(mem_ctx, &p2, &dcerpc_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe2");

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.connect_handle = &handle;

	torture_comment(torture, "samr_Connect to open a policy handle on samr p1\n");
	status = dcerpc_samr_Connect(p1, mem_ctx, &r);
	torture_assert_ntstatus_ok(torture, status, "opening policy handle on p1");

	lc.in.handle 		= &handle;
	lc.out.handle		= &handle2;
	sc.in.handle		= &handle;
	sc.out.handle		= &handle2;

	torture_comment(torture, "use policy handle on lsa p2 - should fail\n");
	status = dcerpc_lsa_Close(p2, mem_ctx, &lc);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing handle on lsa p2");
	torture_assert_int_equal(torture, p2->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing handle on lsa p2");

	torture_comment(torture, "closing policy handle on samr p1\n");
	status = dcerpc_samr_Close(p1, mem_ctx, &sc);
	torture_assert_ntstatus_ok(torture, status, "closing policy handle on p1");

	talloc_free(p1);
	talloc_free(p2);
	msleep(10);

	torture_comment(torture, "connect samr pipe3 - should fail\n");
	status = torture_rpc_connection_transport(mem_ctx, &p3, &dcerpc_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe3");

	torture_comment(torture, "connect lsa pipe4 - should fail\n");
	status = torture_rpc_connection_transport(mem_ctx, &p4, &dcerpc_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening lsa pipe4");

	torture_comment(torture, "connect samr pipe5 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(mem_ctx, &p5, &dcerpc_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe5");

	torture_comment(torture, "connect lsa pipe6 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(mem_ctx, &p6, &dcerpc_table_lsarpc,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening lsa pipe6");

	talloc_free(mem_ctx);

	return true;
}

static bool test_handles_random_assoc(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2, *p3;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	enum dcerpc_transport_t transport;
	uint32_t assoc_group_id;

	torture_comment(torture, "RPC-HANDLE-RANDOM-ASSOC\n");

	torture_comment(torture, "connect samr pipe1\n");
	status = torture_rpc_connection(mem_ctx, &p1, &dcerpc_table_samr);
	torture_assert_ntstatus_ok(torture, status, "opening samr pipe1");

	transport	= p1->conn->transport.transport,
	assoc_group_id	= p1->assoc_group_id;

	torture_comment(torture, "pip1 use assoc_group_id[0x%08X]\n", assoc_group_id);

	torture_comment(torture, "connect samr pipe2 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(mem_ctx, &p2, &dcerpc_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe2");

	torture_comment(torture, "connect samr pipe3 with assoc_group_id[0x%08X]- should fail\n", ++assoc_group_id);
	status = torture_rpc_connection_transport(mem_ctx, &p3, &dcerpc_table_samr,
						  transport,
						  assoc_group_id);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_UNSUCCESSFUL,
				      "opening samr pipe3");

	talloc_free(mem_ctx);

	return true;
}


static bool test_handles_drsuapi(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2;
	struct policy_handle handle;
	struct policy_handle handle2;
	struct GUID bind_guid;
	struct drsuapi_DsBind r;
	struct drsuapi_DsUnbind c;
	TALLOC_CTX *mem_ctx = talloc_new(torture);

	torture_comment(torture, "RPC-HANDLE-DRSUAPI\n");

	status = torture_rpc_connection(mem_ctx, &p1, &dcerpc_table_drsuapi);
	torture_assert_ntstatus_ok(torture, status, "opening drsuapi pipe1");

	status = torture_rpc_connection(mem_ctx, &p2, &dcerpc_table_drsuapi);
	torture_assert_ntstatus_ok(torture, status, "opening drsuapi pipe1");

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &bind_guid);

	r.in.bind_guid = &bind_guid;
	r.in.bind_info = NULL;
	r.out.bind_handle = &handle;

	status = dcerpc_drsuapi_DsBind(p1, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "drsuapi_DsBind not supported - skipping\n");
		talloc_free(mem_ctx);
		return true;
	}

	c.in.bind_handle = &handle;
	c.out.bind_handle = &handle2;

	status = dcerpc_drsuapi_DsUnbind(p2, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p2");
	torture_assert_int_equal(torture, p2->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p2");

	status = dcerpc_drsuapi_DsUnbind(p1, mem_ctx, &c);
	torture_assert_ntstatus_ok(torture, status, "closing policy handle on p1");

	status = dcerpc_drsuapi_DsUnbind(p1, mem_ctx, &c);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_NET_WRITE_FAULT, 
				      "closing policy handle on p1 again");
	torture_assert_int_equal(torture, p1->last_fault_code, DCERPC_FAULT_CONTEXT_MISMATCH, 
				      "closing policy handle on p1 again");
	
	talloc_free(mem_ctx);

	return true;
}


struct torture_suite *torture_rpc_handles(void)
{
	struct torture_suite *suite;

	suite = torture_suite_create(talloc_autofree_context(), "HANDLES");
	torture_suite_add_simple_test(suite, "lsarpc", test_handles_lsa);
	torture_suite_add_simple_test(suite, "lsarpc-shared", test_handles_lsa_shared);
	torture_suite_add_simple_test(suite, "samr", test_handles_samr);
	torture_suite_add_simple_test(suite, "mixed-shared", test_handles_mixed_shared);
	torture_suite_add_simple_test(suite, "random-assoc", test_handles_random_assoc);
	torture_suite_add_simple_test(suite, "drsuapi", test_handles_drsuapi);
	return suite;
}
