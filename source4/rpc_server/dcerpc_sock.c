/* 
   Unix SMB/CIFS implementation.

   server side dcerpc using various kinds of sockets (tcp, unix domain)

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004-2005  
   Copyright (C) Jelmer Vernooij 2004

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
#include "lib/events/events.h"
#include "rpc_server/dcerpc_server.h"
#include "smbd/service_stream.h"

struct dcesrv_socket_context {
	const struct dcesrv_endpoint *endpoint;
	struct dcesrv_context *dcesrv_ctx;	
};

/*
  write_fn callback for dcesrv_output()
*/
static ssize_t dcerpc_write_fn(void *private, DATA_BLOB *out)
{
	NTSTATUS status;
	struct socket_context *sock = private;
	size_t sendlen;

	status = socket_send(sock, out, &sendlen, 0);
	if (NT_STATUS_IS_ERR(status)) {
		return -1;
	}

	return sendlen;
}

static void dcesrv_terminate_connection(struct dcesrv_connection *dce_conn, const char *reason)
{
	stream_terminate_connection(dce_conn->srv_conn, reason);
}


void dcesrv_sock_accept(struct stream_connection *srv_conn)
{
	NTSTATUS status;
	struct dcesrv_socket_context *dcesrv_sock = 
		talloc_get_type(srv_conn->private, struct dcesrv_socket_context);
	struct dcesrv_connection *dcesrv_conn = NULL;

	status = dcesrv_endpoint_connect(dcesrv_sock->dcesrv_ctx,
					 srv_conn,
					 dcesrv_sock->endpoint,
					 srv_conn,
					 &dcesrv_conn);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("dcesrv_sock_accept: dcesrv_endpoint_connect failed: %s\n", 
			nt_errstr(status)));
		return;
	}

	srv_conn->private = dcesrv_conn;

	return;	
}

void dcesrv_sock_recv(struct stream_connection *conn, uint16_t flags)
{
	NTSTATUS status;
	struct dcesrv_connection *dce_conn = talloc_get_type(conn->private, struct dcesrv_connection);
	DATA_BLOB tmp_blob;
	size_t nread;

	tmp_blob = data_blob_talloc(conn->socket, NULL, 0x1000);
	if (tmp_blob.data == NULL) {
		dcesrv_terminate_connection(dce_conn, "out of memory");
		return;
	}

	status = socket_recv(conn->socket, tmp_blob.data, tmp_blob.length, &nread, 0);
	if (NT_STATUS_IS_ERR(status)) {
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}
	if (nread == 0) {
		talloc_free(tmp_blob.data);
		return;
	}

	tmp_blob.length = nread;

	status = dcesrv_input(dce_conn, &tmp_blob);
	talloc_free(tmp_blob.data);

	if (!NT_STATUS_IS_OK(status)) {
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}

	if (dce_conn->call_list && dce_conn->call_list->replies) {
		EVENT_FD_WRITEABLE(conn->event.fde);
	}
}

void dcesrv_sock_send(struct stream_connection *conn, uint16_t flags)
{
	struct dcesrv_connection *dce_conn = talloc_get_type(conn->private, struct dcesrv_connection);
	NTSTATUS status;

	status = dcesrv_output(dce_conn, conn->socket, dcerpc_write_fn);
	if (!NT_STATUS_IS_OK(status)) {
		dcesrv_terminate_connection(dce_conn, "eof on socket");
		return;
	}

	if (!dce_conn->call_list || !dce_conn->call_list->replies) {
		EVENT_FD_NOT_WRITEABLE(conn->event.fde);
	}
}


static const struct stream_server_ops dcesrv_stream_ops = {
	.name			= "rpc",
	.accept_connection	= dcesrv_sock_accept,
	.recv_handler		= dcesrv_sock_recv,
	.send_handler		= dcesrv_sock_send,
};



static NTSTATUS add_socket_rpc_unix(struct dcesrv_context *dce_ctx, struct dcesrv_endpoint *e,
				    struct event_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 1;
	NTSTATUS status;

	dcesrv_sock = talloc(dce_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_socket(event_ctx, model_ops, &dcesrv_stream_ops, 
				     "unix", e->ep_description.endpoint, &port, 
				     dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(path=%s) failed - %s\n",
			 e->ep_description.endpoint, nt_errstr(status)));
	}

	return status;
}

static NTSTATUS add_socket_rpc_ncalrpc(struct dcesrv_context *dce_ctx, struct dcesrv_endpoint *e,
				       struct event_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 1;
	char *full_path;
	NTSTATUS status;

	if (!e->ep_description.endpoint) {
		/* No identifier specified: use DEFAULT. 
		 * DO NOT hardcode this value anywhere else. Rather, specify 
		 * no endpoint and let the epmapper worry about it. */
		e->ep_description.endpoint = talloc_strdup(dce_ctx, "DEFAULT");
	}

	full_path = talloc_asprintf(dce_ctx, "%s/%s", lp_ncalrpc_dir(), e->ep_description.endpoint);

	dcesrv_sock = talloc(dce_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= dce_ctx;

	status = stream_setup_socket(event_ctx, model_ops, &dcesrv_stream_ops, 
				     "unix", full_path, &port, dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(identifier=%s,path=%s) failed - %s\n",
			 e->ep_description.endpoint, full_path, nt_errstr(status)));
	}
	return status;
}

/*
  add a socket address to the list of events, one event per dcerpc endpoint
*/
static NTSTATUS add_socket_rpc_tcp_iface(struct dcesrv_context *dce_ctx, struct dcesrv_endpoint *e,
					 struct event_context *event_ctx, const struct model_ops *model_ops,
					 const char *address)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 0;
	NTSTATUS status;
			
	if (e->ep_description.endpoint) {
		port = atoi(e->ep_description.endpoint);
	}

	dcesrv_sock = talloc(dce_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= dce_ctx;

	status = stream_setup_socket(event_ctx, model_ops, &dcesrv_stream_ops, 
				     "ipv4", address, &port, dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(address=%s,port=%u) failed - %s\n", 
			 address, port, nt_errstr(status)));
	}

	if (e->ep_description.endpoint == NULL) {
		e->ep_description.endpoint = talloc_asprintf(dce_ctx, "%d", port);
	}

	return status;
}

static NTSTATUS add_socket_rpc_tcp(struct dcesrv_context *dce_ctx, struct dcesrv_endpoint *e,
				   struct event_context *event_ctx, const struct model_ops *model_ops)
{
	NTSTATUS status;

	/* Add TCP/IP sockets */
	if (lp_interfaces() && lp_bind_interfaces_only()) {
		int num_interfaces = iface_count();
		int i;
		for(i = 0; i < num_interfaces; i++) {
			const char *address = sys_inet_ntoa(*iface_n_ip(i));
			status = add_socket_rpc_tcp_iface(dce_ctx, e, event_ctx, model_ops, address);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	} else {
		status = add_socket_rpc_tcp_iface(dce_ctx, e, event_ctx, model_ops, lp_socket_address());
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Open the listening sockets for RPC over NCACN_IP_TCP/NCALRPC/NCACN_UNIX_STREAM
****************************************************************************/
NTSTATUS dcesrv_sock_init(struct dcesrv_context *dce_ctx, 
			  struct event_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_endpoint *e;
	NTSTATUS status;

	/* Make sure the directory for NCALRPC exists */
	if (!directory_exist(lp_ncalrpc_dir(), NULL)) {
		mkdir(lp_ncalrpc_dir(), 0755);
	}

	for (e=dce_ctx->endpoint_list;e;e=e->next) {
		switch (e->ep_description.transport) {
		case NCACN_UNIX_STREAM:
			status = add_socket_rpc_unix(dce_ctx, e, event_ctx, model_ops);
			NT_STATUS_NOT_OK_RETURN(status);
			break;
		
		case NCALRPC:
			status = add_socket_rpc_ncalrpc(dce_ctx, e, event_ctx, model_ops);
			NT_STATUS_NOT_OK_RETURN(status);
			break;

		case NCACN_IP_TCP:
			status = add_socket_rpc_tcp(dce_ctx, e, event_ctx, model_ops);
			NT_STATUS_NOT_OK_RETURN(status);
			break;

		default:
			break;
		}
	}

	return NT_STATUS_OK;	
}

