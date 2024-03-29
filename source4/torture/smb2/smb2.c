/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Jelmer Vernooij 2006
   
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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

#include "torture/torture.h"
#include "torture/smb2/proto.h"
#include "lib/util/dlinklist.h"

static bool wrap_simple_1smb2_test(struct torture_context *torture_ctx,
				   struct torture_tcase *tcase,
				   struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct smb2_tree *);
	bool ret;

	struct smb2_tree *tree1;

	if (!torture_smb2_connection(torture_ctx, &tree1))
		return false;

	fn = test->fn;

	ret = fn(torture_ctx, tree1);

	talloc_free(tree1);

	return ret;
}

_PUBLIC_ struct torture_test *torture_suite_add_1smb2_test(struct torture_suite *suite,
							   const char *name,
							   bool (*run) (struct torture_context *,
									struct smb2_tree *))
{
	struct torture_test *test; 
	struct torture_tcase *tcase;
	
	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_simple_1smb2_test;
	test->fn = run;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}

NTSTATUS torture_smb2_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "SMB2");
	torture_suite_add_simple_test(suite, "CONNECT", torture_smb2_connect);
	torture_suite_add_simple_test(suite, "SCAN", torture_smb2_scan);
	torture_suite_add_simple_test(suite, "SCANGETINFO", torture_smb2_getinfo_scan);
	torture_suite_add_simple_test(suite, "SCANSETINFO", torture_smb2_setinfo_scan);
	torture_suite_add_simple_test(suite, "SCANFIND", torture_smb2_find_scan);
	torture_suite_add_simple_test(suite, "GETINFO", torture_smb2_getinfo);
	torture_suite_add_simple_test(suite, "SETINFO", torture_smb2_setinfo);
	torture_suite_add_simple_test(suite, "FIND", torture_smb2_find);
	torture_suite_add_suite(suite, torture_smb2_lock_init());
	torture_suite_add_simple_test(suite, "NOTIFY", torture_smb2_notify);

	suite->description = talloc_strdup(suite, "SMB2-specific tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
