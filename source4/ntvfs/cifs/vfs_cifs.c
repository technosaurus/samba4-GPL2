/* 
   Unix SMB/CIFS implementation.

   CIFS-on-CIFS NTVFS filesystem backend

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James J Myers 2003 <myersjj@samba.org>

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
  this implements a CIFS->CIFS NTVFS filesystem backend. 
  
*/

#include "includes.h"

/* this is stored in ntvfs_private */
struct cvfs_private {
	struct cli_tree *tree;
	struct cli_transport *transport;
	struct tcon_context *conn;
	const char *map_calls;
};


/* a structure used to pass information to an async handler */
struct async_info {
	struct request_context *req;
	void *parms;
};

/*
  an idle function to cope with messages from the smbd client while 
  waiting for a reply from the server
  this function won't be needed once all of the cifs backend
  and the core of smbd is converted to use async calls
*/
static void idle_func(struct cli_transport *transport, void *p_private)
{
	struct cvfs_private *private = p_private;
	if (socket_pending(private->conn->smb->socket.fd)) {
		smbd_process_async(private->conn->smb);
	}
}

/*
  a handler for oplock break events from the server - these need to be passed
  along to the client
 */
static BOOL oplock_handler(struct cli_transport *transport, uint16 tid, uint16 fnum, uint8 level, void *p_private)
{
	struct cvfs_private *private = p_private;
	
	DEBUG(5,("vfs_cifs: sending oplock break level %d for fnum %d\n", level, fnum));
	return req_send_oplock_break(private->conn, fnum, level);
}

/*
  a handler for read events on a connection to a backend server
*/
static void cifs_socket_handler(struct event_context *ev, struct fd_event *fde, time_t t, uint16 flags)
{
	struct tcon_context *conn = fde->private;
	struct cvfs_private *private = conn->ntvfs_private;

	DEBUG(5,("cifs_socket_handler event on fd %d\n", fde->fd));

	if (!cli_request_receive_next(private->transport)) {
		/* the connection to our server is dead */
		close_cnum(conn);
	}
}

/*
  connect to a share - used when a tree_connect operation comes in.
*/
static NTSTATUS cvfs_connect(struct request_context *req, const char *sharename)
{
	struct tcon_context *conn = req->conn;
	NTSTATUS status;
	struct cvfs_private *private;
	char *map_calls;
	struct fd_event fde;
	const char *host, *user, *pass, *domain, *remote_share;

	/* Here we need to determine which server to connect to.
	 * For now we use parametric options, type cifs.
	 * Later we will use security=server and auth_server.c.
	 */
	host = lp_parm_string(req->conn->service, "cifs", "server");
	user = lp_parm_string(req->conn->service, "cifs", "user");
	pass = lp_parm_string(req->conn->service, "cifs", "password");
	domain = lp_parm_string(req->conn->service, "cifs", "domain");
	remote_share = lp_parm_string(req->conn->service, "cifs", "share");
	if (!remote_share) {
		remote_share = sharename;
	}

	if (!host || !user || !pass || !domain) {
		DEBUG(1,("CIFS backend: You must supply server, user, password and domain\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	private = talloc(req->conn->mem_ctx, sizeof(struct cvfs_private));
	if (!private) {
		return NT_STATUS_NO_MEMORY;
	}
	ZERO_STRUCTP(private);

	req->conn->ntvfs_private = (void *)private;

	status = cli_tree_full_connection(&private->tree, 
					  "vfs_cifs",
					  host,
					  0,
					  remote_share, "?????",
					  user, domain,
					  pass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	private->transport = private->tree->session->transport;
	private->tree->session->pid = SVAL(req->in.hdr, HDR_PID);
	private->conn = req->conn;

	conn->fs_type = talloc_strdup(conn->mem_ctx, "NTFS");
	conn->dev_type = talloc_strdup(conn->mem_ctx, "A:");
	
	map_calls = lp_parm_string(req->conn->service, "cifs", "map calls");
	if (map_calls) {
		private->map_calls = talloc_strdup(conn->mem_ctx, map_calls);
	}

	/* if we are mapping trans2, then we need to not give a trans2
	   pointer in the operations structure */
	if (private->map_calls && in_list("trans2", private->map_calls, True)) {
		conn->ntvfs_ops->trans2 = NULL;
	}	  

	/* we need to tell the event loop that we wish to receive read events
	   on our SMB connection to the server */
	fde.fd = private->transport->socket->fd;
	fde.flags = EVENT_FD_READ;
	fde.private = req->conn;
	fde.handler = cifs_socket_handler;

	event_add_fd(conn->smb->events, &fde);

	/* we need to receive oplock break requests from the server */
	cli_oplock_handler(private->transport, oplock_handler, private);
	cli_transport_idle_handler(private->transport, idle_func, 100, private);

	return NT_STATUS_OK;
}

/*
  disconnect from a share
*/
static NTSTATUS cvfs_disconnect(struct tcon_context *conn)
{
	struct cvfs_private *private = conn->ntvfs_private;

	event_remove_fd_all(conn->smb->events, private->transport->socket->fd);
	smb_tree_disconnect(private->tree);
	cli_tree_close(private->tree);

	return NT_STATUS_OK;
}

/*
  a handler for simple async replies
  this handler can only be used for functions that don't return any
  parameters (those that just return a status code)
 */
static void async_simple(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = cli_request_simple_recv(c_req);
	req->async.send_fn(req);
}


/* save some typing for the simple functions */
#define ASYNC_RECV_TAIL(io, async_fn) do { \
	if (!c_req) return NT_STATUS_UNSUCCESSFUL; \
	{ \
		struct async_info *async = c_req->async.private; \
		async = talloc(req->mem_ctx, sizeof(*async)); \
		if (!async) return NT_STATUS_NO_MEMORY; \
		async->parms = io; \
		async->req = req; \
		c_req->async.private = async; \
	} \
	c_req->async.fn = async_fn; \
	req->control_flags |= REQ_CONTROL_ASYNC; \
	return NT_STATUS_OK; \
} while (0)

#define SIMPLE_ASYNC_TAIL ASYNC_RECV_TAIL(NULL, async_simple)

/*
  delete a file - the dirtype specifies the file types to include in the search. 
  The name can contain CIFS wildcards, but rarely does (except with OS/2 clients)
*/
static NTSTATUS cvfs_unlink(struct request_context *req, struct smb_unlink *unl)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	/* see if the front end will allow us to perform this
	   function asynchronously.  */
	if (!req->async.send_fn) {
		return smb_raw_unlink(private->tree, unl);
	}

	c_req = smb_raw_unlink_send(private->tree, unl);

	SIMPLE_ASYNC_TAIL;
}

/*
  a handler for async ioctl replies
 */
static void async_ioctl(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_ioctl_recv(c_req, req->mem_ctx, async->parms);
	req->async.send_fn(req);
}

/*
  ioctl interface
*/
static NTSTATUS cvfs_ioctl(struct request_context *req, struct smb_ioctl *io)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	/* see if the front end will allow us to perform this
	   function asynchronously.  */
	if (!req->async.send_fn) {
		return smb_raw_ioctl(private->tree, req->mem_ctx, io);
	}

	c_req = smb_raw_ioctl_send(private->tree, io);

	ASYNC_RECV_TAIL(io, async_ioctl);
}

/*
  check if a directory exists
*/
static NTSTATUS cvfs_chkpath(struct request_context *req, struct smb_chkpath *cp)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_chkpath(private->tree, cp);
	}

	c_req = smb_raw_chkpath_send(private->tree, cp);

	SIMPLE_ASYNC_TAIL;
}

/*
  a handler for async qpathinfo replies
 */
static void async_qpathinfo(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_pathinfo_recv(c_req, req->mem_ctx, async->parms);
	req->async.send_fn(req);
}

/*
  return info on a pathname
*/
static NTSTATUS cvfs_qpathinfo(struct request_context *req, union smb_fileinfo *info)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_pathinfo(private->tree, req->mem_ctx, info);
	}

	c_req = smb_raw_pathinfo_send(private->tree, info);

	ASYNC_RECV_TAIL(info, async_qpathinfo);
}

/*
  a handler for async qfileinfo replies
 */
static void async_qfileinfo(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_fileinfo_recv(c_req, req->mem_ctx, async->parms);
	req->async.send_fn(req);
}

/*
  query info on a open file
*/
static NTSTATUS cvfs_qfileinfo(struct request_context *req, union smb_fileinfo *info)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_fileinfo(private->tree, req->mem_ctx, info);
	}

	c_req = smb_raw_fileinfo_send(private->tree, info);

	ASYNC_RECV_TAIL(info, async_qfileinfo);
}


/*
  set info on a pathname
*/
static NTSTATUS cvfs_setpathinfo(struct request_context *req, union smb_setfileinfo *st)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_setpathinfo(private->tree, st);
	}

	c_req = smb_raw_setpathinfo_send(private->tree, st);

	SIMPLE_ASYNC_TAIL;
}


/*
  a handler for async open replies
 */
static void async_open(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_open_recv(c_req, req->mem_ctx, async->parms);
	req->async.send_fn(req);
}

/*
  open a file
*/
static NTSTATUS cvfs_open(struct request_context *req, union smb_open *io)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (private->map_calls && in_list("open", private->map_calls, True) &&
	    io->generic.level != RAW_OPEN_GENERIC) {
		return ntvfs_map_open(req, io);
	}

	if (!req->async.send_fn) {
		return smb_raw_open(private->tree, req->mem_ctx, io);
	}

	c_req = smb_raw_open_send(private->tree, io);

	ASYNC_RECV_TAIL(io, async_open);
}

/*
  create a directory
*/
static NTSTATUS cvfs_mkdir(struct request_context *req, union smb_mkdir *md)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_mkdir(private->tree, md);
	}

	c_req = smb_raw_mkdir_send(private->tree, md);

	SIMPLE_ASYNC_TAIL;
}

/*
  remove a directory
*/
static NTSTATUS cvfs_rmdir(struct request_context *req, struct smb_rmdir *rd)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_rmdir(private->tree, rd);
	}
	c_req = smb_raw_rmdir_send(private->tree, rd);

	SIMPLE_ASYNC_TAIL;
}

/*
  rename a set of files
*/
static NTSTATUS cvfs_rename(struct request_context *req, union smb_rename *ren)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_rename(private->tree, ren);
	}

	c_req = smb_raw_rename_send(private->tree, ren);

	SIMPLE_ASYNC_TAIL;
}

/*
  copy a set of files
*/
static NTSTATUS cvfs_copy(struct request_context *req, struct smb_copy *cp)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  a handler for async read replies
 */
static void async_read(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_read_recv(c_req, async->parms);
	req->async.send_fn(req);
}

/*
  read from a file
*/
static NTSTATUS cvfs_read(struct request_context *req, union smb_read *rd)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_read(private->tree, rd);
	}

	c_req = smb_raw_read_send(private->tree, rd);

	ASYNC_RECV_TAIL(rd, async_read);
}

/*
  a handler for async write replies
 */
static void async_write(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_write_recv(c_req, async->parms);
	req->async.send_fn(req);
}

/*
  write to a file
*/
static NTSTATUS cvfs_write(struct request_context *req, union smb_write *wr)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_write(private->tree, wr);
	}

	c_req = smb_raw_write_send(private->tree, wr);

	ASYNC_RECV_TAIL(wr, async_write);
}

/*
  seek in a file
*/
static NTSTATUS cvfs_seek(struct request_context *req, struct smb_seek *io)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  flush a file
*/
static NTSTATUS cvfs_flush(struct request_context *req, struct smb_flush *io)
{
	return NT_STATUS_OK;
}

/*
  close a file
*/
static NTSTATUS cvfs_close(struct request_context *req, union smb_close *io)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_close(private->tree, io);
	}

	c_req = smb_raw_close_send(private->tree, io);

	SIMPLE_ASYNC_TAIL;
}

/*
  exit - closing files?
*/
static NTSTATUS cvfs_exit(struct request_context *req)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  lock a byte range
*/
static NTSTATUS cvfs_lock(struct request_context *req, union smb_lock *lck)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_lock(private->tree, lck);
	}

	c_req = smb_raw_lock_send(private->tree, lck);
	SIMPLE_ASYNC_TAIL;
}

/*
  set info on a open file
*/
static NTSTATUS cvfs_setfileinfo(struct request_context *req, 
				 union smb_setfileinfo *info)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_setfileinfo(private->tree, info);
	}
	c_req = smb_raw_setfileinfo_send(private->tree, info);

	SIMPLE_ASYNC_TAIL;
}


/*
  a handler for async fsinfo replies
 */
static void async_fsinfo(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_fsinfo_recv(c_req, req->mem_ctx, async->parms);
	req->async.send_fn(req);
}

/*
  return filesystem space info
*/
static NTSTATUS cvfs_fsinfo(struct request_context *req, union smb_fsinfo *fs)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_fsinfo(private->tree, req->mem_ctx, fs);
	}

	c_req = smb_raw_fsinfo_send(private->tree, req->mem_ctx, fs);

	ASYNC_RECV_TAIL(fs, async_fsinfo);
}

/*
  return print queue info
*/
static NTSTATUS cvfs_lpq(struct request_context *req, union smb_lpq *lpq)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/* 
   list files in a directory matching a wildcard pattern
*/
static NTSTATUS cvfs_search_first(struct request_context *req, union smb_search_first *io, 
				  void *search_private, 
				  BOOL (*callback)(void *, union smb_search_data *))
{
	struct cvfs_private *private = req->conn->ntvfs_private;

	return smb_raw_search_first(private->tree, req->mem_ctx, io, search_private, callback);
}

/* continue a search */
static NTSTATUS cvfs_search_next(struct request_context *req, union smb_search_next *io, 
				 void *search_private, 
				 BOOL (*callback)(void *, union smb_search_data *))
{
	struct cvfs_private *private = req->conn->ntvfs_private;

	return smb_raw_search_next(private->tree, req->mem_ctx, io, search_private, callback);
}

/* close a search */
static NTSTATUS cvfs_search_close(struct request_context *req, union smb_search_close *io)
{
	struct cvfs_private *private = req->conn->ntvfs_private;

	return smb_raw_search_close(private->tree, io);
}

/*
  a handler for async trans2 replies
 */
static void async_trans2(struct cli_request *c_req)
{
	struct async_info *async = c_req->async.private;
	struct request_context *req = async->req;
	req->async.status = smb_raw_trans2_recv(c_req, req->mem_ctx, async->parms);
	req->async.send_fn(req);
}

/* raw trans2 */
static NTSTATUS cvfs_trans2(struct request_context *req, struct smb_trans2 *trans2)
{
	struct cvfs_private *private = req->conn->ntvfs_private;
	struct cli_request *c_req;

	if (!req->async.send_fn) {
		return smb_raw_trans2(private->tree, req->mem_ctx, trans2);
	}

	c_req = smb_raw_trans2_send(private->tree, trans2);

	ASYNC_RECV_TAIL(trans2, async_trans2);
}

/*
  initialise the CIFS->CIFS backend, registering ourselves with the ntvfs subsystem
 */
BOOL cifs_vfs_init(void)
{
	BOOL ret;
	struct ntvfs_ops ops;

	ZERO_STRUCT(ops);
	
	/* fill in all the operations */
	ops.connect = cvfs_connect;
	ops.disconnect = cvfs_disconnect;
	ops.unlink = cvfs_unlink;
	ops.chkpath = cvfs_chkpath;
	ops.qpathinfo = cvfs_qpathinfo;
	ops.setpathinfo = cvfs_setpathinfo;
	ops.open = cvfs_open;
	ops.mkdir = cvfs_mkdir;
	ops.rmdir = cvfs_rmdir;
	ops.rename = cvfs_rename;
	ops.copy = cvfs_copy;
	ops.ioctl = cvfs_ioctl;
	ops.read = cvfs_read;
	ops.write = cvfs_write;
	ops.seek = cvfs_seek;
	ops.flush = cvfs_flush;	
	ops.close = cvfs_close;
	ops.exit = cvfs_exit;
	ops.lock = cvfs_lock;
	ops.setfileinfo = cvfs_setfileinfo;
	ops.qfileinfo = cvfs_qfileinfo;
	ops.fsinfo = cvfs_fsinfo;
	ops.lpq = cvfs_lpq;
	ops.search_first = cvfs_search_first;
	ops.search_next = cvfs_search_next;
	ops.search_close = cvfs_search_close;

	/* only define this one for trans2 testing */
	ops.trans2 = cvfs_trans2;

	/* register ourselves with the NTVFS subsystem. We register under the name 'cifs'. */

	ret = ntvfs_register("cifs", NTVFS_DISK, &ops);

	if (!ret) {
		DEBUG(0,("Failed to register CIFS backend!\n"));
		return False;
	}
	
	return True;
}
