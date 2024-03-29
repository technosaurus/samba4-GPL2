/* 
   Unix SMB/CIFS implementation.

   locking benchmark

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

#include "includes.h"
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "lib/events/events.h"
#include "lib/cmdline/popt_common.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"

#define BASEDIR "\\benchlock"
#define FNAME BASEDIR "\\lock.dat"

static int nprocs;
static int lock_failed;
static int num_connected;

enum lock_stage {LOCK_INITIAL, LOCK_LOCK, LOCK_UNLOCK};

struct benchlock_state {
	struct event_context *ev;
	struct smbcli_tree *tree;
	TALLOC_CTX *mem_ctx;
	int client_num;
	int fnum;
	enum lock_stage stage;
	int lock_offset;
	int unlock_offset;
	int count;
	int lastcount;
	struct smbcli_request *req;
	struct smb_composite_connect reconnect;
	struct timed_event *te;

	/* these are used for reconnections */
	int dest_port;
	const char *dest_host;
	const char *called_name;
	const char *service_type;
};

static void lock_completion(struct smbcli_request *);

/*
  send the next lock request
*/
static void lock_send(struct benchlock_state *state)
{
	union smb_lock io;
	struct smb_lock_entry lock;

	switch (state->stage) {
	case LOCK_INITIAL:
		io.lockx.in.ulock_cnt = 0;
		io.lockx.in.lock_cnt = 1;
		state->lock_offset = 0;
		state->unlock_offset = 0;
		lock.offset = state->lock_offset;
		break;
	case LOCK_LOCK:
		io.lockx.in.ulock_cnt = 0;
		io.lockx.in.lock_cnt = 1;
		state->lock_offset = (state->lock_offset+1)%(nprocs+1);
		lock.offset = state->lock_offset;
		break;
	case LOCK_UNLOCK:
		io.lockx.in.ulock_cnt = 1;
		io.lockx.in.lock_cnt = 0;
		lock.offset = state->unlock_offset;
		state->unlock_offset = (state->unlock_offset+1)%(nprocs+1);
		break;
	}

	lock.count = 1;
	lock.pid = state->tree->session->pid;

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 100000;
	io.lockx.in.locks = &lock;
	io.lockx.in.file.fnum = state->fnum;

	state->req = smb_raw_lock_send(state->tree, &io);
	if (state->req == NULL) {
		DEBUG(0,("Failed to setup lock\n"));
		lock_failed++;
	}
	state->req->async.private = state;
	state->req->async.fn      = lock_completion;
}

static void reopen_connection(struct event_context *ev, struct timed_event *te, 
			      struct timeval t, void *private_data);


static void reopen_file(struct event_context *ev, struct timed_event *te, 
				      struct timeval t, void *private_data)
{
	struct benchlock_state *state = (struct benchlock_state *)private_data;

	/* reestablish our open file */
	state->fnum = smbcli_open(state->tree, FNAME, O_RDWR|O_CREAT, DENY_NONE);
	if (state->fnum == -1) {
		printf("Failed to open %s on connection %d\n", FNAME, state->client_num);
		exit(1);
	}

	num_connected++;

	DEBUG(0,("reconnect to %s finished (%u connected)\n", state->dest_host,
		 num_connected));

	state->stage = LOCK_INITIAL;
	lock_send(state);
}

/*
  complete an async reconnect
 */
static void reopen_connection_complete(struct composite_context *ctx)
{
	struct benchlock_state *state = (struct benchlock_state *)ctx->async.private_data;
	NTSTATUS status;
	struct smb_composite_connect *io = &state->reconnect;

	status = smb_composite_connect_recv(ctx, state->mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(state->te);
		state->te = event_add_timed(state->ev, state->mem_ctx, 
					    timeval_current_ofs(1,0), 
					    reopen_connection, state);
		return;
	}

	talloc_free(state->tree);
	state->tree = io->out.tree;

	/* do the reopen as a separate event */
	event_add_timed(state->ev, state->mem_ctx, timeval_zero(), reopen_file, state);
}

	

/*
  reopen a connection
 */
static void reopen_connection(struct event_context *ev, struct timed_event *te, 
			      struct timeval t, void *private_data)
{
	struct benchlock_state *state = (struct benchlock_state *)private_data;
	struct composite_context *ctx;
	struct smb_composite_connect *io = &state->reconnect;
	char *host, *share;

	state->te = NULL;

	if (!torture_get_conn_index(state->client_num, state->mem_ctx, &host, &share)) {
		DEBUG(0,("Can't find host/share for reconnect?!\n"));
		exit(1);
	}

	io->in.dest_host    = state->dest_host;
	io->in.port         = state->dest_port;
	io->in.called_name  = state->called_name;
	io->in.service      = share;
	io->in.service_type = state->service_type;
	io->in.credentials  = cmdline_credentials;
	io->in.fallback_to_anonymous = False;
	io->in.workgroup    = lp_workgroup();

	/* kill off the remnants of the old connection */
	talloc_free(state->tree);
	state->tree = NULL;

	ctx = smb_composite_connect_send(io, state->mem_ctx, state->ev);
	if (ctx == NULL) {
		DEBUG(0,("Failed to setup async reconnect\n"));
		exit(1);
	}

	ctx->async.fn = reopen_connection_complete;
	ctx->async.private_data = state;
}


/*
  called when a lock completes
*/
static void lock_completion(struct smbcli_request *req)
{
	struct benchlock_state *state = (struct benchlock_state *)req->async.private;
	NTSTATUS status = smbcli_request_simple_recv(req);
	state->req = NULL;
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE) ||
		    NT_STATUS_EQUAL(status, NT_STATUS_LOCAL_DISCONNECT)) {
			talloc_free(state->tree);
			state->tree = NULL;
			num_connected--;	
			DEBUG(0,("reopening connection to %s\n", state->dest_host));
			talloc_free(state->te);
			state->te = event_add_timed(state->ev, state->mem_ctx, 
						    timeval_current_ofs(1,0), 
						    reopen_connection, state);
		} else {
			DEBUG(0,("Lock failed - %s\n", nt_errstr(status)));
			lock_failed++;
		}
		return;
	}

	switch (state->stage) {
	case LOCK_INITIAL:
		state->stage = LOCK_LOCK;
		break;
	case LOCK_LOCK:
		state->stage = LOCK_UNLOCK;
		break;
	case LOCK_UNLOCK:
		state->stage = LOCK_LOCK;
		break;
	}

	state->count++;
	lock_send(state);
}


static void echo_completion(struct smbcli_request *req)
{
	struct benchlock_state *state = talloc_get_type(req->async.private, 
							struct benchlock_state);
	NTSTATUS status = smbcli_request_simple_recv(req);
	if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_LOCAL_DISCONNECT)) {
		talloc_free(state->tree);
		state->tree = NULL;
		num_connected--;	
		DEBUG(0,("reopening connection to %s\n", state->dest_host));
		talloc_free(state->te);
		state->te = event_add_timed(state->ev, state->mem_ctx, 
					    timeval_current_ofs(1,0), 
					    reopen_connection, state);
	}
}

static void report_rate(struct event_context *ev, struct timed_event *te, 
			struct timeval t, void *private_data)
{
	struct benchlock_state *state = talloc_get_type(private_data, 
							struct benchlock_state);
	int i;
	for (i=0;i<nprocs;i++) {
		printf("%5u ", (unsigned)(state[i].count - state[i].lastcount));
		state[i].lastcount = state[i].count;
	}
	printf("\r");
	fflush(stdout);
	event_add_timed(ev, state, timeval_current_ofs(1, 0), report_rate, state);

	/* send an echo on each interface to ensure it stays alive - this helps
	   with IP takeover */
	for (i=0;i<nprocs;i++) {
		struct smb_echo p;
		struct smbcli_request *req;

		if (!state[i].tree) {
			continue;
		}

		p.in.repeat_count = 1;
		p.in.size = 0;
		p.in.data = NULL;
		req = smb_raw_echo_send(state[i].tree->session->transport, &p);
		req->async.private = state;
		req->async.fn      = echo_completion;
	}
}

/* 
   benchmark locking calls
*/
BOOL torture_bench_lock(struct torture_context *torture)
{
	BOOL ret = True;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	int i;
	int timelimit = torture_setting_int(torture, "timelimit", 10);
	struct timeval tv;
	struct event_context *ev = event_context_find(mem_ctx);
	struct benchlock_state *state;
	int total = 0, minops=0;
	struct smbcli_state *cli;
	bool progress;

	progress = torture_setting_bool(torture, "progress", true);

	nprocs = lp_parm_int(-1, "torture", "nprocs", 4);

	state = talloc_zero_array(mem_ctx, struct benchlock_state, nprocs);

	printf("Opening %d connections\n", nprocs);
	for (i=0;i<nprocs;i++) {
		state[i].mem_ctx = talloc_new(state);
		state[i].client_num = i;
		state[i].ev = ev;
		if (!torture_open_connection_ev(&cli, i, ev)) {
			return False;
		}
		talloc_steal(mem_ctx, state);
		state[i].tree = cli->tree;
		state[i].dest_host = talloc_strdup(state[i].mem_ctx, 
						   cli->tree->session->transport->socket->hostname);
		state[i].dest_port = cli->tree->session->transport->socket->port;
		state[i].called_name  = talloc_strdup(state[i].mem_ctx,
						      cli->tree->session->transport->called.name);
		state[i].service_type = talloc_strdup(state[i].mem_ctx,
						      cli->tree->device);
	}

	num_connected = i;

	if (!torture_setup_dir(cli, BASEDIR)) {
		goto failed;
	}

	for (i=0;i<nprocs;i++) {
		state[i].fnum = smbcli_open(state[i].tree, 
					    FNAME, 
					    O_RDWR|O_CREAT, DENY_NONE);
		if (state[i].fnum == -1) {
			printf("Failed to open %s on connection %d\n", FNAME, i);
			goto failed;
		}

		state[i].stage = LOCK_INITIAL;
		lock_send(&state[i]);
	}

	tv = timeval_current();	

	if (progress) {
		event_add_timed(ev, state, timeval_current_ofs(1, 0), report_rate, state);
	}

	printf("Running for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		event_loop_once(ev);

		if (lock_failed) {
			DEBUG(0,("locking failed\n"));
			goto failed;
		}
	}

	printf("%.2f ops/second\n", total/timeval_elapsed(&tv));
	minops = state[0].count;
	for (i=0;i<nprocs;i++) {
		printf("[%d] %u ops\n", i, state[i].count);
		if (state[i].count < minops) minops = state[i].count;
	}
	if (minops < 0.5*total/nprocs) {
		printf("Failed: unbalanced locking\n");
		goto failed;
	}

	for (i=0;i<nprocs;i++) {
		talloc_free(state[i].req);
		smb_raw_exit(state[i].tree->session);
	}

	smbcli_deltree(state[0].tree, BASEDIR);
	talloc_free(mem_ctx);
	return ret;

failed:
	talloc_free(mem_ctx);
	return False;
}
