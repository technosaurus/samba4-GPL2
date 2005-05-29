/* 
   Unix SMB/CIFS implementation.

   Standalone client for ESP scripting.

   Copyright (C) Tim Potter <tpot@samba.org> 2005

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
#include "dynconfig.h"
#include "lib/ejs/ejs.h"

void ejs_exception(const char *reason)
{
	fprintf(stderr, "smbscript exception: %s", reason);
	exit(1);
}

 int main(int argc, const char *argv[])
{
	EjsId eid;
	EjsHandle handle = 0;
	MprVar result;
	char *emsg;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	const char **argv_list = NULL;
	struct MprVar v;
	int i;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <scriptfile>\n", argv[0]);
		exit(1);
	}

	setup_logging(argv[0],DEBUG_STDOUT);

	if (!lp_load(dyn_CONFIGFILE, False, False, False)) {
		fprintf(stderr, "%s: Can't load %s - run testparm to debug it\n",
			argv[0], dyn_CONFIGFILE);
		exit(1);
	}

	mprSetCtx(mem_ctx);

	if (ejsOpen(NULL, NULL, NULL) != 0) {
		fprintf(stderr, "smbscript: ejsOpen(): unable to initialise "
			"EJ subsystem\n");
		exit(1);
	}

	smb_setup_ejs_functions();

	if ((eid = ejsOpenEngine(handle, 0)) == (EjsId)-1) {
		fprintf(stderr, "smbscript: ejsOpenEngine(): unable to "
			"initialise an EJS engine\n");
		exit(1);
	}

	/* setup ARGV[] in the ejs environment */
	for (i=2;i<argc;i++) {
		argv_list = str_list_add(argv_list, argv[i]);
	}
	v = mprList("ARGV", argv_list);
	mprCreateProperty(ejsGetGlobalObject(eid), "ARGV", &v);

	/* run the script */
	if (ejsEvalFile(eid, discard_const_p(char, argv[1]), &result,
			&emsg) == -1) {
		fprintf(stderr, "smbscript: ejsEvalScript(): %s\n", emsg);
		exit(1);
	}

	ejsClose();

	talloc_free(mem_ctx);

	return 0;
}
