/* 
   Unix SMB/CIFS implementation.
   test suite for echo rpc operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2005
   Copyright (C) Jelmer Vernooij 2005
   
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
#include "lib/events/events.h"
#include "librpc/gen_ndr/ndr_echo_c.h"


/*
  test the AddOne interface
*/
#define TEST_ADDONE(tctx, value) do { \
	n = i = value; \
	r.in.in_data = n; \
	r.out.out_data = &n; \
	status = dcerpc_echo_AddOne(p, tctx, &r); \
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "AddOne(%d) failed", i)); \
	torture_assert (tctx, n == i+1, talloc_asprintf(tctx, "%d + 1 != %u (should be %u)\n", i, n, i+1)); \
	torture_comment (tctx, "%d + 1 = %u\n", i, n); \
} while(0)

static bool test_addone(struct torture_context *tctx, 
						struct dcerpc_pipe *p)
{
	uint32_t i;
	NTSTATUS status;
	uint32_t n;
	struct echo_AddOne r;

	for (i=0;i<10;i++) {
		TEST_ADDONE(tctx, i);
	}

	TEST_ADDONE(tctx, 0x7FFFFFFE);
	TEST_ADDONE(tctx, 0xFFFFFFFE);
	TEST_ADDONE(tctx, 0xFFFFFFFF);
	TEST_ADDONE(tctx, random() & 0xFFFFFFFF);
	return true;
}

/*
  test the EchoData interface
*/
static bool test_echodata(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	int i;
	NTSTATUS status;
	uint8_t *data_in, *data_out;
	int len;
	struct echo_EchoData r;

	if (torture_setting_bool(tctx, "quick", false) &&
	    (p->conn->flags & DCERPC_DEBUG_VALIDATE_BOTH)) {
		len = 1 + (random() % 500);
	} else {
		len = 1 + (random() % 5000);
	}

	data_in = talloc_size(tctx, len);
	data_out = talloc_size(tctx, len);
	for (i=0;i<len;i++) {
		data_in[i] = i;
	}
	
	r.in.len = len;
	r.in.in_data = data_in;

	status = dcerpc_echo_EchoData(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, 
											"EchoData(%d) failed\n", len));

	data_out = r.out.out_data;

	for (i=0;i<len;i++) {
		if (data_in[i] != data_out[i]) {
			torture_comment(tctx, "Bad data returned for len %d at offset %d\n", 
			       len, i);
			torture_comment(tctx, "in:\n");
			dump_data(0, data_in+i, MIN(len-i, 16));
			torture_comment(tctx, "out:\n");
			dump_data(0, data_out+i, MIN(len-1, 16));
			return false;
		}
	}
	return true;
}

/*
  test the SourceData interface
*/
static bool test_sourcedata(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	int i;
	NTSTATUS status;
	int len;
	struct echo_SourceData r;

	if (torture_setting_bool(tctx, "quick", false) &&
	    (p->conn->flags & DCERPC_DEBUG_VALIDATE_BOTH)) {
		len = 100 + (random() % 500);
	} else {
		len = 200000 + (random() % 5000);
	}

	r.in.len = len;

	status = dcerpc_echo_SourceData(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, 
										"SourceData(%d) failed", len));

	for (i=0;i<len;i++) {
		uint8_t *v = (uint8_t *)r.out.data;
		torture_assert(tctx, v[i] == (i & 0xFF),
			talloc_asprintf(tctx, 
						"bad data 0x%x at %d\n", (uint8_t)r.out.data[i], i));
	}
	return true;
}

/*
  test the SinkData interface
*/
static bool test_sinkdata(struct torture_context *tctx, 
						  struct dcerpc_pipe *p)
{
	int i;
	NTSTATUS status;
	uint8_t *data_in;
	int len;
	struct echo_SinkData r;

	if (torture_setting_bool(tctx, "quick", false) &&
	    (p->conn->flags & DCERPC_DEBUG_VALIDATE_BOTH)) {
		len = 100 + (random() % 5000);
	} else {
		len = 200000 + (random() % 5000);
	}

	data_in = talloc_size(tctx, len);
	for (i=0;i<len;i++) {
		data_in[i] = i+1;
	}

	r.in.len = len;
	r.in.data = data_in;

	status = dcerpc_echo_SinkData(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, 
										"SinkData(%d) failed", 
							   len));

	torture_comment(tctx, "sunk %d bytes\n", len);
	return true;
}


/*
  test the testcall interface
*/
static bool test_testcall(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct echo_TestCall r;
	const char *s = NULL;

	r.in.s1 = "input string";
	r.out.s2 = &s;

	status = dcerpc_echo_TestCall(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "TestCall failed");

	torture_assert_str_equal(tctx, s, "input string", "Didn't receive back same string");

	return true;
}

/*
  test the testcall interface
*/
static bool test_testcall2(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct echo_TestCall2 r;
	int i;

	for (i=1;i<=7;i++) {
		r.in.level = i;
		r.out.info = talloc(tctx, union echo_Info);

		torture_comment(tctx, "Testing TestCall2 level %d\n", i);
		status = dcerpc_echo_TestCall2(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "TestCall2 failed");
	}
	return true;
}

/*
  test the TestSleep interface
*/
static bool test_sleep(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	int i;
	NTSTATUS status;
#define ASYNC_COUNT 3
	struct rpc_request *req[ASYNC_COUNT];
	struct echo_TestSleep r[ASYNC_COUNT];
	BOOL done[ASYNC_COUNT];
	struct timeval snd[ASYNC_COUNT];
	struct timeval rcv[ASYNC_COUNT];
	struct timeval diff[ASYNC_COUNT];
	struct event_context *ctx;
	int total_done = 0;

	if (torture_setting_bool(tctx, "quick", false)) {
		torture_skip(tctx, "TestSleep disabled - use \"torture:quick=no\" to enable\n");
	}
	torture_comment(tctx, "Testing TestSleep - use \"torture:quick=no\" to disable\n");

	for (i=0;i<ASYNC_COUNT;i++) {
		done[i]		= False;
		snd[i]		= timeval_current();
		rcv[i]		= timeval_zero();
		r[i].in.seconds = ASYNC_COUNT-i;
		req[i] = dcerpc_echo_TestSleep_send(p, tctx, &r[i]);
		torture_assert(tctx, req[i], "Failed to send async sleep request\n");
	}

	ctx = dcerpc_event_context(p);
	while (total_done < ASYNC_COUNT) {
		torture_assert(tctx, event_loop_once(ctx) == 0, 
					   "Event context loop failed");
		for (i=0;i<ASYNC_COUNT;i++) {
			if (done[i] == False && req[i]->state == RPC_REQUEST_DONE) {
				int rounded_tdiff;
				total_done++;
				done[i] = True;
				rcv[i]	= timeval_current();
				diff[i]	= timeval_until(&snd[i], &rcv[i]);
				rounded_tdiff = (int)(0.5 + diff[i].tv_sec + (1.0e-6*diff[i].tv_usec));
				status	= dcerpc_ndr_request_recv(req[i]);
				printf("rounded_tdiff=%d\n", rounded_tdiff);
				torture_assert_ntstatus_ok(tctx, status, 
							talloc_asprintf(tctx, "TestSleep(%d) failed", i));
				torture_assert(tctx, r[i].out.result == r[i].in.seconds,
					talloc_asprintf(tctx, "Failed - Asked to sleep for %u seconds (server replied with %u seconds and the reply takes only %u seconds)", 
					       	r[i].out.result, r[i].in.seconds, (uint_t)diff[i].tv_sec));
				torture_assert(tctx, r[i].out.result <= rounded_tdiff, 
					talloc_asprintf(tctx, "Failed - Slept for %u seconds (but reply takes only %u.%06u seconds)", 
						r[i].out.result, (uint_t)diff[i].tv_sec, (uint_t)diff[i].tv_usec));
				if (r[i].out.result+1 == rounded_tdiff) {
					torture_comment(tctx, "Slept for %u seconds (but reply takes %u.%06u seconds - busy server?)\n", 
							r[i].out.result, (uint_t)diff[i].tv_sec, (uint_t)diff[i].tv_usec);
				} else if (r[i].out.result == rounded_tdiff) {
					torture_comment(tctx, "Slept for %u seconds (reply takes %u.%06u seconds - ok)\n", 
							r[i].out.result, (uint_t)diff[i].tv_sec, (uint_t)diff[i].tv_usec);
				} else {
						torture_comment(tctx, "(Failed) - Not async - Slept for %u seconds (but reply takes %u.%06u seconds)", 
							r[i].out.result, (uint_t)diff[i].tv_sec, (uint_t)diff[i].tv_usec);
					/* TODO: let the test fail here, when we support async rpc on ncacn_np */
				}
			}
		}
	}
	return true;
}

/*
  test enum handling
*/
static bool test_enum(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct echo_TestEnum r;
	enum echo_Enum1 v = ECHO_ENUM1;
	struct echo_Enum2 e2;
	union echo_Enum3 e3;

	r.in.foo1 = &v;
	r.in.foo2 = &e2;
	r.in.foo3 = &e3;
	r.out.foo1 = &v;
	r.out.foo2 = &e2;
	r.out.foo3 = &e3;

	e2.e1 = 76;
	e2.e2 = ECHO_ENUM1_32;
	e3.e1 = ECHO_ENUM2;

	status = dcerpc_echo_TestEnum(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "TestEnum failed");
	return true;
}

/*
  test surrounding conformant array handling
*/
static bool test_surrounding(struct torture_context *tctx,
						  struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct echo_TestSurrounding r;

	ZERO_STRUCT(r);
	r.in.data = talloc(tctx, struct echo_Surrounding);

	r.in.data->x = 20;
	r.in.data->surrounding = talloc_zero_array(tctx, uint16_t, r.in.data->x);

	r.out.data = talloc(tctx, struct echo_Surrounding);

	status = dcerpc_echo_TestSurrounding(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "TestSurrounding failed");
	
	torture_assert(tctx, r.out.data->x == 2 * r.in.data->x,
		"TestSurrounding did not make the array twice as large");

	return true;
}

/*
  test multiple levels of pointers
*/
static bool test_doublepointer(struct torture_context *tctx,
							   struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct echo_TestDoublePointer r;
	uint16_t value = 12;
	uint16_t *pvalue = &value;
	uint16_t **ppvalue = &pvalue;

	ZERO_STRUCT(r);
	r.in.data = &ppvalue;

	status = dcerpc_echo_TestDoublePointer(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "TestDoublePointer failed");

	torture_assert_int_equal(tctx, value, r.out.result, 
					"TestDoublePointer did not return original value");
	return true;
}


/*
  test request timeouts
*/
static bool test_timeout(struct torture_context *tctx,
						 struct dcerpc_pipe *p)
{
	NTSTATUS status;
	struct rpc_request *req;
	struct echo_TestSleep r;
	int timeout_saved = p->request_timeout;

	if (torture_setting_bool(tctx, "quick", false)) {
		torture_skip(tctx, "timeout testing disabled - use \"torture:quick=no\" to enable\n");
	}

	torture_comment(tctx, "testing request timeouts\n");
	r.in.seconds = 2;
	p->request_timeout = 1;

	req = dcerpc_echo_TestSleep_send(p, tctx, &r);
	if (!req) {
		torture_comment(tctx, "Failed to send async sleep request\n");
		goto failed;
	}
	req->ignore_timeout = True;

	status	= dcerpc_ndr_request_recv(req);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_IO_TIMEOUT, 
								  "request should have timed out");

	torture_comment(tctx, "testing request destruction\n");
	req = dcerpc_echo_TestSleep_send(p, tctx, &r);
	if (!req) {
		torture_comment(tctx, "Failed to send async sleep request\n");
		goto failed;
	}
	talloc_free(req);

	req = dcerpc_echo_TestSleep_send(p, tctx, &r);
	if (!req) {
		torture_comment(tctx, "Failed to send async sleep request\n");
		goto failed;
	}
	req->ignore_timeout = True;
	status	= dcerpc_ndr_request_recv(req);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_IO_TIMEOUT, 
		"request should have timed out");

	p->request_timeout = timeout_saved;
	
	return test_addone(tctx, p);

failed:
	p->request_timeout = timeout_saved;
	return false;
}


struct torture_suite *torture_rpc_echo(void)
{
	struct torture_suite *suite = torture_suite_create(
		talloc_autofree_context(), "ECHO");
	struct torture_tcase *tcase;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "echo", 
						  &dcerpc_table_rpcecho);

	torture_rpc_tcase_add_test(tcase, "addone", test_addone);
	torture_rpc_tcase_add_test(tcase, "sinkdata", test_sinkdata);
	torture_rpc_tcase_add_test(tcase, "echodata", test_echodata);
	torture_rpc_tcase_add_test(tcase, "sourcedata", test_sourcedata);
	torture_rpc_tcase_add_test(tcase, "testcall", test_testcall);
	torture_rpc_tcase_add_test(tcase, "testcall2", test_testcall2);
	torture_rpc_tcase_add_test(tcase, "enum", test_enum);
	torture_rpc_tcase_add_test(tcase, "surrounding", test_surrounding);
	torture_rpc_tcase_add_test(tcase, "doublepointer", test_doublepointer);
	torture_rpc_tcase_add_test(tcase, "sleep", test_sleep);
	torture_rpc_tcase_add_test(tcase, "timeout", test_timeout);

	return suite;
}
