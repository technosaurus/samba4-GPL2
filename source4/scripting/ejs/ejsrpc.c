/* 
   Unix SMB/CIFS implementation.

   provide interfaces to rpc calls from ejs scripts

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
#include "lib/ejs/ejs.h"
#include "scripting/ejs/smbcalls.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "scripting/ejs/ejsrpc.h"

/*
  set the switch var to be used by the next union switch
*/
void ejs_set_switch(struct ejs_rpc *ejs, uint32_t switch_var)
{
	ejs->switch_var = switch_var;
}

/*
  panic in the ejs wrapper code
 */
NTSTATUS ejs_panic(struct ejs_rpc *ejs, const char *why)
{
	ejsSetErrorMsg(ejs->eid, "rpc_call '%s' failed - %s", ejs->callname, why);
	return NT_STATUS_INTERNAL_ERROR;
}

/*
  start the ejs pull process for a structure
*/
NTSTATUS ejs_pull_struct_start(struct ejs_rpc *ejs, struct MprVar **v, const char *name)
{
	return mprGetVar(v, name);
}


/*
  start the ejs push process for a structure
*/
NTSTATUS ejs_push_struct_start(struct ejs_rpc *ejs, struct MprVar **v, const char *name)
{
	NDR_CHECK(mprSetVar(*v, name, mprObject(name)));
	return mprGetVar(v, name);
}

/*
  pull a uint8 from a mpr variable to a C element
*/
NTSTATUS ejs_pull_uint8(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, uint8_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
	
}

NTSTATUS ejs_push_uint8(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, const uint8_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}

/*
  pull a uint16 from a mpr variable to a C element
*/
NTSTATUS ejs_pull_uint16(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, uint16_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
	
}

NTSTATUS ejs_push_uint16(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, const uint16_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}

/*
  pull a uint32 from a mpr variable to a C element
*/
NTSTATUS ejs_pull_uint32(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, uint32_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
}

NTSTATUS ejs_push_uint32(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, const uint32_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}

/*
  pull a int32 from a mpr variable to a C element
*/
NTSTATUS ejs_pull_int32(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, int32_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
}

NTSTATUS ejs_push_int32(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, const int32_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}

NTSTATUS ejs_pull_hyper(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, uint64_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
}

NTSTATUS ejs_push_hyper(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, const uint64_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}

NTSTATUS ejs_pull_dlong(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, uint64_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
}

NTSTATUS ejs_push_dlong(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, const uint64_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}

NTSTATUS ejs_pull_udlong(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, uint64_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
}

NTSTATUS ejs_push_udlong(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, const uint64_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}

NTSTATUS ejs_pull_NTTIME(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, uint64_t *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
}

NTSTATUS ejs_push_NTTIME(struct ejs_rpc *ejs, 
			struct MprVar *v, const char *name, const uint64_t *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}


/*
  pull a enum from a mpr variable to a C element
  a enum is just treating as an unsigned integer at this level
*/
NTSTATUS ejs_pull_enum(struct ejs_rpc *ejs, 
		       struct MprVar *v, const char *name, unsigned *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	*r = mprVarToInteger(v);
	return NT_STATUS_OK;
	
}

NTSTATUS ejs_push_enum(struct ejs_rpc *ejs, 
		       struct MprVar *v, const char *name, const unsigned *r)
{
	return mprSetVar(v, name, mprCreateIntegerVar(*r));
}


/*
  pull a string
*/
NTSTATUS ejs_pull_string(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, const char **s)
{
	NDR_CHECK(mprGetVar(&v, name));
	*s = mprToString(v);
	return NT_STATUS_OK;
}

/*
  push a string
*/
NTSTATUS ejs_push_string(struct ejs_rpc *ejs, 
			 struct MprVar *v, const char *name, const char *s)
{
	return mprSetVar(v, name, mprString(s));
}

/*
  setup a constant int
*/
void ejs_set_constant_int(int eid, const char *name, int value)
{
	struct MprVar *v = ejsGetGlobalObject(eid);
	mprSetVar(v, name, mprCreateIntegerVar(value));
}

/*
  setup a constant string
*/
void ejs_set_constant_string(int eid, const char *name, const char *value)
{
	struct MprVar *v = ejsGetGlobalObject(eid);
	mprSetVar(v, name, mprCreateStringVar(value, False));
}


NTSTATUS ejs_pull_dom_sid(struct ejs_rpc *ejs, 
			  struct MprVar *v, const char *name, struct dom_sid *r)
{
	struct dom_sid *sid;
	NDR_CHECK(mprGetVar(&v, name));
	sid = dom_sid_parse_talloc(ejs, mprToString(v));
	NT_STATUS_HAVE_NO_MEMORY(sid);
	*r = *sid;
	return NT_STATUS_OK;
}

NTSTATUS ejs_push_dom_sid(struct ejs_rpc *ejs, 
			  struct MprVar *v, const char *name, const struct dom_sid *r)
{
	char *sidstr = dom_sid_string(ejs, r);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);
	return mprSetVar(v, name, mprString(sidstr));
}

NTSTATUS ejs_pull_GUID(struct ejs_rpc *ejs, 
		       struct MprVar *v, const char *name, struct GUID *r)
{
	NDR_CHECK(mprGetVar(&v, name));
	return GUID_from_string(mprToString(v), r);
}

NTSTATUS ejs_push_GUID(struct ejs_rpc *ejs, 
		       struct MprVar *v, const char *name, const struct GUID *r)
{
	char *guid = GUID_string(ejs, r);
	NT_STATUS_HAVE_NO_MEMORY(guid);
	return mprSetVar(v, name, mprString(guid));
}

NTSTATUS ejs_push_null(struct ejs_rpc *ejs, struct MprVar *v, const char *name)
{
	return mprSetVar(v, name, mprCreatePtrVar(NULL));
}

BOOL ejs_pull_null(struct ejs_rpc *ejs, struct MprVar *v, const char *name)
{
	NTSTATUS status = mprGetVar(&v, name);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}
	if (v->type == MPR_TYPE_PTR && v->ptr == NULL) {
		return True;
	}
	return False;
}

/*
  pull a lsa_String
*/
NTSTATUS ejs_pull_lsa_String(struct ejs_rpc *ejs, 
			     struct MprVar *v, const char *name, struct lsa_String *r)
{
	return ejs_pull_string(ejs, v, name, &r->string);
}

/*
  push a lsa_String
*/
NTSTATUS ejs_push_lsa_String(struct ejs_rpc *ejs, 
			     struct MprVar *v, const char *name, const struct lsa_String *r)
{
	return ejs_push_string(ejs, v, name, r->string);
}

