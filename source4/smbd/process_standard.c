/* 
   Unix SMB/CIFS implementation.

   process model: standard (1 process per client connection)

   Copyright (C) Andrew Tridgell 1992-2005
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   Copyright (C) Stefan (metze) Metzmacher 2004
   
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
#include "lib/tdb/include/tdb.h"
#include "dlinklist.h"
#include "smb_server/smb_server.h"

/*
  called when the process model is selected
*/
static void standard_model_init(struct event_context *ev)
{
	signal(SIGCHLD, SIG_IGN);
}

/*
  called when a listening socket becomes readable. 
*/
static void standard_accept_connection(struct event_context *ev, 
				       struct socket_context *sock, 
				       void (*new_conn)(struct event_context *, struct socket_context *, 
							uint32_t , void *), 
				       void *private)
{
	NTSTATUS status;
	struct socket_context *sock2;
	pid_t pid;
	struct event_context *ev2;

	/* accept an incoming connection. */
	status = socket_accept(sock, &sock2);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("standard_accept_connection: accept: %s\n",
			 nt_errstr(status)));
		return;
	}

	pid = fork();

	if (pid != 0) {
		/* parent or error code ... */
		talloc_free(sock2);
		/* go back to the event loop */
		return;
	}

	/* This is now the child code. We need a completely new event_context to work with */
	ev2 = event_context_init(NULL);

	/* the service has given us a private pointer that
	   encapsulates the context it needs for this new connection -
	   everything else will be freed */
	talloc_steal(ev2, private);
	talloc_steal(private, sock2);

	/* this will free all the listening sockets and all state that
	   is not associated with this new connection */
	talloc_free(sock);
	talloc_free(ev);

	/* we don't care if the dup fails, as its only a select()
	   speed optimisation */
	socket_dup(sock2);
			
	/* tdb needs special fork handling */
	if (tdb_reopen_all() == -1) {
		DEBUG(0,("standard_accept_connection: tdb_reopen_all failed.\n"));
	}

	/* Ensure that the forked children do not expose identical random streams */
	set_need_random_reseed();

	/* setup this new connection */
	new_conn(ev2, sock2, getpid(), private);

	/* we can't return to the top level here, as that event context is gone,
	   so we now process events in the new event context until there are no
	   more to process */	   
	event_loop_wait(ev2);

	talloc_free(ev2);
	exit(0);
}

/*
  called to create a new server task
*/
static void standard_new_task(struct event_context *ev, 
			      void (*new_task)(struct event_context *, uint32_t , void *), 
			      void *private)
{
	pid_t pid;
	struct event_context *ev2;

	pid = fork();

	if (pid != 0) {
		/* parent or error code ... go back to the event loop */
		return;
	}

	/* This is now the child code. We need a completely new event_context to work with */
	ev2 = event_context_init(NULL);

	/* the service has given us a private pointer that
	   encapsulates the context it needs for this new connection -
	   everything else will be freed */
	talloc_steal(ev2, private);

	/* this will free all the listening sockets and all state that
	   is not associated with this new connection */
	talloc_free(ev);

	/* tdb needs special fork handling */
	if (tdb_reopen_all() == -1) {
		DEBUG(0,("standard_accept_connection: tdb_reopen_all failed.\n"));
	}

	/* Ensure that the forked children do not expose identical random streams */
	set_need_random_reseed();

	/* setup this new connection */
	new_task(ev2, getpid(), private);

	/* we can't return to the top level here, as that event context is gone,
	   so we now process events in the new event context until there are no
	   more to process */	   
	event_loop_wait(ev2);

	talloc_free(ev2);
	exit(0);
}


/* called when a task goes down */
static void standard_terminate(struct event_context *ev, const char *reason) 
{
	DEBUG(2,("standard_terminate: reason[%s]\n",reason));

	/* this init_iconv() has the effect of freeing the iconv context memory,
	   which makes leak checking easier */
	init_iconv();

	/* the secrets db should really hang off the connection structure */
	secrets_shutdown();

	talloc_free(ev);

	/* terminate this process */
	exit(0);
}


static const struct model_ops standard_ops = {
	.name			= "standard",
	.model_init		= standard_model_init,
	.accept_connection	= standard_accept_connection,
	.new_task               = standard_new_task,
	.terminate              = standard_terminate,
};

/*
  initialise the standard process model, registering ourselves with the process model subsystem
 */
NTSTATUS process_model_standard_init(void)
{
	return register_process_model(&standard_ops);
}
