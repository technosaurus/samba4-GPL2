/* 
   Unix SMB/CIFS implementation.

   KDC Server startup

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Stefan Metzmacher	2005

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
#include "smbd/service_task.h"
#include "smbd/service_stream.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "kdc/kdc.h"
#include "system/network.h"
#include "dlinklist.h"
#include "lib/messaging/irpc.h"

/* hold all the info needed to send a reply */
struct kdc_reply {
	struct kdc_reply *next, *prev;
	const char *dest_address;
	int dest_port;
	DATA_BLOB packet;
};

/*
  top level context structure for the kdc server
*/
struct kdc_server {
	struct task_server *task;
	krb5_kdc_configuration *config;
	struct smb_krb5_context *smb_krb5_context;
};

/* hold information about one kdc socket */
struct kdc_socket {
	struct socket_context *sock;
	struct kdc_server *kdc;
	struct fd_event *fde;

	/* a queue of outgoing replies that have been deferred */
	struct kdc_reply *send_queue;

	int (*process)(krb5_context context, 
		       krb5_kdc_configuration *config,
		       unsigned char *buf, 
		       size_t len, 
		       krb5_data *reply,
		       const char *from,
		       struct sockaddr *addr);
};
/*
  state of an open tcp connection
*/
struct kdc_tcp_connection {
	/* stream connection we belong to */
	struct stream_connection *conn;

	/* the kdc_server the connection belongs to */
	struct kdc_server *kdc;

	/* the partial data we've receiced yet */
	DATA_BLOB partial;

	/* the amount that we used yet from the partial buffer */
	uint32_t partial_read;

	/* prevent loops when we use half async code, while processing a requuest */
	BOOL processing;

	/* a queue of outgoing replies that have been deferred */
	struct data_blob_list_item *send_queue;

	int (*process)(krb5_context context, 
					     krb5_kdc_configuration *config,
					     unsigned char *buf, 
					     size_t len, 
					     krb5_data *reply,
					     const char *from,
					     struct sockaddr *addr);
};

/*
  handle fd send events on a KDC socket
*/
static void kdc_send_handler(struct kdc_socket *kdc_socket)
{
	while (kdc_socket->send_queue) {
		struct kdc_reply *rep = kdc_socket->send_queue;
		NTSTATUS status;
		size_t sendlen;

		status = socket_sendto(kdc_socket->sock, &rep->packet, &sendlen, 0,
				       rep->dest_address, rep->dest_port);
		if (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
			break;
		}
		
		DLIST_REMOVE(kdc_socket->send_queue, rep);
		talloc_free(rep);
	}

	if (kdc_socket->send_queue == NULL) {
		EVENT_FD_NOT_WRITEABLE(kdc_socket->fde);
	}
}


/*
  handle fd recv events on a KDC socket
*/
static void kdc_recv_handler(struct kdc_socket *kdc_socket)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(kdc_socket);
	DATA_BLOB blob;
	struct kdc_reply *rep;
	krb5_data reply;
	size_t nread, dsize;
	const char *src_addr;
	int src_port;
	struct sockaddr_in src_sock_addr;
	struct ipv4_addr addr;
	int ret;

	status = socket_pending(kdc_socket->sock, &dsize);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}

	blob = data_blob_talloc(kdc_socket, NULL, dsize);
	if (blob.data == NULL) {
		/* hope this is a temporary low memory condition */
		talloc_free(tmp_ctx);
		return;
	}

	status = socket_recvfrom(kdc_socket->sock, blob.data, blob.length, &nread, 0,
				 &src_addr, &src_port);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}
	talloc_steal(tmp_ctx, src_addr);
	blob.length = nread;
	
	DEBUG(2,("Received krb5 UDP packet of length %u from %s:%u\n", 
		 blob.length, src_addr, (uint16_t)src_port));
	
	/* TODO:  This really should be in a utility function somewhere */
	ZERO_STRUCT(src_sock_addr);
#ifdef HAVE_SOCK_SIN_LEN
	src_sock_addr.sin_len         = sizeof(src_sock_addr);
#endif
	addr                     = interpret_addr2(src_addr);
	src_sock_addr.sin_addr.s_addr = addr.addr;
	src_sock_addr.sin_port        = htons(src_port);
	src_sock_addr.sin_family      = PF_INET;
	
	/* Call krb5 */
	ret = kdc_socket->process(kdc_socket->kdc->smb_krb5_context->krb5_context, 
				  kdc_socket->kdc->config,
				  blob.data, blob.length, 
				  &reply,
				  src_addr,
				  (struct sockaddr *)&src_sock_addr);
	if (ret == -1) {
		talloc_free(tmp_ctx);
		return;
	}

	/* queue a pending reply */
	rep = talloc(kdc_socket, struct kdc_reply);
	if (rep == NULL) {
		krb5_data_free(&reply);
		talloc_free(tmp_ctx);
		return;
	}
	rep->dest_address = talloc_steal(rep, src_addr);
	rep->dest_port    = src_port;
	rep->packet       = data_blob_talloc(rep, reply.data, reply.length);
	krb5_data_free(&reply);

	if (rep->packet.data == NULL) {
		talloc_free(rep);
		talloc_free(tmp_ctx);
		return;
	}

	DLIST_ADD_END(kdc_socket->send_queue, rep, struct kdc_reply *);
	EVENT_FD_WRITEABLE(kdc_socket->fde);
	talloc_free(tmp_ctx);
}

/*
  handle fd events on a KDC socket
*/
static void kdc_socket_handler(struct event_context *ev, struct fd_event *fde,
			       uint16_t flags, void *private)
{
	struct kdc_socket *kdc_socket = talloc_get_type(private, struct kdc_socket);
	if (flags & EVENT_FD_WRITE) {
		kdc_send_handler(kdc_socket);
	} 
	if (flags & EVENT_FD_READ) {
		kdc_recv_handler(kdc_socket);
	}
}

static void kdc_tcp_terminate_connection(struct kdc_tcp_connection *kdcconn, const char *reason)
{
	stream_terminate_connection(kdcconn->conn, reason);
}

/*
  called when we get a new connection
*/
static void kdc_tcp_accept(struct stream_connection *conn)
{
	struct kdc_server *kdc = talloc_get_type(conn->private, struct kdc_server);
	struct kdc_tcp_connection *kdcconn;

	kdcconn = talloc_zero(conn, struct kdc_tcp_connection);
	if (!kdcconn) {
		stream_terminate_connection(conn, "kdc_tcp_accept: out of memory");
		return;
	}
	kdcconn->conn	 = conn;
	kdcconn->kdc	 = kdc;
	kdcconn->process = krb5_kdc_process_krb5_request;
	conn->private    = kdcconn;
}

/*
  receive some data on a KDC connection
*/
static void kdc_tcp_recv(struct stream_connection *conn, uint16_t flags)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(conn->private, struct kdc_tcp_connection);
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *tmp_ctx = talloc_new(kdcconn);
	struct data_blob_list_item *rep;
	krb5_data reply;
	size_t nread;
	const char *src_addr;
	int src_port;
	struct sockaddr_in src_sock_addr;
	struct ipv4_addr addr;
	int ret;

	/* avoid recursion, because of half async code */
	if (kdcconn->processing) {
		EVENT_FD_NOT_READABLE(conn->event.fde);
		return;
	}

	if (kdcconn->partial.length == 0) {
		kdcconn->partial = data_blob_talloc(kdcconn, NULL, 4);
		if (!kdcconn->partial.data) goto nomem;

		kdcconn->partial_read = 0;
	}

	/* read in the packet length */
	if (kdcconn->partial_read < 4) {
		uint32_t packet_length;

		status = socket_recv(conn->socket, 
				     kdcconn->partial.data + kdcconn->partial_read,
				     4 - kdcconn->partial_read,
				     &nread, 0);
		if (NT_STATUS_IS_ERR(status)) goto failed;
		if (!NT_STATUS_IS_OK(status)) return;

		kdcconn->partial_read += nread;
		if (kdcconn->partial_read != 4) return;

		packet_length = RIVAL(kdcconn->partial.data, 0) + 4;

		kdcconn->partial.data = talloc_realloc(kdcconn, kdcconn->partial.data, 
						       uint8_t, packet_length);
		if (!kdcconn->partial.data) goto nomem;

		kdcconn->partial.length = packet_length;
	}

	/* read in the body */
	status = socket_recv(conn->socket, 
			     kdcconn->partial.data + kdcconn->partial_read,
			     kdcconn->partial.length - kdcconn->partial_read,
			     &nread, 0);
	if (NT_STATUS_IS_ERR(status)) goto failed;
	if (!NT_STATUS_IS_OK(status)) return;

	kdcconn->partial_read += nread;
	if (kdcconn->partial_read != kdcconn->partial.length) return;

	/*
	 * we have parsed the request, so we can reset the kdcconn->partial_read,
	 * maybe we could also free kdcconn->partial, but for now we keep it,
	 * and overwrite it the next time
	 */
	kdcconn->partial_read = 0;

	src_addr = socket_get_peer_addr(kdcconn->conn->socket, tmp_ctx);
	if (!src_addr) goto nomem;
	src_port = socket_get_peer_port(kdcconn->conn->socket);

	DEBUG(2,("Received krb5 TCP packet of length %u from %s:%u\n", 
		 kdcconn->partial.length - 4, src_addr, src_port));

	/* TODO:  This really should be in a utility function somewhere */
	ZERO_STRUCT(src_sock_addr);
#ifdef HAVE_SOCK_SIN_LEN
	src_sock_addr.sin_len		= sizeof(src_sock_addr);
#endif
	addr				= interpret_addr2(src_addr);
	src_sock_addr.sin_addr.s_addr	= addr.addr;
	src_sock_addr.sin_port		= htons(src_port);
	src_sock_addr.sin_family	= PF_INET;

	/* Call krb5 */
	kdcconn->processing = True;
	ret = kdcconn->process(kdcconn->kdc->smb_krb5_context->krb5_context, 
			       kdcconn->kdc->config,
			       kdcconn->partial.data + 4, kdcconn->partial.length - 4, 
			       &reply,
			       src_addr,
			       (struct sockaddr *)&src_sock_addr);
	kdcconn->processing = False;
	if (ret == -1) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto failed;
	}

	/* and now encode the reply */
	rep = talloc(kdcconn, struct data_blob_list_item);
	if (!rep) {
		krb5_data_free(&reply);
		goto nomem;
	}

	rep->blob = data_blob_talloc(rep, NULL, reply.length + 4);
	if (!rep->blob.data)  {
		krb5_data_free(&reply);
		goto nomem;
	}

	RSIVAL(rep->blob.data, 0, reply.length);
	memcpy(rep->blob.data + 4, reply.data, reply.length);	
	krb5_data_free(&reply);

	if (!kdcconn->send_queue) {
		EVENT_FD_WRITEABLE(kdcconn->conn->event.fde);
	}
	DLIST_ADD_END(kdcconn->send_queue, rep, struct data_blob_list_item *);

	EVENT_FD_READABLE(kdcconn->conn->event.fde);

	/* the call isn't needed any more */
	talloc_free(tmp_ctx);
	return;
nomem:
	status = NT_STATUS_NO_MEMORY;
failed:
	kdc_tcp_terminate_connection(kdcconn, nt_errstr(status));
}

/*
  called when we can write to a connection
*/
static void kdc_tcp_send(struct stream_connection *conn, uint16_t flags)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(conn->private, struct kdc_tcp_connection);
	NTSTATUS status;

	while (kdcconn->send_queue) {
		struct data_blob_list_item *q = kdcconn->send_queue;
		size_t sendlen;

		status = socket_send(conn->socket, &q->blob, &sendlen, 0);
		if (NT_STATUS_IS_ERR(status)) goto failed;
		if (!NT_STATUS_IS_OK(status)) return;

		q->blob.length -= sendlen;
		q->blob.data   += sendlen;

		if (q->blob.length == 0) {
			DLIST_REMOVE(kdcconn->send_queue, q);
			talloc_free(q);
		}
	}

	EVENT_FD_NOT_WRITEABLE(conn->event.fde);
	return;
failed:
	kdc_tcp_terminate_connection(kdcconn, nt_errstr(status));
}

static const struct stream_server_ops kdc_tcp_stream_ops = {
	.name			= "kdc_tcp",
	.accept_connection	= kdc_tcp_accept,
	.recv_handler		= kdc_tcp_recv,
	.send_handler		= kdc_tcp_send
};

/*
  start listening on the given address
*/
static NTSTATUS kdc_add_socket(struct kdc_server *kdc, const char *address)
{
	const struct model_ops *model_ops;
 	struct kdc_socket *kdc_socket;
	NTSTATUS status;
	uint16_t port = lp_krb5_port();

	kdc_socket = talloc(kdc, struct kdc_socket);
	NT_STATUS_HAVE_NO_MEMORY(kdc_socket);

	status = socket_create("ip", SOCKET_TYPE_DGRAM, &kdc_socket->sock, 0);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(kdc_socket);
		return status;
	}

	kdc_socket->kdc = kdc;
	kdc_socket->send_queue = NULL;
	kdc_socket->process = krb5_kdc_process_krb5_request;

	talloc_steal(kdc_socket, kdc_socket->sock);

	kdc_socket->fde = event_add_fd(kdc->task->event_ctx, kdc, 
				       socket_get_fd(kdc_socket->sock), EVENT_FD_READ,
				       kdc_socket_handler, kdc_socket);

	status = socket_listen(kdc_socket->sock, address, port, 0, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s:%d UDP - %s\n", 
			 address, port, nt_errstr(status)));
		talloc_free(kdc_socket);
		return status;
	}

	/* within the kdc task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_byname("single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops\n"));
		talloc_free(kdc_socket);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = stream_setup_socket(kdc->task->event_ctx, model_ops, &kdc_tcp_stream_ops, 
				     "ip", address, &port, kdc);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s:%u TCP - %s\n",
			 address, port, nt_errstr(status)));
		talloc_free(kdc_socket);
		return status;
	}

	return NT_STATUS_OK;
}


/*
  setup our listening sockets on the configured network interfaces
*/
static NTSTATUS kdc_startup_interfaces(struct kdc_server *kdc)
{
	int num_interfaces = iface_count();
	TALLOC_CTX *tmp_ctx = talloc_new(kdc);
	NTSTATUS status;

	/* if we are allowing incoming packets from any address, then
	   we need to bind to the wildcard address */
	if (!lp_bind_interfaces_only()) {
		status = kdc_add_socket(kdc, "0.0.0.0");
		NT_STATUS_NOT_OK_RETURN(status);
	} else {
		int i;

		for (i=0; i<num_interfaces; i++) {
			const char *address = talloc_strdup(tmp_ctx, iface_n_ip(i));
			status = kdc_add_socket(kdc, address);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	}

	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

/*
  startup the kdc task
*/
static void kdc_task_init(struct task_server *task)
{
	struct kdc_server *kdc;
	NTSTATUS status;
	krb5_error_code ret;

	if (iface_count() == 0) {
		task_server_terminate(task, "kdc: no network interfaces configured");
		return;
	}

	kdc = talloc(task, struct kdc_server);
	if (kdc == NULL) {
		task_server_terminate(task, "kdc: out of memory");
		return;
	}

	kdc->task = task;

	/* Setup the KDC configuration */
	kdc->config = talloc(kdc, krb5_kdc_configuration);
	if (!kdc->config) {
		task_server_terminate(task, "kdc: out of memory");
		return;
	}
	krb5_kdc_default_config(kdc->config);

	/* NAT and the like make this pointless, and painful */
	kdc->config->check_ticket_addresses = FALSE;

	initialize_krb5_error_table();

	ret = smb_krb5_init_context(kdc, &kdc->smb_krb5_context);
	if (ret) {
		DEBUG(1,("kdc_task_init: krb5_init_context failed (%s)\n", 
			 error_message(ret)));
		task_server_terminate(task, "kdc: krb5_init_context failed");
		return; 
	}

	krb5_add_et_list(kdc->smb_krb5_context->krb5_context, initialize_hdb_error_table_r);

	kdc->config->logf = kdc->smb_krb5_context->logf;
	kdc->config->db = talloc(kdc->config, struct HDB *);
	if (!kdc->config->db) {
		task_server_terminate(task, "kdc: out of memory");
		return;
	}
	kdc->config->num_db = 1;
		
	ret = hdb_ldb_create(kdc, kdc->smb_krb5_context->krb5_context, 
			     &kdc->config->db[0], lp_sam_url());
	if (ret != 0) {
		DEBUG(1, ("kdc_task_init: hdb_ldb_create fails: %s\n", 
			  smb_get_krb5_error_message(kdc->smb_krb5_context->krb5_context, ret, kdc))); 
		task_server_terminate(task, "kdc: hdb_ldb_create failed");
		return; 
	}

	/* start listening on the configured network interfaces */
	status = kdc_startup_interfaces(kdc);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "kdc failed to setup interfaces");
		return;
	}

	irpc_add_name(task->msg_ctx, "kdc_server");
}


/*
  called on startup of the KDC service 
*/
static NTSTATUS kdc_init(struct event_context *event_ctx, 
			 const struct model_ops *model_ops)
{	
	return task_server_startup(event_ctx, model_ops, kdc_task_init);
}

/* called at smbd startup - register ourselves as a server service */
NTSTATUS server_service_kdc_init(void)
{
	return register_server_service("kdc", kdc_init);
}
