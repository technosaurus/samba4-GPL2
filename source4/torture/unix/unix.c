/*
   UNIX Extensions test registration.

   Copyright (C) 2007 James Peach

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
#include "torture/unix/proto.h"

NTSTATUS torture_unix_init(void)
{
        struct torture_suite *suite =
                torture_suite_create(talloc_autofree_context(), "UNIX");

        suite->description =
                talloc_strdup(suite, "CIFS UNIX extensions tests");

	torture_suite_add_simple_test(suite,
                        "WHOAMI", torture_unix_whoami);
	torture_suite_add_simple_test(suite,
			"INFO2", unix_torture_unix_info2);

        return (torture_register_suite(suite)) ? NT_STATUS_OK
                                        : NT_STATUS_UNSUCCESSFUL;

}
