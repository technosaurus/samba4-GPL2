/* 
   Unix SMB/CIFS implementation.

   provide access to string functions

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
      var len = strlen(str);
*/
static int ejs_strlen(MprVarHandle eid, int argc, char **argv)
{
	if (argc != 1) {
		ejsSetErrorMsg(eid, "strlen invalid arguments");
		return -1;
	}
	mpr_Return(eid, mprCreateIntegerVar(strlen_m(argv[0])));
	return 0;
}

/*
  usage:
      var s = strlower("UPPER");
*/
static int ejs_strlower(MprVarHandle eid, int argc, char **argv)
{
	char *s;
	if (argc != 1) {
		ejsSetErrorMsg(eid, "strlower invalid arguments");
		return -1;
	}
	s = strlower_talloc(mprMemCtx(), argv[0]);
	mpr_Return(eid, mprString(s));
	talloc_free(s);
	return 0;
}

/*
  usage:
      var s = strupper("lower");
*/
static int ejs_strupper(MprVarHandle eid, int argc, char **argv)
{
	char *s;
	if (argc != 1) {
		ejsSetErrorMsg(eid, "strupper invalid arguments");
		return -1;
	}
	s = strupper_talloc(mprMemCtx(), argv[0]);
	mpr_Return(eid, mprString(s));
	talloc_free(s);
	return 0;
}

/*
  usage:
     list = split(".", "a.foo.bar");

  NOTE: does not take a regular expression, unlink perl split()
*/
static int ejs_split(MprVarHandle eid, int argc, char **argv)
{
	const char *separator;
	char *s, *p;
	struct MprVar ret;
	int count = 0;
	TALLOC_CTX *tmp_ctx = talloc_new(mprMemCtx());
	if (argc != 2) {
		ejsSetErrorMsg(eid, "split invalid arguments");
		return -1;
	}
	separator = argv[0];
	s = argv[1];

	ret = mprObject("list");

	while ((p = strstr(s, separator))) {
		char *s2 = talloc_strndup(tmp_ctx, s, (int)(p-s));
		mprAddArray(&ret, count++, mprString(s2));
		talloc_free(s2);
		s = p + strlen(separator);
	}
	if (*s) {
		mprAddArray(&ret, count++, mprString(s));
	}
	talloc_free(tmp_ctx);
	mpr_Return(eid, ret);
	return 0;
}


/*
  usage:
     str = join("DC=", list);
*/
static int ejs_join(MprVarHandle eid, int argc, struct MprVar **argv)
{
	int i;
	const char *separator;
	char *ret = NULL;
	const char **list;
	TALLOC_CTX *tmp_ctx = talloc_new(mprMemCtx());
	if (argc != 2 ||
	    argv[0]->type != MPR_TYPE_STRING ||
	    argv[1]->type != MPR_TYPE_OBJECT) {
		ejsSetErrorMsg(eid, "join invalid arguments");
		return -1;
	}

	separator = mprToString(argv[0]);
	list      = mprToArray(tmp_ctx, argv[1]);

	if (list == NULL || list[0] == NULL) {
		talloc_free(tmp_ctx);
		mpr_Return(eid, mprString(NULL));
		return 0;
	}
	
	ret = talloc_strdup(tmp_ctx, list[0]);
	if (ret == NULL) {
		goto failed;
	}
	for (i=1;list[i];i++) {
		ret = talloc_asprintf_append(ret, "%s%s", separator, list[i]);
		if (ret == NULL) {
			goto failed;
		}
	}
	mpr_Return(eid, mprString(ret));
	talloc_free(tmp_ctx);
	return 0;
failed:
	ejsSetErrorMsg(eid, "out of memory");
	return -1;
}


/*
  blergh, C certainly makes this hard!
  usage:
     str = sprintf("i=%d s=%7s", 7, "foo");
*/
static int ejs_sprintf(MprVarHandle eid, int argc, struct MprVar **argv)
{
	const char *format;
	const char *p;
	char *ret;
	int a = 1;
	char *(*_asprintf_append)(char *, const char *, ...);
	TALLOC_CTX *tmp_ctx;
	if (argc < 1 || argv[0]->type != MPR_TYPE_STRING) {
		ejsSetErrorMsg(eid, "sprintf invalid arguments");
		return -1;
	}
	format = mprToString(argv[0]);
	tmp_ctx = talloc_new(mprMemCtx());
	ret = talloc_strdup(tmp_ctx, "");

	/* avoid all the format string warnings */
	_asprintf_append = talloc_asprintf_append;

	/*
	  hackity hack ...
	*/
	while ((p = strchr(format, '%'))) {
		char *fmt2;
		int len, len_count=0;
		char *tstr;
		ret = talloc_asprintf_append(ret, "%*.*s", 
					     (int)(p-format), (int)(p-format), 
					     format);
		if (ret == NULL) goto failed;
		format += (int)(p-format);
		len = strcspn(p+1, "dxuiofgGpXeEFcs%") + 1;
		fmt2 = talloc_strndup(tmp_ctx, p, len+1);
		if (fmt2 == NULL) goto failed;
		len_count = count_chars(fmt2, '*');
		/* find the type string */
		tstr = &fmt2[len];
		while (tstr > fmt2 && isalpha((unsigned char)tstr[-1])) {
			tstr--;
		}
		if (strcmp(tstr, "%") == 0) {
			ret = talloc_asprintf_append(ret, "%%");
			if (ret == NULL) {
				goto failed;
			}
			format += len+1;
			continue;
		}
		if (len_count > 2 || 
		    argc < a + len_count + 1) {
			ejsSetErrorMsg(eid, "sprintf: not enough arguments for format");
			goto failed;
		}
#define FMT_ARG(fn, type) do { \
			switch (len_count) { \
			case 0: \
				ret = _asprintf_append(ret, fmt2, \
							     (type)fn(argv[a])); \
				break; \
			case 1: \
				ret = _asprintf_append(ret, fmt2, \
							     (int)mprVarToNumber(argv[a]), \
							     (type)fn(argv[a+1])); \
				break; \
			case 2: \
				ret = _asprintf_append(ret, fmt2, \
							     (int)mprVarToNumber(argv[a]), \
							     (int)mprVarToNumber(argv[a+1]), \
							     (type)fn(argv[a+2])); \
				break; \
			} \
			a += len_count + 1; \
			if (ret == NULL) { \
				goto failed; \
			} \
} while (0)

		if (strcmp(tstr, "s")==0)        FMT_ARG(mprToString,    const char *);
		else if (strcmp(tstr, "c")==0)   FMT_ARG(*mprToString,   char);
		else if (strcmp(tstr, "d")==0)   FMT_ARG(mprVarToNumber, int);
		else if (strcmp(tstr, "ld")==0)  FMT_ARG(mprVarToNumber, long);
		else if (strcmp(tstr, "lld")==0) FMT_ARG(mprVarToNumber, long long);
		else if (strcmp(tstr, "x")==0)   FMT_ARG(mprVarToNumber, int);
		else if (strcmp(tstr, "lx")==0)  FMT_ARG(mprVarToNumber, long);
		else if (strcmp(tstr, "llx")==0) FMT_ARG(mprVarToNumber, long long);
		else if (strcmp(tstr, "X")==0)   FMT_ARG(mprVarToNumber, int);
		else if (strcmp(tstr, "lX")==0)  FMT_ARG(mprVarToNumber, long);
		else if (strcmp(tstr, "llX")==0) FMT_ARG(mprVarToNumber, long long);
		else if (strcmp(tstr, "u")==0)   FMT_ARG(mprVarToNumber, int);
		else if (strcmp(tstr, "lu")==0)  FMT_ARG(mprVarToNumber, long);
		else if (strcmp(tstr, "llu")==0) FMT_ARG(mprVarToNumber, long long);
		else if (strcmp(tstr, "i")==0)   FMT_ARG(mprVarToNumber, int);
		else if (strcmp(tstr, "li")==0)  FMT_ARG(mprVarToNumber, long);
		else if (strcmp(tstr, "lli")==0) FMT_ARG(mprVarToNumber, long long);
		else if (strcmp(tstr, "o")==0)   FMT_ARG(mprVarToNumber, int);
		else if (strcmp(tstr, "lo")==0)  FMT_ARG(mprVarToNumber, long);
		else if (strcmp(tstr, "llo")==0) FMT_ARG(mprVarToNumber, long long);
		else if (strcmp(tstr, "f")==0)   FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "lf")==0)  FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "g")==0)   FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "lg")==0)  FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "e")==0)   FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "le")==0)  FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "E")==0)   FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "lE")==0)  FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "F")==0)   FMT_ARG(mprVarToFloat,  double);
		else if (strcmp(tstr, "lF")==0)  FMT_ARG(mprVarToFloat,  double);
		else {
			ejsSetErrorMsg(eid, "sprintf: unknown format string '%s'", fmt2);
			goto failed;
		}
		format += len+1;
	}

	ret = talloc_asprintf_append(ret, "%s", format);
	mpr_Return(eid, mprString(ret));
	talloc_free(tmp_ctx);
	return 0;	   
	
failed:
	talloc_free(tmp_ctx);
	return -1;
}

/*
  used to build your own print function
     str = vsprintf(args);
*/
static int ejs_vsprintf(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct MprVar **args, *len, *v;
	int i, ret, length;
	if (argc != 1 || argv[0]->type != MPR_TYPE_OBJECT) {
		ejsSetErrorMsg(eid, "vsprintf invalid arguments");
		return -1;
	}
	v = argv[0];
	len = mprGetProperty(v, "length", NULL);
	if (len == NULL) {
		ejsSetErrorMsg(eid, "vsprintf takes an array");
		return -1;
	}
	length = mprToInt(len);
	args = talloc_array(mprMemCtx(), struct MprVar *, length);
	if (args == NULL) {
		return -1;
	}

	for (i=0;i<length;i++) {
		char idx[16];
		mprItoa(i, idx, sizeof(idx));
		args[i] = mprGetProperty(v, idx, NULL);
	}
	
	ret = ejs_sprintf(eid, length, args);
	talloc_free(args);
	return ret;
}

/*
  initialise string ejs subsystem
*/
static int ejs_string_init(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct MprVar *obj = mprInitObject(eid, "string", argc, argv);

	mprSetStringCFunction(obj, "strlen", ejs_strlen);
	mprSetStringCFunction(obj, "strlower", ejs_strlower);
	mprSetStringCFunction(obj, "strupper", ejs_strupper);
	mprSetStringCFunction(obj, "split", ejs_split);
	mprSetCFunction(obj, "join", ejs_join);
	mprSetCFunction(obj, "sprintf", ejs_sprintf);
	mprSetCFunction(obj, "vsprintf", ejs_vsprintf);

	return 0;
}

/*
  setup C functions that be called from ejs
*/
void smb_setup_ejs_string(void)
{
	ejsDefineCFunction(-1, "string_init", ejs_string_init, NULL, MPR_VAR_SCRIPT_HANDLE);
}
