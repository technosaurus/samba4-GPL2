/* 
   Unix SMB/CIFS implementation.
   process model: process (1 process handles all client connections)
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

#include "includes.h"

/*
  called when the process model is selected
*/
static void model_startup(void)
{
	smbd_process_init();
}

/*
  called when a listening socket becomes readable
*/
static void accept_connection(struct event_context *ev, struct fd_event *fde, time_t t, uint16 flags)
{
	int accepted_fd;
	struct sockaddr addr;
	socklen_t in_addrlen = sizeof(addr);
	struct model_ops *model_ops = fde->private;
	
	/* accept an incoming connection. */
	accepted_fd = accept(fde->fd,&addr,&in_addrlen);
	if (accepted_fd == -1) {
		DEBUG(0,("accept_connection_single: accept: %s\n",
			 strerror(errno)));
		return;
	}

	/* create a smb server context and add it to out event
	   handling */
	init_smbsession(ev, model_ops, accepted_fd, smbd_read_handler); 

	/* return to event handling */
}


/*
  called when a rpc listening socket becomes readable
*/
static void accept_rpc_connection(struct event_context *ev, struct fd_event *fde, time_t t, uint16 flags)
{
	int accepted_fd;
	struct sockaddr addr;
	socklen_t in_addrlen = sizeof(addr);
	
	/* accept an incoming connection. */
	accepted_fd = accept(fde->fd,&addr,&in_addrlen);
	if (accepted_fd == -1) {
		DEBUG(0,("accept_connection_single: accept: %s\n",
			 strerror(errno)));
		return;
	}

	init_rpc_session(ev, fde->private, accepted_fd); 
}

/* called when a SMB connection goes down */
static void terminate_connection(struct server_context *server, const char *reason) 
{
	server_terminate(server);
}

static int get_id(struct request_context *req)
{
	return (int)req->smb->pid;
}

/*
  initialise the single process model, registering ourselves with the model subsystem
 */
void process_model_single_init(void)
{
	struct model_ops ops;

	ZERO_STRUCT(ops);
	
	/* fill in all the operations */
	ops.model_startup = model_startup;
	ops.accept_connection = accept_connection;
	ops.accept_rpc_connection = accept_rpc_connection;
	ops.terminate_connection = terminate_connection;
	ops.exit_server = NULL;
	ops.get_id = get_id;

	/* register ourselves with the process model subsystem. We register under the name 'single'. */
	register_process_model("single", &ops);
}
