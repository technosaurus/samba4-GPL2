/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2006
   
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
  notify implementation using inotify
*/

#include "includes.h"
#include "system/filesys.h"
#include "ntvfs/sysdep/sys_notify.h"
#include "lib/events/events.h"
#include "smb.h"
#include "dlinklist.h"

#include <linux/inotify.h>
#include <asm/unistd.h>

#ifndef HAVE_INOTIFY_INIT
/*
  glibc doesn't define these functions yet (as of March 2006)
*/
static int inotify_init(void)
{
	return syscall(__NR_inotify_init);
}

static int inotify_add_watch(int fd, const char *path, __u32 mask)
{
	return syscall(__NR_inotify_add_watch, fd, path, mask);
}

static int inotify_rm_watch(int fd, int wd)
{
	return syscall(__NR_inotify_rm_watch, fd, wd);
}
#endif


/* older glibc headers don't have these defines either */
#ifndef IN_ONLYDIR
#define IN_ONLYDIR 0x01000000
#endif
#ifndef IN_MASK_ADD
#define IN_MASK_ADD 0x20000000
#endif

struct inotify_private {
	struct sys_notify_context *ctx;
	int fd;
	struct watch_context *watches;
};

struct watch_context {
	struct watch_context *next, *prev;
	struct inotify_private *in;
	int wd;
	sys_notify_callback_t callback;
	void *private;
	uint32_t mask;
};


/*
  destroy the inotify private context
*/
static int inotify_destructor(void *ptr)
{
	struct inotify_private *in = talloc_get_type(ptr, struct inotify_private);
	close(in->fd);
	return 0;
}


/*
  dispatch one inotify event
*/
static void inotify_dispatch(struct inotify_private *in, struct inotify_event *e)
{
	struct watch_context *w;
	struct notify_event ne;

	/* ignore extraneous events, such as unmount and IN_IGNORED events */
	if ((e->mask & (IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO)) == 0) {
		return;
	}

	/* map the inotify mask to a action */
	if (e->mask & IN_CREATE) {
		ne.action = NOTIFY_ACTION_ADDED;
	} else if (e->mask & IN_DELETE) {
		ne.action = NOTIFY_ACTION_REMOVED;
	} else {
		ne.action = NOTIFY_ACTION_MODIFIED;
	}
	ne.path = e->name;

	/* find any watches that have this watch descriptor */
	for (w=in->watches;w;w=w->next) {
		/* checking the mask copes with multiple watches */
		if (w->wd == e->wd && (e->mask & w->mask) != 0) {
			w->callback(in->ctx, w->private, &ne);
		}
	}
}

/*
  called when the kernel has some events for us
*/
static void inotify_handler(struct event_context *ev, struct fd_event *fde,
			    uint16_t flags, void *private)
{
	struct inotify_private *in = talloc_get_type(private, struct inotify_private);
	int bufsize = 0;
	struct inotify_event *e0, *e;

	/*
	  we must use FIONREAD as we cannot predict the length of the
	  filenames, and thus can't know how much to allocate
	  otherwise
	*/
	if (ioctl(in->fd, FIONREAD, &bufsize) != 0 || 
	    bufsize == 0) {
		DEBUG(0,("No data on inotify fd?!\n"));
		return;
	}

	e0 = e = talloc_size(in, bufsize);
	if (e == NULL) return;

	if (read(in->fd, e0, bufsize) != bufsize) {
		DEBUG(0,("Failed to read all inotify data\n"));
		talloc_free(e0);
		return;
	}

	/* we can get more than one event in the buffer */
	while (bufsize >= sizeof(*e)) {
		inotify_dispatch(in, e);
		bufsize -= e->len + sizeof(*e);
		e = (struct inotify_event *)(e->len + sizeof(*e) + (char *)e);
	}

	talloc_free(e0);
}

/*
  setup the inotify handle - called the first time a watch is added on
  this context
*/
static NTSTATUS inotify_setup(struct sys_notify_context *ctx)
{
	struct inotify_private *in;

	in = talloc(ctx, struct inotify_private);
	NT_STATUS_HAVE_NO_MEMORY(in);
	in->fd = inotify_init();
	if (in->fd == -1) {
		DEBUG(0,("Failed to init inotify - %s\n", strerror(errno)));
		talloc_free(in);
		return map_nt_error_from_unix(errno);
	}
	in->ctx = ctx;
	in->watches = NULL;

	ctx->private = in;
	talloc_set_destructor(in, inotify_destructor);

	/* add a event waiting for the inotify fd to be readable */
	event_add_fd(ctx->ev, in, in->fd, EVENT_FD_READ, inotify_handler, in);
	
	return NT_STATUS_OK;
}


/*
  map from a change notify mask to a inotify mask. Approximate only :(
*/
static const struct {
	uint32_t notify_mask;
	uint32_t inotify_mask;
} inotify_mapping[] = {
	{FILE_NOTIFY_CHANGE_FILE_NAME,  IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO},
	{FILE_NOTIFY_CHANGE_ATTRIBUTES, IN_ATTRIB},
	{FILE_NOTIFY_CHANGE_SIZE,       IN_MODIFY},
	{FILE_NOTIFY_CHANGE_LAST_WRITE, IN_ATTRIB},
	{FILE_NOTIFY_CHANGE_EA,         IN_ATTRIB},
	{FILE_NOTIFY_CHANGE_SECURITY,   IN_ATTRIB}
};

static uint32_t inotify_map(uint32_t mask)
{
	int i;
	uint32_t out=0;
	for (i=0;i<ARRAY_SIZE(inotify_mapping);i++) {
		if (inotify_mapping[i].notify_mask & mask) {
			out |= inotify_mapping[i].inotify_mask;
		}
	}
	return out;
}

/*
  destroy a watch
*/
static int watch_destructor(void *ptr)
{
	struct watch_context *w = talloc_get_type(ptr, struct watch_context);
	struct inotify_private *in = w->in;
	int wd = w->wd;
	DLIST_REMOVE(w->in->watches, w);

	/* only rm the watch if its the last one with this wd */
	for (w=in->watches;w;w=w->next) {
		if (w->wd == wd) break;
	}
	if (w == NULL) {
		inotify_rm_watch(in->fd, wd);
	}
	return 0;
}


/*
  add a watch. The watch is removed when the caller calls
  talloc_free() on handle
*/
static NTSTATUS inotify_watch(struct sys_notify_context *ctx, const char *dirpath,
			      uint32_t filter, sys_notify_callback_t callback,
			      void *private, void **handle)
{
	struct inotify_private *in;
	int wd;
	uint32_t mask;
	struct watch_context *w;

	/* maybe setup the inotify fd */
	if (ctx->private == NULL) {
		NTSTATUS status;
		status = inotify_setup(ctx);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	in = talloc_get_type(ctx->private, struct inotify_private);

	mask = inotify_map(filter);
	if (mask == 0) {
		/* this filter can't be handled by inotify */
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* using IN_MASK_ADD allows us to cope with inotify() returning the same
	   watch descriptor for muliple watches on the same path */
	mask |= (IN_MASK_ADD | IN_ONLYDIR);

	/* get a new watch descriptor for this path */
	wd = inotify_add_watch(in->fd, dirpath, mask);
	if (wd == -1) {
		return map_nt_error_from_unix(errno);
	}

	w = talloc(in, struct watch_context);
	if (w == NULL) {
		inotify_rm_watch(in->fd, wd);
		return NT_STATUS_NO_MEMORY;
	}

	w->in = in;
	w->wd = wd;
	w->callback = callback;
	w->private = private;
	w->mask = mask;

	(*handle) = w;

	DLIST_ADD(in->watches, w);

	/* the caller frees the handle to stop watching */
	talloc_set_destructor(w, watch_destructor);
	
	return NT_STATUS_OK;
}


static struct sys_notify_backend inotify = {
	.name = "inotify",
	.notify_watch = inotify_watch
};

/*
  initialialise the inotify module
 */
NTSTATUS ntvfs_inotify_init(void)
{
	/* register ourselves as a system inotify module */
	return sys_notify_register(&inotify);
}