/* 
   Unix SMB/CIFS implementation.

   provide access to system functions

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
#include "system/time.h"

/*
  return the list of configured network interfaces
*/
static int ejs_sys_interfaces(MprVarHandle eid, int argc, struct MprVar **argv)
{
	int i, count = iface_count();
	struct MprVar ret = mprObject("interfaces");
	for (i=0;i<count;i++) {
		mprAddArray(&ret, i, mprString(iface_n_ip(i)));
	}
	mpr_Return(eid, ret);
	return 0;	
}

/*
  return the hostname from gethostname()
*/
static int ejs_sys_hostname(MprVarHandle eid, int argc, struct MprVar **argv)
{
	char name[200];
	if (gethostname(name, sizeof(name)-1) == -1) {
		ejsSetErrorMsg(eid, "gethostname failed - %s", strerror(errno));
		return -1;
	}
	mpr_Return(eid, mprString(name));
	return 0;	
}


/*
  return current time as a 64 bit nttime value
*/
static int ejs_sys_nttime(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct timeval tv = timeval_current();
	struct MprVar v = mprCreateNumberVar(timeval_to_nttime(&tv));
	mpr_Return(eid, v);
	return 0;
}

/*
  return the given time as a gmtime structure
*/
static int ejs_sys_gmtime(MprVarHandle eid, int argc, struct MprVar **argv)
{
	time_t t;
	struct MprVar ret;
	struct tm *tm;
	if (argc != 1 || !mprVarIsNumber(argv[0]->type)) {
		ejsSetErrorMsg(eid, "sys_gmtime invalid arguments");
		return -1;
	}
	t = nt_time_to_unix(mprVarToNumber(argv[0]));
	tm = gmtime(&t);
	if (tm == NULL) {
		mpr_Return(eid, mprCreateUndefinedVar());
		return 0;
	}
	ret = mprObject("gmtime");
#define TM_EL(n) mprSetVar(&ret, #n, mprCreateIntegerVar(tm->n))
	TM_EL(tm_sec);
	TM_EL(tm_min);
	TM_EL(tm_hour);
	TM_EL(tm_mday);
	TM_EL(tm_mon);
	TM_EL(tm_year);
	TM_EL(tm_wday);
	TM_EL(tm_yday);
	TM_EL(tm_isdst);

	mpr_Return(eid, ret);
	return 0;
}

/*
  return a ldap time string from a nttime
*/
static int ejs_sys_ldaptime(MprVarHandle eid, int argc, struct MprVar **argv)
{
	char *s;
	time_t t;
	if (argc != 1 || !mprVarIsNumber(argv[0]->type)) {
		ejsSetErrorMsg(eid, "sys_ldaptime invalid arguments");
		return -1;
	}
	t = nt_time_to_unix(mprVarToNumber(argv[0]));
	s = ldap_timestring(mprMemCtx(), t);
	mpr_Return(eid, mprString(s));
	talloc_free(s);
	return 0;
}

/*
  unlink a file
   ok = unlink(fname);
*/
static int ejs_sys_unlink(MprVarHandle eid, int argc, char **argv)
{
	int ret;
	if (argc != 1) {
		ejsSetErrorMsg(eid, "sys_unlink invalid arguments");
		return -1;
	}
	ret = unlink(argv[0]);
	mpr_Return(eid, mprCreateBoolVar(ret == 0));
	return 0;
}

/*
  load a file as a string
  usage:
     string = sys_file_load(filename);
*/
static int ejs_sys_file_load(MprVarHandle eid, int argc, char **argv)
{
	char *s;
	if (argc != 1) {
		ejsSetErrorMsg(eid, "sys_file_load invalid arguments");
		return -1;
	}

	s = file_load(argv[0], NULL, mprMemCtx());
	mpr_Return(eid, mprString(s));
	talloc_free(s);
	return 0;
}

/*
  save a file from a string
  usage:
     ok = sys_file_save(filename, str);
*/
static int ejs_sys_file_save(MprVarHandle eid, int argc, char **argv)
{
	BOOL ret;
	if (argc != 2) {
		ejsSetErrorMsg(eid, "sys_file_save invalid arguments");
		return -1;
	}
	ret = file_save(argv[0], argv[1], strlen(argv[1]));
	mpr_Return(eid, mprCreateBoolVar(ret));
	return 0;
}


/*
  initialise sys ejs subsystem
*/
static int ejs_sys_init(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct MprVar obj = mprObject("sys");

	mprSetCFunction(&obj, "interfaces", ejs_sys_interfaces);
	mprSetCFunction(&obj, "hostname", ejs_sys_hostname);
	mprSetCFunction(&obj, "nttime", ejs_sys_nttime);
	mprSetCFunction(&obj, "gmtime", ejs_sys_gmtime);
	mprSetCFunction(&obj, "ldaptime", ejs_sys_ldaptime);
	mprSetStringCFunction(&obj, "unlink", ejs_sys_unlink);
	mprSetStringCFunction(&obj, "file_load", ejs_sys_file_load);
	mprSetStringCFunction(&obj, "file_save", ejs_sys_file_save);

	mpr_Return(eid, obj);
	return 0;
}


/*
  setup C functions that be called from ejs
*/
void smb_setup_ejs_system(void)
{
	ejsDefineCFunction(-1, "sys_init", ejs_sys_init, NULL, MPR_VAR_SCRIPT_HANDLE);
}
