/* this tests tdb by doing lots of ops from several simultaneous
   writers - that stresses the locking code. Build with TDB_DEBUG=1
   for best effect */

#ifndef _SAMBA_BUILD_
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include "tdb.h"

#else

#include "includes.h"
#include "lib/tdb/include/tdb.h"
#include "system/time.h"
#include "system/wait.h"
#include "system/filesys.h"

#endif

#define REOPEN_PROB 30
#define DELETE_PROB 8
#define STORE_PROB 4
#define APPEND_PROB 6
#define LOCKSTORE_PROB 0
#define TRAVERSE_PROB 20
#define CULL_PROB 100
#define KEYLEN 3
#define DATALEN 100
#define LOCKLEN 20

static struct tdb_context *db;

#ifdef PRINTF_ATTRIBUTE
static void tdb_log(struct tdb_context *tdb, int level, const char *format, ...) PRINTF_ATTRIBUTE(3,4);
#endif
static void tdb_log(struct tdb_context *tdb, int level, const char *format, ...)
{
	va_list ap;
    
	va_start(ap, format);
	vfprintf(stdout, format, ap);
	va_end(ap);
	fflush(stdout);
#if 0
	{
		char *ptr;
		asprintf(&ptr,"xterm -e gdb /proc/%d/exe %d", getpid(), getpid());
		system(ptr);
		free(ptr);
	}
#endif	
}

static void fatal(const char *why)
{
	perror(why);
	exit(1);
}

static char *randbuf(int len)
{
	char *buf;
	int i;
	buf = (char *)malloc(len+1);

	for (i=0;i<len;i++) {
		buf[i] = 'a' + (rand() % 26);
	}
	buf[i] = 0;
	return buf;
}

static int cull_traverse(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
			 void *state)
{
	if (random() % CULL_PROB == 0) {
		tdb_delete(tdb, key);
	}
	return 0;
}

static void addrec_db(void)
{
	int klen, dlen, slen;
	char *k, *d, *s;
	TDB_DATA key, data, lockkey;

	klen = 1 + (rand() % KEYLEN);
	dlen = 1 + (rand() % DATALEN);
	slen = 1 + (rand() % LOCKLEN);

	k = randbuf(klen);
	d = randbuf(dlen);
	s = randbuf(slen);

	key.dptr = (unsigned char *)k;
	key.dsize = klen+1;

	data.dptr = (unsigned char *)d;
	data.dsize = dlen+1;

	lockkey.dptr = (unsigned char *)s;
	lockkey.dsize = slen+1;

#if REOPEN_PROB
	if (random() % REOPEN_PROB == 0) {
		tdb_reopen_all();
		goto next;
	} 
#endif

#if DELETE_PROB
	if (random() % DELETE_PROB == 0) {
		tdb_delete(db, key);
		goto next;
	}
#endif

#if STORE_PROB
	if (random() % STORE_PROB == 0) {
		if (tdb_store(db, key, data, TDB_REPLACE) != 0) {
			fatal("tdb_store failed");
		}
		goto next;
	}
#endif

#if APPEND_PROB
	if (random() % APPEND_PROB == 0) {
		if (tdb_append(db, key, data) != 0) {
			fatal("tdb_append failed");
		}
		goto next;
	}
#endif

#if LOCKSTORE_PROB
	if (random() % LOCKSTORE_PROB == 0) {
		tdb_chainlock(db, lockkey);
		data = tdb_fetch(db, key);
		if (tdb_store(db, key, data, TDB_REPLACE) != 0) {
			fatal("tdb_store failed");
		}
		if (data.dptr) free(data.dptr);
		tdb_chainunlock(db, lockkey);
		goto next;
	} 
#endif

#if TRAVERSE_PROB
	if (random() % TRAVERSE_PROB == 0) {
		tdb_traverse(db, cull_traverse, NULL);
		goto next;
	}
#endif

	data = tdb_fetch(db, key);
	if (data.dptr) free(data.dptr);

next:
	free(k);
	free(d);
	free(s);
}

static int traverse_fn(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
                       void *state)
{
	tdb_delete(tdb, key);
	return 0;
}

#ifndef NPROC
#define NPROC 2
#endif

#ifndef NLOOPS
#define NLOOPS 5000
#endif

 int main(int argc, const char *argv[])
{
	int i, seed=0;
	int loops = NLOOPS;
	pid_t pids[NPROC];

	pids[0] = getpid();

	unlink("torture.tdb");

	for (i=0;i<NPROC-1;i++) {
		if ((pids[i+1]=fork()) == 0) break;
	}

	db = tdb_open("torture.tdb", 2, TDB_CLEAR_IF_FIRST, 
		      O_RDWR | O_CREAT, 0600);
	if (!db) {
		fatal("db open failed");
	}
	tdb_logging_function(db, tdb_log);

	srand(seed + getpid());
	srandom(seed + getpid() + time(NULL));
	for (i=0;i<loops;i++) addrec_db();

	tdb_traverse(db, NULL, NULL);
	tdb_traverse(db, traverse_fn, NULL);
	tdb_traverse(db, traverse_fn, NULL);

	tdb_close(db);

	if (getpid() == pids[0]) {
		for (i=0;i<NPROC-1;i++) {
			int status;
			if (waitpid(pids[i+1], &status, 0) != pids[i+1]) {
				printf("failed to wait for %d\n",
				       (int)pids[i+1]);
				exit(1);
			}
			if (WEXITSTATUS(status) != 0) {
				printf("child %d exited with status %d\n",
				       (int)pids[i+1], WEXITSTATUS(status));
				exit(1);
			}
		}
		printf("OK\n");
	}

	return 0;
}
