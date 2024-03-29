/* 
   Unix SMB/CIFS implementation.

   testing of the events subsystem
   
   Copyright (C) Stefan Metzmacher
   
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
#include "system/filesys.h"
#include "torture/torture.h"

static int fde_count;

static void fde_handler(struct event_context *ev_ctx, struct fd_event *f, 
			uint16_t flags, void *private)
{
	int *fd = private;
	char c;
#ifdef SA_SIGINFO
	kill(getpid(), SIGUSR1);
#endif
	kill(getpid(), SIGALRM);
	read(fd[0], &c, 1);
	write(fd[1], &c, 1);
	fde_count++;
}

static void finished_handler(struct event_context *ev_ctx, struct timed_event *te,
			     struct timeval tval, void *private)
{
	int *finished = private;
	(*finished) = 1;
}

static void count_handler(struct event_context *ev_ctx, struct signal_event *te,
			  int signum, int count, void *info, void *private)
{
	int *countp = private;
	(*countp) += count;
}

static bool test_event_context(struct torture_context *test,
			       const void *test_data)
{
	struct event_context *ev_ctx;
	int fd[2] = { -1, -1 };
	const char *backend = (const char *)test_data;
	int alarm_count=0, info_count=0;
	struct fd_event *fde;
	struct signal_event *se1, *se2, *se3;
	int finished=0;
	struct timeval t;
	char c = 0;

	ev_ctx = event_context_init_byname(test, backend);
	if (ev_ctx == NULL) {
		torture_comment(test, "event backend '%s' not supported\n", backend);
		return true;
	}

	torture_comment(test, "Testing event backend '%s'\n", backend);

	/* reset globals */
	fde_count = 0;

	/* create a pipe */
	pipe(fd);

	fde = event_add_fd(ev_ctx, ev_ctx, fd[0], EVENT_FD_READ|EVENT_FD_AUTOCLOSE, 
			   fde_handler, fd);

	event_add_timed(ev_ctx, ev_ctx, timeval_current_ofs(2,0), 
			finished_handler, &finished);

	se1 = event_add_signal(ev_ctx, ev_ctx, SIGALRM, SA_RESTART, count_handler, &alarm_count);
	se2 = event_add_signal(ev_ctx, ev_ctx, SIGALRM, SA_RESETHAND, count_handler, &alarm_count);
#ifdef SA_SIGINFO
	se3 = event_add_signal(ev_ctx, ev_ctx, SIGUSR1, SA_SIGINFO, count_handler, &info_count);
#endif

	write(fd[1], &c, 1);

	t = timeval_current();
	while (!finished) {
		if (event_loop_once(ev_ctx) == -1) {
			talloc_free(ev_ctx);
			torture_fail(test, "Failed event loop\n");
		}
	}

	talloc_free(fde);
	close(fd[1]);

	while (alarm_count < fde_count+1) {
		if (event_loop_once(ev_ctx) == -1) {
			break;
		}
	}

	torture_comment(test, "Got %.2f pipe events/sec\n", fde_count/timeval_elapsed(&t));

	talloc_free(se1);

	torture_assert_int_equal(test, alarm_count, 1+fde_count, "alarm count mismatch");

#ifdef SA_SIGINFO
	talloc_free(se3);
	torture_assert_int_equal(test, info_count, fde_count, "info count mismatch");
#endif

	talloc_free(ev_ctx);

	return true;
}

struct torture_suite *torture_local_event(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "EVENT");
	const char **list = event_backend_list(suite);
	int i;

	for (i=0;list && list[i];i++) {
		torture_suite_add_simple_tcase(suite, list[i],
					       test_event_context,
					       (const void *)list[i]);
	}

	return suite;
}
