/* 
   Unix SMB/CIFS implementation.

   SERVER SERVICE code

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Stefan (metze) Metzmacher	2004
   
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
#include "dlinklist.h"
#include "process_model.h"

/*
  a linked list of registered servers
*/
static struct registered_server {
	struct registered_server *next, *prev;
	const char *service_name;
	NTSTATUS (*service_init)(struct event_context *, const struct model_ops *);
} *registered_servers;

/*
  register a server service. 
*/
NTSTATUS register_server_service(const char *name,
				 NTSTATUS (*service_init)(struct event_context *, const struct model_ops *))
{
	struct registered_server *srv;
	srv = talloc(talloc_autofree_context(), struct registered_server);
	NT_STATUS_HAVE_NO_MEMORY(srv);
	srv->service_name = name;
	srv->service_init = service_init;
	DLIST_ADD_END(registered_servers, srv, struct registered_server *);
	return NT_STATUS_OK;
}


/*
  initialise a server service
*/
static NTSTATUS server_service_init(const char *name,
				    struct event_context *event_ctx,
				    const struct model_ops *model_ops)
{
	struct registered_server *srv;
	for (srv=registered_servers; srv; srv=srv->next) {
		if (strcasecmp(name, srv->service_name) == 0) {
			return srv->service_init(event_ctx, model_ops);
		}
	}
	return NT_STATUS_INVALID_SYSTEM_SERVICE;
}


/*
  startup all of our server services
*/
NTSTATUS server_service_startup(struct event_context *event_ctx, 
				const char *model, const char **server_services)
{
	int i;
	const struct model_ops *model_ops;

	if (!server_services) {
		DEBUG(0,("server_service_startup: no endpoint servers configured\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	model_ops = process_model_startup(event_ctx, model);
	if (!model_ops) {
		DEBUG(0,("process_model_startup('%s') failed\n", model));
		return NT_STATUS_INTERNAL_ERROR;
	}

	for (i=0;server_services[i];i++) {
		NTSTATUS status;

		status = server_service_init(server_services[i], event_ctx, model_ops);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}
