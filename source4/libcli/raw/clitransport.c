/* 
   Unix SMB/CIFS implementation.
   SMB client transport context management functions

   Copyright (C) Andrew Tridgell 1994-2005
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
#include "system/time.h"
#include "dlinklist.h"
#include "events.h"


static void smbcli_transport_process_recv(struct smbcli_transport *transport);
static void smbcli_transport_process_send(struct smbcli_transport *transport);

/*
  an event has happened on the socket
*/
static void smbcli_transport_event_handler(struct event_context *ev, 
					   struct fd_event *fde, 
					   struct timeval t, 
					   uint16_t flags, void *private)
{
	struct smbcli_transport *transport = talloc_get_type(private,
							     struct smbcli_transport);
	if (flags & EVENT_FD_READ) {
		smbcli_transport_process_recv(transport);
		return;
	}
	if (flags & EVENT_FD_WRITE) {
		smbcli_transport_process_send(transport);
	}
}

/*
  destroy a transport
 */
static int transport_destructor(void *ptr)
{
	struct smbcli_transport *transport = ptr;

	smbcli_transport_dead(transport);
	return 0;
}

/*
  create a transport structure based on an established socket
*/
struct smbcli_transport *smbcli_transport_init(struct smbcli_socket *sock,
					       TALLOC_CTX *parent_ctx, BOOL primary)
{
	struct smbcli_transport *transport;

	transport = talloc_zero(parent_ctx, struct smbcli_transport);
	if (!transport) return NULL;

	if (primary) {
		transport->socket = talloc_steal(transport, sock);
	} else {
		transport->socket = talloc_reference(transport, sock);
	}
	transport->negotiate.protocol = PROTOCOL_NT1;
	transport->options.use_spnego = lp_use_spnego();
	transport->options.max_xmit = lp_max_xmit();
	transport->options.max_mux = lp_maxmux();

	transport->negotiate.max_xmit = transport->options.max_xmit;
	
	smbcli_init_signing(transport);

	ZERO_STRUCT(transport->called);

	/* take over event handling from the socket layer - it only
	   handles events up until we are connected */
	talloc_free(transport->socket->event.fde);
	transport->socket->event.fde = event_add_fd(transport->socket->event.ctx,
						    transport->socket,
						    socket_get_fd(transport->socket->sock),
						    EVENT_FD_READ,
						    smbcli_transport_event_handler,
						    transport);

	talloc_set_destructor(transport, transport_destructor);

	return transport;
}

/*
  mark the transport as dead
*/
void smbcli_transport_dead(struct smbcli_transport *transport)
{
	smbcli_sock_dead(transport->socket);

	/* all pending sends become errors */
	while (transport->pending_send) {
		struct smbcli_request *req = transport->pending_send;
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = NT_STATUS_NET_WRITE_FAULT;
		DLIST_REMOVE(transport->pending_send, req);
		if (req->async.fn) {
			req->async.fn(req);
		}
	}

	/* as do all pending receives */
	while (transport->pending_recv) {
		struct smbcli_request *req = transport->pending_recv;
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = NT_STATUS_NET_WRITE_FAULT;
		DLIST_REMOVE(transport->pending_recv, req);
		if (req->async.fn) {
			req->async.fn(req);
		}
	}
}


/*
  enable select for write on a transport
*/
static void smbcli_transport_write_enable(struct smbcli_transport *transport)
{
	struct fd_event *fde = transport->socket->event.fde;
	EVENT_FD_WRITEABLE(fde);
}

/*
  disable select for write on a transport
*/
static void smbcli_transport_write_disable(struct smbcli_transport *transport)
{
	struct fd_event *fde = transport->socket->event.fde;
	EVENT_FD_NOT_WRITEABLE(fde);
}

/*
  send a session request
*/
struct smbcli_request *smbcli_transport_connect_send(struct smbcli_transport *transport,
						     struct nbt_name *calling, 
						     struct nbt_name *called)
{
	uint8_t *p;
	struct smbcli_request *req;
	DATA_BLOB calling_blob, called_blob;
	TALLOC_CTX *tmp_ctx = talloc_new(transport);
	NTSTATUS status;

	status = nbt_name_dup(transport, called, &transport->called);
	if (!NT_STATUS_IS_OK(status)) goto failed;
	
	status = nbt_name_to_blob(tmp_ctx, &calling_blob, calling);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	status = nbt_name_to_blob(tmp_ctx, &called_blob, called);
	if (!NT_STATUS_IS_OK(status)) goto failed;

  	/* allocate output buffer */
	req = smbcli_request_setup_nonsmb(transport, 
					  NBT_HDR_SIZE + 
					  calling_blob.length + called_blob.length);
	if (req == NULL) goto failed;

	/* put in the destination name */
	p = req->out.buffer + NBT_HDR_SIZE;
	memcpy(p, called_blob.data, called_blob.length);
	p += called_blob.length;

	memcpy(p, calling_blob.data, calling_blob.length);
	p += calling_blob.length;

	_smb_setlen(req->out.buffer, PTR_DIFF(p, req->out.buffer)-4);
	SCVAL(req->out.buffer,0,0x81);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		goto failed;
	}

	talloc_free(tmp_ctx);
	return req;

failed:
	talloc_free(tmp_ctx);
	return NULL;
}

/*
  map a session request error to a NTSTATUS
 */
static NTSTATUS map_session_refused_error(uint8_t error)
{
	switch (error) {
	case 0x80:
	case 0x81:
		return NT_STATUS_REMOTE_NOT_LISTENING;
	case 0x82:
		return NT_STATUS_RESOURCE_NAME_NOT_FOUND;
	case 0x83:
		return NT_STATUS_REMOTE_RESOURCES;
	}
	return NT_STATUS_UNEXPECTED_IO_ERROR;
}


/*
  finish a smbcli_transport_connect()
*/
NTSTATUS smbcli_transport_connect_recv(struct smbcli_request *req)
{
	NTSTATUS status;

	if (!smbcli_request_receive(req)) {
		smbcli_request_destroy(req);
		return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	switch (CVAL(req->in.buffer,0)) {
	case 0x82:
		status = NT_STATUS_OK;
		break;
	case 0x83:
		status = map_session_refused_error(CVAL(req->in.buffer,4));
		break;
	case 0x84:
		DEBUG(1,("Warning: session retarget not supported\n"));
		status = NT_STATUS_NOT_SUPPORTED;
		break;
	default:
		status = NT_STATUS_UNEXPECTED_IO_ERROR;
		break;
	}

	smbcli_request_destroy(req);
	return status;
}


/*
  send a session request (if needed)
*/
BOOL smbcli_transport_connect(struct smbcli_transport *transport,
			      struct nbt_name *calling, 
			      struct nbt_name *called)
{
	struct smbcli_request *req;
	NTSTATUS status;

	if (transport->socket->port == 445) {
		return True;
	}

	req = smbcli_transport_connect_send(transport, 
					    calling, called);
	status = smbcli_transport_connect_recv(req);
	return NT_STATUS_IS_OK(status);
}

/****************************************************************************
get next mid in sequence
****************************************************************************/
uint16_t smbcli_transport_next_mid(struct smbcli_transport *transport)
{
	uint16_t mid;
	struct smbcli_request *req;

	mid = transport->next_mid;

again:
	/* now check to see if this mid is being used by one of the 
	   pending requests. This is quite efficient because the list is
	   usually very short */

	/* the zero mid is reserved for requests that don't have a mid */
	if (mid == 0) mid = 1;

	for (req=transport->pending_recv; req; req=req->next) {
		if (req->mid == mid) {
			mid++;
			goto again;
		}
	}

	transport->next_mid = mid+1;
	return mid;
}

static void idle_handler(struct event_context *ev, 
			 struct timed_event *te, struct timeval t, void *private)
{
	struct smbcli_transport *transport = talloc_get_type(private,
							     struct smbcli_transport);
	struct timeval next = timeval_add(&t, 0, transport->idle.period);
	transport->socket->event.te = event_add_timed(transport->socket->event.ctx, 
						      transport,
						      next,
						      idle_handler, transport);
	transport->idle.func(transport, transport->idle.private);
}

/*
  setup the idle handler for a transport
  the period is in microseconds
*/
void smbcli_transport_idle_handler(struct smbcli_transport *transport, 
				   void (*idle_func)(struct smbcli_transport *, void *),
				   uint64_t period,
				   void *private)
{
	transport->idle.func = idle_func;
	transport->idle.private = private;
	transport->idle.period = period;

	if (transport->socket->event.te != NULL) {
		talloc_free(transport->socket->event.te);
	}

	transport->socket->event.te = event_add_timed(transport->socket->event.ctx, 
						      transport,
						      timeval_current_ofs(0, period),
						      idle_handler, transport);
}

/*
  process some pending sends
*/
static void smbcli_transport_process_send(struct smbcli_transport *transport)
{
	while (transport->pending_send) {
		struct smbcli_request *req = transport->pending_send;
		ssize_t ret;
		ret = smbcli_sock_write(transport->socket, req->out.buffer, req->out.size);
		if (ret == -1) {
			if (errno == EAGAIN || errno == EINTR) {
				return;
			}
			smbcli_transport_dead(transport);
			return;
		}
		req->out.buffer += ret;
		req->out.size -= ret;
		if (req->out.size == 0) {
			DLIST_REMOVE(transport->pending_send, req);
			if (req->one_way_request) {
				req->state = SMBCLI_REQUEST_DONE;
				smbcli_request_destroy(req);
			} else {
				req->state = SMBCLI_REQUEST_RECV;
				DLIST_ADD(transport->pending_recv, req);
			}
		}
	}

	/* we're out of requests to send, so don't wait for write
	   events any more */
	smbcli_transport_write_disable(transport);
}

/*
  we have a full request in our receive buffer - match it to a pending request
  and process
 */
static void smbcli_transport_finish_recv(struct smbcli_transport *transport)
{
	uint8_t *buffer, *hdr, *vwv;
	int len;
	uint16_t wct=0, mid = 0;
	struct smbcli_request *req;

	buffer = transport->recv_buffer.buffer;
	len = transport->recv_buffer.req_size;

	ZERO_STRUCT(transport->recv_buffer);

	hdr = buffer+NBT_HDR_SIZE;
	vwv = hdr + HDR_VWV;

	/* see if it could be an oplock break request */
	if (handle_oplock_break(transport, len, hdr, vwv)) {
		talloc_free(buffer);
		return;
	}

	/* at this point we need to check for a readbraw reply, as
	   these can be any length */
	if (transport->readbraw_pending) {
		transport->readbraw_pending = 0;

		/* it must match the first entry in the pending queue
		   as the client is not allowed to have outstanding
		   readbraw requests */
		req = transport->pending_recv;
		if (!req) goto error;

		req->in.buffer = buffer;
		talloc_steal(req, buffer);
		req->in.size = len;
		req->in.allocated = req->in.size;
		goto async;
	}

	if (len >= MIN_SMB_SIZE) {
		/* extract the mid for matching to pending requests */
		mid = SVAL(hdr, HDR_MID);
		wct = CVAL(hdr, HDR_WCT);
	}

	/* match the incoming request against the list of pending requests */
	for (req=transport->pending_recv; req; req=req->next) {
		if (req->mid == mid) break;
	}

	if (!req) {
		DEBUG(1,("Discarding unmatched reply with mid %d op %d\n", 
			 mid, CVAL(hdr, HDR_COM)));
		goto error;
	}

	/* fill in the 'in' portion of the matching request */
	req->in.buffer = buffer;
	talloc_steal(req, buffer);
	req->in.size = len;
	req->in.allocated = req->in.size;

	/* handle NBT session replies */
	if (req->in.size >= 4 && req->in.buffer[0] != 0) {
		req->status = NT_STATUS_OK;
		goto async;
	}

	/* handle non-SMB replies */
	if (req->in.size < NBT_HDR_SIZE + MIN_SMB_SIZE) {
		req->state = SMBCLI_REQUEST_ERROR;
		goto error;
	}

	if (req->in.size < NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct)) {
		DEBUG(2,("bad reply size for mid %d\n", mid));
		req->status = NT_STATUS_UNSUCCESSFUL;
		req->state = SMBCLI_REQUEST_ERROR;
		goto error;
	}

	req->in.hdr = hdr;
	req->in.vwv = vwv;
	req->in.wct = wct;
	if (req->in.size >= NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct)) {
		req->in.data = req->in.vwv + VWV(wct) + 2;
		req->in.data_size = SVAL(req->in.vwv, VWV(wct));
		if (req->in.size < NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct) + req->in.data_size) {
			DEBUG(3,("bad data size for mid %d\n", mid));
			/* blergh - w2k3 gives a bogus data size values in some
			   openX replies */
			req->in.data_size = req->in.size - (NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct));
		}
	}
	req->in.ptr = req->in.data;
	req->flags2 = SVAL(req->in.hdr, HDR_FLG2);

	if (!(req->flags2 & FLAGS2_32_BIT_ERROR_CODES)) {
		transport->error.etype = ETYPE_DOS;
		transport->error.e.dos.eclass = CVAL(req->in.hdr,HDR_RCLS);
		transport->error.e.dos.ecode = SVAL(req->in.hdr,HDR_ERR);
		req->status = dos_to_ntstatus(transport->error.e.dos.eclass, 
					      transport->error.e.dos.ecode);
	} else {
		transport->error.etype = ETYPE_NT;
		transport->error.e.nt_status = NT_STATUS(IVAL(req->in.hdr, HDR_RCLS));
		req->status = transport->error.e.nt_status;
	}

	if (!smbcli_request_check_sign_mac(req)) {
		transport->error.etype = ETYPE_SOCKET;
		transport->error.e.socket_error = SOCKET_READ_BAD_SIG;
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = NT_STATUS_ACCESS_DENIED;
		goto error;
	};

async:
	/* if this request has an async handler then call that to
	   notify that the reply has been received. This might destroy
	   the request so it must happen last */
	DLIST_REMOVE(transport->pending_recv, req);
	req->state = SMBCLI_REQUEST_DONE;
	if (req->async.fn) {
		req->async.fn(req);
	}
	return;

error:
	if (req) {
		DLIST_REMOVE(transport->pending_recv, req);
		req->state = SMBCLI_REQUEST_ERROR;
	}
}

/*
  process some pending receives
*/
static void smbcli_transport_process_recv(struct smbcli_transport *transport)
{
	/* a incoming packet goes through 2 stages - first we read the
	   4 byte header, which tells us how much more is coming. Then
	   we read the rest */
	if (transport->recv_buffer.received < NBT_HDR_SIZE) {
		ssize_t ret;
		ret = smbcli_sock_read(transport->socket, 
				    transport->recv_buffer.header + 
				    transport->recv_buffer.received,
				    NBT_HDR_SIZE - transport->recv_buffer.received);
		if (ret == -1) {
			smbcli_transport_dead(transport);
			return;
		}

		transport->recv_buffer.received += ret;

		if (transport->recv_buffer.received == NBT_HDR_SIZE) {
			/* we've got a full header */
			transport->recv_buffer.req_size = smb_len(transport->recv_buffer.header) + NBT_HDR_SIZE;
			transport->recv_buffer.buffer = talloc_size(transport,
								    NBT_HDR_SIZE+transport->recv_buffer.req_size);
			if (transport->recv_buffer.buffer == NULL) {
				smbcli_transport_dead(transport);
				return;
			}
			memcpy(transport->recv_buffer.buffer, transport->recv_buffer.header, NBT_HDR_SIZE);
		}
	}

	if (transport->recv_buffer.received < transport->recv_buffer.req_size) {
		ssize_t ret;
		ret = smbcli_sock_read(transport->socket, 
				    transport->recv_buffer.buffer + 
				    transport->recv_buffer.received,
				    transport->recv_buffer.req_size - 
				    transport->recv_buffer.received);
		if (ret == -1) {
			smbcli_transport_dead(transport);
			return;
		}
		transport->recv_buffer.received += ret;
	}

	if (transport->recv_buffer.received != 0 &&
	    transport->recv_buffer.received == transport->recv_buffer.req_size) {
		smbcli_transport_finish_recv(transport);
	}
}

/*
  process some read/write requests that are pending
  return False if the socket is dead
*/
BOOL smbcli_transport_process(struct smbcli_transport *transport)
{
	smbcli_transport_process_send(transport);
	smbcli_transport_process_recv(transport);
	if (transport->socket->sock == NULL) {
		return False;
	}
	return True;
}



/*
  put a request into the send queue
*/
void smbcli_transport_send(struct smbcli_request *req)
{
	/* check if the transport is dead */
	if (req->transport->socket->sock == NULL) {
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = NT_STATUS_NET_WRITE_FAULT;
		return;
	}

	/* put it on the outgoing socket queue */
	req->state = SMBCLI_REQUEST_SEND;
	DLIST_ADD_END(req->transport->pending_send, req, struct smbcli_request *);

	/* make sure we look for write events */
	smbcli_transport_write_enable(req->transport);
}
