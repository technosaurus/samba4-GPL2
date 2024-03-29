/* 
   Unix SMB/CIFS implementation.
   test suite for dfs ndr operations

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
#include "librpc/gen_ndr/ndr_dfs.h"

static const uint8_t getmanagerversion_out_data[] = {
  0x04, 0x00, 0x00, 0x00
};

static bool getmanagerversion_out_check(struct torture_context *tctx,
										struct dfs_GetManagerVersion *r)
{
	torture_assert_int_equal(tctx, *r->out.version, 4, "version");
	return true;
}

static const uint8_t enumex_in_data300[] = {
  0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
  0x77, 0x00, 0x32, 0x00, 0x6b, 0x00, 0x33, 0x00, 0x64, 0x00, 0x63, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x2c, 0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  0x38, 0xf5, 0x07, 0x00, 0x2c, 0x01, 0x00, 0x00, 0x2c, 0x01, 0x00, 0x00,
  0x40, 0xf5, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xa8, 0xf5, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00
};

static bool enumex_in_check300(struct torture_context *tctx,
							struct dfs_EnumEx *r)
{
	torture_assert_str_equal(tctx, r->in.dfs_name, "w2k3dc", "dfs name");
	torture_assert_int_equal(tctx, r->in.level, 300, "level");
	torture_assert(tctx, r->in.total != NULL, "total ptr");
	torture_assert_int_equal(tctx, *r->in.total, 0, "total");
	torture_assert_int_equal(tctx, r->in.bufsize, -1, "buf size");
	torture_assert(tctx, r->in.info != NULL, "info ptr");
	torture_assert_int_equal(tctx, r->in.info->level, 300, "info level");
	torture_assert(tctx, r->in.info->e.info300->s == NULL, "info data ptr");
	return true;
}


static const uint8_t enumex_out_data300[] = {
  0x00, 0x00, 0x02, 0x00, 0x2c, 0x01, 0x00, 0x00, 0x2c, 0x01, 0x00, 0x00,
  0x04, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x0c, 0x00, 0x02, 0x00,
  0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00,
  0x14, 0x00, 0x02, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x17, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x57, 0x00, 0x32, 0x00, 0x4b, 0x00,
  0x33, 0x00, 0x44, 0x00, 0x43, 0x00, 0x5c, 0x00, 0x73, 0x00, 0x74, 0x00,
  0x61, 0x00, 0x6e, 0x00, 0x64, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x6f, 0x00,
  0x6e, 0x00, 0x65, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x6f, 0x00, 0x74, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x57, 0x00, 0x32, 0x00, 0x4b, 0x00,
  0x33, 0x00, 0x44, 0x00, 0x43, 0x00, 0x5c, 0x00, 0x73, 0x00, 0x74, 0x00,
  0x61, 0x00, 0x6e, 0x00, 0x64, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x6f, 0x00,
  0x6e, 0x00, 0x65, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x6f, 0x00, 0x74, 0x00,
  0x32, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x57, 0x00, 0x32, 0x00, 0x4b, 0x00,
  0x33, 0x00, 0x44, 0x00, 0x4f, 0x00, 0x4d, 0x00, 0x5c, 0x00, 0x74, 0x00,
  0x65, 0x00, 0x73, 0x00, 0x74, 0x00, 0x64, 0x00, 0x6f, 0x00, 0x6d, 0x00,
  0x61, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x6f, 0x00,
  0x74, 0x00, 0x00, 0x00, 0x18, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

static bool enumex_out_check300(struct torture_context *tctx,
							struct dfs_EnumEx *r)
{
	torture_assert_werr_ok(tctx, r->out.result, "return code");
	torture_assert(tctx, r->out.total != NULL, "total ptr");
	torture_assert_int_equal(tctx, *r->out.total, 3, "total");
	torture_assert(tctx, r->out.info != NULL, "info ptr");
	torture_assert_int_equal(tctx, r->out.info->level, 300, "info level");
	torture_assert(tctx, r->out.info->e.info300 != NULL, "info data ptr");
	torture_assert_int_equal(tctx, r->out.info->e.info300->count, 3, "info enum array");
	torture_assert_str_equal(tctx, r->out.info->e.info300->s[0].dom_root, "\\W2K3DC\\standaloneroot", "info enum array 0");
	torture_assert_int_equal(tctx, r->out.info->e.info300->s[0].flavor, 256, "info enum flavor 0");
	torture_assert_str_equal(tctx, r->out.info->e.info300->s[1].dom_root, "\\W2K3DC\\standaloneroot2", "info enum array 1");
	torture_assert_int_equal(tctx, r->out.info->e.info300->s[1].flavor, 256, "info enum flavor 1");
	torture_assert_str_equal(tctx, r->out.info->e.info300->s[2].dom_root, "\\W2K3DOM\\testdomainroot", "info enum array 2");
	torture_assert_int_equal(tctx, r->out.info->e.info300->s[2].flavor, 512, "info enum flavor 2");
	return true;
}

struct torture_suite *ndr_dfs_suite(TALLOC_CTX *ctx)
{
	struct torture_suite *suite = torture_suite_create(ctx, "dfs");

	torture_suite_add_ndr_pull_fn_test(suite, dfs_GetManagerVersion, getmanagerversion_out_data, NDR_OUT, getmanagerversion_out_check );

	torture_suite_add_ndr_pull_fn_test(suite, dfs_EnumEx, enumex_in_data300, NDR_IN, enumex_in_check300 );
	torture_suite_add_ndr_pull_fn_test(suite, dfs_EnumEx, enumex_out_data300, NDR_OUT, enumex_out_check300 );

	return suite;
}

