/* 
   Unix SMB/CIFS implementation.

   provide hooks into smbd C calls from ejs scripts

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Tim Potter 2005
   
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
#include "lib/appweb/ejs/ejs.h"
#include "scripting/ejs/smbcalls.h"
#include "smb_build.h"

/*
  return the type of a variable
*/
static int ejs_typeof(MprVarHandle eid, int argc, struct MprVar **argv)
{
	const struct {
		MprType type;
		const char *name;
	} types[] = {
		{ MPR_TYPE_UNDEFINED,        "undefined" },
		{ MPR_TYPE_NULL,             "object" },
		{ MPR_TYPE_BOOL,             "boolean" },
		{ MPR_TYPE_CFUNCTION,        "function" },
		{ MPR_TYPE_FLOAT,            "number" },
		{ MPR_TYPE_INT,              "number" },
		{ MPR_TYPE_INT64,            "number" },
		{ MPR_TYPE_OBJECT,           "object" },
		{ MPR_TYPE_FUNCTION,         "function" },
		{ MPR_TYPE_STRING,           "string" },
		{ MPR_TYPE_STRING_CFUNCTION, "function" },
		{ MPR_TYPE_PTR,              "pointer" }
	};
	int i;
	const char *type = NULL;

	if (argc != 1) return -1;
	
	for (i=0;i<ARRAY_SIZE(types);i++) {
		if (argv[0]->type == types[i].type) {
			type = types[i].name;
			break;
		}
	}
	if (type == NULL) return -1;

	mpr_ReturnString(eid, type);
	return 0;
}

/*
  libinclude() allows you to include js files using a search path specified
  in "js include =" in smb.conf. 
*/
static int ejs_libinclude(int eid, int argc, char **argv)
{
	int i, j;
	const char **js_include = lp_js_include();

	if (js_include == NULL || js_include[0] == NULL) {
		ejsSetErrorMsg(eid, "js include path not set");
		return -1;
	}

	for (i = 0; i < argc; i++) {
		const char *script = argv[i];

		for (j=0;js_include[j];j++) {
			char *path;
			path = talloc_asprintf(mprMemCtx(), "%s/%s", js_include[j], script);
			if (path == NULL) {
				return -1;
			}
			if (file_exist(path)) {
				int ret;
				struct MprVar result;
				char *emsg;

				ret = ejsEvalFile(eid, path, &result, &emsg);
				talloc_free(path);
				if (ret < 0) {
					ejsSetErrorMsg(eid, "%s: %s", script, emsg);
					return -1;
				}
				break;
			}
			talloc_free(path);
		}
		if (js_include[j] == NULL) {
			ejsSetErrorMsg(eid, "unable to include '%s'", script);
			return -1;
		}
	}
	return 0;
}


/*
  setup C functions that be called from ejs
*/
void smb_setup_ejs_functions(void)
{
	init_module_fn static_init[] = STATIC_SMBCALLS_MODULES;
	init_module_fn *shared_init;

	smb_setup_ejs_config();
	smb_setup_ejs_ldb();
	smb_setup_ejs_nbt();
	smb_setup_ejs_cli();
	smb_setup_ejs_auth();
	smb_setup_ejs_options();
	smb_setup_ejs_nss();
	smb_setup_ejs_string();
	smb_setup_ejs_random();
	smb_setup_ejs_system();
	smb_setup_ejs_credentials();
	smb_setup_ejs_samba3();
	smb_setup_ejs_param();
	smb_setup_ejs_datablob();
	
	ejsnet_setup();

	shared_init = load_samba_modules(NULL, "ejs");
	
	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);

	ejsDefineCFunction(-1, "typeof", ejs_typeof, NULL, MPR_VAR_SCRIPT_HANDLE);
	ejsDefineStringCFunction(-1, "libinclude", ejs_libinclude, NULL, MPR_VAR_SCRIPT_HANDLE);
}

