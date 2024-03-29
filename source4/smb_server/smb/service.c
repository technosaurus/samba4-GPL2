/* 
   Unix SMB/CIFS implementation.
   service (connection) handling
   Copyright (C) Andrew Tridgell 1992-2003
   
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
#include "smb_server/smb_server.h"
#include "smbd/service_stream.h"
#include "ntvfs/ntvfs.h"

/****************************************************************************
  Make a connection, given the snum to connect to, and the vuser of the
  connecting user if appropriate.
****************************************************************************/
static NTSTATUS make_connection_scfg(struct smbsrv_request *req,
				     struct share_config *scfg,
				     enum ntvfs_type type,
				     DATA_BLOB password, 
				     const char *dev)
{
	struct smbsrv_tcon *tcon;
	NTSTATUS status;

	tcon = smbsrv_smb_tcon_new(req->smb_conn, scfg->name);
	if (!tcon) {
		DEBUG(0,("Couldn't find free connection.\n"));
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}
	req->tcon = tcon;

	/* init ntvfs function pointers */
	status = ntvfs_init_connection(tcon, scfg, type,
				       req->smb_conn->negotiate.protocol,
				       req->smb_conn->connection->event.ctx,
				       req->smb_conn->connection->msg_ctx,
				       req->smb_conn->connection->server_id,
				       &tcon->ntvfs);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("make_connection_scfg: connection failed for service %s\n", 
			  scfg->name));
		goto failed;
	}

	status = ntvfs_set_oplock_handler(tcon->ntvfs, smbsrv_send_oplock_break, tcon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("make_connection: NTVFS failed to set the oplock handler!\n"));
		goto failed;
	}

	status = ntvfs_set_addr_callbacks(tcon->ntvfs, smbsrv_get_my_addr, smbsrv_get_peer_addr, req->smb_conn);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("make_connection: NTVFS failed to set the addr callbacks!\n"));
		goto failed;
	}

	status = ntvfs_set_handle_callbacks(tcon->ntvfs,
					    smbsrv_handle_create_new,
					    smbsrv_handle_make_valid,
					    smbsrv_handle_destroy,
					    smbsrv_handle_search_by_wire_key,
					    smbsrv_handle_get_wire_key,
					    tcon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("make_connection: NTVFS failed to set the handle callbacks!\n"));
		goto failed;
	}

	req->ntvfs = ntvfs_request_create(req->tcon->ntvfs, req,
					  req->session->session_info,
					  SVAL(req->in.hdr,HDR_PID),
					  req->request_time,
					  req, NULL, 0);
	if (!req->ntvfs) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	/* Invoke NTVFS connection hook */
	status = ntvfs_connect(req->ntvfs, scfg->name);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("make_connection: NTVFS make connection failed!\n"));
		goto failed;
	}

	return NT_STATUS_OK;

failed:
	req->tcon = NULL;
	talloc_free(tcon);
	return status;
}

/****************************************************************************
 Make a connection to a service.
 *
 * @param service 
****************************************************************************/
static NTSTATUS make_connection(struct smbsrv_request *req,
				const char *service, DATA_BLOB password, 
				const char *dev)
{
	NTSTATUS status;
	enum ntvfs_type type;
	const char *type_str;
	struct share_config *scfg;
	const char *sharetype;

	/* the service might be of the form \\SERVER\SHARE. Should we put
	   the server name we get from this somewhere? */
	if (strncmp(service, "\\\\", 2) == 0) {
		char *p = strchr(service+2, '\\');
		if (p) {
			service = p + 1;
		}
	}

	status = share_get_config(req, req->smb_conn->share_context, service, &scfg);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("make_connection: couldn't find service %s\n", service));
		return NT_STATUS_BAD_NETWORK_NAME;
	}

	/* TODO: check the password, when it's share level security! */

	if (!socket_check_access(req->smb_conn->connection->socket, 
				 scfg->name, 
				 share_string_list_option(req, scfg, SHARE_HOSTS_ALLOW), 
				 share_string_list_option(req, scfg, SHARE_HOSTS_DENY))) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* work out what sort of connection this is */
	sharetype = share_string_option(scfg, "type", "DISK");
	if (sharetype && strcmp(sharetype, "IPC") == 0) {
		type = NTVFS_IPC;
		type_str = "IPC";
	} else if (sharetype && strcmp(sharetype, "PRINTER") == 0) {
		type = NTVFS_PRINT;
		type_str = "LPT:";
	} else {
		type = NTVFS_DISK;
		type_str = "A:";
	}

	if (strcmp(dev, "?????") != 0 && strcasecmp(type_str, dev) != 0) {
		/* the client gave us the wrong device type */
		return NT_STATUS_BAD_DEVICE_TYPE;
	}

	return make_connection_scfg(req, scfg, type, password, dev);
}

/*
  backend for tree connect call
*/
NTSTATUS smbsrv_tcon_backend(struct smbsrv_request *req, union smb_tcon *con)
{
	NTSTATUS status;

	if (con->generic.level == RAW_TCON_TCON) {
		DATA_BLOB password;
		password = data_blob_string_const(con->tcon.in.password);

		status = make_connection(req, con->tcon.in.service, password, con->tcon.in.dev);
		
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		con->tcon.out.max_xmit = req->smb_conn->negotiate.max_recv;
		con->tcon.out.tid = req->tcon->tid;

		return status;
	} 

	status = make_connection(req, con->tconx.in.path, con->tconx.in.password, 
				 con->tconx.in.device);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	con->tconx.out.tid = req->tcon->tid;
	con->tconx.out.dev_type = talloc_strdup(req, req->tcon->ntvfs->dev_type);
	con->tconx.out.fs_type = talloc_strdup(req, req->tcon->ntvfs->fs_type);
	con->tconx.out.options = SMB_SUPPORT_SEARCH_BITS | (share_int_option(req->tcon->ntvfs->config, SHARE_CSC_POLICY, SHARE_CSC_POLICY_DEFAULT) << 2);
	if (share_bool_option(req->tcon->ntvfs->config, SHARE_MSDFS_ROOT, SHARE_MSDFS_ROOT_DEFAULT) && lp_host_msdfs()) {
		con->tconx.out.options |= SMB_SHARE_IN_DFS;
	}

	return status;
}
