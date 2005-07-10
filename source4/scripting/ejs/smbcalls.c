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
#include "lib/ejs/ejs.h"
#include "scripting/ejs/smbcalls.h"

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
		{ MPR_TYPE_PTR,              "C pointer" }
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
  setup C functions that be called from ejs
*/
void smb_setup_ejs_functions(void)
{
	smb_setup_ejs_config();
	smb_setup_ejs_ldb();
	smb_setup_ejs_nbt();
	smb_setup_ejs_cli();
	smb_setup_ejs_rpc();
	smb_setup_ejs_auth();

	ejsDefineCFunction(-1, "typeof", ejs_typeof, NULL, MPR_VAR_SCRIPT_HANDLE);
}

/*
  setup constants that can be used from ejs
*/
void smb_setup_ejs_constants(int eid)
{
	smb_setup_ejs_rpc_constants(eid);
}
