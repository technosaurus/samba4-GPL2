/* 
   Unix SMB/CIFS implementation.

   Samba internal messaging functions

   Copyright (C) Andrew Tridgell 2004
   
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

/* change the message version with any incompatible changes in the protocol */
#define MESSAGING_VERSION 1

struct messaging_state {
	servid_t server_id;
	struct socket_context *sock;
	char *path;
	struct dispatch_fn *dispatch;

	struct {
		struct event_context *ev;
		struct fd_event *fde;
	} event;
};

/* we have a linked list of dispatch handlers that this messaging
   server can deal with */
struct dispatch_fn {
	struct dispatch_fn *next, *prev;
	uint32_t msg_type;
	void *private;
	void (*fn)(void *msg_ctx, void *private, 
		   uint32_t msg_type, servid_t server_id, DATA_BLOB *data);
};

/* an individual message */
struct messaging_rec {
	struct messaging_state *msg;
	struct socket_context *sock;
	struct fd_event *fde;
	const char *path;

	struct {
		uint32_t version;
		uint32_t msg_type;
		servid_t from;
		servid_t to;
		uint32_t length;
	} header;

	DATA_BLOB data;

	uint32_t ndone;
};

/*
 A useful function for testing the message system.
*/
static void ping_message(void *msg_ctx, void *private, 
			 uint32_t msg_type, servid_t src, DATA_BLOB *data)
{
	DEBUG(1,("INFO: Received PING message from server %u [%.*s]\n",
		 (uint_t)src, data->length, data->data?(const char *)data->data:""));
	messaging_send(msg_ctx, src, MSG_PONG, data);
}

/* 
   return the path to a messaging socket
*/
static char *messaging_path(TALLOC_CTX *mem_ctx, servid_t server_id)
{
	char *name = talloc_asprintf(mem_ctx, "messaging/msg.%u", (unsigned)server_id);
	char *ret;
	ret = lock_path(mem_ctx, name);
	talloc_free(name);
	return ret;
}

/*
  dispatch a fully received message
*/
static void messaging_dispatch(struct messaging_state *msg, struct messaging_rec *rec)
{
	struct dispatch_fn *d;
	for (d=msg->dispatch;d;d=d->next) {
		if (d->msg_type == rec->header.msg_type) {
			d->fn(msg, d->private, d->msg_type, rec->header.from, &rec->data);
		}
	}

	/* we don't free the record itself here as there may
	   be more messages from this client */
	data_blob_free(&rec->data);
	rec->header.length = 0;
	rec->ndone = 0;
}


/*
  handle IO for a single message
*/
static void messaging_recv_handler(struct event_context *ev, struct fd_event *fde, 
				 time_t t, uint16_t flags)
{
	struct messaging_rec *rec = fde->private;
	struct messaging_state *msg = rec->msg;
	NTSTATUS status;

	if (rec->ndone < sizeof(rec->header)) {
		/* receive the header */
		DATA_BLOB blob;
		blob.length = 0;
		status = socket_recv(rec->sock, rec, 
				     &blob, sizeof(rec->header) - rec->ndone, 0);
		if (NT_STATUS_IS_ERR(status)) {
			talloc_free(rec);
			return;
		}

		if (blob.length == 0) {
			return;
		}

		memcpy(rec->ndone + (char *)&rec->header, blob.data, blob.length);
		rec->ndone += blob.length;
		data_blob_free(&blob);

		if (rec->ndone == sizeof(rec->header)) {
			if (rec->header.version != MESSAGING_VERSION) {
				DEBUG(0,("meessage with wrong version %u\n",
					 rec->header.version));
				talloc_free(rec);
			}
			rec->data = data_blob_talloc(rec, NULL, rec->header.length);
			if (rec->data.length != rec->header.length) {
				DEBUG(0,("Unable to allocate message of size %u\n",
					 rec->header.length));
				talloc_free(rec);
			}
		}
	}

	if (rec->ndone >= sizeof(rec->header) && 
	    rec->ndone < sizeof(rec->header) + rec->header.length) {
		/* receive the body, if any */
		DATA_BLOB blob;
		blob.length = 0;
		status = socket_recv(rec->sock, rec, 
				     &blob, sizeof(rec->header) + rec->header.length - rec->ndone, 0);
		if (NT_STATUS_IS_ERR(status)) {
			talloc_free(rec);
			return;
		}

		if (blob.length == 0) {
			return;
		}

		memcpy(rec->data.data + (rec->ndone - sizeof(rec->header)), 
		       blob.data, blob.length);

		rec->ndone += blob.length;
	}

	if (rec->ndone == sizeof(rec->header) + rec->header.length) {
		/* we've got the whole message */
		messaging_dispatch(msg, rec);
	}
}

/*
  destroy a messaging record
*/
static int rec_destructor(void *ptr)
{
	struct messaging_rec *rec = ptr;
	struct messaging_state *msg = rec->msg;
	event_remove_fd(msg->event.ev, rec->fde);
	return 0;
}

/*
  handle a new incoming connection
*/
static void messaging_listen_handler(struct event_context *ev, struct fd_event *fde, 
				     time_t t, uint16_t flags)
{
	struct messaging_state *msg = fde->private;
	struct messaging_rec *rec;
	NTSTATUS status;
	struct fd_event fde2;

	rec = talloc_p(msg, struct messaging_rec);
	if (rec == NULL) {
		smb_panic("Unable to allocate messaging_rec");
	}

	status = socket_accept(msg->sock, &rec->sock, 0);
	if (!NT_STATUS_IS_OK(status)) {
		smb_panic("Unable to accept messaging_rec");
	}
	talloc_steal(rec, rec->sock);

	rec->msg = msg;
	rec->ndone = 0;
	rec->header.length = 0;
	rec->path = msg->path;

	fde2.private 	= rec;
	fde2.fd		= socket_get_fd(rec->sock);
	fde2.flags	= EVENT_FD_READ;
	fde2.handler	= messaging_recv_handler;

	rec->fde	= event_add_fd(msg->event.ev, &fde2);

	talloc_set_destructor(rec, rec_destructor);
}

/*
  Register a dispatch function for a particular message type.
*/
void messaging_register(void *ctx, void *private,
			uint32_t msg_type, 
			void (*fn)(void *, void *, uint32_t, servid_t, DATA_BLOB *))
{
	struct messaging_state *msg = ctx;
	struct dispatch_fn *d;

	d = talloc_p(msg, struct dispatch_fn);
	d->msg_type = msg_type;
	d->private = private;
	d->fn = fn;
	DLIST_ADD(msg->dispatch, d);
}

/*
  De-register the function for a particular message type.
*/
void messaging_deregister(void *ctx, uint32_t msg_type)
{
	struct messaging_state *msg = ctx;
	struct dispatch_fn *d, *next;

	for (d = msg->dispatch; d; d = next) {
		next = d->next;
		if (d->msg_type == msg_type) {
			DLIST_REMOVE(msg->dispatch, d);
			talloc_free(d);
		}
	}	
}



/*
  handle IO for sending a message
*/
static void messaging_send_handler(struct event_context *ev, struct fd_event *fde, 
				   time_t t, uint16_t flags)
{
	struct messaging_rec *rec = fde->private;
	NTSTATUS status;

	if (rec->ndone < sizeof(rec->header)) {
		/* send the header */
		size_t nsent;
		DATA_BLOB blob;

		blob.data = rec->ndone + (char *)&rec->header;
		blob.length = sizeof(rec->header) - rec->ndone;

		status = socket_send(rec->sock, rec, &blob, &nsent, 0);
		if (NT_STATUS_IS_ERR(status)) {
			talloc_free(rec);
			return;
		}

		if (nsent == 0) {
			return;
		}

		rec->ndone += nsent;
	}

	if (rec->ndone >= sizeof(rec->header) && 
	    rec->ndone < sizeof(rec->header) + rec->header.length) {
		/* send the body, if any */
		DATA_BLOB blob;
		size_t nsent;

		blob.data = rec->data.data + (rec->ndone - sizeof(rec->header));
		blob.length = rec->header.length - (rec->ndone - sizeof(rec->header));

		status = socket_send(rec->sock, rec, &blob, &nsent, 0);
		if (NT_STATUS_IS_ERR(status)) {
			talloc_free(rec);
			return;
		}

		rec->ndone += nsent;
	}

	if (rec->ndone == sizeof(rec->header) + rec->header.length) {
		/* we've done the whole message */
		talloc_free(rec);
	}
}


/*
  when the servers listen queue is full we use this to backoff the message
*/
static void messaging_backoff_handler(struct event_context *ev, struct timed_event *te, time_t t)
{
	struct messaging_rec *rec = te->private;
	struct messaging_state *msg = rec->msg;
	NTSTATUS status;
	struct fd_event fde;

	status = socket_connect(rec->sock, NULL, 0, rec->path, 0, 0);
	if (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
		/* backoff again */
		te->next_event = t+1;
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("messaging: Lost message from %u to %u of type %u after backoff - %s\n", 
			 rec->header.from, rec->header.to, rec->header.msg_type, nt_errstr(status)));
		talloc_free(rec);
		return;
	}

	fde.private 	= rec;
	fde.fd		= socket_get_fd(rec->sock);
	fde.flags	= EVENT_FD_WRITE;
	fde.handler	= messaging_send_handler;

	rec->fde	= event_add_fd(msg->event.ev, &fde);

	talloc_set_destructor(rec, rec_destructor);

	messaging_send_handler(msg->event.ev, rec->fde, 0, EVENT_FD_WRITE);
}


/*
  Send a message to a particular server
*/
NTSTATUS messaging_send(void *msg_ctx, servid_t server, uint32_t msg_type, DATA_BLOB *data)
{
	struct messaging_state *msg = msg_ctx;
	struct messaging_rec *rec;
	NTSTATUS status;
	struct fd_event fde;

	rec = talloc_p(msg, struct messaging_rec);
	if (rec == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rec->msg = msg;
	rec->header.version = MESSAGING_VERSION;
	rec->header.msg_type = msg_type;
	rec->header.from = msg->server_id;
	rec->header.to = server;
	rec->header.length = data?data->length:0;
	if (rec->header.length != 0) {
		rec->data = data_blob_talloc(rec, data->data, data->length);
	} else {
		rec->data = data_blob(NULL, 0);
	}
	rec->ndone = 0;

	status = socket_create("unix", SOCKET_TYPE_STREAM, &rec->sock, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(rec);
		return status;
	}
	talloc_steal(rec, rec->sock);

	rec->path = messaging_path(rec, server);

	status = socket_connect(rec->sock, NULL, 0, rec->path, 0, 0);
	if (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
		/* backoff on this message - the servers listen queue is full */
		struct timed_event te;
		te.next_event = time(NULL)+1;
		te.handler = messaging_backoff_handler;
		te.private = rec;
		event_add_timed(msg->event.ev, &te);
		return NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(rec);
		return status;
	}

	fde.private 	= rec;
	fde.fd		= socket_get_fd(rec->sock);
	fde.flags	= EVENT_FD_WRITE;
	fde.handler	= messaging_send_handler;

	rec->fde	= event_add_fd(msg->event.ev, &fde);

	talloc_set_destructor(rec, rec_destructor);

	messaging_send_handler(msg->event.ev, rec->fde, 0, EVENT_FD_WRITE);

	return NT_STATUS_OK;
}


/*
  destroy the messaging context
*/
static int messaging_destructor(void *msg_ctx)
{
	struct messaging_state *msg = msg_ctx;
	event_remove_fd(msg->event.ev, msg->event.fde);
	return 0;
}

/*
  create the listening socket and setup the dispatcher
*/
void *messaging_init(TALLOC_CTX *mem_ctx, servid_t server_id, struct event_context *ev)
{
	struct messaging_state *msg;
	NTSTATUS status;
	struct fd_event fde;

	msg = talloc_p(mem_ctx, struct messaging_state);
	if (msg == NULL) {
		return NULL;
	}

	/* create the messaging directory if needed */
	msg->path = lock_path(msg, "messaging");
	mkdir(msg->path, 0700);
	talloc_free(msg->path);

	msg->server_id = server_id;
	msg->dispatch = NULL;
	msg->path = messaging_path(msg, server_id);

	status = socket_create("unix", SOCKET_TYPE_STREAM, &msg->sock, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(msg);
		return NULL;
	}

	/* by stealing here we ensure that the socket is cleaned up (and even 
	   deleted) on exit */
	talloc_steal(msg, msg->sock);

	status = socket_listen(msg->sock, msg->path, 0, 50, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to setup messaging listener for '%s'\n", msg->path));
		talloc_free(msg);
		return NULL;
	}

	fde.private 	= msg;
	fde.fd		= socket_get_fd(msg->sock);
	fde.flags	= EVENT_FD_READ;
	fde.handler	= messaging_listen_handler;

	msg->event.ev   = ev;
	msg->event.fde	= event_add_fd(ev, &fde);

	talloc_set_destructor(msg, messaging_destructor);
	
	messaging_register(msg, NULL, MSG_PING, ping_message);

	return msg;
}


