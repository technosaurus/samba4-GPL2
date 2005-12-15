/* 
   Unix SMB/CIFS implementation.

   Main SMB server routines

   Copyright (C) Andrew Tridgell		1992-2005
   Copyright (C) Martin Pool			2002
   Copyright (C) Jelmer Vernooij		2002
   Copyright (C) James J Myers 			2003 <myersjj@samba.org>
   
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
#include "version.h"
#include "lib/cmdline/popt_common.h"
#include "system/dir.h"
#include "system/filesys.h"
#include "system/kerberos.h"

/* For sepecifiying event context to GSSAPI below */
#include "heimdal/lib/gssapi/gssapi_locl.h"

/*
  recursively delete a directory tree
*/
static void recursive_delete(const char *path)
{
	DIR *dir;
	struct dirent *de;

	dir = opendir(path);
	if (!dir) {
		return;
	}

	for (de=readdir(dir);de;de=readdir(dir)) {
		char *fname;
		struct stat st;

		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0) {
			continue;
		}

		fname = talloc_asprintf(path, "%s/%s", path, de->d_name);
		if (stat(fname, &st) != 0) {
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			recursive_delete(fname);
			talloc_free(fname);
			continue;
		}
		if (unlink(fname) != 0) {
			DEBUG(0,("Unabled to delete '%s' - %s\n", 
				 fname, strerror(errno)));
			smb_panic("unable to cleanup tmp files");
		}
		talloc_free(fname);
	}
	closedir(dir);
}

/*
  cleanup temporary files. This is the new alternative to
  TDB_CLEAR_IF_FIRST. Unfortunately TDB_CLEAR_IF_FIRST is not
  efficient on unix systems due to the lack of scaling of the byte
  range locking system. So instead of putting the burden on tdb to
  cleanup tmp files, this function deletes them. 
*/
static void cleanup_tmp_files(void)
{
	char *path;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	path = smbd_tmp_path(mem_ctx, NULL);

	recursive_delete(path);
	talloc_free(mem_ctx);
}

/*
  setup signal masks
*/
static void setup_signals(void)
{
	/* we are never interested in SIGPIPE */
	BlockSignals(True,SIGPIPE);

#if defined(SIGFPE)
	/* we are never interested in SIGFPE */
	BlockSignals(True,SIGFPE);
#endif

	/* We are no longer interested in USR1 */
	BlockSignals(True, SIGUSR1);

#if defined(SIGUSR2)
	/* We are no longer interested in USR2 */
	BlockSignals(True,SIGUSR2);
#endif

	/* POSIX demands that signals are inherited. If the invoking process has
	 * these signals masked, we will have problems, as we won't recieve them. */
	BlockSignals(False, SIGHUP);
	BlockSignals(False, SIGTERM);

	/* as we don't handle on this signals yet, we need to ignore them,
	 * instead of terminating */
	CatchSignal(SIGHUP, SIG_IGN);
}

/*
  handle io on stdin
*/
static void server_stdin_handler(struct event_context *event_ctx, struct fd_event *fde, 
				 uint16_t flags, void *private)
{
	uint8_t c;
	if (read(0, &c, 1) == 0) {
		DEBUG(0,("smbd: EOF on stdin - terminating\n"));
		exit(0);
	}
}


/*
  die if the user selected maximum runtime is exceeded
*/
static void max_runtime_handler(struct event_context *ev, struct timed_event *te, 
				struct timeval t, void *private)
{
	DEBUG(0,("smbd maximum runtime exceeded - terminating\n"));
	exit(0);
}


/*
 main server.
*/
static int binary_smbd_main(int argc, const char *argv[])
{
	BOOL interactive = False;
	int opt;
	poptContext pc;
	struct event_context *event_ctx;
	NTSTATUS status;
	const char *model = "standard";
	int max_runtime = 0;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"interactive", 'i', POPT_ARG_VAL, &interactive, True, 
		 "Run interactive (not a daemon)", NULL},
		{"model", 'M', POPT_ARG_STRING, &model, True, 
		 "Select process model", "MODEL"},
		{"maximum-runtime", 0, POPT_ARG_INT, &max_runtime, True, 
		 "set maximum time for smbd to live", "seconds"},
		POPT_COMMON_SAMBA
		POPT_COMMON_VERSION
		POPT_TABLEEND
	};
	
	pc = poptGetContext("smbd", argc, argv, long_options, 0);
	
	while((opt = poptGetNextOpt(pc)) != -1) /* noop */ ;

	poptFreeContext(pc);

	setup_logging(argv[0], interactive?DEBUG_STDOUT:DEBUG_FILE);
	setup_signals();

	/* we want total control over the permissions on created files,
	   so set our umask to 0 */
	umask(0);

	DEBUG(0,("smbd version %s started.\n", SAMBA_VERSION_STRING));
	DEBUGADD(0,("Copyright Andrew Tridgell and the Samba Team 1992-2005\n"));

	if (sizeof(uint16_t) < 2 || sizeof(uint32_t) < 4 || sizeof(uint64_t) < 8) {
		DEBUG(0,("ERROR: Samba is not configured correctly for the word size on your machine\n"));
		exit(1);
	}

	if (!interactive) {
		DEBUG(3,("Becoming a daemon.\n"));
		become_daemon(True);
	}

	cleanup_tmp_files();

	if (!directory_exist(lp_lockdir())) {
		mkdir(lp_lockdir(), 0755);
	}

	pidfile_create("smbd");

	/* Do *not* remove this, until you have removed
	 * passdb/secrets.c, and proved that Samba still builds... */
	/* Setup the SECRETS subsystem */
	if (!secrets_init()) {
		exit(1);
	}

	smbd_init_subsystems;

	/* the event context is the top level structure in smbd. Everything else
	   should hang off that */
	event_ctx = event_context_init(NULL);

	if (interactive) {
		/* catch EOF on stdin */
#ifdef SIGTTIN
		signal(SIGTTIN, SIG_IGN);
#endif
		event_add_fd(event_ctx, event_ctx, 0, EVENT_FD_READ, 
			     server_stdin_handler, NULL);
	}


	if (max_runtime) {
		event_add_timed(event_ctx, event_ctx, 
				timeval_current_ofs(max_runtime, 0), 
				max_runtime_handler, NULL);
	}

	DEBUG(0,("Using %s process model\n", model));
	status = server_service_startup(event_ctx, model, lp_server_services());
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Starting Services failed - %s\n", nt_errstr(status)));
		return 1;
	}

	/* Hack to ensure that GSSAPI uses the right event context */
	gssapi_krb5_init_ev(event_ctx);

	/* wait for events - this is where smbd sits for most of its
	   life */
	event_loop_wait(event_ctx);

	/* as everything hangs off this event context, freeing it
	   should initiate a clean shutdown of all services */
	talloc_free(event_ctx);

	return 0;
}

 int main(int argc, const char *argv[])
{
	return binary_smbd_main(argc, argv);
}
