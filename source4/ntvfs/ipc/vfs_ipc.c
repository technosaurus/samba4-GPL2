/* 
   Unix SMB/CIFS implementation.
   default IPC$ NTVFS backend

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004

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
/*
  this implements the IPC$ backend, called by the NTVFS subsystem to
  handle requests on IPC$ shares
*/


#include "includes.h"

/* this is the private structure used to keep the state of an open
   ipc$ connection. It needs to keep information about all open
   pipes */
struct ipc_private {

	uint16_t next_fnum;
	uint16_t num_open;

	/* a list of open pipes */
	struct pipe_state {
		struct pipe_state *next, *prev;
		TALLOC_CTX *mem_ctx;
		const char *pipe_name;
		uint16_t fnum;
		struct dcesrv_connection *dce_conn;
		uint16_t ipc_state;
	} *pipe_list;

};


/*
  find the next fnum available on this connection
*/
static uint16_t find_next_fnum(struct ipc_private *ipc)
{
	struct pipe_state *p;
	uint32_t ret;

	if (ipc->num_open == 0xFFFF) {
		return 0;
	}

again:
	ret = ipc->next_fnum++;

	for (p=ipc->pipe_list; p; p=p->next) {
		if (p->fnum == ret) {
			goto again;
		}
	}

	return ret;
}


/*
  shutdown a single pipe. Called on a close or disconnect
*/
static void pipe_shutdown(struct ipc_private *private, struct pipe_state *p)
{
	TALLOC_CTX *mem_ctx = private->pipe_list->mem_ctx;
	dcesrv_endpoint_disconnect(private->pipe_list->dce_conn);
	DLIST_REMOVE(private->pipe_list, private->pipe_list);
	talloc_destroy(mem_ctx);
}


/*
  find a open pipe give a file descriptor
*/
static struct pipe_state *pipe_state_find(struct ipc_private *private, uint16_t fnum)
{
	struct pipe_state *p;
	
	for (p=private->pipe_list; p; p=p->next) {
		if (p->fnum == fnum) {
			return p;
		}
	}

	return NULL;
}


/*
  connect to a share - always works 
*/
static NTSTATUS ipc_connect(struct smbsrv_request *req, const char *sharename, int depth)
{
	struct smbsrv_tcon *tcon = req->tcon;
	struct ipc_private *private;

	tcon->fs_type = talloc_strdup(tcon, "IPC");
	tcon->dev_type = talloc_strdup(tcon, "IPC");

	/* prepare the private state for this connection */
	private = talloc_p(tcon, struct ipc_private);
	if (!private) {
		return NT_STATUS_NO_MEMORY;
	}
	ntvfs_set_private(tcon, depth, private);

	private->pipe_list = NULL;
	private->next_fnum = 1;
	private->num_open = 0;

	return NT_STATUS_OK;
}

/*
  disconnect from a share
*/
static NTSTATUS ipc_disconnect(struct smbsrv_tcon *tcon, int depth)
{
	struct ipc_private *private = tcon->ntvfs_private_list[depth];

	/* close any pipes that are open. Discard any unread data */
	while (private->pipe_list) {
		pipe_shutdown(private, private->pipe_list);
	}

	return NT_STATUS_OK;
}

/*
  delete a file
*/
static NTSTATUS ipc_unlink(struct smbsrv_request *req, struct smb_unlink *unl)
{
	return NT_STATUS_ACCESS_DENIED;
}


/*
  ioctl interface - we don't do any
*/
static NTSTATUS ipc_ioctl(struct smbsrv_request *req, union smb_ioctl *io)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  check if a directory exists
*/
static NTSTATUS ipc_chkpath(struct smbsrv_request *req, struct smb_chkpath *cp)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  return info on a pathname
*/
static NTSTATUS ipc_qpathinfo(struct smbsrv_request *req, union smb_fileinfo *info)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  set info on a pathname
*/
static NTSTATUS ipc_setpathinfo(struct smbsrv_request *req, union smb_setfileinfo *st)
{
	return NT_STATUS_ACCESS_DENIED;
}



/*
  open a file backend - used for MSRPC pipes
*/
static NTSTATUS ipc_open_generic(struct smbsrv_request *req, const char *fname, 
				 struct pipe_state **ps)
{
	struct pipe_state *p;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	struct dcesrv_ep_description ep_description;
	struct auth_session_info *session_info = NULL;
	NTVFS_GET_PRIVATE(ipc_private, private, req);

	mem_ctx = talloc_init("ipc_open '%s'", fname);
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	p = talloc(mem_ctx, sizeof(struct pipe_state));
	if (!p) {
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	p->mem_ctx = mem_ctx;

	p->pipe_name = talloc_strdup(mem_ctx, fname);
	if (!p->pipe_name) {
		talloc_destroy(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	p->fnum = find_next_fnum(private);
	if (p->fnum == 0) {
		talloc_destroy(mem_ctx);
		return NT_STATUS_TOO_MANY_OPENED_FILES;
	}

	while (p->pipe_name[0] == '\\') {
		p->pipe_name++;
	}
	p->ipc_state = 0x5ff;

	/*
	  we're all set, now ask the dcerpc server subsystem to open the 
	  endpoint. At this stage the pipe isn't bound, so we don't
	  know what interface the user actually wants, just that they want
	  one of the interfaces attached to this pipe endpoint.

	  TODO: note that we aren't passing any credentials here. We
	  will need to do that once the credentials infrastructure is
	  finalised for Samba4
	*/

	ep_description.type = ENDPOINT_SMB;
	ep_description.info.smb_pipe = p->pipe_name;

	/* tell the RPC layer the session_info */
	if (req->session) {
		/* The session info is refcount-increased in the 
		   dcesrv_endpoint_search_connect() function */

		session_info = req->session->session_info;
	}

	status = dcesrv_endpoint_search_connect(req->smb_conn->dcesrv, 
						&ep_description, 
						session_info,
						&p->dce_conn);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_destroy(mem_ctx);
		return status;
	}

	private->num_open++;

	DLIST_ADD(private->pipe_list, p);

	*ps = p;

	return NT_STATUS_OK;
}

/*
  open a file with ntcreatex - used for MSRPC pipes
*/
static NTSTATUS ipc_open_ntcreatex(struct smbsrv_request *req, union smb_open *oi)
{
	struct pipe_state *p;
	NTSTATUS status;

	status = ipc_open_generic(req, oi->ntcreatex.in.fname, &p);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ZERO_STRUCT(oi->ntcreatex.out);
	oi->ntcreatex.out.fnum = p->fnum;
	oi->ntcreatex.out.ipc_state = p->ipc_state;

	return status;
}

/*
  open a file with openx - used for MSRPC pipes
*/
static NTSTATUS ipc_open_openx(struct smbsrv_request *req, union smb_open *oi)
{
	struct pipe_state *p;
	NTSTATUS status;
	const char *fname = oi->openx.in.fname;

	if (strncasecmp(fname, "PIPE\\", 5) != 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	fname += 4;

	status = ipc_open_generic(req, fname, &p);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ZERO_STRUCT(oi->openx.out);
	oi->openx.out.fnum = p->fnum;
	oi->openx.out.ftype = 2;
	oi->openx.out.devstate = p->ipc_state;
	
	return status;
}

/*
  open a file - used for MSRPC pipes
*/
static NTSTATUS ipc_open(struct smbsrv_request *req, union smb_open *oi)
{
	NTSTATUS status;

	switch (oi->generic.level) {
	case RAW_OPEN_NTCREATEX:
		status = ipc_open_ntcreatex(req, oi);
		break;
	case RAW_OPEN_OPENX:
		status = ipc_open_openx(req, oi);
		break;
	default:
		status = NT_STATUS_NOT_SUPPORTED;
		break;
	}

	return status;
}

/*
  create a directory
*/
static NTSTATUS ipc_mkdir(struct smbsrv_request *req, union smb_mkdir *md)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  remove a directory
*/
static NTSTATUS ipc_rmdir(struct smbsrv_request *req, struct smb_rmdir *rd)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  rename a set of files
*/
static NTSTATUS ipc_rename(struct smbsrv_request *req, union smb_rename *ren)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  copy a set of files
*/
static NTSTATUS ipc_copy(struct smbsrv_request *req, struct smb_copy *cp)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  read from a file
*/
static NTSTATUS ipc_read(struct smbsrv_request *req, union smb_read *rd)
{
	NTVFS_GET_PRIVATE(ipc_private, private, req);
	DATA_BLOB data;
	uint16_t fnum;
	struct pipe_state *p;
	NTSTATUS status;

	switch (rd->generic.level) {
	case RAW_READ_READ:
		fnum = rd->read.in.fnum;
		data.length = rd->read.in.count;
		data.data = rd->read.out.data;
		break;
	case RAW_READ_READX:
		fnum = rd->readx.in.fnum;
		data.length = rd->readx.in.maxcnt;
		data.data = rd->readx.out.data;
		break;
	default:
		return NT_STATUS_NOT_SUPPORTED;
	}

	p = pipe_state_find(private, fnum);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	status = dcesrv_output_blob(p->dce_conn, &data);
	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	switch (rd->generic.level) {
	case RAW_READ_READ:
		rd->read.out.nread = data.length;
		break;
	case RAW_READ_READX:
		rd->readx.out.remaining = 0;
		rd->readx.out.compaction_mode = 0;
		rd->readx.out.nread = data.length;
		break;
	default:
		return NT_STATUS_NOT_SUPPORTED;
	}

	return status;
}

/*
  write to a file
*/
static NTSTATUS ipc_write(struct smbsrv_request *req, union smb_write *wr)
{
	NTVFS_GET_PRIVATE(ipc_private, private, req);
	DATA_BLOB data;
	uint16_t fnum;
	struct pipe_state *p;
	NTSTATUS status;

	switch (wr->generic.level) {
	case RAW_WRITE_WRITE:
		fnum = wr->write.in.fnum;
		data.data = discard_const_p(void, wr->write.in.data);
		data.length = wr->write.in.count;
		break;

	case RAW_WRITE_WRITEX:
		fnum = wr->writex.in.fnum;
		data.data = discard_const_p(void, wr->writex.in.data);
		data.length = wr->writex.in.count;
		break;

	default:
		return NT_STATUS_NOT_SUPPORTED;
	}

	p = pipe_state_find(private, fnum);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	status = dcesrv_input(p->dce_conn, &data);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	switch (wr->generic.level) {
	case RAW_WRITE_WRITE:
		wr->write.out.nwritten = data.length;
		break;
	case RAW_WRITE_WRITEX:
		wr->writex.out.nwritten = data.length;
		wr->writex.out.remaining = 0;
		break;
	default:
		return NT_STATUS_NOT_SUPPORTED;
	}

	return NT_STATUS_OK;
}

/*
  seek in a file
*/
static NTSTATUS ipc_seek(struct smbsrv_request *req, struct smb_seek *io)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  flush a file
*/
static NTSTATUS ipc_flush(struct smbsrv_request *req, struct smb_flush *io)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  close a file
*/
static NTSTATUS ipc_close(struct smbsrv_request *req, union smb_close *io)
{
	NTVFS_GET_PRIVATE(ipc_private, private, req);
	struct pipe_state *p;

	if (io->generic.level != RAW_CLOSE_CLOSE) {
		return NT_STATUS_ACCESS_DENIED;
	}

	p = pipe_state_find(private, io->close.in.fnum);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	pipe_shutdown(private, p);
	private->num_open--;

	return NT_STATUS_OK;
}

/*
  exit - closing files?
*/
static NTSTATUS ipc_exit(struct smbsrv_request *req)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  lock a byte range
*/
static NTSTATUS ipc_lock(struct smbsrv_request *req, union smb_lock *lck)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  set info on a open file
*/
static NTSTATUS ipc_setfileinfo(struct smbsrv_request *req, union smb_setfileinfo *info)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  query info on a open file
*/
static NTSTATUS ipc_qfileinfo(struct smbsrv_request *req, union smb_fileinfo *info)
{
	return NT_STATUS_ACCESS_DENIED;
}


/*
  return filesystem info
*/
static NTSTATUS ipc_fsinfo(struct smbsrv_request *req, union smb_fsinfo *fs)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  return print queue info
*/
static NTSTATUS ipc_lpq(struct smbsrv_request *req, union smb_lpq *lpq)
{
	return NT_STATUS_ACCESS_DENIED;
}

/* 
   list files in a directory matching a wildcard pattern
*/
NTSTATUS ipc_search_first(struct smbsrv_request *req, union smb_search_first *io,
			  void *search_private, 
			  BOOL (*callback)(void *, union smb_search_data *))
{
	return NT_STATUS_ACCESS_DENIED;
}

/* 
   continue listing files in a directory 
*/
NTSTATUS ipc_search_next(struct smbsrv_request *req, union smb_search_next *io,
			 void *search_private, 
			 BOOL (*callback)(void *, union smb_search_data *))
{
	return NT_STATUS_ACCESS_DENIED;
}

/* 
   end listing files in a directory 
*/
NTSTATUS ipc_search_close(struct smbsrv_request *req, union smb_search_close *io)
{
	return NT_STATUS_ACCESS_DENIED;
}


/* SMBtrans - handle a DCERPC command */
static NTSTATUS ipc_dcerpc_cmd(struct smbsrv_request *req, struct smb_trans2 *trans)
{
	struct pipe_state *p;
	NTVFS_GET_PRIVATE(ipc_private, private, req);
	NTSTATUS status;

	/* the fnum is in setup[1] */
	p = pipe_state_find(private, trans->in.setup[1]);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	trans->out.data = data_blob_talloc(req, NULL, trans->in.max_data);
	if (!trans->out.data.data) {
		return NT_STATUS_NO_MEMORY;
	}

	/* pass the data to the dcerpc server. Note that we don't
	   expect this to fail, and things like NDR faults are not
	   reported at this stage. Those sorts of errors happen in the
	   dcesrv_output stage */
	status = dcesrv_input(p->dce_conn, &trans->in.data);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/*
	  now ask the dcerpc system for some output. This doesn't yet handle
	  async calls. Again, we only expect NT_STATUS_OK. If the call fails then
	  the error is encoded at the dcerpc level
	*/
	status = dcesrv_output_blob(p->dce_conn, &trans->out.data);
	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	trans->out.setup_count = 0;
	trans->out.setup = NULL;
	trans->out.params = data_blob(NULL, 0);

	return status;
}


/* SMBtrans - set named pipe state */
static NTSTATUS ipc_set_nm_pipe_state(struct smbsrv_request *req, struct smb_trans2 *trans)
{
	NTVFS_GET_PRIVATE(ipc_private, private, req);
	struct pipe_state *p;

	/* the fnum is in setup[1] */
	p = pipe_state_find(private, trans->in.setup[1]);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (trans->in.params.length != 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	p->ipc_state = SVAL(trans->in.params.data, 0);

	trans->out.setup_count = 0;
	trans->out.setup = NULL;
	trans->out.params = data_blob(NULL, 0);
	trans->out.data = data_blob(NULL, 0);

	return NT_STATUS_OK;
}


/* SMBtrans - used to provide access to SMB pipes */
static NTSTATUS ipc_trans(struct smbsrv_request *req, struct smb_trans2 *trans)
{
	NTSTATUS status;

	if (strequal(trans->in.trans_name, "\\PIPE\\LANMAN"))
		return ipc_rap_call(req, trans);

       	if (trans->in.setup_count != 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (trans->in.setup[0]) {
	case TRANSACT_SETNAMEDPIPEHANDLESTATE:
		status = ipc_set_nm_pipe_state(req, trans);
		break;
	case TRANSACT_DCERPCCMD:
		status = ipc_dcerpc_cmd(req, trans);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}



/*
  initialialise the IPC backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_ipc_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;

	ZERO_STRUCT(ops);
	
	/* fill in the name and type */
	ops.name = "default";
	ops.type = NTVFS_IPC;

	/* fill in all the operations */
	ops.connect = ipc_connect;
	ops.disconnect = ipc_disconnect;
	ops.unlink = ipc_unlink;
	ops.chkpath = ipc_chkpath;
	ops.qpathinfo = ipc_qpathinfo;
	ops.setpathinfo = ipc_setpathinfo;
	ops.open = ipc_open;
	ops.mkdir = ipc_mkdir;
	ops.rmdir = ipc_rmdir;
	ops.rename = ipc_rename;
	ops.copy = ipc_copy;
	ops.ioctl = ipc_ioctl;
	ops.read = ipc_read;
	ops.write = ipc_write;
	ops.seek = ipc_seek;
	ops.flush = ipc_flush;	
	ops.close = ipc_close;
	ops.exit = ipc_exit;
	ops.lock = ipc_lock;
	ops.setfileinfo = ipc_setfileinfo;
	ops.qfileinfo = ipc_qfileinfo;
	ops.fsinfo = ipc_fsinfo;
	ops.lpq = ipc_lpq;
	ops.search_first = ipc_search_first;
	ops.search_next = ipc_search_next;
	ops.search_close = ipc_search_close;
	ops.trans = ipc_trans;

	/* register ourselves with the NTVFS subsystem. */
	ret = register_backend("ntvfs", &ops);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register IPC backend!\n"));
		return ret;
	}

	return ret;
}
