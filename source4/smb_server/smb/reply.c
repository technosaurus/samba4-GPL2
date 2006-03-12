/* 
   Unix SMB/CIFS implementation.
   Main SMB reply routines
   Copyright (C) Andrew Tridgell 1992-2003
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
   This file handles most of the reply_ calls that the server
   makes to handle specific protocols
*/

#include "includes.h"
#include "smb_server/smb_server.h"
#include "ntvfs/ntvfs.h"


/* useful way of catching wct errors with file and line number */
#define REQ_CHECK_WCT(req, wcount) do { \
	if ((req)->in.wct != (wcount)) { \
		DEBUG(1,("Unexpected WCT %d at %s(%d) - expected %d\n", \
			 (req)->in.wct, __FILE__, __LINE__, wcount)); \
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror)); \
		return; \
	}} while (0)

/* check req->async_states->status and if not OK then send an error reply */
#define CHECK_ASYNC_STATUS do { \
	if (!NT_STATUS_IS_OK(req->async_states->status)) { \
		smbsrv_send_error(req, req->async_states->status); \
		return; \
	}} while (0)
	
/* useful wrapper for talloc with NO_MEMORY reply */
#define REQ_TALLOC(ptr, type) do { \
	ptr = talloc(req, type); \
	if (!ptr) { \
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY); \
		return; \
	}} while (0)

/* 
   check if the backend wants to handle the request asynchronously.
   if it wants it handled synchronously then call the send function
   immediately
*/
#define REQ_ASYNC_TAIL do { \
	if (!(req->async_states->state & NTVFS_ASYNC_STATE_ASYNC)) { \
		req->async_states->send_fn(req); \
	}} while (0)

/* zero out some reserved fields in a reply */
#define REQ_VWV_RESERVED(start, count) memset(req->out.vwv + VWV(start), 0, (count)*2)

/****************************************************************************
 Reply to a simple request (async send)
****************************************************************************/
static void reply_simple_send(struct smbsrv_request *req)
{
	CHECK_ASYNC_STATUS;

	smbsrv_setup_reply(req, 0, 0);
	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a tcon.
****************************************************************************/
void smbsrv_reply_tcon(struct smbsrv_request *req)
{
	union smb_tcon con;
	NTSTATUS status;
	uint8_t *p;
	
	/* parse request */
	REQ_CHECK_WCT(req, 0);

	con.tcon.level = RAW_TCON_TCON;

	p = req->in.data;	
	p += req_pull_ascii4(req, &con.tcon.in.service, p, STR_TERMINATE);
	p += req_pull_ascii4(req, &con.tcon.in.password, p, STR_TERMINATE);
	p += req_pull_ascii4(req, &con.tcon.in.dev, p, STR_TERMINATE);

	if (!con.tcon.in.service || !con.tcon.in.password || !con.tcon.in.dev) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* call backend */
	status = smbsrv_tcon_backend(req, &con);

	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_send_error(req, status);
		return;
	}

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);

	SSVAL(req->out.vwv, VWV(0), con.tcon.out.max_xmit);
	SSVAL(req->out.vwv, VWV(1), con.tcon.out.tid);
	SSVAL(req->out.hdr, HDR_TID, req->tcon->tid);
  
	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a tcon and X.
****************************************************************************/
void smbsrv_reply_tcon_and_X(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_tcon con;
	uint8_t *p;
	uint16_t passlen;

	con.tconx.level = RAW_TCON_TCONX;

	/* parse request */
	REQ_CHECK_WCT(req, 4);

	con.tconx.in.flags  = SVAL(req->in.vwv, VWV(2));
	passlen             = SVAL(req->in.vwv, VWV(3));

	p = req->in.data;

	if (!req_pull_blob(req, p, passlen, &con.tconx.in.password)) {
		smbsrv_send_error(req, NT_STATUS_ILL_FORMED_PASSWORD);
		return;
	}
	p += passlen;

	p += req_pull_string(req, &con.tconx.in.path, p, -1, STR_TERMINATE);
	p += req_pull_string(req, &con.tconx.in.device, p, -1, STR_ASCII);

	if (!con.tconx.in.path || !con.tconx.in.device) {
		smbsrv_send_error(req, NT_STATUS_BAD_DEVICE_TYPE);
		return;
	}

	/* call backend */
	status = smbsrv_tcon_backend(req, &con);

	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_send_error(req, status);
		return;
	}

	/* construct reply - two variants */
	if (req->smb_conn->negotiate.protocol < PROTOCOL_NT1) {
		smbsrv_setup_reply(req, 2, 0);

		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);

		req_push_str(req, NULL, con.tconx.out.dev_type, -1, STR_TERMINATE|STR_ASCII);
	} else {
		smbsrv_setup_reply(req, 3, 0);

		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), con.tconx.out.options);

		req_push_str(req, NULL, con.tconx.out.dev_type, -1, STR_TERMINATE|STR_ASCII);
		req_push_str(req, NULL, con.tconx.out.fs_type, -1, STR_TERMINATE);
	}

	/* set the incoming and outgoing tid to the just created one */
	SSVAL(req->in.hdr, HDR_TID, con.tconx.out.tid);
	SSVAL(req->out.hdr,HDR_TID, con.tconx.out.tid);

	smbsrv_chain_reply(req);
}


/****************************************************************************
 Reply to an unknown request
****************************************************************************/
void smbsrv_reply_unknown(struct smbsrv_request *req)
{
	int type;

	type = CVAL(req->in.hdr, HDR_COM);
  
	DEBUG(0,("unknown command type %d (0x%X)\n", type, type));

	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRunknownsmb));
}


/****************************************************************************
 Reply to an ioctl (async reply)
****************************************************************************/
static void reply_ioctl_send(struct smbsrv_request *req)
{
	union smb_ioctl *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* the +1 is for nicer alignment */
	smbsrv_setup_reply(req, 8, io->ioctl.out.blob.length+1);
	SSVAL(req->out.vwv, VWV(1), io->ioctl.out.blob.length);
	SSVAL(req->out.vwv, VWV(5), io->ioctl.out.blob.length);
	SSVAL(req->out.vwv, VWV(6), PTR_DIFF(req->out.data, req->out.hdr) + 1);

	memcpy(req->out.data+1, io->ioctl.out.blob.data, io->ioctl.out.blob.length);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to an ioctl.
****************************************************************************/
void smbsrv_reply_ioctl(struct smbsrv_request *req)
{
	union smb_ioctl *io;

	/* parse request */
	REQ_CHECK_WCT(req, 3);
	REQ_TALLOC(io, union smb_ioctl);

	io->ioctl.level		= RAW_IOCTL_IOCTL;
	io->ioctl.in.file.fnum	= req_fnum(req, req->in.vwv, VWV(0));
	io->ioctl.in.request	= IVAL(req->in.vwv, VWV(1));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_ioctl_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_ioctl(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a chkpth.
****************************************************************************/
void smbsrv_reply_chkpth(struct smbsrv_request *req)
{
	union smb_chkpath *io;

	REQ_TALLOC(io, union smb_chkpath);

	req_pull_ascii4(req, &io->chkpath.in.path, req->in.data, STR_TERMINATE);

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	req->async_states->status = ntvfs_chkpath(req, io);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a getatr (async reply)
****************************************************************************/
static void reply_getatr_send(struct smbsrv_request *req)
{
	union smb_fileinfo *st = req->async_states->private_data;

	CHECK_ASYNC_STATUS;
	
	/* construct reply */
	smbsrv_setup_reply(req, 10, 0);

	SSVAL(req->out.vwv,         VWV(0), st->getattr.out.attrib);
	srv_push_dos_date3(req->smb_conn, req->out.vwv, VWV(1), st->getattr.out.write_time);
	SIVAL(req->out.vwv,         VWV(3), st->getattr.out.size);

	REQ_VWV_RESERVED(5, 5);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a getatr.
****************************************************************************/
void smbsrv_reply_getatr(struct smbsrv_request *req)
{
	union smb_fileinfo *st;

	REQ_TALLOC(st, union smb_fileinfo);
	
	st->getattr.level = RAW_FILEINFO_GETATTR;

	/* parse request */
	req_pull_ascii4(req, &st->getattr.in.file.path, req->in.data, STR_TERMINATE);
	if (!st->getattr.in.file.path) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_getatr_send;
	req->async_states->private_data = st;

	/* call backend */
	req->async_states->status = ntvfs_qpathinfo(req, st);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a setatr.
****************************************************************************/
void smbsrv_reply_setatr(struct smbsrv_request *req)
{
	union smb_setfileinfo *st;

	/* parse request */
	REQ_CHECK_WCT(req, 8);
	REQ_TALLOC(st, union smb_setfileinfo);

	st->setattr.level = RAW_SFILEINFO_SETATTR;
	st->setattr.in.attrib     = SVAL(req->in.vwv, VWV(0));
	st->setattr.in.write_time = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));
	
	req_pull_ascii4(req, &st->setattr.in.file.path, req->in.data, STR_TERMINATE);

	if (!st->setattr.in.file.path) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}
	
	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_setpathinfo(req, st);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a dskattr (async reply)
****************************************************************************/
static void reply_dskattr_send(struct smbsrv_request *req)
{
	union smb_fsinfo *fs = req->async_states->private_data;

	CHECK_ASYNC_STATUS;
	
	/* construct reply */
	smbsrv_setup_reply(req, 5, 0);

	SSVAL(req->out.vwv, VWV(0), fs->dskattr.out.units_total);
	SSVAL(req->out.vwv, VWV(1), fs->dskattr.out.blocks_per_unit);
	SSVAL(req->out.vwv, VWV(2), fs->dskattr.out.block_size);
	SSVAL(req->out.vwv, VWV(3), fs->dskattr.out.units_free);

	REQ_VWV_RESERVED(4, 1);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a dskattr.
****************************************************************************/
void smbsrv_reply_dskattr(struct smbsrv_request *req)
{
	union smb_fsinfo *fs;

	REQ_TALLOC(fs, union smb_fsinfo);
	
	fs->dskattr.level = RAW_QFS_DSKATTR;

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_dskattr_send;
	req->async_states->private_data = fs;

	/* call backend */
	req->async_states->status = ntvfs_fsinfo(req, fs);

	REQ_ASYNC_TAIL;
}



/****************************************************************************
 Reply to an open (async reply)
****************************************************************************/
static void reply_open_send(struct smbsrv_request *req)
{
	union smb_open *oi = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 7, 0);

	SSVAL(req->out.vwv, VWV(0), oi->openold.out.file.fnum);
	SSVAL(req->out.vwv, VWV(1), oi->openold.out.attrib);
	srv_push_dos_date3(req->smb_conn, req->out.vwv, VWV(2), oi->openold.out.write_time);
	SIVAL(req->out.vwv, VWV(4), oi->openold.out.size);
	SSVAL(req->out.vwv, VWV(6), oi->openold.out.rmode);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to an open.
****************************************************************************/
void smbsrv_reply_open(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse request */
	REQ_CHECK_WCT(req, 2);
	REQ_TALLOC(oi, union smb_open);

	oi->openold.level = RAW_OPEN_OPEN;
	oi->openold.in.open_mode = SVAL(req->in.vwv, VWV(0));
	oi->openold.in.search_attrs = SVAL(req->in.vwv, VWV(1));

	req_pull_ascii4(req, &oi->openold.in.fname, req->in.data, STR_TERMINATE);

	if (!oi->openold.in.fname) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_open_send;
	req->async_states->private_data = oi;
	
	/* call backend */
	req->async_states->status = ntvfs_open(req, oi);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to an open and X (async reply)
****************************************************************************/
static void reply_open_and_X_send(struct smbsrv_request *req)
{
	union smb_open *oi = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* build the reply */
	if (oi->openx.in.flags & OPENX_FLAGS_EXTENDED_RETURN) {
		smbsrv_setup_reply(req, 19, 0);
	} else {
		smbsrv_setup_reply(req, 15, 0);
	}

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), oi->openx.out.file.fnum);
	SSVAL(req->out.vwv, VWV(3), oi->openx.out.attrib);
	srv_push_dos_date3(req->smb_conn, req->out.vwv, VWV(4), oi->openx.out.write_time);
	SIVAL(req->out.vwv, VWV(6), oi->openx.out.size);
	SSVAL(req->out.vwv, VWV(8), oi->openx.out.access);
	SSVAL(req->out.vwv, VWV(9), oi->openx.out.ftype);
	SSVAL(req->out.vwv, VWV(10),oi->openx.out.devstate);
	SSVAL(req->out.vwv, VWV(11),oi->openx.out.action);
	SIVAL(req->out.vwv, VWV(12),oi->openx.out.unique_fid);
	SSVAL(req->out.vwv, VWV(14),0); /* reserved */
	if (oi->openx.in.flags & OPENX_FLAGS_EXTENDED_RETURN) {
		SIVAL(req->out.vwv, VWV(15),oi->openx.out.access_mask);
		REQ_VWV_RESERVED(17, 2);
	}

	req->chained_fnum = oi->openx.out.file.fnum;

	smbsrv_chain_reply(req);
}


/****************************************************************************
 Reply to an open and X.
****************************************************************************/
void smbsrv_reply_open_and_X(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse the request */
	REQ_CHECK_WCT(req, 15);
	REQ_TALLOC(oi, union smb_open);

	oi->openx.level = RAW_OPEN_OPENX;
	oi->openx.in.flags        = SVAL(req->in.vwv, VWV(2));
	oi->openx.in.open_mode    = SVAL(req->in.vwv, VWV(3));
	oi->openx.in.search_attrs = SVAL(req->in.vwv, VWV(4));
	oi->openx.in.file_attrs   = SVAL(req->in.vwv, VWV(5));
	oi->openx.in.write_time   = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(6));
	oi->openx.in.open_func    = SVAL(req->in.vwv, VWV(8));
	oi->openx.in.size         = IVAL(req->in.vwv, VWV(9));
	oi->openx.in.timeout      = IVAL(req->in.vwv, VWV(11));

	req_pull_ascii4(req, &oi->openx.in.fname, req->in.data, STR_TERMINATE);

	if (!oi->openx.in.fname) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_open_and_X_send;
	req->async_states->private_data = oi;

	/* call the backend */
	req->async_states->status = ntvfs_open(req, oi);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a mknew or a create.
****************************************************************************/
static void reply_mknew_send(struct smbsrv_request *req)
{
	union smb_open *oi = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* build the reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), oi->mknew.out.file.fnum);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a mknew or a create.
****************************************************************************/
void smbsrv_reply_mknew(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse the request */
	REQ_CHECK_WCT(req, 3);
	REQ_TALLOC(oi, union smb_open);

	if (CVAL(req->in.hdr, HDR_COM) == SMBmknew) {
		oi->mknew.level = RAW_OPEN_MKNEW;
	} else {
		oi->mknew.level = RAW_OPEN_CREATE;
	}
	oi->mknew.in.attrib  = SVAL(req->in.vwv, VWV(0));
	oi->mknew.in.write_time  = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));

	req_pull_ascii4(req, &oi->mknew.in.fname, req->in.data, STR_TERMINATE);

	if (!oi->mknew.in.fname) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_mknew_send;
	req->async_states->private_data = oi;

	/* call the backend */
	req->async_states->status = ntvfs_open(req, oi);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a create temporary file (async reply)
****************************************************************************/
static void reply_ctemp_send(struct smbsrv_request *req)
{
	union smb_open *oi = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* build the reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), oi->ctemp.out.file.fnum);

	/* the returned filename is relative to the directory */
	req_push_str(req, NULL, oi->ctemp.out.name, -1, STR_TERMINATE | STR_ASCII);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a create temporary file.
****************************************************************************/
void smbsrv_reply_ctemp(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse the request */
	REQ_CHECK_WCT(req, 3);
	REQ_TALLOC(oi, union smb_open);

	oi->ctemp.level = RAW_OPEN_CTEMP;
	oi->ctemp.in.attrib = SVAL(req->in.vwv, VWV(0));
	oi->ctemp.in.write_time = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));

	/* the filename is actually a directory name, the server provides a filename
	   in that directory */
	req_pull_ascii4(req, &oi->ctemp.in.directory, req->in.data, STR_TERMINATE);

	if (!oi->ctemp.in.directory) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_ctemp_send;
	req->async_states->private_data = oi;

	/* call the backend */
	req->async_states->status = ntvfs_open(req, oi);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a unlink
****************************************************************************/
void smbsrv_reply_unlink(struct smbsrv_request *req)
{
	union smb_unlink *unl;

	/* parse the request */
	REQ_CHECK_WCT(req, 1);
	REQ_TALLOC(unl, union smb_unlink);
	
	unl->unlink.in.attrib = SVAL(req->in.vwv, VWV(0));

	req_pull_ascii4(req, &unl->unlink.in.pattern, req->in.data, STR_TERMINATE);
	
	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_unlink(req, unl);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a readbraw (core+ protocol).
 this is a strange packet because it doesn't use a standard SMB header in the reply,
 only the 4 byte NBT header
 This command must be replied to synchronously
****************************************************************************/
void smbsrv_reply_readbraw(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_read io;

	io.readbraw.level = RAW_READ_READBRAW;

	/* there are two variants, one with 10 and one with 8 command words */
	if (req->in.wct < 8) {
		goto failed;
	}

	io.readbraw.in.file.fnum = req_fnum(req, req->in.vwv, VWV(0));
	io.readbraw.in.offset  = IVAL(req->in.vwv, VWV(1));
	io.readbraw.in.maxcnt  = SVAL(req->in.vwv, VWV(3));
	io.readbraw.in.mincnt  = SVAL(req->in.vwv, VWV(4));
	io.readbraw.in.timeout = IVAL(req->in.vwv, VWV(5));

	/* the 64 bit variant */
	if (req->in.wct == 10) {
		uint32_t offset_high = IVAL(req->in.vwv, VWV(8));
		io.readbraw.in.offset |= (((off_t)offset_high) << 32);
	}

	/* before calling the backend we setup the raw buffer. This
	 * saves a copy later */
	req->out.size = io.readbraw.in.maxcnt + NBT_HDR_SIZE;
	req->out.buffer = talloc_size(req, req->out.size);
	if (req->out.buffer == NULL) {
		goto failed;
	}
	SIVAL(req->out.buffer, 0, 0); /* init NBT header */

	/* tell the backend where to put the data */
	io.readbraw.out.data = req->out.buffer + NBT_HDR_SIZE;

	/* call the backend */
	status = ntvfs_read(req, &io);

	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	req->out.size = io.readbraw.out.nread + NBT_HDR_SIZE;

	smbsrv_send_reply_nosign(req);
	return;

failed:
	/* any failure in readbraw is equivalent to reading zero bytes */
	req->out.size = 4;
	req->out.buffer = talloc_size(req, req->out.size);
	SIVAL(req->out.buffer, 0, 0); /* init NBT header */

	smbsrv_send_reply_nosign(req);
}


/****************************************************************************
 Reply to a lockread (async reply)
****************************************************************************/
static void reply_lockread_send(struct smbsrv_request *req)
{
	union smb_read *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* trim packet */
	io->lockread.out.nread = MIN(io->lockread.out.nread,
		req_max_data(req) - 3);
	req_grow_data(req, 3 + io->lockread.out.nread);

	/* construct reply */
	SSVAL(req->out.vwv, VWV(0), io->lockread.out.nread);
	REQ_VWV_RESERVED(1, 4);

	SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
	SSVAL(req->out.data, 1, io->lockread.out.nread);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a lockread (core+ protocol).
 note that the lock is a write lock, not a read lock!
****************************************************************************/
void smbsrv_reply_lockread(struct smbsrv_request *req)
{
	union smb_read *io;
	
	/* parse request */
	REQ_CHECK_WCT(req, 5);
	REQ_TALLOC(io, union smb_read);

	io->lockread.level = RAW_READ_LOCKREAD;
	io->lockread.in.file.fnum = req_fnum(req, req->in.vwv, VWV(0));
	io->lockread.in.count     = SVAL(req->in.vwv, VWV(1));
	io->lockread.in.offset    = IVAL(req->in.vwv, VWV(2));
	io->lockread.in.remaining = SVAL(req->in.vwv, VWV(4));
	
	/* setup the reply packet assuming the maximum possible read */
	smbsrv_setup_reply(req, 5, 3 + io->lockread.in.count);

	/* tell the backend where to put the data */
	io->lockread.out.data = req->out.data + 3;

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_lockread_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_read(req, io);

	REQ_ASYNC_TAIL;
}



/****************************************************************************
 Reply to a read (async reply)
****************************************************************************/
static void reply_read_send(struct smbsrv_request *req)
{
	union smb_read *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* trim packet */
	io->read.out.nread = MIN(io->read.out.nread,
		req_max_data(req) - 3);
	req_grow_data(req, 3 + io->read.out.nread);

	/* construct reply */
	SSVAL(req->out.vwv, VWV(0), io->read.out.nread);
	REQ_VWV_RESERVED(1, 4);

	SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
	SSVAL(req->out.data, 1, io->read.out.nread);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a read.
****************************************************************************/
void smbsrv_reply_read(struct smbsrv_request *req)
{
	union smb_read *io;

	/* parse request */
	REQ_CHECK_WCT(req, 5);
	REQ_TALLOC(io, union smb_read);
	
	io->read.level = RAW_READ_READ;
	io->read.in.file.fnum     = req_fnum(req, req->in.vwv, VWV(0));
	io->read.in.count         = SVAL(req->in.vwv, VWV(1));
	io->read.in.offset        = IVAL(req->in.vwv, VWV(2));
	io->read.in.remaining     = SVAL(req->in.vwv, VWV(4));
	
	/* setup the reply packet assuming the maximum possible read */
	smbsrv_setup_reply(req, 5, 3 + io->read.in.count);

	/* tell the backend where to put the data */
	io->read.out.data = req->out.data + 3;

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_read_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_read(req, io);

	REQ_ASYNC_TAIL;
}



/****************************************************************************
 Reply to a read and X (async reply)
****************************************************************************/
static void reply_read_and_X_send(struct smbsrv_request *req)
{
	union smb_read *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* readx reply packets can be over-sized */
	req->control_flags |= REQ_CONTROL_LARGE;
	if (io->readx.in.maxcnt != 0xFFFF &&
	    io->readx.in.mincnt != 0xFFFF) {
		req_grow_data(req, 1 + io->readx.out.nread);
		SCVAL(req->out.data, 0, 0); /* padding */
	} else {
		req_grow_data(req, io->readx.out.nread);
	}

	/* construct reply */
	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), io->readx.out.remaining);
	SSVAL(req->out.vwv, VWV(3), io->readx.out.compaction_mode);
	REQ_VWV_RESERVED(4, 1);
	SSVAL(req->out.vwv, VWV(5), io->readx.out.nread);
	SSVAL(req->out.vwv, VWV(6), PTR_DIFF(io->readx.out.data, req->out.hdr));
	REQ_VWV_RESERVED(7, 5);

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to a read and X.
****************************************************************************/
void smbsrv_reply_read_and_X(struct smbsrv_request *req)
{
	union smb_read *io;

	/* parse request */
	if (req->in.wct != 12) {
		REQ_CHECK_WCT(req, 10);
	}

	REQ_TALLOC(io, union smb_read);

	io->readx.level = RAW_READ_READX;
	io->readx.in.file.fnum     = req_fnum(req, req->in.vwv, VWV(2));
	io->readx.in.offset        = IVAL(req->in.vwv, VWV(3));
	io->readx.in.maxcnt        = SVAL(req->in.vwv, VWV(5));
	io->readx.in.mincnt        = SVAL(req->in.vwv, VWV(6));
	io->readx.in.remaining     = SVAL(req->in.vwv, VWV(9));
	if (req->flags2 & FLAGS2_READ_PERMIT_EXECUTE) {
		io->readx.in.read_for_execute = True;
	} else {
		io->readx.in.read_for_execute = False;
	}

	if (req->smb_conn->negotiate.client_caps & CAP_LARGE_READX) {
		uint32_t high_part = IVAL(req->in.vwv, VWV(7));
		if (high_part == 1) {
			io->readx.in.maxcnt |= high_part << 16;
		}
	}
	
	/* the 64 bit variant */
	if (req->in.wct == 12) {
		uint32_t offset_high = IVAL(req->in.vwv, VWV(10));
		io->readx.in.offset |= (((uint64_t)offset_high) << 32);
	}

	/* setup the reply packet assuming the maximum possible read */
	smbsrv_setup_reply(req, 12, 1 + io->readx.in.maxcnt);

	/* tell the backend where to put the data. Notice the pad byte. */
	if (io->readx.in.maxcnt != 0xFFFF &&
	    io->readx.in.mincnt != 0xFFFF) {
		io->readx.out.data = req->out.data + 1;
	} else {
		io->readx.out.data = req->out.data;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_read_and_X_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_read(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a writebraw (core+ or LANMAN1.0 protocol).
****************************************************************************/
void smbsrv_reply_writebraw(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}


/****************************************************************************
 Reply to a writeunlock (async reply)
****************************************************************************/
static void reply_writeunlock_send(struct smbsrv_request *req)
{
	union smb_write *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), io->writeunlock.out.nwritten);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a writeunlock (core+).
****************************************************************************/
void smbsrv_reply_writeunlock(struct smbsrv_request *req)
{
	union smb_write *io;

	REQ_CHECK_WCT(req, 5);
	REQ_TALLOC(io, union smb_write);

	io->writeunlock.level = RAW_WRITE_WRITEUNLOCK;
	io->writeunlock.in.file.fnum   = req_fnum(req, req->in.vwv, VWV(0));
	io->writeunlock.in.count       = SVAL(req->in.vwv, VWV(1));
	io->writeunlock.in.offset      = IVAL(req->in.vwv, VWV(2));
	io->writeunlock.in.remaining   = SVAL(req->in.vwv, VWV(4));
	io->writeunlock.in.data        = req->in.data + 3;

	/* make sure they gave us the data they promised */
	if (io->writeunlock.in.count+3 > req->in.data_size) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* make sure the data block is big enough */
	if (SVAL(req->in.data, 1) < io->writeunlock.in.count) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_writeunlock_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_write(req, io);

	REQ_ASYNC_TAIL;
}



/****************************************************************************
 Reply to a write (async reply)
****************************************************************************/
static void reply_write_send(struct smbsrv_request *req)
{
	union smb_write *io = req->async_states->private_data;
	
	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), io->write.out.nwritten);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a write
****************************************************************************/
void smbsrv_reply_write(struct smbsrv_request *req)
{
	union smb_write *io;

	REQ_CHECK_WCT(req, 5);
	REQ_TALLOC(io, union smb_write);

	io->write.level = RAW_WRITE_WRITE;
	io->write.in.file.fnum   = req_fnum(req, req->in.vwv, VWV(0));
	io->write.in.count       = SVAL(req->in.vwv, VWV(1));
	io->write.in.offset      = IVAL(req->in.vwv, VWV(2));
	io->write.in.remaining   = SVAL(req->in.vwv, VWV(4));
	io->write.in.data        = req->in.data + 3;

	/* make sure they gave us the data they promised */
	if (req_data_oob(req, io->write.in.data, io->write.in.count)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* make sure the data block is big enough */
	if (SVAL(req->in.data, 1) < io->write.in.count) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_write_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_write(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a write and X (async reply)
****************************************************************************/
static void reply_write_and_X_send(struct smbsrv_request *req)
{
	union smb_write *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 6, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), io->writex.out.nwritten & 0xFFFF);
	SSVAL(req->out.vwv, VWV(3), io->writex.out.remaining);
	SSVAL(req->out.vwv, VWV(4), io->writex.out.nwritten >> 16);
	REQ_VWV_RESERVED(5, 1);

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to a write and X.
****************************************************************************/
void smbsrv_reply_write_and_X(struct smbsrv_request *req)
{
	union smb_write *io;
	
	if (req->in.wct != 14) {
		REQ_CHECK_WCT(req, 12);
	}

	REQ_TALLOC(io, union smb_write);

	io->writex.level = RAW_WRITE_WRITEX;
	io->writex.in.file.fnum = req_fnum(req, req->in.vwv, VWV(2));
	io->writex.in.offset    = IVAL(req->in.vwv, VWV(3));
	io->writex.in.wmode     = SVAL(req->in.vwv, VWV(7));
	io->writex.in.remaining = SVAL(req->in.vwv, VWV(8));
	io->writex.in.count     = SVAL(req->in.vwv, VWV(10));
	io->writex.in.data      = req->in.hdr + SVAL(req->in.vwv, VWV(11));

	if (req->in.wct == 14) {
		uint32_t offset_high = IVAL(req->in.vwv, VWV(12));
		uint16_t count_high = SVAL(req->in.vwv, VWV(9));
		io->writex.in.offset |= (((uint64_t)offset_high) << 32);
		io->writex.in.count |= ((uint32_t)count_high) << 16;
	}

	/* make sure the data is in bounds */
	if (req_data_oob(req, io->writex.in.data, io->writex.in.count)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	} 

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_write_and_X_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_write(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a lseek (async reply)
****************************************************************************/
static void reply_lseek_send(struct smbsrv_request *req)
{
	union smb_seek *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);

	SIVALS(req->out.vwv, VWV(0), io->lseek.out.offset);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a lseek.
****************************************************************************/
void smbsrv_reply_lseek(struct smbsrv_request *req)
{
	union smb_seek *io;

	REQ_CHECK_WCT(req, 4);
	REQ_TALLOC(io, union smb_seek);

	io->lseek.in.file.fnum	= req_fnum(req, req->in.vwv,  VWV(0));
	io->lseek.in.mode	= SVAL(req->in.vwv,  VWV(1));
	io->lseek.in.offset	= IVALS(req->in.vwv, VWV(2));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_lseek_send;
	req->async_states->private_data = io;
	
	/* call backend */
	req->async_states->status = ntvfs_seek(req, io);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a flush.
****************************************************************************/
void smbsrv_reply_flush(struct smbsrv_request *req)
{
	union smb_flush *io;

	/* parse request */
	REQ_CHECK_WCT(req, 1);
	REQ_TALLOC(io, union smb_flush);

	io->flush.in.file.fnum = req_fnum(req, req->in.vwv,  VWV(0));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_flush(req, io);
	
	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a exit. This closes all files open by a smbpid
****************************************************************************/
void smbsrv_reply_exit(struct smbsrv_request *req)
{
	NTSTATUS status;
	struct smbsrv_tcon *tcon;
	REQ_CHECK_WCT(req, 0);

	for (tcon=req->smb_conn->smb_tcons.list;tcon;tcon=tcon->next) {
		req->tcon = tcon;
		status = ntvfs_exit(req);
		req->tcon = NULL;
		if (!NT_STATUS_IS_OK(status)) {
			smbsrv_send_error(req, status);
			return;
		}
	}

	smbsrv_setup_reply(req, 0, 0);
	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a close 

 Note that this has to deal with closing a directory opened by NT SMB's.
****************************************************************************/
void smbsrv_reply_close(struct smbsrv_request *req)
{
	union smb_close *io;

	/* parse request */
	REQ_CHECK_WCT(req, 3);
	REQ_TALLOC(io, union smb_close);

	io->close.level = RAW_CLOSE_CLOSE;
	io->close.in.file.fnum  = req_fnum(req, req->in.vwv,  VWV(0));
	io->close.in.write_time = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_close(req, io);

	REQ_ASYNC_TAIL;
}



/****************************************************************************
 Reply to a writeclose (async reply)
****************************************************************************/
static void reply_writeclose_send(struct smbsrv_request *req)
{
	union smb_write *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), io->write.out.nwritten);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a writeclose (Core+ protocol).
****************************************************************************/
void smbsrv_reply_writeclose(struct smbsrv_request *req)
{
	union smb_write *io;

	/* this one is pretty weird - the wct can be 6 or 12 */
	if (req->in.wct != 12) {
		REQ_CHECK_WCT(req, 6);
	}

	REQ_TALLOC(io, union smb_write);

	io->writeclose.level = RAW_WRITE_WRITECLOSE;
	io->writeclose.in.file.fnum = req_fnum(req, req->in.vwv, VWV(0));
	io->writeclose.in.count  = SVAL(req->in.vwv, VWV(1));
	io->writeclose.in.offset = IVAL(req->in.vwv, VWV(2));
	io->writeclose.in.mtime  = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(4));
	io->writeclose.in.data   = req->in.data + 1;

	/* make sure they gave us the data they promised */
	if (req_data_oob(req, io->writeclose.in.data, io->writeclose.in.count)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_writeclose_send;
	req->async_states->private_data = io;

	/* call backend */
	req->async_states->status = ntvfs_write(req, io);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a lock.
****************************************************************************/
void smbsrv_reply_lock(struct smbsrv_request *req)
{
	union smb_lock *lck;

	/* parse request */
	REQ_CHECK_WCT(req, 5);
	REQ_TALLOC(lck, union smb_lock);

	lck->lock.level		= RAW_LOCK_LOCK;
	lck->lock.in.file.fnum	= req_fnum(req, req->in.vwv, VWV(0));
	lck->lock.in.count	= IVAL(req->in.vwv, VWV(1));
	lck->lock.in.offset	= IVAL(req->in.vwv, VWV(3));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_lock(req, lck);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a unlock.
****************************************************************************/
void smbsrv_reply_unlock(struct smbsrv_request *req)
{
	union smb_lock *lck;

	/* parse request */
	REQ_CHECK_WCT(req, 5);
	REQ_TALLOC(lck, union smb_lock);

	lck->unlock.level		= RAW_LOCK_UNLOCK;
	lck->unlock.in.file.fnum	= req_fnum(req, req->in.vwv, VWV(0));
	lck->unlock.in.count 		= IVAL(req->in.vwv, VWV(1));
	lck->unlock.in.offset		= IVAL(req->in.vwv, VWV(3));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_lock(req, lck);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a tdis.
****************************************************************************/
void smbsrv_reply_tdis(struct smbsrv_request *req)
{
	REQ_CHECK_WCT(req, 0);

	if (req->tcon == NULL) {
		smbsrv_send_error(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	talloc_free(req->tcon);

	/* construct reply */
	smbsrv_setup_reply(req, 0, 0);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a echo. This is one of the few calls that is handled directly (the
 backends don't see it at all)
****************************************************************************/
void smbsrv_reply_echo(struct smbsrv_request *req)
{
	uint16_t count;
	int i;

	REQ_CHECK_WCT(req, 0);

	count = SVAL(req->in.vwv, VWV(0));

	smbsrv_setup_reply(req, 1, req->in.data_size);

	memcpy(req->out.data, req->in.data, req->in.data_size);

	for (i=1; i <= count;i++) {
		struct smbsrv_request *this_req;
		
		if (i != count) {
			this_req = smbsrv_setup_secondary_request(req);
		} else {
			this_req = req;
		}

		SSVAL(this_req->out.vwv, VWV(0), i);
		smbsrv_send_reply(this_req);
	}
}



/****************************************************************************
 Reply to a printopen (async reply)
****************************************************************************/
static void reply_printopen_send(struct smbsrv_request *req)
{
	union smb_open *oi = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), oi->openold.out.file.fnum);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a printopen.
****************************************************************************/
void smbsrv_reply_printopen(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse request */
	REQ_CHECK_WCT(req, 2);
	REQ_TALLOC(oi, union smb_open);

	oi->splopen.level = RAW_OPEN_SPLOPEN;
	oi->splopen.in.setup_length = SVAL(req->in.vwv, VWV(0));
	oi->splopen.in.mode         = SVAL(req->in.vwv, VWV(1));

	req_pull_ascii4(req, &oi->splopen.in.ident, req->in.data, STR_TERMINATE);

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_printopen_send;
	req->async_states->private_data = oi;

	/* call backend */
	req->async_states->status = ntvfs_open(req, oi);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a printclose.
****************************************************************************/
void smbsrv_reply_printclose(struct smbsrv_request *req)
{
	union smb_close *io;

	/* parse request */
	REQ_CHECK_WCT(req, 3);
	REQ_TALLOC(io, union smb_close);

	io->splclose.level = RAW_CLOSE_SPLCLOSE;
	io->splclose.in.file.fnum = req_fnum(req, req->in.vwv,  VWV(0));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_close(req, io);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a printqueue.
****************************************************************************/
static void reply_printqueue_send(struct smbsrv_request *req)
{
	union smb_lpq *lpq = req->async_states->private_data;
	int i, maxcount;
	const uint_t el_size = 28;	

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);

	/* truncate the returned list to fit in the negotiated buffer size */
	maxcount = (req_max_data(req) - 3) / el_size;
	if (maxcount < lpq->retq.out.count) {
		lpq->retq.out.count = maxcount;
	}

	/* setup enough space in the reply */
	req_grow_data(req, 3 + el_size*lpq->retq.out.count);
	
	/* and fill it in */
	SSVAL(req->out.vwv, VWV(0), lpq->retq.out.count);
	SSVAL(req->out.vwv, VWV(1), lpq->retq.out.restart_idx);

	SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
	SSVAL(req->out.data, 1, el_size*lpq->retq.out.count);

	req->out.ptr = req->out.data + 3;

	for (i=0;i<lpq->retq.out.count;i++) {
		srv_push_dos_date2(req->smb_conn, req->out.ptr, 0 , lpq->retq.out.queue[i].time);
		SCVAL(req->out.ptr,  4, lpq->retq.out.queue[i].status);
		SSVAL(req->out.ptr,  5, lpq->retq.out.queue[i].job);
		SIVAL(req->out.ptr,  7, lpq->retq.out.queue[i].size);
		SCVAL(req->out.ptr, 11, 0); /* reserved */
		req_push_str(req, req->out.ptr+12, lpq->retq.out.queue[i].user, 16, STR_ASCII);
		req->out.ptr += el_size;
	}

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a printqueue.
****************************************************************************/
void smbsrv_reply_printqueue(struct smbsrv_request *req)
{
	union smb_lpq *lpq;

	/* parse request */
	REQ_CHECK_WCT(req, 2);
	REQ_TALLOC(lpq, union smb_lpq);

	lpq->retq.level = RAW_LPQ_RETQ;
	lpq->retq.in.maxcount = SVAL(req->in.vwv,  VWV(0));
	lpq->retq.in.startidx = SVAL(req->in.vwv,  VWV(1));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_printqueue_send;
	req->async_states->private_data = lpq;

	/* call backend */
	req->async_states->status = ntvfs_lpq(req, lpq);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a printwrite.
****************************************************************************/
void smbsrv_reply_printwrite(struct smbsrv_request *req)
{
	union smb_write *io;

	/* parse request */
	REQ_CHECK_WCT(req, 1);
	REQ_TALLOC(io, union smb_write);

	io->splwrite.level = RAW_WRITE_SPLWRITE;

	if (req->in.data_size < 3) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	io->splwrite.in.file.fnum = req_fnum(req, req->in.vwv, VWV(0));
	io->splwrite.in.count = SVAL(req->in.data, 1);
	io->splwrite.in.data  = req->in.data + 3;

	/* make sure they gave us the data they promised */
	if (req_data_oob(req, io->splwrite.in.data, io->splwrite.in.count)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_write(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a mkdir.
****************************************************************************/
void smbsrv_reply_mkdir(struct smbsrv_request *req)
{
	union smb_mkdir *io;

	/* parse the request */
	REQ_CHECK_WCT(req, 0);
	REQ_TALLOC(io, union smb_mkdir);

	io->generic.level = RAW_MKDIR_MKDIR;
	req_pull_ascii4(req, &io->mkdir.in.path, req->in.data, STR_TERMINATE);

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_mkdir(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a rmdir.
****************************************************************************/
void smbsrv_reply_rmdir(struct smbsrv_request *req)
{
	struct smb_rmdir *io;
 
	/* parse the request */
	REQ_CHECK_WCT(req, 0);
	REQ_TALLOC(io, struct smb_rmdir);

	req_pull_ascii4(req, &io->in.path, req->in.data, STR_TERMINATE);

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_rmdir(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a mv.
****************************************************************************/
void smbsrv_reply_mv(struct smbsrv_request *req)
{
	union smb_rename *io;
	uint8_t *p;
 
	/* parse the request */
	REQ_CHECK_WCT(req, 1);
	REQ_TALLOC(io, union smb_rename);

	io->generic.level = RAW_RENAME_RENAME;
	io->rename.in.attrib = SVAL(req->in.vwv, VWV(0));

	p = req->in.data;
	p += req_pull_ascii4(req, &io->rename.in.pattern1, p, STR_TERMINATE);
	p += req_pull_ascii4(req, &io->rename.in.pattern2, p, STR_TERMINATE);

	if (!io->rename.in.pattern1 || !io->rename.in.pattern2) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_rename(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to an NT rename.
****************************************************************************/
void smbsrv_reply_ntrename(struct smbsrv_request *req)
{
	union smb_rename *io;
	uint8_t *p;
 
	/* parse the request */
	REQ_CHECK_WCT(req, 4);
	REQ_TALLOC(io, union smb_rename);

	io->generic.level = RAW_RENAME_NTRENAME;
	io->ntrename.in.attrib  = SVAL(req->in.vwv, VWV(0));
	io->ntrename.in.flags   = SVAL(req->in.vwv, VWV(1));
	io->ntrename.in.cluster_size = IVAL(req->in.vwv, VWV(2));

	p = req->in.data;
	p += req_pull_ascii4(req, &io->ntrename.in.old_name, p, STR_TERMINATE);
	p += req_pull_ascii4(req, &io->ntrename.in.new_name, p, STR_TERMINATE);

	if (!io->ntrename.in.old_name || !io->ntrename.in.new_name) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_rename(req, io);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a file copy (async reply)
****************************************************************************/
static void reply_copy_send(struct smbsrv_request *req)
{
	struct smb_copy *cp = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* build the reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), cp->out.count);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a file copy.
****************************************************************************/
void smbsrv_reply_copy(struct smbsrv_request *req)
{
	struct smb_copy *cp;
	uint8_t *p;

	/* parse request */
	REQ_CHECK_WCT(req, 3);
	REQ_TALLOC(cp, struct smb_copy);

	cp->in.tid2  = SVAL(req->in.vwv, VWV(0));
	cp->in.ofun  = SVAL(req->in.vwv, VWV(1));
	cp->in.flags = SVAL(req->in.vwv, VWV(2));

	p = req->in.data;
	p += req_pull_ascii4(req, &cp->in.path1, p, STR_TERMINATE);
	p += req_pull_ascii4(req, &cp->in.path2, p, STR_TERMINATE);

	if (!cp->in.path1 || !cp->in.path2) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_copy_send;
	req->async_states->private_data = cp;

	/* call backend */
	req->async_states->status = ntvfs_copy(req, cp);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a lockingX request (async send)
****************************************************************************/
static void reply_lockingX_send(struct smbsrv_request *req)
{
	union smb_lock *lck = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* if it was an oplock break ack then we only send a reply if
	   there was an error */
	if (lck->lockx.in.ulock_cnt + lck->lockx.in.lock_cnt == 0) {
		talloc_free(req);
		return;
	}

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);
	
	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);

	smbsrv_chain_reply(req);
}


/****************************************************************************
 Reply to a lockingX request.
****************************************************************************/
void smbsrv_reply_lockingX(struct smbsrv_request *req)
{
	union smb_lock *lck;
	uint_t total_locks, i;
	uint_t lck_size;
	uint8_t *p;

	/* parse request */
	REQ_CHECK_WCT(req, 8);
	REQ_TALLOC(lck, union smb_lock);

	lck->lockx.level = RAW_LOCK_LOCKX;
	lck->lockx.in.file.fnum = req_fnum(req, req->in.vwv, VWV(2));
	lck->lockx.in.mode      = SVAL(req->in.vwv, VWV(3));
	lck->lockx.in.timeout   = IVAL(req->in.vwv, VWV(4));
	lck->lockx.in.ulock_cnt = SVAL(req->in.vwv, VWV(6));
	lck->lockx.in.lock_cnt  = SVAL(req->in.vwv, VWV(7));

	total_locks = lck->lockx.in.ulock_cnt + lck->lockx.in.lock_cnt;

	/* there are two variants, one with 64 bit offsets and counts */
	if (lck->lockx.in.mode & LOCKING_ANDX_LARGE_FILES) {
		lck_size = 20;
	} else {
		lck_size = 10;		
	}

	/* make sure we got the promised data */
	if (req_data_oob(req, req->in.data, total_locks * lck_size)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* allocate the locks array */
	if (total_locks) {
		lck->lockx.in.locks = talloc_array(req, struct smb_lock_entry, 
						   total_locks);
		if (lck->lockx.in.locks == NULL) {
			smbsrv_send_error(req, NT_STATUS_NO_MEMORY);
			return;
		}
	}

	p = req->in.data;

	/* construct the locks array */
	for (i=0;i<total_locks;i++) {
		uint32_t ofs_high=0, count_high=0;

		lck->lockx.in.locks[i].pid = SVAL(p, 0);

		if (lck->lockx.in.mode & LOCKING_ANDX_LARGE_FILES) {
			ofs_high   = IVAL(p, 4);
			lck->lockx.in.locks[i].offset = IVAL(p, 8);
			count_high = IVAL(p, 12);
			lck->lockx.in.locks[i].count  = IVAL(p, 16);
		} else {
			lck->lockx.in.locks[i].offset = IVAL(p, 2);
			lck->lockx.in.locks[i].count  = IVAL(p, 6);
		}
		if (ofs_high != 0 || count_high != 0) {
			lck->lockx.in.locks[i].count  |= ((uint64_t)count_high) << 32;
			lck->lockx.in.locks[i].offset |= ((uint64_t)ofs_high) << 32;
		}
		p += lck_size;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_lockingX_send;
	req->async_states->private_data = lck;

	/* call backend */
	req->async_states->status = ntvfs_lock(req, lck);

	REQ_ASYNC_TAIL;
}

/****************************************************************************
 Reply to a SMBreadbmpx (read block multiplex) request.
****************************************************************************/
void smbsrv_reply_readbmpx(struct smbsrv_request *req)
{
	/* tell the client to not use a multiplexed read - its too broken to use */
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}


/****************************************************************************
 Reply to a SMBsetattrE.
****************************************************************************/
void smbsrv_reply_setattrE(struct smbsrv_request *req)
{
	union smb_setfileinfo *info;

	/* parse request */
	REQ_CHECK_WCT(req, 7);
	REQ_TALLOC(info, union smb_setfileinfo);

	info->setattre.level = RAW_SFILEINFO_SETATTRE;
	info->setattre.in.file.fnum   = req_fnum(req, req->in.vwv,    VWV(0));
	info->setattre.in.create_time = srv_pull_dos_date2(req->smb_conn, req->in.vwv + VWV(1));
	info->setattre.in.access_time = srv_pull_dos_date2(req->smb_conn, req->in.vwv + VWV(3));
	info->setattre.in.write_time  = srv_pull_dos_date2(req->smb_conn, req->in.vwv + VWV(5));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_simple_send;

	/* call backend */
	req->async_states->status = ntvfs_setfileinfo(req, info);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to a SMBwritebmpx (write block multiplex primary) request.
****************************************************************************/
void smbsrv_reply_writebmpx(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}


/****************************************************************************
 Reply to a SMBwritebs (write block multiplex secondary) request.
****************************************************************************/
void smbsrv_reply_writebs(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}



/****************************************************************************
 Reply to a SMBgetattrE (async reply)
****************************************************************************/
static void reply_getattrE_send(struct smbsrv_request *req)
{
	union smb_fileinfo *info = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* setup reply */
	smbsrv_setup_reply(req, 11, 0);

	srv_push_dos_date2(req->smb_conn, req->out.vwv, VWV(0), info->getattre.out.create_time);
	srv_push_dos_date2(req->smb_conn, req->out.vwv, VWV(2), info->getattre.out.access_time);
	srv_push_dos_date2(req->smb_conn, req->out.vwv, VWV(4), info->getattre.out.write_time);
	SIVAL(req->out.vwv,         VWV(6), info->getattre.out.size);
	SIVAL(req->out.vwv,         VWV(8), info->getattre.out.alloc_size);
	SSVAL(req->out.vwv,        VWV(10), info->getattre.out.attrib);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a SMBgetattrE.
****************************************************************************/
void smbsrv_reply_getattrE(struct smbsrv_request *req)
{
	union smb_fileinfo *info;

	/* parse request */
	REQ_CHECK_WCT(req, 1);
	REQ_TALLOC(info, union smb_fileinfo);

	info->getattr.level = RAW_FILEINFO_GETATTRE;
	info->getattr.in.file.fnum = req_fnum(req, req->in.vwv, VWV(0));

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_getattrE_send;
	req->async_states->private_data = info;

	/* call backend */
	req->async_states->status = ntvfs_qfileinfo(req, info);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
reply to an old style session setup command
****************************************************************************/
static void reply_sesssetup_old(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_sesssetup sess;
	uint8_t *p;
	uint16_t passlen;

	sess.old.level = RAW_SESSSETUP_OLD;

	/* parse request */
	sess.old.in.bufsize = SVAL(req->in.vwv, VWV(2));
	sess.old.in.mpx_max = SVAL(req->in.vwv, VWV(3));
	sess.old.in.vc_num  = SVAL(req->in.vwv, VWV(4));
	sess.old.in.sesskey = IVAL(req->in.vwv, VWV(5));
	passlen             = SVAL(req->in.vwv, VWV(7));

	/* check the request isn't malformed */
	if (req_data_oob(req, req->in.data, passlen)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	
	p = req->in.data;
	if (!req_pull_blob(req, p, passlen, &sess.old.in.password)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += passlen;
	
	p += req_pull_string(req, &sess.old.in.user,   p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.old.in.domain, p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.old.in.os,     p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.old.in.lanman, p, -1, STR_TERMINATE);

	/* call the generic handler */
	status = smbsrv_sesssetup_backend(req, &sess);

	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_send_error(req, status);
		return;
	}

	/* construct reply */
	smbsrv_setup_reply(req, 3, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), sess.old.out.action);

	SSVAL(req->out.hdr, HDR_UID, sess.old.out.vuid);

	smbsrv_chain_reply(req);
}


/****************************************************************************
reply to an NT1 style session setup command
****************************************************************************/
static void reply_sesssetup_nt1(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_sesssetup sess;
	uint8_t *p;
	uint16_t passlen1, passlen2;

	sess.nt1.level = RAW_SESSSETUP_NT1;

	/* parse request */
	sess.nt1.in.bufsize      = SVAL(req->in.vwv, VWV(2));
	sess.nt1.in.mpx_max      = SVAL(req->in.vwv, VWV(3));
	sess.nt1.in.vc_num       = SVAL(req->in.vwv, VWV(4));
	sess.nt1.in.sesskey      = IVAL(req->in.vwv, VWV(5));
	passlen1                 = SVAL(req->in.vwv, VWV(7));
	passlen2                 = SVAL(req->in.vwv, VWV(8));
	sess.nt1.in.capabilities = IVAL(req->in.vwv, VWV(11));

	/* check the request isn't malformed */
	if (req_data_oob(req, req->in.data, passlen1) ||
	    req_data_oob(req, req->in.data + passlen1, passlen2)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	
	p = req->in.data;
	if (!req_pull_blob(req, p, passlen1, &sess.nt1.in.password1)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += passlen1;
	if (!req_pull_blob(req, p, passlen2, &sess.nt1.in.password2)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += passlen2;
	
	p += req_pull_string(req, &sess.nt1.in.user,   p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.nt1.in.domain, p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.nt1.in.os,     p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.nt1.in.lanman, p, -1, STR_TERMINATE);

	/* call the generic handler */
	status = smbsrv_sesssetup_backend(req, &sess);

	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_send_error(req, status);
		return;
	}

	/* construct reply */
	smbsrv_setup_reply(req, 3, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), sess.nt1.out.action);

	SSVAL(req->out.hdr, HDR_UID, sess.nt1.out.vuid);

	req_push_str(req, NULL, sess.nt1.out.os, -1, STR_TERMINATE);
	req_push_str(req, NULL, sess.nt1.out.lanman, -1, STR_TERMINATE);
	req_push_str(req, NULL, sess.nt1.out.domain, -1, STR_TERMINATE);

	smbsrv_chain_reply(req);
}


/****************************************************************************
reply to an SPNEGO style session setup command
****************************************************************************/
static void reply_sesssetup_spnego(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_sesssetup sess;
	uint8_t *p;
	uint16_t blob_len;

	sess.spnego.level = RAW_SESSSETUP_SPNEGO;

	/* parse request */
	sess.spnego.in.bufsize      = SVAL(req->in.vwv, VWV(2));
	sess.spnego.in.mpx_max      = SVAL(req->in.vwv, VWV(3));
	sess.spnego.in.vc_num       = SVAL(req->in.vwv, VWV(4));
	sess.spnego.in.sesskey      = IVAL(req->in.vwv, VWV(5));
	blob_len                    = SVAL(req->in.vwv, VWV(7));
	sess.spnego.in.capabilities = IVAL(req->in.vwv, VWV(10));

	p = req->in.data;
	if (!req_pull_blob(req, p, blob_len, &sess.spnego.in.secblob)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += blob_len;
	
	p += req_pull_string(req, &sess.spnego.in.os,        p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.spnego.in.lanman,    p, -1, STR_TERMINATE);
	p += req_pull_string(req, &sess.spnego.in.workgroup, p, -1, STR_TERMINATE);

	/* call the generic handler */
	status = smbsrv_sesssetup_backend(req, &sess);

	if (!NT_STATUS_IS_OK(status) && 
	    !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		smbsrv_send_error(req, status);
		return;
	}

	/* construct reply */
	smbsrv_setup_reply(req, 4, sess.spnego.out.secblob.length);

	if (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		smbsrv_setup_error(req, status);
	}

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), sess.spnego.out.action);
	SSVAL(req->out.vwv, VWV(3), sess.spnego.out.secblob.length);

	SSVAL(req->out.hdr, HDR_UID, sess.spnego.out.vuid);

	memcpy(req->out.data, sess.spnego.out.secblob.data, sess.spnego.out.secblob.length);
	req_push_str(req, NULL, sess.spnego.out.os,        -1, STR_TERMINATE);
	req_push_str(req, NULL, sess.spnego.out.lanman,    -1, STR_TERMINATE);
	req_push_str(req, NULL, sess.spnego.out.workgroup, -1, STR_TERMINATE);

	smbsrv_chain_reply(req);
}


/****************************************************************************
reply to a session setup command
****************************************************************************/
void smbsrv_reply_sesssetup(struct smbsrv_request *req)
{
	switch (req->in.wct) {
	case 10:
		/* a pre-NT1 call */
		reply_sesssetup_old(req);
		return;
	case 13:
		/* a NT1 call */
		reply_sesssetup_nt1(req);
		return;
	case 12:
		/* a SPNEGO call */
		reply_sesssetup_spnego(req);
		return;
	}

	/* unsupported variant */
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}

/****************************************************************************
 Reply to a SMBulogoffX.
****************************************************************************/
void smbsrv_reply_ulogoffX(struct smbsrv_request *req)
{
	struct smbsrv_tcon *tcon;
	NTSTATUS status;

	if (!req->session) {
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRbaduid));
		return;
	}

	/* in user level security we are supposed to close any files
	   open by this user on all open tree connects */
	for (tcon=req->smb_conn->smb_tcons.list;tcon;tcon=tcon->next) {
		req->tcon = tcon;
		status = ntvfs_logoff(req);
		req->tcon = NULL;
		if (!NT_STATUS_IS_OK(status)) {
			smbsrv_send_error(req, status);
			return;
		}
	}

	talloc_free(req->session);
	req->session = NULL; /* it is now invalid, don't use on 
				any chained packets */

	smbsrv_setup_reply(req, 2, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);	
	
	smbsrv_chain_reply(req);
}


/****************************************************************************
 Reply to an SMBfindclose request
****************************************************************************/
void smbsrv_reply_findclose(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_search_close io;

	io.findclose.level = RAW_FINDCLOSE_FINDCLOSE;

	/* parse request */
	REQ_CHECK_WCT(req, 1);

	io.findclose.in.handle  = SVAL(req->in.vwv, VWV(0));
	
	/* call backend */
	status = ntvfs_search_close(req, &io);

	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_send_error(req, status);
		return;
	}

	/* construct reply */
	smbsrv_setup_reply(req, 0, 0);

	smbsrv_send_reply(req);	
}

/****************************************************************************
 Reply to an SMBfindnclose request
****************************************************************************/
void smbsrv_reply_findnclose(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}


/****************************************************************************
 Reply to an SMBntcreateX request (async send)
****************************************************************************/
static void reply_ntcreate_and_X_send(struct smbsrv_request *req)
{
	union smb_open *io = req->async_states->private_data;

	CHECK_ASYNC_STATUS;

	/* construct reply */
	smbsrv_setup_reply(req, 34, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);	
	SCVAL(req->out.vwv, VWV(2), io->ntcreatex.out.oplock_level);

	/* the rest of the parameters are not aligned! */
	SSVAL(req->out.vwv,        5, io->ntcreatex.out.file.fnum);
	SIVAL(req->out.vwv,        7, io->ntcreatex.out.create_action);
	push_nttime(req->out.vwv, 11, io->ntcreatex.out.create_time);
	push_nttime(req->out.vwv, 19, io->ntcreatex.out.access_time);
	push_nttime(req->out.vwv, 27, io->ntcreatex.out.write_time);
	push_nttime(req->out.vwv, 35, io->ntcreatex.out.change_time);
	SIVAL(req->out.vwv,       43, io->ntcreatex.out.attrib);
	SBVAL(req->out.vwv,       47, io->ntcreatex.out.alloc_size);
	SBVAL(req->out.vwv,       55, io->ntcreatex.out.size);
	SSVAL(req->out.vwv,       63, io->ntcreatex.out.file_type);
	SSVAL(req->out.vwv,       65, io->ntcreatex.out.ipc_state);
	SCVAL(req->out.vwv,       67, io->ntcreatex.out.is_directory);

	req->chained_fnum = io->ntcreatex.out.file.fnum;

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to an SMBntcreateX request
****************************************************************************/
void smbsrv_reply_ntcreate_and_X(struct smbsrv_request *req)
{
	union smb_open *io;
	uint16_t fname_len;

	/* parse the request */
	REQ_CHECK_WCT(req, 24);
	REQ_TALLOC(io, union smb_open);

	io->ntcreatex.level = RAW_OPEN_NTCREATEX;

	/* notice that the word parameters are not word aligned, so we don't use VWV() */
	fname_len =                         SVAL(req->in.vwv, 5);
	io->ntcreatex.in.flags =            IVAL(req->in.vwv, 7);
	io->ntcreatex.in.root_fid =         IVAL(req->in.vwv, 11);
	io->ntcreatex.in.access_mask =      IVAL(req->in.vwv, 15);
	io->ntcreatex.in.alloc_size =       BVAL(req->in.vwv, 19);
	io->ntcreatex.in.file_attr =        IVAL(req->in.vwv, 27);
	io->ntcreatex.in.share_access =     IVAL(req->in.vwv, 31);
	io->ntcreatex.in.open_disposition = IVAL(req->in.vwv, 35);
	io->ntcreatex.in.create_options =   IVAL(req->in.vwv, 39);
	io->ntcreatex.in.impersonation =    IVAL(req->in.vwv, 43);
	io->ntcreatex.in.security_flags =   CVAL(req->in.vwv, 47);
	io->ntcreatex.in.ea_list          = NULL;
	io->ntcreatex.in.sec_desc         = NULL;

	/* we need a neater way to handle this alignment */
	if ((req->flags2 & FLAGS2_UNICODE_STRINGS) && 
	    ucs2_align(req->in.buffer, req->in.data, STR_TERMINATE|STR_UNICODE)) {
		fname_len++;
	}

	req_pull_string(req, &io->ntcreatex.in.fname, req->in.data, fname_len, STR_TERMINATE);
	if (!io->ntcreatex.in.fname) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	req->async_states->state |= NTVFS_ASYNC_STATE_MAY_ASYNC;
	req->async_states->send_fn = reply_ntcreate_and_X_send;
	req->async_states->private_data = io;

	/* call the backend */
	req->async_states->status = ntvfs_open(req, io);

	REQ_ASYNC_TAIL;
}


/****************************************************************************
 Reply to an SMBntcancel request
****************************************************************************/
void smbsrv_reply_ntcancel(struct smbsrv_request *req)
{
	/* NOTE: this request does not generate a reply */
	ntvfs_cancel(req);
	talloc_free(req);
}

/****************************************************************************
 Reply to an SMBsends request
****************************************************************************/
void smbsrv_reply_sends(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}

/****************************************************************************
 Reply to an SMBsendstrt request
****************************************************************************/
void smbsrv_reply_sendstrt(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}

/****************************************************************************
 Reply to an SMBsendend request
****************************************************************************/
void smbsrv_reply_sendend(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}

/****************************************************************************
 Reply to an SMBsendtxt request
****************************************************************************/
void smbsrv_reply_sendtxt(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}


/*
  parse the called/calling names from session request
*/
static NTSTATUS parse_session_request(struct smbsrv_request *req)
{
	DATA_BLOB blob;
	NTSTATUS status;
	
	blob.data = req->in.buffer + 4;
	blob.length = ascii_len_n((const char *)blob.data, req->in.size - PTR_DIFF(blob.data, req->in.buffer));
	if (blob.length == 0) return NT_STATUS_BAD_NETWORK_NAME;

	req->smb_conn->negotiate.called_name  = talloc(req->smb_conn, struct nbt_name);
	req->smb_conn->negotiate.calling_name = talloc(req->smb_conn, struct nbt_name);
	if (req->smb_conn->negotiate.called_name == NULL ||
	    req->smb_conn->negotiate.calling_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = nbt_name_from_blob(req->smb_conn, &blob,
				    req->smb_conn->negotiate.called_name);
	NT_STATUS_NOT_OK_RETURN(status);

	blob.data += blob.length;
	blob.length = ascii_len_n((const char *)blob.data, req->in.size - PTR_DIFF(blob.data, req->in.buffer));
	if (blob.length == 0) return NT_STATUS_BAD_NETWORK_NAME;

	status = nbt_name_from_blob(req->smb_conn, &blob,
				    req->smb_conn->negotiate.calling_name);
	NT_STATUS_NOT_OK_RETURN(status);

	req->smb_conn->negotiate.done_nbt_session = True;

	return NT_STATUS_OK;
}	



/****************************************************************************
 Reply to a special message - a SMB packet with non zero NBT message type
****************************************************************************/
void smbsrv_reply_special(struct smbsrv_request *req)
{
	uint8_t msg_type;
	uint8_t *buf = talloc_zero_array(req, uint8_t, 4);

	msg_type = CVAL(req->in.buffer,0);

	SIVAL(buf, 0, 0);
	
	switch (msg_type) {
	case 0x81: /* session request */
		if (req->smb_conn->negotiate.done_nbt_session) {
			DEBUG(0,("Warning: ignoring secondary session request\n"));
			return;
		}
		
		SCVAL(buf,0,0x82);
		SCVAL(buf,3,0);

		/* we don't check the status - samba always accepts session
		   requests for any name */
		parse_session_request(req);

		req->out.buffer = buf;
		req->out.size = 4;
		smbsrv_send_reply_nosign(req);
		return;
		
	case 0x89: /* session keepalive request 
		      (some old clients produce this?) */
		SCVAL(buf, 0, SMBkeepalive);
		SCVAL(buf, 3, 0);
		req->out.buffer = buf;
		req->out.size = 4;
		smbsrv_send_reply_nosign(req);
		return;
		
	case SMBkeepalive: 
		/* session keepalive - swallow it */
		talloc_free(req);
		return;
	}

	DEBUG(0,("Unexpected NBT session packet (%d)\n", msg_type));
	talloc_free(req);
}
