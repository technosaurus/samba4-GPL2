#define NBDEBUG 0

/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-1998
   
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

#define MAX_FILES 100

extern int nbench_line_count;
static int nbio_id;
static int nprocs;
static BOOL bypass_io;
static int warmup;

static struct {
	int fd;
	int handle;
} ftable[MAX_FILES];

static struct {
	double bytes_in, bytes_out;
	int line;
	int done;
} *children;

double nbio_total(void)
{
	int i;
	double total = 0;
	for (i=0;i<nprocs;i++) {
		total += children[i].bytes_out + children[i].bytes_in;
	}
	return total;
}

void nb_warmup_done(void)
{
	children[nbio_id].bytes_out = 0;
	children[nbio_id].bytes_in = 0;
}


void nb_alarm(void)
{
	int i;
	int lines=0, num_clients=0;
	double t;

	if (nbio_id != -1) return;

	for (i=0;i<nprocs;i++) {
		lines += children[i].line;
		if (!children[i].done) num_clients++;
	}

	t = end_timer();

	if (warmup) {
		printf("%4d  %8d  %.2f MB/sec  warmup %.0f sec   \r", 
		       num_clients, lines/nprocs, 
		       1.0e-6 * nbio_total() / t, 
		       t);
	} else {
		printf("%4d  %8d  %.2f MB/sec  execute %.0f sec   \r", 
		       num_clients, lines/nprocs, 
		       1.0e-6 * nbio_total() / t, 
		       t);
	}

	if (warmup && t >= warmup) {
		start_timer();
		warmup = 0;
	}

	fflush(stdout);

	signal(SIGALRM, nb_alarm);
	alarm(1);	
}

void nbio_shmem(int n)
{
	nprocs = n;
	children = shm_setup(sizeof(*children) * nprocs);
	if (!children) {
		printf("Failed to setup shared memory!\n");
		exit(1);
	}
}

static int find_handle(int handle)
{
	int i;
	children[nbio_id].line = nbench_line_count;
	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == handle) return i;
	}
	printf("(%d) ERROR: handle %d was not found\n", 
	       nbench_line_count, handle);
	exit(1);

	return -1;		/* Not reached */
}


static struct cli_state *c;

void nb_setup(struct cli_state *cli, int id, int warmupt)
{
	warmup = warmupt;
	nbio_id = id;
	c = cli;
	start_timer();
	if (children) {
		children[nbio_id].done = 0;
	}
	if (bypass_io)
		printf("skipping I/O\n");
}


static void check_status(const char *op, NTSTATUS status, NTSTATUS ret)
{
	if (!NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(ret)) {
		printf("[%d] Error: %s should have failed with %s\n", 
		       nbench_line_count, op, nt_errstr(status));
		exit(1);
	}

	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(ret)) {
		printf("[%d] Error: %s should have succeeded - %s\n", 
		       nbench_line_count, op, nt_errstr(ret));
		exit(1);
	}

	if (!NT_STATUS_EQUAL(status, ret)) {
		printf("[%d] Warning: got status %s but expected %s\n",
		       nbench_line_count, nt_errstr(ret), nt_errstr(status));
	}
}


void nb_unlink(const char *fname, int attr, NTSTATUS status)
{
	struct smb_unlink io;
	NTSTATUS ret;

	io.in.pattern = fname;

	io.in.attrib = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	if (strchr(fname, '*') == 0) {
		io.in.attrib |= FILE_ATTRIBUTE_DIRECTORY;
	}

	ret = smb_raw_unlink(c->tree, &io);

	check_status("Unlink", status, ret);
}


void nb_createx(const char *fname, 
		unsigned create_options, unsigned create_disposition, int handle,
		NTSTATUS status)
{
	union smb_open io;	
	int i;
	uint32 desired_access;
	NTSTATUS ret;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("raw_open");

	if (create_options & NTCREATEX_OPTIONS_DIRECTORY) {
		desired_access = SA_RIGHT_FILE_READ_DATA;
	} else {
		desired_access = 
			SA_RIGHT_FILE_READ_DATA | 
			SA_RIGHT_FILE_WRITE_DATA |
			SA_RIGHT_FILE_READ_ATTRIBUTES |
			SA_RIGHT_FILE_WRITE_ATTRIBUTES;
	}

	io.ntcreatex.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.access_mask = desired_access;
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = create_disposition;
	io.ntcreatex.in.create_options = create_options;
	io.ntcreatex.in.impersonation = 0;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	ret = smb_raw_open(c->tree, mem_ctx, &io);

	talloc_destroy(mem_ctx);

	check_status("NTCreateX", status, ret);

	if (!NT_STATUS_IS_OK(ret)) return;

	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == 0) break;
	}
	if (i == MAX_FILES) {
		printf("(%d) file table full for %s\n", nbench_line_count, 
		       fname);
		exit(1);
	}
	ftable[i].handle = handle;
	ftable[i].fd = io.ntcreatex.out.fnum;
}

void nb_writex(int handle, int offset, int size, int ret_size, NTSTATUS status)
{
	union smb_write io;
	int i;
	NTSTATUS ret;
	char *buf;

	i = find_handle(handle);

	if (bypass_io) return;

	buf = malloc(size);
	memset(buf, 0xab, size);

	io.writex.level = RAW_WRITE_WRITEX;
	io.writex.in.fnum = ftable[i].fd;
	io.writex.in.wmode = 0;
	io.writex.in.remaining = 0;
	io.writex.in.offset = offset;
	io.writex.in.count = size;
	io.writex.in.data = buf;

	ret = smb_raw_write(c->tree, &io);

	free(buf);

	check_status("WriteX", status, ret);

	if (NT_STATUS_IS_OK(ret) && io.writex.out.nwritten != ret_size) {
		printf("[%d] Warning: WriteX got count %d expected %d\n", 
		       nbench_line_count,
		       io.writex.out.nwritten, ret_size);
	}	

	children[nbio_id].bytes_out += ret_size;
}

void nb_write(int handle, int offset, int size, int ret_size, NTSTATUS status)
{
	union smb_write io;
	int i;
	NTSTATUS ret;
	char *buf;

	i = find_handle(handle);

	if (bypass_io) return;

	buf = malloc(size);

	memset(buf, 0x12, size);

	io.write.level = RAW_WRITE_WRITE;
	io.write.in.fnum = ftable[i].fd;
	io.write.in.remaining = 0;
	io.write.in.offset = offset;
	io.write.in.count = size;
	io.write.in.data = buf;

	ret = smb_raw_write(c->tree, &io);

	free(buf);

	check_status("Write", status, ret);

	if (NT_STATUS_IS_OK(ret) && io.write.out.nwritten != ret_size) {
		printf("[%d] Warning: Write got count %d expected %d\n", 
		       nbench_line_count,
		       io.write.out.nwritten, ret_size);
	}	

	children[nbio_id].bytes_out += ret_size;
}


void nb_lockx(int handle, unsigned offset, int size, NTSTATUS status)
{
	union smb_lock io;
	int i;
	NTSTATUS ret;
	struct smb_lock_entry lck;

	i = find_handle(handle);

	lck.pid = getpid();
	lck.offset = offset;
	lck.count = size;

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.fnum = ftable[i].fd;
	io.lockx.in.mode = 0;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lck;

	ret = smb_raw_lock(c->tree, &io);

	check_status("LockX", status, ret);
}

void nb_unlockx(int handle, unsigned offset, int size, NTSTATUS status)
{
	union smb_lock io;
	int i;
	NTSTATUS ret;
	struct smb_lock_entry lck;

	i = find_handle(handle);

	lck.pid = getpid();
	lck.offset = offset;
	lck.count = size;

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.fnum = ftable[i].fd;
	io.lockx.in.mode = 0;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = &lck;

	ret = smb_raw_lock(c->tree, &io);

	check_status("UnlockX", status, ret);
}

void nb_readx(int handle, int offset, int size, int ret_size, NTSTATUS status)
{
	union smb_read io;
	int i;
	NTSTATUS ret;
	char *buf;

	i = find_handle(handle);

	if (bypass_io) return;

	buf = malloc(size);

	io.readx.level = RAW_READ_READX;
	io.readx.in.fnum = ftable[i].fd;
	io.readx.in.offset    = offset;
	io.readx.in.mincnt    = size;
	io.readx.in.maxcnt    = size;
	io.readx.in.remaining = 0;
	io.readx.out.data     = buf;
		
	ret = smb_raw_read(c->tree, &io);

	free(buf);

	check_status("ReadX", status, ret);

	if (NT_STATUS_IS_OK(ret) && io.readx.out.nread != ret_size) {
		printf("[%d] Warning: ReadX got count %d expected %d\n", 
		       nbench_line_count,
		       io.readx.out.nread, ret_size);
	}	

	children[nbio_id].bytes_in += ret_size;
}

void nb_close(int handle, NTSTATUS status)
{
	int i;
	NTSTATUS ret;
	union smb_close io;
	
	i = find_handle(handle);

	io.close.level = RAW_CLOSE_CLOSE;
	io.close.in.fnum = ftable[i].fd;
	io.close.in.write_time = 0;

	ret = smb_raw_close(c->tree, &io);

	check_status("Close", status, ret);

	if (NT_STATUS_IS_OK(ret)) {
		ftable[i].handle = 0;
	}
}

void nb_rmdir(const char *dname, NTSTATUS status)
{
	NTSTATUS ret;
	struct smb_rmdir io;

	io.in.path = dname;

	ret = smb_raw_rmdir(c->tree, &io);

	check_status("Rmdir", status, ret);
}

void nb_mkdir(const char *dname, NTSTATUS status)
{
	union smb_mkdir io;

	io.mkdir.level = RAW_MKDIR_MKDIR;
	io.mkdir.in.path = dname;

	/* NOTE! no error checking. Used for base fileset creation */
	smb_raw_mkdir(c->tree, &io);
}

void nb_rename(const char *old, const char *new, NTSTATUS status)
{
	NTSTATUS ret;
	union smb_rename io;

	io.generic.level = RAW_RENAME_RENAME;
	io.rename.in.attrib = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY;
	io.rename.in.pattern1 = old;
	io.rename.in.pattern2 = new;

	ret = smb_raw_rename(c->tree, &io);

	check_status("Rename", status, ret);
}


void nb_qpathinfo(const char *fname, int level, NTSTATUS status)
{
	union smb_fileinfo io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;

	mem_ctx = talloc_init("nb_qpathinfo");

	io.generic.level = level;
	io.generic.in.fname = fname;

	ret = smb_raw_pathinfo(c->tree, mem_ctx, &io);

	talloc_destroy(mem_ctx);

	check_status("Pathinfo", status, ret);
}


void nb_qfileinfo(int fnum, int level, NTSTATUS status)
{
	union smb_fileinfo io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;
	int i;

	i = find_handle(fnum);

	mem_ctx = talloc_init("nb_qfileinfo");

	io.generic.level = level;
	io.generic.in.fnum = ftable[i].fd;

	ret = smb_raw_fileinfo(c->tree, mem_ctx, &io);

	talloc_destroy(mem_ctx);

	check_status("Fileinfo", status, ret);
}

void nb_sfileinfo(int fnum, int level, NTSTATUS status)
{
	union smb_setfileinfo io;
	NTSTATUS ret;
	int i;

	if (level != RAW_SFILEINFO_BASIC_INFORMATION) {
		printf("[%d] Warning: setfileinfo level %d not handled\n", nbench_line_count, level);
		return;
	}

	ZERO_STRUCT(io);

	i = find_handle(fnum);

	io.generic.level = level;
	io.generic.file.fnum = ftable[i].fd;
	unix_to_nt_time(&io.basic_info.in.create_time, time(NULL));
	unix_to_nt_time(&io.basic_info.in.access_time, 0);
	unix_to_nt_time(&io.basic_info.in.write_time, 0);
	unix_to_nt_time(&io.basic_info.in.change_time, 0);
	io.basic_info.in.attrib = 0;

	ret = smb_raw_setfileinfo(c->tree, &io);

	check_status("Setfileinfo", status, ret);
}

void nb_qfsinfo(int level, NTSTATUS status)
{
	union smb_fsinfo io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;

	mem_ctx = talloc_init("cli_dskattr");

	io.generic.level = level;
	ret = smb_raw_fsinfo(c->tree, mem_ctx, &io);

	talloc_destroy(mem_ctx);
	
	check_status("Fsinfo", status, ret);	
}

/* callback function used for trans2 search */
static BOOL findfirst_callback(void *private, union smb_search_data *file)
{
	return True;
}

void nb_findfirst(const char *mask, int level, int maxcnt, int count, NTSTATUS status)
{
	union smb_search_first io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;

	mem_ctx = talloc_init("cli_dskattr");

	io.t2ffirst.level = level;
	io.t2ffirst.in.max_count = maxcnt;
	io.t2ffirst.in.search_attrib = FILE_ATTRIBUTE_DIRECTORY;
	io.t2ffirst.in.pattern = mask;
	io.t2ffirst.in.flags = FLAG_TRANS2_FIND_CLOSE;
	io.t2ffirst.in.storage_type = 0;
			
	ret = smb_raw_search_first(c->tree, mem_ctx, &io, NULL, findfirst_callback);

	talloc_destroy(mem_ctx);

	check_status("Search", status, ret);

	if (NT_STATUS_IS_OK(ret) && io.t2ffirst.out.count != count) {
		printf("[%d] Warning: got count %d expected %d\n", 
		       nbench_line_count,
		       io.t2ffirst.out.count, count);
	}
}

void nb_flush(int fnum, NTSTATUS status)
{
	struct smb_flush io;
	NTSTATUS ret;
	int i;
	i = find_handle(fnum);

	io.in.fnum = ftable[i].fd;

	ret = smb_raw_flush(c->tree, &io);

	check_status("Flush", status, ret);
}

void nb_deltree(const char *dname)
{
	int total_deleted;

	smb_raw_exit(c->session);

	ZERO_STRUCT(ftable);

	total_deleted = cli_deltree(c->tree, dname);

	if (total_deleted == -1) {
		printf("Failed to cleanup tree %s - exiting\n", dname);
		exit(1);
	}

	cli_rmdir(c->tree, dname);
}

void nb_cleanup(const char *cname)
{
	char *dname = NULL;
	asprintf(&dname, "\\clients\\%s", cname);
	nb_deltree(dname);
	free(dname);
	cli_rmdir(c->tree, "clients");
	children[nbio_id].done = 1;
}
