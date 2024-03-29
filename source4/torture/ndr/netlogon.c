/* 
   Unix SMB/CIFS implementation.
   test suite for netlogon ndr operations

   Copyright (C) Jelmer Vernooij 2007
   
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
#include "torture/ndr/ndr.h"
#include "librpc/gen_ndr/ndr_netlogon.h"

static const uint8_t netrserverauthenticate3_in_data[] = {
  0xb0, 0x2e, 0x0a, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x5c, 0x00, 0x4e, 0x00, 0x41, 0x00,
  0x54, 0x00, 0x49, 0x00, 0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x44, 0x00,
  0x43, 0x00, 0x2e, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2e, 0x00, 0x42, 0x00, 0x41, 0x00, 0x53, 0x00,
  0x45, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0b, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x24, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0a, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x00, 0x00,
  0x68, 0x8e, 0x3c, 0xdf, 0x23, 0x02, 0xb1, 0x51, 0xff, 0xff, 0x07, 0x60
};

static bool netrserverauthenticate3_in_check(struct torture_context *tctx,
											struct netr_ServerAuthenticate3 *r)
{
	uint8_t cred_expected[8] = { 0x68, 0x8e, 0x3c, 0xdf, 0x23, 0x02, 0xb1, 0x51 };
	torture_assert_str_equal(tctx, r->in.server_name, "\\\\NATIVE-DC.NATIVE.BASE", "server name");
	torture_assert_str_equal(tctx, r->in.account_name, "NATIVE-2K$", "account name");
	torture_assert_int_equal(tctx, r->in.secure_channel_type, 2, "secure channel type");
	torture_assert_str_equal(tctx, r->in.computer_name, "NATIVE-2K", "computer name");
	torture_assert_int_equal(tctx, *r->in.negotiate_flags, 0x6007ffff, "negotiate flags");
	torture_assert(tctx, memcmp(cred_expected, r->in.credentials->data, 8) == 0, "credentials");
	return true;
}

static const uint8_t netrserverauthenticate3_out_data[] = {
  0x22, 0x0c, 0x86, 0x8a, 0xe9, 0x92, 0x93, 0xc9, 0xff, 0xff, 0x07, 0x60,
  0x54, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static bool netrserverauthenticate3_out_check(struct torture_context *tctx,
											struct netr_ServerAuthenticate3 *r)
{
	uint8_t cred_expected[8] = { 0x22, 0x0c, 0x86, 0x8a, 0xe9, 0x92, 0x93, 0xc9 };
	torture_assert(tctx, memcmp(cred_expected, r->out.credentials->data, 8) == 0, "credentials");
	torture_assert_int_equal(tctx, *r->out.negotiate_flags, 0x6007ffff, "negotiate flags");
	torture_assert_int_equal(tctx, *r->out.rid, 0x454, "rid");
	torture_assert_ntstatus_ok(tctx, r->out.result, "return code");
	
	return true;
}

static const uint8_t netrserverreqchallenge_in_data[] = {
  0xb0, 0x2e, 0x0a, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x5c, 0x00, 0x4e, 0x00, 0x41, 0x00,
  0x54, 0x00, 0x49, 0x00, 0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x44, 0x00,
  0x43, 0x00, 0x2e, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2e, 0x00, 0x42, 0x00, 0x41, 0x00, 0x53, 0x00,
  0x45, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0a, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00,
  0x56, 0x00, 0x45, 0x00, 0x2d, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x00, 0x00,
  0xa3, 0x2c, 0xa2, 0x95, 0x40, 0xcc, 0xb7, 0xbb
};

static bool netrserverreqchallenge_in_check(struct torture_context *tctx,
											struct netr_ServerReqChallenge *r)
{
	uint8_t cred_expected[8] = { 0xa3, 0x2c, 0xa2, 0x95, 0x40, 0xcc, 0xb7, 0xbb };
	torture_assert_str_equal(tctx, r->in.server_name, "\\\\NATIVE-DC.NATIVE.BASE", "server name");
	torture_assert_str_equal(tctx, r->in.computer_name, "NATIVE-2K", "account name");
	torture_assert(tctx, memcmp(cred_expected, r->in.credentials->data, 8) == 0, "credentials");

	return true;
}

static const uint8_t netrserverreqchallenge_out_data[] = {
  0x22, 0xfc, 0xc1, 0x17, 0xc0, 0xae, 0x27, 0x8e, 0x00, 0x00, 0x00, 0x00
};

static bool netrserverreqchallenge_out_check(struct torture_context *tctx,
											struct netr_ServerReqChallenge *r)
{
	uint8_t cred_expected[8] = { 0x22, 0xfc, 0xc1, 0x17, 0xc0, 0xae, 0x27, 0x8e };
	torture_assert(tctx, memcmp(cred_expected, r->out.credentials->data, 8) == 0, "credentials");
	torture_assert_ntstatus_ok(tctx, r->out.result, "return code");

	return true;
}


struct torture_suite *ndr_netlogon_suite(TALLOC_CTX *ctx)
{
	struct torture_suite *suite = torture_suite_create(ctx, "netlogon");

	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerReqChallenge, netrserverreqchallenge_in_data, NDR_IN, netrserverreqchallenge_in_check );
	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerReqChallenge, netrserverreqchallenge_out_data, NDR_OUT, netrserverreqchallenge_out_check );

	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerAuthenticate3, netrserverauthenticate3_in_data, NDR_IN, netrserverauthenticate3_in_check );
	torture_suite_add_ndr_pull_fn_test(suite, netr_ServerAuthenticate3, netrserverauthenticate3_out_data, NDR_OUT, netrserverauthenticate3_out_check );

	return suite;
}

