/* 
   Unix SMB/CIFS implementation.
   seek test suite
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

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%d) Incorrect status %s - should be %s\n", \
		       __LINE__, nt_errstr(status), nt_errstr(correct)); \
		ret = False; \
		goto done; \
	}} while (0)

#define CHECK_VALUE(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%d) Incorrect value %s=%d - should be %d\n", \
		       __LINE__, #v, v, correct); \
		ret = False; \
		goto done; \
	}} while (0)

#define BASEDIR "\\testseek"

/*
  test seek ops
*/
static BOOL test_seek(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	struct smb_seek io;
	union smb_fileinfo finfo;
	NTSTATUS status;
	BOOL ret = True;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";

	if (cli_deltree(cli, BASEDIR) == -1 ||
	    !cli_mkdir(cli, BASEDIR)) {
		printf("Unable to setup %s - %s\n", BASEDIR, cli_errstr(cli));
		return False;
	}

	fnum = create_complex_file(cli, mem_ctx, fname);
	if (fnum == -1) {
		printf("Failed to open test.txt - %s\n", cli_errstr(cli));
		ret = False;
		goto done;
	}

	finfo.generic.level = RAW_FILEINFO_POSITION_INFORMATION;
	finfo.position_information.in.fnum = fnum;
	
	printf("Trying bad handle\n");
	io.in.fnum = fnum+1;
	io.in.mode = SEEK_MODE_START;
	io.in.offset = 0;
	status = smb_raw_seek(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("Trying simple seek\n");
	io.in.fnum = fnum;
	io.in.mode = SEEK_MODE_START;
	io.in.offset = 17;
	status = smb_raw_seek(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.out.offset, 17);
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(finfo.position_information.out.position, 0);
	
	printf("Trying relative seek\n");
	io.in.fnum = fnum;
	io.in.mode = SEEK_MODE_CURRENT;
	io.in.offset = -3;
	status = smb_raw_seek(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.out.offset, 14);

	printf("Trying end seek\n");
	io.in.fnum = fnum;
	io.in.mode = SEEK_MODE_END;
	io.in.offset = 0;
	status = smb_raw_seek(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.all_info.in.fnum = fnum;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.out.offset, finfo.all_info.out.size);

	printf("Trying max seek\n");
	io.in.fnum = fnum;
	io.in.mode = SEEK_MODE_START;
	io.in.offset = -1;
	status = smb_raw_seek(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.out.offset, 0xffffffff);

	printf("Trying max overflow\n");
	io.in.fnum = fnum;
	io.in.mode = SEEK_MODE_CURRENT;
	io.in.offset = 1000;
	status = smb_raw_seek(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(io.out.offset, 999);

done:
	smb_raw_exit(cli->session);
	cli_deltree(cli, BASEDIR);
	return ret;
}


/* 
   basic testing of seek calls
*/
BOOL torture_raw_seek(int dummy)
{
	struct cli_state *cli;
	BOOL ret = True;
	TALLOC_CTX *mem_ctx;

	if (!torture_open_connection(&cli)) {
		return False;
	}

	mem_ctx = talloc_init("torture_raw_seek");

	if (!test_seek(cli, mem_ctx)) {
		ret = False;
	}

	torture_close_connection(cli);
	talloc_destroy(mem_ctx);
	return ret;
}
