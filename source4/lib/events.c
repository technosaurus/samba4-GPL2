/* 
   Unix SMB/CIFS implementation.
   main select loop and event handling
   Copyright (C) Andrew Tridgell 2003
   
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
  PLEASE READ THIS BEFORE MODIFYING!

  This module is a general abstraction for the main select loop and
  event handling. Do not ever put any localised hacks in here, instead
  register one of the possible event types and implement that event
  somewhere else.

  There are 2 types of event handling that are handled in this module:

  1) a file descriptor becoming readable or writeable. This is mostly
     used for network sockets, but can be used for any type of file
     descriptor. You may only register one handler for each file
     descriptor/io combination or you will get unpredictable results
     (this means that you can have a handler for read events, and a
     separate handler for write events, but not two handlers that are
     both handling read events)

  2) a timed event. You can register an event that happens at a
     specific time.  You can register as many of these as you
     like. When they are called the handler can choose to set the time
     for the next event. If next_event is not set then the event is removed.

  To setup a set of events you first need to create a event_context
  structure using the function event_context_init(); This returns a
  'struct event_context' that you use in all subsequent calls.

  After that you can add/remove events that you are interested in
  using event_add_*() and talloc_free()

  Finally, you call event_loop_wait() to block waiting for one of the
  events to occor. In normal operation event_loop_wait() will loop
  forever.

*/

#include "includes.h"
#include "system/time.h"
#include "system/select.h"
#include "dlinklist.h"
#include "events.h"


/*
  please read the comments in events.c before modifying
*/


struct event_context {	
	/* list of filedescriptor events */
	struct fd_event {
		struct event_context *event_ctx;
		struct fd_event *next, *prev;
		int fd;
		uint16_t flags; /* see EVENT_FD_* flags */
		event_fd_handler_t handler;
		void *private;
	} *fd_events;

	/* list of timed events */
	struct timed_event {
		struct event_context *event_ctx;
		struct timed_event *next, *prev;
		struct timeval next_event;
		event_timed_handler_t handler;
		void *private;
	} *timed_events;

	/* the maximum file descriptor number in fd_events */
	int maxfd;

	/* information for exiting from the event loop */
	int exit_code;

	/* this is changed by the destructors for any event type. It
	   is used to detect event destruction by event handlers,
	   which means the code that is calling all event handles
	   needs to assume that the linked list is no longer valid 
	*/
	uint32_t destruction_count;
};

/*
  create a event_context structure. This must be the first events
  call, and all subsequent calls pass this event_context as the first
  element. Event handlers also receive this as their first argument.
*/
struct event_context *event_context_init(TALLOC_CTX *mem_ctx)
{
	struct event_context *ev;

	ev = talloc_zero(mem_ctx, struct event_context);
	if (!ev) return NULL;

	return ev;
}

/*
  recalculate the maxfd
*/
static void calc_maxfd(struct event_context *ev)
{
	struct fd_event *e;
	ev->maxfd = 0;
	for (e=ev->fd_events; e; e=e->next) {
		if (e->fd > ev->maxfd) {
			ev->maxfd = e->fd;
		}
	}
}

/* to mark the ev->maxfd invalid
 * this means we need to recalculate it
 */
#define EVENT_INVALID_MAXFD (-1)


static int event_fd_destructor(void *ptr)
{
	struct fd_event *fde = talloc_get_type(ptr, struct fd_event);
	if (fde->event_ctx->maxfd == fde->fd) {
		fde->event_ctx->maxfd = EVENT_INVALID_MAXFD;
	}
	DLIST_REMOVE(fde->event_ctx->fd_events, fde);
	fde->event_ctx->destruction_count++;
	return 0;
}

/*
  add a fd based event
  return NULL on failure (memory allocation error)
*/
struct fd_event *event_add_fd(struct event_context *ev, TALLOC_CTX *mem_ctx,
			      int fd, uint16_t flags, event_fd_handler_t handler,
			      void *private)
{
	struct fd_event *e = talloc(ev, struct fd_event);
	if (!e) return NULL;

	e->event_ctx = ev;
	e->fd        = fd;
	e->flags     = flags;
	e->handler   = handler;
	e->private   = private;

	DLIST_ADD(ev->fd_events, e);

	if (e->fd > ev->maxfd) {
		ev->maxfd = e->fd;
	}

	talloc_set_destructor(e, event_fd_destructor);
	if (mem_ctx) {
		talloc_steal(mem_ctx, e);
	}
	return e;
}


/*
  return the fd event flags
*/
uint16_t event_fd_flags(struct fd_event *fde)
{
	return fde->flags;
}

/*
  set the fd event flags
*/
void event_fd_setflags(struct fd_event *fde, uint16_t flags)
{
	fde->flags = flags;
}

/*
  destroy a timed event
*/
static int event_timed_destructor(void *ptr)
{
	struct timed_event *te = talloc_get_type(ptr, struct timed_event);
	DLIST_REMOVE(te->event_ctx->timed_events, te);
	te->event_ctx->destruction_count++;
	return 0;
}

/*
  add a timed event
  return NULL on failure (memory allocation error)
*/
struct timed_event *event_add_timed(struct event_context *ev, TALLOC_CTX *mem_ctx,
				    struct timeval next_event, 
				    event_timed_handler_t handler, 
				    void *private) 
{
	struct timed_event *e = talloc(ev, struct timed_event);
	if (!e) return NULL;

	e->event_ctx  = ev;
	e->next_event = next_event;
	e->handler    = handler;
	e->private    = private;

	DLIST_ADD(ev->timed_events, e);
	talloc_set_destructor(e, event_timed_destructor);
	if (mem_ctx) {
		talloc_steal(mem_ctx, e);
	}
	return e;
}

/*
  do a single event loop using the events defined in ev this function
*/
int event_loop_once(struct event_context *ev)
{
	fd_set r_fds, w_fds;
	struct fd_event *fe;
	struct timed_event *te, *te_next;
	int selrtn;
	struct timeval tval, t, *tvalp;
	uint32_t destruction_count = ev->destruction_count;

	FD_ZERO(&r_fds);
	FD_ZERO(&w_fds);

	/* setup any fd events */
	for (fe=ev->fd_events; fe; ) {
		struct fd_event *next = fe->next;
		if (fe->flags & EVENT_FD_READ) {
			FD_SET(fe->fd, &r_fds);
		}
		if (fe->flags & EVENT_FD_WRITE) {
			FD_SET(fe->fd, &w_fds);
		}
		fe = next;
	}

	tvalp = NULL;
	t = timeval_current();
		
	/* work out the right timeout for all timed events */
	for (te=ev->timed_events;te;te=te_next) {
		struct timeval tv;
		te_next = te->next;
		if (timeval_is_zero(&te->next_event)) {
			talloc_free(te);
			continue;
		}
		tv = timeval_diff(&te->next_event, &t);
		if (tvalp == NULL) {
			tval = tv;
		} else {
			tval = timeval_min(&tv, &tval);
		}
		tvalp = &tval;
	}

	/* only do a select() if there're fd_events
	 * otherwise we would block for a the time in tval,
	 * and if there're no fd_events present anymore we want to
	 * leave the event loop directly
	 */
	if (ev->fd_events) {
		/* we maybe need to recalculate the maxfd */
		if (ev->maxfd == EVENT_INVALID_MAXFD) {
			calc_maxfd(ev);
		}
		
		/* TODO:
		 * we don't use sys_select() as it isn't thread
		 * safe. We need to replace the magic pipe handling in
		 * sys_select() with something in the events
		 * structure - for now just use select() 
		 */
		selrtn = select(ev->maxfd+1, &r_fds, &w_fds, NULL, tvalp);
		
		t = timeval_current();
		
		if (selrtn == -1 && errno == EBADF) {
			/* the socket is dead! this should never
			   happen as the socket should have first been
			   made readable and that should have removed
			   the event, so this must be a bug. This is a
			   fatal error. */
			DEBUG(0,("EBADF on event_loop_once - exiting\n"));
			ev->exit_code = EBADF;
			return -1;
		}
		
		if (selrtn > 0) {
			/* at least one file descriptor is ready - check
			   which ones and call the handler, being careful to allow
			   the handler to remove itself when called */
			for (fe=ev->fd_events; fe; fe=fe->next) {
				uint16_t flags = 0;
				if (FD_ISSET(fe->fd, &r_fds)) flags |= EVENT_FD_READ;
				if (FD_ISSET(fe->fd, &w_fds)) flags |= EVENT_FD_WRITE;
				if (flags) {
					fe->handler(ev, fe, t, flags, fe->private);
					if (destruction_count != ev->destruction_count) {
						break;
					}
				}
			}
		}
	}

	/* call any timed events that are now due */
	for (te=ev->timed_events;te;) {
		struct timed_event *next = te->next;
		if (timeval_compare(&te->next_event, &t) >= 0) {
			te->next_event = timeval_zero();
			te->handler(ev, te, t, te->private);
			if (destruction_count != ev->destruction_count) {
				break;
			}
		}
		te = next;
	}
	
	return 0;
}

/*
  go into an event loop using the events defined in ev this function
  will return with the specified code if one of the handlers calls
  event_loop_exit()

  also return (with code 0) if all fd events are removed
*/
int event_loop_wait(struct event_context *ev)
{
	ev->exit_code = 0;
	ev->maxfd = EVENT_INVALID_MAXFD;

	while (ev->fd_events && ev->exit_code == 0) {
		if (event_loop_once(ev) != 0) {
			break;
		}
	}

	return ev->exit_code;
}
