/* 
   Unix SMB/CIFS implementation.

   provide access to randomisation functions

   Copyright (C) Andrew Tridgell 2005
   
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
#include "scripting/ejs/smbcalls.h"
#include "lib/appweb/ejs/ejs.h"
#include "system/passwd.h"

/*
  usage:
      var i = random();
*/
static int ejs_random(MprVarHandle eid, int argc, struct MprVar **argv)
{
	mpr_Return(eid, mprCreateIntegerVar(generate_random()));
	return 0;
}

/*
  usage:
      var s = randpass(len);
*/
static int ejs_randpass(MprVarHandle eid, int argc, struct MprVar **argv)
{
	char *s;
	if (argc != 1 || !mprVarIsNumber(argv[0]->type)) {
		ejsSetErrorMsg(eid, "randpass invalid arguments");
		return -1;
	}
	s = generate_random_str(mprMemCtx(), mprToInt(argv[0]));
	mpr_Return(eid, mprString(s));
	talloc_free(s);
	return 0;
}

/*
  usage:
      var guid = randguid();
*/
static int ejs_randguid(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct GUID guid = GUID_random();
	char *s = GUID_string(mprMemCtx(), &guid);
	mpr_Return(eid, mprString(s));
	talloc_free(s);
	return 0;
}

/*
  usage:
      var sid = randsid();
*/
static int ejs_randsid(MprVarHandle eid, int argc, struct MprVar **argv)
{
	char *s = talloc_asprintf(mprMemCtx(), "S-1-5-21-%8u-%8u-%8u", 
				  (unsigned)generate_random(), 
				  (unsigned)generate_random(), 
				  (unsigned)generate_random());
	mpr_Return(eid, mprString(s));
	talloc_free(s);
	return 0;
}

/*
  setup C functions that be called from ejs
*/
void smb_setup_ejs_random(void)
{
	ejsDefineCFunction(-1, "random", ejs_random, NULL, MPR_VAR_SCRIPT_HANDLE);
	ejsDefineCFunction(-1, "randpass", ejs_randpass, NULL, MPR_VAR_SCRIPT_HANDLE);
	ejsDefineCFunction(-1, "randguid", ejs_randguid, NULL, MPR_VAR_SCRIPT_HANDLE);
	ejsDefineCFunction(-1, "randsid", ejs_randsid, NULL, MPR_VAR_SCRIPT_HANDLE);
}
