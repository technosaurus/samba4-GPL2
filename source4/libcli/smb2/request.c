/* 
   Unix SMB/CIFS implementation.

   SMB2 client request handling

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
#include "libcli/raw/libcliraw.h"
#include "libcli/smb2/smb2.h"
#include "include/dlinklist.h"
#include "lib/events/events.h"
#include "librpc/gen_ndr/ndr_misc.h"

/*
  initialise a smb2 request
*/
struct smb2_request *smb2_request_init(struct smb2_transport *transport, 
				       uint16_t opcode, uint32_t body_size)
{
	struct smb2_request *req;

	req = talloc(transport, struct smb2_request);
	if (req == NULL) return NULL;

	req->state     = SMB2_REQUEST_INIT;
	req->transport = transport;
	req->seqnum    = transport->seqnum++;
	req->status    = NT_STATUS_OK;
	req->async.fn  = NULL;
	req->next = req->prev = NULL;

	ZERO_STRUCT(req->in);
	
	req->out.allocated = SMB2_HDR_BODY+NBT_HDR_SIZE+body_size;
	req->out.buffer    = talloc_size(req, req->out.allocated);
	if (req->out.buffer == NULL) {
		talloc_free(req);
		return NULL;
	}

	req->out.size      = SMB2_HDR_BODY+NBT_HDR_SIZE + body_size;
	req->out.hdr       = req->out.buffer + NBT_HDR_SIZE;
	req->out.body      = req->out.hdr + SMB2_HDR_BODY;
	req->out.body_size = body_size;
	req->out.ptr       = req->out.body;

	SIVAL(req->out.hdr, 0,                SMB2_MAGIC);
	SSVAL(req->out.hdr, SMB2_HDR_LENGTH,  SMB2_HDR_BODY);
	SSVAL(req->out.hdr, SMB2_HDR_PAD1,    0);
	SIVAL(req->out.hdr, SMB2_HDR_STATUS,  0);
	SSVAL(req->out.hdr, SMB2_HDR_OPCODE,  opcode);
	SSVAL(req->out.hdr, SMB2_HDR_PAD2,    0);
	SIVAL(req->out.hdr, SMB2_HDR_FLAGS,   0);
	SIVAL(req->out.hdr, SMB2_HDR_UNKNOWN, 0);
	SBVAL(req->out.hdr, SMB2_HDR_SEQNUM,  req->seqnum);
	SIVAL(req->out.hdr, SMB2_HDR_PID,     0);
	SIVAL(req->out.hdr, SMB2_HDR_TID,     0);
	SIVAL(req->out.hdr, SMB2_HDR_UID,     0);
	SIVAL(req->out.hdr, SMB2_HDR_UID2,    0);
	memset(req->out.hdr+SMB2_HDR_SIG, 0, 16);

	return req;
}

/* destroy a request structure and return final status */
NTSTATUS smb2_request_destroy(struct smb2_request *req)
{
	NTSTATUS status;

	/* this is the error code we give the application for when a
	   _send() call fails completely */
	if (!req) return NT_STATUS_UNSUCCESSFUL;

	if (req->transport) {
		/* remove it from the list of pending requests (a null op if
		   its not in the list) */
		DLIST_REMOVE(req->transport->pending_recv, req);
	}

	if (req->state == SMBCLI_REQUEST_ERROR &&
	    NT_STATUS_IS_OK(req->status)) {
		req->status = NT_STATUS_INTERNAL_ERROR;
	}

	status = req->status;
	talloc_free(req);
	return status;
}

/*
  receive a response to a packet
*/
BOOL smb2_request_receive(struct smb2_request *req)
{
	/* req can be NULL when a send has failed. This eliminates lots of NULL
	   checks in each module */
	if (!req) return False;

	/* keep receiving packets until this one is replied to */
	while (req->state <= SMB2_REQUEST_RECV) {
		if (event_loop_once(req->transport->socket->event.ctx) != 0) {
			return False;
		}
	}

	return req->state == SMB2_REQUEST_DONE;
}

/* Return true if the last packet was in error */
BOOL smb2_request_is_error(struct smb2_request *req)
{
	return NT_STATUS_IS_ERR(req->status);
}

/*
  check if a range in the reply body is out of bounds
*/
BOOL smb2_oob(struct smb2_request *req, const uint8_t *ptr, uint_t size)
{
	/* be careful with wraparound! */
	if (ptr < req->in.body ||
	    ptr >= req->in.body + req->in.body_size ||
	    size > req->in.body_size ||
	    ptr + size > req->in.body + req->in.body_size) {
		return True;
	}
	return False;
}

/*
  pull a data blob from the body of a reply
*/
DATA_BLOB smb2_pull_blob(struct smb2_request *req, uint8_t *ptr, uint_t size)
{
	if (smb2_oob(req, ptr, size)) {
		return data_blob(NULL, 0);
	}
	return data_blob_talloc(req, ptr, size);
}

/*
  pull a guid from the reply body
*/
NTSTATUS smb2_pull_guid(struct smb2_request *req, uint8_t *ptr, struct GUID *guid)
{
	NTSTATUS status;
	DATA_BLOB blob = smb2_pull_blob(req, ptr, 16);
	if (blob.data == NULL) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}
	status = ndr_pull_struct_blob(&blob, req, guid, 
				      (ndr_pull_flags_fn_t)ndr_pull_GUID);
	data_blob_free(&blob);
	return status;
}
