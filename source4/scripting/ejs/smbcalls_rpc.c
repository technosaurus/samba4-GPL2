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
#include "librpc/gen_ndr/ndr_echo.h"
#include "lib/cmdline/popt_common.h"

/*
  connect to an rpc server
     example: 
        var conn = new Object();
        status = rpc_connect(conn, "ncacn_ip_tcp:localhost", "rpcecho");
*/
static int ejs_rpc_connect(MprVarHandle eid, int argc, struct MprVar **argv)
{
	const char *binding, *pipe_name;
	const struct dcerpc_interface_table *iface;
	NTSTATUS status;
	struct dcerpc_pipe *p;
	struct MprVar *conn;

	/* validate arguments */
	if (argc != 3 ||
	    argv[0]->type != MPR_TYPE_OBJECT ||
	    argv[1]->type != MPR_TYPE_STRING ||
	    argv[2]->type != MPR_TYPE_STRING) {
		ejsSetErrorMsg(eid, "rpc_connect invalid arguments");
		return -1;
	}

	conn       = argv[0];
	binding    = mprToString(argv[1]);
	pipe_name  = mprToString(argv[2]);

	iface = idl_iface_by_name(pipe_name);
	if (iface == NULL) {
		status = NT_STATUS_OBJECT_NAME_INVALID;
		goto done;
	}

	status = dcerpc_pipe_connect(mprMemCtx(), &p, binding, 
				     iface->uuid, iface->if_version, 
				     cmdline_credentials, NULL);
	if (NT_STATUS_IS_OK(status)) {
		mprSetPtr(conn, "pipe", p);
	}

done:
	ejsSetReturnValue(eid, mprNTSTATUS(status));
	return 0;
}


/*
  make an rpc call
     example:
            status = rpc_call(conn, "echo_AddOne", io);
*/
static int ejs_rpc_call(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct dcerpc_pipe *p;
	struct MprVar *conn, *io;
	const char *call;
	NTSTATUS status;

	if (argc != 3 ||
	    argv[0]->type != MPR_TYPE_OBJECT ||
	    argv[1]->type != MPR_TYPE_STRING ||
	    argv[2]->type != MPR_TYPE_OBJECT) {
		ejsSetErrorMsg(eid, "rpc_call invalid arguments");
		return -1;
	}
	    
	conn = argv[0];
	call = mprToString(argv[1]);
	io   = argv[2];

	p = mprGetPtr(conn, "pipe");
	if (p == NULL) {
		ejsSetErrorMsg(eid, "rpc_call invalid pipe");
		return -1;
	}

	status = NT_STATUS_NOT_IMPLEMENTED;
	ejsSetReturnValue(eid, mprNTSTATUS(status));
	return 0;
}

/*
  setup C functions that be called from ejs
*/
void smb_setup_ejs_rpc(void)
{
	ejsDefineCFunction(-1, "rpc_connect", ejs_rpc_connect, NULL, MPR_VAR_SCRIPT_HANDLE);
	ejsDefineCFunction(-1, "rpc_call", ejs_rpc_call, NULL, MPR_VAR_SCRIPT_HANDLE);
}
