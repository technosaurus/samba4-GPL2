/* 
   Unix SMB/CIFS implementation.
   client file read/write routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) James Myers 2003
   
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

#define SETUP_REQUEST(cmd, wct, buflen) do { \
	req = cli_request_setup(tree, cmd, wct, buflen); \
	if (!req) return NULL; \
} while (0)


/****************************************************************************
 low level read operation (async send)
****************************************************************************/
struct cli_request *smb_raw_read_send(struct cli_tree *tree, union smb_read *parms)
{
	BOOL bigoffset = False;
	struct cli_request *req; 

	switch (parms->generic.level) {
	case RAW_READ_GENERIC:
		return NULL;
		
	case RAW_READ_READBRAW:
		if (tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES) {
			bigoffset = True;
		}
		SETUP_REQUEST(SMBreadbraw, bigoffset? 10:8, 0);
		SSVAL(req->out.vwv, VWV(0), parms->readbraw.in.fnum);
		SIVAL(req->out.vwv, VWV(1), parms->readbraw.in.offset);
		SSVAL(req->out.vwv, VWV(3), parms->readbraw.in.maxcnt);
		SSVAL(req->out.vwv, VWV(4), parms->readbraw.in.mincnt);
		SIVAL(req->out.vwv, VWV(5), parms->readbraw.in.timeout);
		SSVAL(req->out.vwv, VWV(7), 0); /* reserved */
		if (bigoffset) {
			SIVAL(req->out.vwv, VWV(8),parms->readbraw.in.offset>>32);
		}
		break;

	case RAW_READ_LOCKREAD:
		SETUP_REQUEST(SMBlockread, 5, 0);
		SSVAL(req->out.vwv, VWV(0), parms->lockread.in.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->lockread.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->lockread.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->lockread.in.remaining);
		break;

	case RAW_READ_READ:
		SETUP_REQUEST(SMBread, 5, 0);
		SSVAL(req->out.vwv, VWV(0), parms->read.in.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->read.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->read.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->read.in.remaining);
		break;

	case RAW_READ_READX:
		if (tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES) {
			bigoffset = True;
		}
		SETUP_REQUEST(SMBreadX, bigoffset ? 12 : 10, 0);
		SSVAL(req->out.vwv, VWV(0), 0xFF);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->readx.in.fnum);
		SIVAL(req->out.vwv, VWV(3), parms->readx.in.offset);
		SSVAL(req->out.vwv, VWV(5), parms->readx.in.maxcnt);
		SSVAL(req->out.vwv, VWV(6), parms->readx.in.mincnt);
		SIVAL(req->out.vwv, VWV(7), 0); /* reserved */
		SSVAL(req->out.vwv, VWV(9), parms->readx.in.remaining);
		if (bigoffset) {
			SIVAL(req->out.vwv, VWV(10),parms->readx.in.offset>>32);
		}
		break;
	}

	if (!cli_request_send(req)) {
		cli_request_destroy(req);
		return NULL;
	}

	/* the transport layer needs to know that a readbraw is pending
	   and handle receives a little differently */
	if (parms->generic.level == RAW_READ_READBRAW) {
		tree->session->transport->readbraw_pending = 1;
	}

	return req;
}

/****************************************************************************
 low level read operation (async recv)
****************************************************************************/
NTSTATUS smb_raw_read_recv(struct cli_request *req, union smb_read *parms)
{
	if (!cli_request_receive(req) ||
	    cli_request_is_error(req)) {
		goto failed;
	}

	switch (parms->generic.level) {
	case RAW_READ_GENERIC:
		/* handled in _send() */
		break;

	case RAW_READ_READBRAW:
		parms->readbraw.out.nread = req->in.size - NBT_HDR_SIZE;
		if (parms->readbraw.out.nread > 
		    MAX(parms->readx.in.mincnt, parms->readx.in.maxcnt)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
			goto failed;
		}
		memcpy(parms->readbraw.out.data, req->in.buffer + NBT_HDR_SIZE, parms->readbraw.out.nread);
		break;
		
	case RAW_READ_LOCKREAD:
		CLI_CHECK_WCT(req, 5);
		parms->lockread.out.nread = SVAL(req->in.vwv, VWV(0));
		if (parms->lockread.out.nread > parms->lockread.in.count ||
		    !cli_raw_pull_data(req, req->in.data+3, 
				       parms->lockread.out.nread, parms->lockread.out.data)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
		}
		break;

	case RAW_READ_READ:
		/* there are 4 reserved words in the reply */
		CLI_CHECK_WCT(req, 5);
		parms->read.out.nread = SVAL(req->in.vwv, VWV(0));
		if (parms->read.out.nread > parms->read.in.count ||
		    !cli_raw_pull_data(req, req->in.data+3, 
				       parms->read.out.nread, parms->read.out.data)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
		}
		break;

	case RAW_READ_READX:
		/* there are 5 reserved words in the reply */
		CLI_CHECK_WCT(req, 12);
		parms->readx.out.remaining       = SVAL(req->in.vwv, VWV(2));
		parms->readx.out.compaction_mode = SVAL(req->in.vwv, VWV(3));
		parms->readx.out.nread = SVAL(req->in.vwv, VWV(5));
		if (parms->readx.out.nread > MAX(parms->readx.in.mincnt, parms->readx.in.maxcnt) ||
		    !cli_raw_pull_data(req, req->in.hdr + SVAL(req->in.vwv, VWV(6)), 
				       parms->readx.out.nread, 
				       parms->readx.out.data)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
		}
		break;
	}

failed:
	return cli_request_destroy(req);
}

/****************************************************************************
 low level read operation (sync interface)
****************************************************************************/
NTSTATUS smb_raw_read(struct cli_tree *tree, union smb_read *parms)
{
	struct cli_request *req = smb_raw_read_send(tree, parms);
	return smb_raw_read_recv(req, parms);
}


/****************************************************************************
 raw write interface (async send)
****************************************************************************/
struct cli_request *smb_raw_write_send(struct cli_tree *tree, union smb_write *parms)
{
	BOOL bigoffset = False;
	struct cli_request *req; 

	switch (parms->generic.level) {
	case RAW_WRITE_GENERIC:
		return NULL;
		
	case RAW_WRITE_WRITEUNLOCK:
		SETUP_REQUEST(SMBwriteunlock, 5, 3 + parms->writeunlock.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->writeunlock.in.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->writeunlock.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->writeunlock.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->writeunlock.in.remaining);
		SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
		SSVAL(req->out.data, 1, parms->writeunlock.in.count);
		if (parms->writeunlock.in.count > 0) {
			memcpy(req->out.data+3, parms->writeunlock.in.data, 
			       parms->writeunlock.in.count);
		}
		break;

	case RAW_WRITE_WRITE:
		SETUP_REQUEST(SMBwrite, 5,  3 + parms->write.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->write.in.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->write.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->write.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->write.in.remaining);
		SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
		SSVAL(req->out.data, 1, parms->write.in.count);
		if (parms->write.in.count > 0) {
			memcpy(req->out.data+3, parms->write.in.data, parms->write.in.count);
		}
		break;

	case RAW_WRITE_WRITECLOSE:
		SETUP_REQUEST(SMBwriteclose, 6, 1 + parms->writeclose.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->writeclose.in.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->writeclose.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->writeclose.in.offset);
		put_dos_date3(req->out.vwv, VWV(4), parms->writeclose.in.mtime);
		SCVAL(req->out.data, 0, 0);
		if (parms->writeclose.in.count > 0) {
			memcpy(req->out.data+1, parms->writeclose.in.data, 
			       parms->writeclose.in.count);
		}
		break;

	case RAW_WRITE_WRITEX:
		if (tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES) {
			bigoffset = True;
		}
		SETUP_REQUEST(SMBwriteX, bigoffset ? 14 : 12, parms->writex.in.count);
		SSVAL(req->out.vwv, VWV(0), 0xFF);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->writex.in.fnum);
		SIVAL(req->out.vwv, VWV(3), parms->writex.in.offset);
		SIVAL(req->out.vwv, VWV(5), 0); /* reserved */
		SSVAL(req->out.vwv, VWV(7), parms->writex.in.wmode);
		SSVAL(req->out.vwv, VWV(8), parms->writex.in.remaining);
		SSVAL(req->out.vwv, VWV(9), parms->writex.in.count>>16);
		SSVAL(req->out.vwv, VWV(10), parms->writex.in.count);
		SSVAL(req->out.vwv, VWV(11), PTR_DIFF(req->out.data, req->out.hdr));
		if (bigoffset) {
	      		SIVAL(req->out.vwv,VWV(12),parms->writex.in.offset>>32);
		}
	      	if (parms->writex.in.count > 0) {
			memcpy(req->out.data, parms->writex.in.data, parms->writex.in.count);
		}
		break;

	case RAW_WRITE_SPLWRITE:
		SETUP_REQUEST(SMBsplwr, 1, parms->splwrite.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->splwrite.in.fnum);
		if (parms->splwrite.in.count > 0) {
			memcpy(req->out.data, parms->splwrite.in.data, parms->splwrite.in.count);
		}
		break;
	}

	if (!cli_request_send(req)) {
cli_request_destroy(req);
		return NULL;
	}

	return req;
}


/****************************************************************************
 raw write interface (async recv)
****************************************************************************/
NTSTATUS smb_raw_write_recv(struct cli_request *req, union smb_write *parms)
{
	if (!cli_request_receive(req) ||
	    cli_request_is_error(req)) {
		goto failed;
	}

	switch (parms->generic.level) {
	case RAW_WRITE_GENERIC:
		break;
	case RAW_WRITE_WRITEUNLOCK:
		CLI_CHECK_WCT(req, 1);		
		parms->writeunlock.out.nwritten = SVAL(req->in.vwv, VWV(0));
		break;
	case RAW_WRITE_WRITE:
		CLI_CHECK_WCT(req, 1);
		parms->write.out.nwritten = SVAL(req->in.vwv, VWV(0));
		break;
	case RAW_WRITE_WRITECLOSE:
		CLI_CHECK_WCT(req, 1);
		parms->writeclose.out.nwritten = SVAL(req->in.vwv, VWV(0));
		break;
	case RAW_WRITE_WRITEX:
		CLI_CHECK_WCT(req, 6);
		parms->writex.out.nwritten  = SVAL(req->in.vwv, VWV(2));
		parms->writex.out.nwritten += (CVAL(req->in.vwv, VWV(4)) << 16);
		parms->writex.out.remaining = SVAL(req->in.vwv, VWV(3));
		break;
	case RAW_WRITE_SPLWRITE:
		break;
	}

failed:
	return cli_request_destroy(req);
}

/****************************************************************************
 raw write interface (sync interface)
****************************************************************************/
NTSTATUS smb_raw_write(struct cli_tree *tree, union smb_write *parms)
{
	struct cli_request *req = smb_raw_write_send(tree, parms);
	return smb_raw_write_recv(req, parms);
}
