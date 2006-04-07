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
  abstract the various kernel interfaces to change notify into a
  single Samba friendly interface
*/

#include "includes.h"
#include "system/filesys.h"
#include "ntvfs/sysdep/sys_notify.h"
#include "lib/events/events.h"
#include "dlinklist.h"

/* list of registered backends */
static struct sys_notify_backend *backends;
static uint32_t num_backends;

/*
  initialise a system change notify backend
*/
struct sys_notify_context *sys_notify_init(int snum,
					   TALLOC_CTX *mem_ctx, 
					   struct event_context *ev)
{
	struct sys_notify_context *ctx;
	const char *bname;
	int i;

	if (num_backends == 0) {
		return NULL;
	}

	if (ev == NULL) {
		ev = event_context_find(mem_ctx);
	}

	ctx = talloc_zero(mem_ctx, struct sys_notify_context);
	if (ctx == NULL) {
		return NULL;
	}

	ctx->ev = ev;

	bname = lp_parm_string(snum, "notify", "backend");
	if (!bname) {
		if (num_backends) {
			bname = backends[0].name;
		} else {
			bname = "__unknown__";
		}
	}

	for (i=0;i<num_backends;i++) {
		if (strcasecmp(backends[i].name, bname) == 0) {
			bname = backends[i].name;
			break;
		}
	}

	ctx->name = bname;
	ctx->notify_watch = NULL;

	if (i < num_backends) {
		ctx->notify_watch = backends[i].notify_watch;
	}

	return ctx;
}

/*
  add a watch

  note that this call must modify the e->filter and e->subdir_filter
  bits to remove ones handled by this backend. Any remaining bits will
  be handled by the generic notify layer
*/
NTSTATUS sys_notify_watch(struct sys_notify_context *ctx, struct notify_entry *e,
			  sys_notify_callback_t callback, void *private, void **handle)
{
	if (!ctx->notify_watch) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}
	return ctx->notify_watch(ctx, e, callback, private, handle);
}

/*
  register a notify backend
*/
NTSTATUS sys_notify_register(struct sys_notify_backend *backend)
{
	struct sys_notify_backend *b;
	b = talloc_realloc(talloc_autofree_context(), backends, 
			   struct sys_notify_backend, num_backends+1);
	NT_STATUS_HAVE_NO_MEMORY(b);
	backends = b;
	backends[num_backends] = *backend;
	num_backends++;
	return NT_STATUS_OK;
}
