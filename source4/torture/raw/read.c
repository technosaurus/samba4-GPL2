/* 
   Unix SMB/CIFS implementation.
   test suite for various read operations
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

#include "includes.h"
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = False; \
		goto done; \
	}} while (0)

#define CHECK_VALUE(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%s) Incorrect value %s=%ld - should be %ld\n", \
		       __location__, #v, (long)v, (long)correct); \
		ret = False; \
		goto done; \
	}} while (0)

#define CHECK_BUFFER(buf, seed, len) do { \
	if (!check_buffer(buf, seed, len, __LINE__)) { \
		ret = False; \
		goto done; \
	}} while (0)

#define BASEDIR "\\testread"


/*
  setup a random buffer based on a seed
*/
static void setup_buffer(uint8_t *buf, uint_t seed, int len)
{
	int i;
	srandom(seed);
	for (i=0;i<len;i++) buf[i] = random();
}

/*
  check a random buffer based on a seed
*/
static BOOL check_buffer(uint8_t *buf, uint_t seed, int len, int line)
{
	int i;
	srandom(seed);
	for (i=0;i<len;i++) {
		uint8_t v = random();
		if (buf[i] != v) {
			printf("Buffer incorrect at line %d! ofs=%d v1=0x%x v2=0x%x\n", 
			       line, i, buf[i], v);
			return False;
		}
	}
	return True;
}

/*
  test read ops
*/
static BOOL test_read(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_read io;
	NTSTATUS status;
	BOOL ret = True;
	int fnum;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	const char *test_data = "TEST DATA";
	uint_t seed = time(NULL);

	buf = talloc_zero_size(mem_ctx, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	printf("Testing RAW_READ_READ\n");
	io.generic.level = RAW_READ_READ;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = False;
		goto done;
	}

	printf("Trying empty file read\n");
	io.read.in.file.fnum = fnum;
	io.read.in.count = 1;
	io.read.in.offset = 0;
	io.read.in.remaining = 0;
	io.read.out.data = buf;
	status = smb_raw_read(cli->tree, &io);

	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.read.out.nread, 0);

	printf("Trying zero file read\n");
	io.read.in.count = 0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.read.out.nread, 0);

	printf("Trying bad fnum\n");
	io.read.in.file.fnum = fnum+1;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);
	io.read.in.file.fnum = fnum;

	smbcli_write(cli->tree, fnum, 0, test_data, 0, strlen(test_data));

	printf("Trying small read\n");
	io.read.in.file.fnum = fnum;
	io.read.in.offset = 0;
	io.read.in.remaining = 0;
	io.read.in.count = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.read.out.nread, strlen(test_data));
	if (memcmp(buf, test_data, strlen(test_data)) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data, buf);
		goto done;
	}

	printf("Trying short read\n");
	io.read.in.offset = 1;
	io.read.in.count = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.read.out.nread, strlen(test_data)-1);
	if (memcmp(buf, test_data+1, strlen(test_data)-1) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data+1, buf);
		goto done;
	}

	printf("Trying max offset\n");
	io.read.in.offset = ~0;
	io.read.in.count = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.read.out.nread, 0);

	setup_buffer(buf, seed, maxsize);
	smbcli_write(cli->tree, fnum, 0, buf, 0, maxsize);
	memset(buf, 0, maxsize);

	printf("Trying large read\n");
	io.read.in.offset = 0;
	io.read.in.count = ~0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_BUFFER(buf, seed, io.read.out.nread);


	printf("Trying locked region\n");
	cli->session->pid++;
	if (NT_STATUS_IS_ERR(smbcli_lock(cli->tree, fnum, 103, 1, 0, WRITE_LOCK))) {
		printf("Failed to lock file at %d\n", __LINE__);
		ret = False;
		goto done;
	}
	cli->session->pid--;
	memset(buf, 0, maxsize);
	io.read.in.offset = 0;
	io.read.in.count = ~0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test lockread ops
*/
static BOOL test_lockread(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_read io;
	NTSTATUS status;
	BOOL ret = True;
	int fnum;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	const char *test_data = "TEST DATA";
	uint_t seed = time(NULL);

	buf = talloc_zero_size(mem_ctx, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	printf("Testing RAW_READ_LOCKREAD\n");
	io.generic.level = RAW_READ_LOCKREAD;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = False;
		goto done;
	}

	printf("Trying empty file read\n");
	io.lockread.in.file.fnum = fnum;
	io.lockread.in.count = 1;
	io.lockread.in.offset = 1;
	io.lockread.in.remaining = 0;
	io.lockread.out.data = buf;
	status = smb_raw_read(cli->tree, &io);

	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.lockread.out.nread, 0);

	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	printf("Trying zero file read\n");
	io.lockread.in.count = 0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_unlock(cli->tree, fnum, 1, 1);

	printf("Trying bad fnum\n");
	io.lockread.in.file.fnum = fnum+1;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);
	io.lockread.in.file.fnum = fnum;

	smbcli_write(cli->tree, fnum, 0, test_data, 0, strlen(test_data));

	printf("Trying small read\n");
	io.lockread.in.file.fnum = fnum;
	io.lockread.in.offset = 0;
	io.lockread.in.remaining = 0;
	io.lockread.in.count = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	smbcli_unlock(cli->tree, fnum, 1, 0);

	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.lockread.out.nread, strlen(test_data));
	if (memcmp(buf, test_data, strlen(test_data)) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data, buf);
		goto done;
	}

	printf("Trying short read\n");
	io.lockread.in.offset = 1;
	io.lockread.in.count = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	smbcli_unlock(cli->tree, fnum, 0, strlen(test_data));
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	CHECK_VALUE(io.lockread.out.nread, strlen(test_data)-1);
	if (memcmp(buf, test_data+1, strlen(test_data)-1) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data+1, buf);
		goto done;
	}

	printf("Trying max offset\n");
	io.lockread.in.offset = ~0;
	io.lockread.in.count = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.lockread.out.nread, 0);

	setup_buffer(buf, seed, maxsize);
	smbcli_write(cli->tree, fnum, 0, buf, 0, maxsize);
	memset(buf, 0, maxsize);

	printf("Trying large read\n");
	io.lockread.in.offset = 0;
	io.lockread.in.count = ~0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	smbcli_unlock(cli->tree, fnum, 1, strlen(test_data));
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_BUFFER(buf, seed, io.lockread.out.nread);
	smbcli_unlock(cli->tree, fnum, 0, 0xFFFF);


	printf("Trying locked region\n");
	cli->session->pid++;
	if (NT_STATUS_IS_ERR(smbcli_lock(cli->tree, fnum, 103, 1, 0, WRITE_LOCK))) {
		printf("Failed to lock file at %d\n", __LINE__);
		ret = False;
		goto done;
	}
	cli->session->pid--;
	memset(buf, 0, maxsize);
	io.lockread.in.offset = 0;
	io.lockread.in.count = ~0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test readx ops
*/
static BOOL test_readx(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_read io;
	NTSTATUS status;
	BOOL ret = True;
	int fnum;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	const char *test_data = "TEST DATA";
	uint_t seed = time(NULL);

	buf = talloc_zero_size(mem_ctx, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	printf("Testing RAW_READ_READX\n");
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = False;
		goto done;
	}

	printf("Trying empty file read\n");
	io.generic.level = RAW_READ_READX;
	io.readx.in.file.fnum = fnum;
	io.readx.in.mincnt = 1;
	io.readx.in.maxcnt = 1;
	io.readx.in.offset = 0;
	io.readx.in.remaining = 0;
	io.readx.in.read_for_execute = False;
	io.readx.out.data = buf;
	status = smb_raw_read(cli->tree, &io);

	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.nread, 0);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);

	printf("Trying zero file read\n");
	io.readx.in.mincnt = 0;
	io.readx.in.maxcnt = 0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.nread, 0);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);

	printf("Trying bad fnum\n");
	io.readx.in.file.fnum = fnum+1;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);
	io.readx.in.file.fnum = fnum;

	smbcli_write(cli->tree, fnum, 0, test_data, 0, strlen(test_data));

	printf("Trying small read\n");
	io.readx.in.file.fnum = fnum;
	io.readx.in.offset = 0;
	io.readx.in.remaining = 0;
	io.readx.in.read_for_execute = False;
	io.readx.in.mincnt = strlen(test_data);
	io.readx.in.maxcnt = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.nread, strlen(test_data));
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);
	if (memcmp(buf, test_data, strlen(test_data)) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data, buf);
		goto done;
	}

	printf("Trying short read\n");
	io.readx.in.offset = 1;
	io.readx.in.mincnt = strlen(test_data);
	io.readx.in.maxcnt = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.nread, strlen(test_data)-1);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);
	if (memcmp(buf, test_data+1, strlen(test_data)-1) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data+1, buf);
		goto done;
	}

	printf("Trying max offset\n");
	io.readx.in.offset = 0xffffffff;
	io.readx.in.mincnt = strlen(test_data);
	io.readx.in.maxcnt = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.nread, 0);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);

	setup_buffer(buf, seed, maxsize);
	smbcli_write(cli->tree, fnum, 0, buf, 0, maxsize);
	memset(buf, 0, maxsize);

	printf("Trying large read\n");
	io.readx.in.offset = 0;
	io.readx.in.mincnt = 0xFFFF;
	io.readx.in.maxcnt = 0xFFFF;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);
	CHECK_VALUE(io.readx.out.nread, io.readx.in.maxcnt);
	CHECK_BUFFER(buf, seed, io.readx.out.nread);

	printf("Trying extra large read\n");
	io.readx.in.offset = 0;
	io.readx.in.mincnt = 100;
	io.readx.in.maxcnt = 80000;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);
	if (lp_parm_bool(-1, "target", "samba3", False)) {
		printf("SAMBA3: ignore wrong nread[%d] should be [%d]\n",
			io.readx.out.nread, 0);
	} else {
		CHECK_VALUE(io.readx.out.nread, 0);
	}
	CHECK_BUFFER(buf, seed, io.readx.out.nread);

	printf("Trying mincnt > maxcnt\n");
	memset(buf, 0, maxsize);
	io.readx.in.offset = 0;
	io.readx.in.mincnt = 30000;
	io.readx.in.maxcnt = 20000;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);
	CHECK_VALUE(io.readx.out.nread, io.readx.in.maxcnt);
	CHECK_BUFFER(buf, seed, io.readx.out.nread);

	printf("Trying mincnt < maxcnt\n");
	memset(buf, 0, maxsize);
	io.readx.in.offset = 0;
	io.readx.in.mincnt = 20000;
	io.readx.in.maxcnt = 30000;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(io.readx.out.compaction_mode, 0);
	CHECK_VALUE(io.readx.out.nread, io.readx.in.maxcnt);
	CHECK_BUFFER(buf, seed, io.readx.out.nread);

	printf("Trying locked region\n");
	cli->session->pid++;
	if (NT_STATUS_IS_ERR(smbcli_lock(cli->tree, fnum, 103, 1, 0, WRITE_LOCK))) {
		printf("Failed to lock file at %d\n", __LINE__);
		ret = False;
		goto done;
	}
	cli->session->pid--;
	memset(buf, 0, maxsize);
	io.readx.in.offset = 0;
	io.readx.in.mincnt = 100;
	io.readx.in.maxcnt = 200;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);	

	printf("Trying large offset read\n");
	io.readx.in.offset = ((uint64_t)0x2) << 32;
	io.readx.in.mincnt = 10;
	io.readx.in.maxcnt = 10;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.nread, 0);

	if (NT_STATUS_IS_ERR(smbcli_lock64(cli->tree, fnum, io.readx.in.offset, 1, 0, WRITE_LOCK))) {
		printf("Failed to lock file at %d\n", __LINE__);
		ret = False;
		goto done;
	}

	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readx.out.nread, 0);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test readbraw ops
*/
static BOOL test_readbraw(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_read io;
	NTSTATUS status;
	BOOL ret = True;
	int fnum;
	uint8_t *buf;
	const int maxsize = 90000;
	const char *fname = BASEDIR "\\test.txt";
	const char *test_data = "TEST DATA";
	uint_t seed = time(NULL);

	buf = talloc_zero_size(mem_ctx, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	printf("Testing RAW_READ_READBRAW\n");
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
		ret = False;
		goto done;
	}

	printf("Trying empty file read\n");
	io.generic.level = RAW_READ_READBRAW;
	io.readbraw.in.file.fnum = fnum;
	io.readbraw.in.mincnt = 1;
	io.readbraw.in.maxcnt = 1;
	io.readbraw.in.offset = 0;
	io.readbraw.in.timeout = 0;
	io.readbraw.out.data = buf;
	status = smb_raw_read(cli->tree, &io);

	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0);

	printf("Trying zero file read\n");
	io.readbraw.in.mincnt = 0;
	io.readbraw.in.maxcnt = 0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0);

	printf("Trying bad fnum\n");
	io.readbraw.in.file.fnum = fnum+1;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0);
	io.readbraw.in.file.fnum = fnum;

	smbcli_write(cli->tree, fnum, 0, test_data, 0, strlen(test_data));

	printf("Trying small read\n");
	io.readbraw.in.file.fnum = fnum;
	io.readbraw.in.offset = 0;
	io.readbraw.in.mincnt = strlen(test_data);
	io.readbraw.in.maxcnt = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, strlen(test_data));
	if (memcmp(buf, test_data, strlen(test_data)) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data, buf);
		goto done;
	}

	printf("Trying short read\n");
	io.readbraw.in.offset = 1;
	io.readbraw.in.mincnt = strlen(test_data);
	io.readbraw.in.maxcnt = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, strlen(test_data)-1);
	if (memcmp(buf, test_data+1, strlen(test_data)-1) != 0) {
		ret = False;
		printf("incorrect data at %d!? (%s:%s)\n", __LINE__, test_data+1, buf);
		goto done;
	}

	printf("Trying max offset\n");
	io.readbraw.in.offset = ~0;
	io.readbraw.in.mincnt = strlen(test_data);
	io.readbraw.in.maxcnt = strlen(test_data);
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0);

	setup_buffer(buf, seed, maxsize);
	smbcli_write(cli->tree, fnum, 0, buf, 0, maxsize);
	memset(buf, 0, maxsize);

	printf("Trying large read\n");
	io.readbraw.in.offset = 0;
	io.readbraw.in.mincnt = ~0;
	io.readbraw.in.maxcnt = ~0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0xFFFF);
	CHECK_BUFFER(buf, seed, io.readbraw.out.nread);

	printf("Trying mincnt > maxcnt\n");
	memset(buf, 0, maxsize);
	io.readbraw.in.offset = 0;
	io.readbraw.in.mincnt = 30000;
	io.readbraw.in.maxcnt = 20000;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, io.readbraw.in.maxcnt);
	CHECK_BUFFER(buf, seed, io.readbraw.out.nread);

	printf("Trying mincnt < maxcnt\n");
	memset(buf, 0, maxsize);
	io.readbraw.in.offset = 0;
	io.readbraw.in.mincnt = 20000;
	io.readbraw.in.maxcnt = 30000;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, io.readbraw.in.maxcnt);
	CHECK_BUFFER(buf, seed, io.readbraw.out.nread);

	printf("Trying locked region\n");
	cli->session->pid++;
	if (NT_STATUS_IS_ERR(smbcli_lock(cli->tree, fnum, 103, 1, 0, WRITE_LOCK))) {
		printf("Failed to lock file at %d\n", __LINE__);
		ret = False;
		goto done;
	}
	cli->session->pid--;
	memset(buf, 0, maxsize);
	io.readbraw.in.offset = 0;
	io.readbraw.in.mincnt = 100;
	io.readbraw.in.maxcnt = 200;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0);

	printf("Trying locked region with timeout\n");
	memset(buf, 0, maxsize);
	io.readbraw.in.offset = 0;
	io.readbraw.in.mincnt = 100;
	io.readbraw.in.maxcnt = 200;
	io.readbraw.in.timeout = 10000;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0);

	printf("Trying large offset read\n");
	io.readbraw.in.offset = ((uint64_t)0x2) << 32;
	io.readbraw.in.mincnt = 10;
	io.readbraw.in.maxcnt = 10;
	io.readbraw.in.timeout = 0;
	status = smb_raw_read(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.readbraw.out.nread, 0);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test read for execute
*/
static BOOL test_read_for_execute(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open op;
	union smb_write wr;
	union smb_read rd;
	NTSTATUS status;
	BOOL ret = True;
	int fnum=0;
	uint8_t *buf;
	const int maxsize = 900;
	const char *fname = BASEDIR "\\test.txt";
	const uint8_t data[] = "TEST DATA";

	buf = talloc_zero_size(mem_ctx, maxsize);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	printf("Testing RAW_READ_READX with read_for_execute\n");

	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	op.ntcreatex.in.create_options = 0;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = ARRAY_SIZE(data);
	wr.writex.in.data = data;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, ARRAY_SIZE(data));

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("open file with SEC_FILE_EXECUTE\n");
	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_FILE_EXECUTE;
	op.ntcreatex.in.create_options = 0;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	printf("read with FLAGS2_READ_PERMIT_EXECUTE\n");
	rd.generic.level = RAW_READ_READX;
	rd.readx.in.file.fnum = fnum;
	rd.readx.in.mincnt = 0;
	rd.readx.in.maxcnt = maxsize;
	rd.readx.in.offset = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = True;
	rd.readx.out.data = buf;
	status = smb_raw_read(cli->tree, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(rd.readx.out.nread, ARRAY_SIZE(data));
	CHECK_VALUE(rd.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(rd.readx.out.compaction_mode, 0);

	printf("read without FLAGS2_READ_PERMIT_EXECUTE (should fail)\n");
	rd.generic.level = RAW_READ_READX;
	rd.readx.in.file.fnum = fnum;
	rd.readx.in.mincnt = 0;
	rd.readx.in.maxcnt = maxsize;
	rd.readx.in.offset = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = False;
	rd.readx.out.data = buf;
	status = smb_raw_read(cli->tree, &rd);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("open file with SEC_FILE_READ_DATA\n");
	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	op.ntcreatex.in.create_options = 0;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	printf("read with FLAGS2_READ_PERMIT_EXECUTE\n");
	rd.generic.level = RAW_READ_READX;
	rd.readx.in.file.fnum = fnum;
	rd.readx.in.mincnt = 0;
	rd.readx.in.maxcnt = maxsize;
	rd.readx.in.offset = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = True;
	rd.readx.out.data = buf;
	status = smb_raw_read(cli->tree, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(rd.readx.out.nread, ARRAY_SIZE(data));
	CHECK_VALUE(rd.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(rd.readx.out.compaction_mode, 0);

	printf("read without FLAGS2_READ_PERMIT_EXECUTE\n");
	rd.generic.level = RAW_READ_READX;
	rd.readx.in.file.fnum = fnum;
	rd.readx.in.mincnt = 0;
	rd.readx.in.maxcnt = maxsize;
	rd.readx.in.offset = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = False;
	rd.readx.out.data = buf;
	status = smb_raw_read(cli->tree, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(rd.readx.out.nread, ARRAY_SIZE(data));
	CHECK_VALUE(rd.readx.out.remaining, 0xFFFF);
	CHECK_VALUE(rd.readx.out.compaction_mode, 0);

done:
	smbcli_close(cli->tree, fnum);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/* 
   basic testing of read calls
*/
BOOL torture_raw_read(struct torture_context *torture)
{
	struct smbcli_state *cli;
	BOOL ret = True;
	TALLOC_CTX *mem_ctx;

	if (!torture_open_connection(&cli, 0)) {
		return False;
	}

	mem_ctx = talloc_init("torture_raw_read");

	ret &= test_read(cli, mem_ctx);
	ret &= test_readx(cli, mem_ctx);
	ret &= test_lockread(cli, mem_ctx);
	ret &= test_readbraw(cli, mem_ctx);
	ret &= test_read_for_execute(cli, mem_ctx);

	torture_close_connection(cli);
	talloc_free(mem_ctx);
	return ret;
}
