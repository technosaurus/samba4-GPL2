/* 
   Unix SMB/CIFS implementation.

   Running object table functions

   Copyright (C) Jelmer Vernooij 2004
   
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
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/common/common.h"
#include "rpc_server/dcom/dcom.h"

struct dcom_object *dcom_object_by_oid(struct GUID *oid)
{
	/* FIXME */
	return NULL;
}

struct dcom_class *dcom_class_by_clsid(struct GUID *clsid)
{
	/* FIXME */
	return NULL;
}

struct dcom_object *dcom_call_get_object(struct dcesrv_call_state *call)
{
	struct GUID *object;

	if (! (call->pkt.pfc_flags & DCERPC_PFC_FLAG_ORPC) ) {
		return NULL;
	}
	
	object = &call->pkt.u.request.object.object;
	/* FIXME */
	return NULL; 
}
