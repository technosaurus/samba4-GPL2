/* 
   Unix SMB/CIFS implementation.
   RAW_OPEN_* individual test suite
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

/* enum for whether reads/writes are possible on a file */
enum rdwr_mode {RDWR_NONE, RDWR_RDONLY, RDWR_WRONLY, RDWR_RDWR};

#define BASEDIR "\\rawopen"

/*
  check if a open file can be read/written
*/
static enum rdwr_mode check_rdwr(struct cli_tree *tree, int fnum)
{
	char c = 1;
	BOOL can_read  = (cli_read(tree, fnum, &c, 0, 1) == 1);
	BOOL can_write = (cli_write(tree, fnum, 0, &c, 0, 1) == 1);
	if ( can_read &&  can_write) return RDWR_RDWR;
	if ( can_read && !can_write) return RDWR_RDONLY;
	if (!can_read &&  can_write) return RDWR_WRONLY;
	return RDWR_NONE;
}

/*
  describe a RDWR mode as a string
*/
static const char *rdwr_string(enum rdwr_mode m)
{
	switch (m) {
	case RDWR_NONE: return "NONE";
	case RDWR_RDONLY: return "RDONLY";
	case RDWR_WRONLY: return "WRONLY";
	case RDWR_RDWR: return "RDWR";
	}
	return "-";
}

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%d) Incorrect status %s - should be %s\n", \
		       __LINE__, nt_errstr(status), nt_errstr(correct)); \
		ret = False; \
		goto done; \
	}} while (0)

#define CREATE_FILE do { \
	fnum = create_complex_file(cli, mem_ctx, fname); \
	if (fnum == -1) { \
		printf("(%d) Failed to create %s - %s\n", __LINE__, fname, cli_errstr(cli->tree)); \
		ret = False; \
		goto done; \
	}} while (0)

#define CHECK_RDWR(fnum, correct) do { \
	enum rdwr_mode m = check_rdwr(cli->tree, fnum); \
	if (m != correct) { \
		printf("(%d) Incorrect readwrite mode %s - expected %s\n", \
		       __LINE__, rdwr_string(m), rdwr_string(correct)); \
		ret = False; \
	}} while (0)

#define CHECK_TIME(t, field) do { \
	time_t t1, t2; \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFO; \
	finfo.all_info.in.fname = fname; \
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	t1 = t & ~1; \
	t2 = nt_time_to_unix(finfo.all_info.out.field) & ~1; \
	if (ABS(t1-t2) > 2) { \
		printf("(%d) wrong time for field %s  %s - %s\n", \
		       __LINE__, #field, \
		       timestring(mem_ctx, t1), \
		       timestring(mem_ctx, t2)); \
		dump_all_info(mem_ctx, &finfo); \
		ret = False; \
	}} while (0)

#define CHECK_NTTIME(t, field) do { \
	NTTIME t2; \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFO; \
	finfo.all_info.in.fname = fname; \
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	t2 = finfo.all_info.out.field; \
	if (t != t2) { \
		printf("(%d) wrong time for field %s  %s - %s\n", \
		       __LINE__, #field, \
		       nt_time_string(mem_ctx, t), \
		       nt_time_string(mem_ctx, t2)); \
		dump_all_info(mem_ctx, &finfo); \
		ret = False; \
	}} while (0)

#define CHECK_ALL_INFO(v, field) do { \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFO; \
	finfo.all_info.in.fname = fname; \
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	if ((v) != finfo.all_info.out.field) { \
		printf("(%d) wrong value for field %s  0x%x - 0x%x\n", \
		       __LINE__, #field, (int)v, (int)finfo.all_info.out.field); \
		dump_all_info(mem_ctx, &finfo); \
		ret = False; \
	}} while (0)

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%d) wrong value for %s  0x%x - 0x%x\n", \
		       __LINE__, #v, (int)v, (int)correct); \
		ret = False; \
	}} while (0)

#define SET_ATTRIB(sattrib) do { \
	union smb_setfileinfo sfinfo; \
	sfinfo.generic.level = RAW_SFILEINFO_BASIC_INFORMATION; \
	sfinfo.generic.file.fname = fname; \
	ZERO_STRUCT(sfinfo.basic_info.in); \
	sfinfo.basic_info.in.attrib = sattrib; \
	status = smb_raw_setpathinfo(cli->tree, &sfinfo); \
	if (!NT_STATUS_IS_OK(status)) { \
		printf("(%d) Failed to set attrib 0x%x on %s\n", \
		       __LINE__, sattrib, fname); \
	}} while (0)

/*
  test RAW_OPEN_OPEN
*/
static BOOL test_open(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_open.txt";
	NTSTATUS status;
	int fnum, fnum2;
	BOOL ret = True;

	printf("Checking RAW_OPEN_OPEN\n");

	io.open.level = RAW_OPEN_OPEN;
	io.open.in.fname = fname;
	io.open.in.flags = OPEN_FLAGS_FCB;
	io.open.in.search_attrs = 0;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	fnum = io.open.out.fnum;

	cli_unlink(cli->tree, fname);
	CREATE_FILE;
	cli_close(cli->tree, fnum);

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.open.out.fnum;
	CHECK_RDWR(fnum, RDWR_RDWR);

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.open.out.fnum;
	CHECK_RDWR(fnum2, RDWR_RDWR);
	cli_close(cli->tree, fnum2);
	cli_close(cli->tree, fnum);

	/* check the read/write modes */
	io.open.level = RAW_OPEN_OPEN;
	io.open.in.fname = fname;
	io.open.in.search_attrs = 0;

	io.open.in.flags = OPEN_FLAGS_OPEN_READ;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.open.out.fnum;
	CHECK_RDWR(fnum, RDWR_RDONLY);
	cli_close(cli->tree, fnum);

	io.open.in.flags = OPEN_FLAGS_OPEN_WRITE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.open.out.fnum;
	CHECK_RDWR(fnum, RDWR_WRONLY);
	cli_close(cli->tree, fnum);

	io.open.in.flags = OPEN_FLAGS_OPEN_RDWR;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.open.out.fnum;
	CHECK_RDWR(fnum, RDWR_RDWR);
	cli_close(cli->tree, fnum);

	/* check the share modes roughly - not a complete matrix */
	io.open.in.flags = OPEN_FLAGS_OPEN_RDWR | OPEN_FLAGS_DENY_WRITE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.open.out.fnum;
	CHECK_RDWR(fnum, RDWR_RDWR);
	
	if (io.open.in.flags != io.open.out.rmode) {
		printf("(%d) rmode should equal flags - 0x%x 0x%x\n",
		       __LINE__, io.open.out.rmode, io.open.in.flags);
	}

	io.open.in.flags = OPEN_FLAGS_OPEN_RDWR | OPEN_FLAGS_DENY_NONE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	io.open.in.flags = OPEN_FLAGS_OPEN_READ | OPEN_FLAGS_DENY_NONE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.open.out.fnum;
	CHECK_RDWR(fnum2, RDWR_RDONLY);
	cli_close(cli->tree, fnum);
	cli_close(cli->tree, fnum2);


	/* check the returned write time */
	io.open.level = RAW_OPEN_OPEN;
	io.open.in.fname = fname;
	io.open.in.search_attrs = 0;
	io.open.in.flags = OPEN_FLAGS_OPEN_READ;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.open.out.fnum;

	/* check other reply fields */
	CHECK_TIME(io.open.out.write_time, write_time);
	CHECK_ALL_INFO(io.open.out.size, size);
	CHECK_ALL_INFO(io.open.out.attrib, attrib);

done:
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	return ret;
}


/*
  test RAW_OPEN_OPENX
*/
static BOOL test_openx(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_openx.txt";
	NTSTATUS status;
	int fnum, fnum2;
	BOOL ret = True;
	int i;
	struct {
		uint16 open_func;
		BOOL with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ OPENX_OPEN_FUNC_OPEN, 	                  True,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN,  	                  False, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, True,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, False, NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_FAIL, 	                  True,  NT_STATUS_INVALID_LOCK_SEQUENCE },
		{ OPENX_OPEN_FUNC_FAIL, 	                  False, NT_STATUS_INVALID_LOCK_SEQUENCE },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, True,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, False, NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  True,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  False, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, True,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, False, NT_STATUS_OK },
	};

	printf("Checking RAW_OPEN_OPENX\n");
	cli_unlink(cli->tree, fname);

	io.openx.level = RAW_OPEN_OPENX;
	io.openx.in.fname = fname;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 1024*1024;
	io.openx.in.timeout = 0;

	/* check all combinations of open_func */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		if (open_funcs[i].with_file) {
			fnum = create_complex_file(cli, mem_ctx, fname);
			if (fnum == -1) {
				d_printf("Failed to create file %s - %s\n", fname, cli_errstr(cli->tree));
				ret = False;
				goto done;
			}
			cli_close(cli->tree, fnum);
		}
		io.openx.in.open_func = open_funcs[i].open_func;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			printf("(%d) incorrect status %s should be %s (i=%d with_file=%d open_func=0x%x)\n", 
			       __LINE__, nt_errstr(status), nt_errstr(open_funcs[i].correct_status),
			       i, (int)open_funcs[i].with_file, (int)open_funcs[i].open_func);
			ret = False;
		}
		if (NT_STATUS_IS_OK(status) || open_funcs[i].with_file) {
			cli_close(cli->tree, io.openx.out.fnum);
			cli_unlink(cli->tree, fname);
		}
	}

	/* check the basic return fields */
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openx.out.fnum;

	CHECK_ALL_INFO(io.openx.out.size, size);
	CHECK_VAL(io.openx.out.size, 1024*1024);
	CHECK_ALL_INFO(io.openx.in.size, size);
	CHECK_TIME(io.openx.out.write_time, write_time);
	CHECK_ALL_INFO(io.openx.out.attrib, attrib);
	CHECK_VAL(io.openx.out.access, OPENX_MODE_ACCESS_RDWR);
	CHECK_VAL(io.openx.out.ftype, 0);
	CHECK_VAL(io.openx.out.devstate, 0);
	CHECK_VAL(io.openx.out.action, OPENX_ACTION_CREATED);
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	/* check the fields when the file already existed */
	fnum2 = create_complex_file(cli, mem_ctx, fname);
	if (fnum2 == -1) {
		ret = False;
		goto done;
	}
	cli_close(cli->tree, fnum2);

	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openx.out.fnum;

	CHECK_ALL_INFO(io.openx.out.size, size);
	CHECK_TIME(io.openx.out.write_time, write_time);
	CHECK_VAL(io.openx.out.action, OPENX_ACTION_EXISTED);
	CHECK_VAL(io.openx.out.unknown, 0);
	CHECK_ALL_INFO(io.openx.out.attrib, attrib);
	cli_close(cli->tree, fnum);

	/* now check the search attrib for hidden files - win2003 ignores this? */
	SET_ATTRIB(FILE_ATTRIBUTE_HIDDEN);
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN, attrib);

	io.openx.in.search_attrs = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli_close(cli->tree, io.openx.out.fnum);

	io.openx.in.search_attrs = 0;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli_close(cli->tree, io.openx.out.fnum);

	SET_ATTRIB(FILE_ATTRIBUTE_NORMAL);
	cli_unlink(cli->tree, fname);

	/* and check attrib on create */
	io.openx.in.open_func = OPENX_OPEN_FUNC_FAIL | OPENX_OPEN_FUNC_CREATE;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = FILE_ATTRIBUTE_SYSTEM;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ALL_INFO(FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE, attrib);
	cli_close(cli->tree, io.openx.out.fnum);
	cli_unlink(cli->tree, fname);

	/* check timeout on create - win2003 ignores the timeout! */
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.openx.in.file_attrs = 0;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_ALL;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.openx.out.fnum;

	io.openx.in.timeout = 20000;
	start_timer();
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_NONE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);
	if (end_timer() > 3) {
		printf("(%d) Incorrect timing in openx with timeout - waited %d seconds\n",
		       __LINE__, (int)end_timer());
		ret = False;
	}
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	/* now this is a really weird one - open for execute implies create?! */
	io.openx.in.fname = fname;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_EXEC | OPENX_MODE_DENY_NONE;
	io.openx.in.search_attrs = 0;
	io.openx.in.open_func = OPENX_OPEN_FUNC_FAIL;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 0;
	io.openx.in.timeout = 0;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli_close(cli->tree, io.openx.out.fnum);

	/* check the extended return flag */
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO | OPENX_FLAGS_EXTENDED_RETURN;
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(io.openx.out.access_mask, STD_RIGHT_ALL_ACCESS);
	cli_close(cli->tree, io.openx.out.fnum);

done:
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	return ret;
}


/*
  test RAW_OPEN_T2OPEN

  I can't work out how to get win2003 to accept a create file via TRANS2_OPEN, which
  is why you see all the ACCESS_DENIED results below. When we finally work this out then this
  test will make more sense
*/
static BOOL test_t2open(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_t2open.txt";
	NTSTATUS status;
	int fnum;
	BOOL ret = True;
	int i;
	struct {
		uint16 open_func;
		BOOL with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ OPENX_OPEN_FUNC_OPEN, 	                  True,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN,  	                  False, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, True,  NT_STATUS_OK },
		{ OPENX_OPEN_FUNC_OPEN  | OPENX_OPEN_FUNC_CREATE, False, NT_STATUS_ACCESS_DENIED },
		{ OPENX_OPEN_FUNC_FAIL, 	                  True,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_FAIL, 	                  False, NT_STATUS_ACCESS_DENIED },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, True,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ OPENX_OPEN_FUNC_FAIL  | OPENX_OPEN_FUNC_CREATE, False, NT_STATUS_ACCESS_DENIED },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  True,  NT_STATUS_ACCESS_DENIED },
		{ OPENX_OPEN_FUNC_TRUNC, 	                  False, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, True,  NT_STATUS_ACCESS_DENIED },
		{ OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE, False, NT_STATUS_ACCESS_DENIED },
	};

	printf("Checking RAW_OPEN_T2OPEN\n");

	io.t2open.level = RAW_OPEN_T2OPEN;
	io.t2open.in.fname = fname;
	io.t2open.in.flags = OPENX_FLAGS_ADDITIONAL_INFO | 
		OPENX_FLAGS_EA_LEN | OPENX_FLAGS_EXTENDED_RETURN;
	io.t2open.in.open_mode = OPENX_MODE_DENY_NONE | OPENX_MODE_ACCESS_RDWR;
	io.t2open.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.t2open.in.file_attrs = 0;
	io.t2open.in.write_time = 0;
	io.t2open.in.size = 0;
	io.t2open.in.timeout = 0;

	io.t2open.in.eas = talloc(mem_ctx, sizeof(io.t2open.in.eas[0]));
	io.t2open.in.num_eas = 1;
	io.t2open.in.eas[0].flags = 0;
	io.t2open.in.eas[0].name.s = "EAONE";
	io.t2open.in.eas[0].value = data_blob_talloc(mem_ctx, "1", 1);

	/* check all combinations of open_func */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		if (open_funcs[i].with_file) {
			fnum = create_complex_file(cli, mem_ctx, fname);
			if (fnum == -1) {
				d_printf("Failed to create file %s - %s\n", fname, cli_errstr(cli->tree));
				ret = False;
				goto done;
			}
			cli_close(cli->tree, fnum);
		}
		io.t2open.in.open_func = open_funcs[i].open_func;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			printf("(%d) incorrect status %s should be %s (i=%d with_file=%d open_func=0x%x)\n", 
			       __LINE__, nt_errstr(status), nt_errstr(open_funcs[i].correct_status),
			       i, (int)open_funcs[i].with_file, (int)open_funcs[i].open_func);
			ret = False;
		}
		if (NT_STATUS_IS_OK(status) || open_funcs[i].with_file) {
			cli_close(cli->tree, io.t2open.out.fnum);
			cli_unlink(cli->tree, fname);
		}
	}

	/* check the basic return fields */
	fnum = create_complex_file(cli, mem_ctx, fname);
	cli_close(cli->tree, fnum);
	io.t2open.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.t2open.out.fnum;

	CHECK_ALL_INFO(io.t2open.out.size, size);
	CHECK_VAL(io.t2open.out.write_time, 0);
	CHECK_ALL_INFO(io.t2open.out.attrib, attrib);
	CHECK_VAL(io.t2open.out.access, OPENX_MODE_DENY_NONE | OPENX_MODE_ACCESS_RDWR);
	CHECK_VAL(io.t2open.out.ftype, 0);
	CHECK_VAL(io.t2open.out.devstate, 0);
	CHECK_VAL(io.t2open.out.action, OPENX_ACTION_EXISTED);
	cli_close(cli->tree, fnum);

	/* now check the search attrib for hidden files - win2003 ignores this? */
	SET_ATTRIB(FILE_ATTRIBUTE_HIDDEN);
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN, attrib);

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli_close(cli->tree, io.t2open.out.fnum);

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli_close(cli->tree, io.t2open.out.fnum);

	SET_ATTRIB(FILE_ATTRIBUTE_NORMAL);
	cli_unlink(cli->tree, fname);

	/* and check attrib on create */
	io.t2open.in.open_func = OPENX_OPEN_FUNC_FAIL | OPENX_OPEN_FUNC_CREATE;
	io.t2open.in.file_attrs = FILE_ATTRIBUTE_SYSTEM;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	/* check timeout on create - win2003 ignores the timeout! */
	io.t2open.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.t2open.in.file_attrs = 0;
	io.t2open.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_ALL;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

done:
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	return ret;
}
	

/*
  test RAW_OPEN_NTCREATEX
*/
static BOOL test_ntcreatex(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = BASEDIR "\\torture_ntcreatex.txt";
	const char *dname = BASEDIR "\\torture_ntcreatex.dir";
	NTSTATUS status;
	int fnum;
	BOOL ret = True;
	int i;
	struct {
		uint32 open_disp;
		BOOL with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ NTCREATEX_DISP_SUPERSEDE, 	True,  NT_STATUS_OK },
		{ NTCREATEX_DISP_SUPERSEDE, 	False, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN, 	        True,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN, 	        False, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_CREATE, 	True,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ NTCREATEX_DISP_CREATE, 	False, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	True,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF, 	False, NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	True,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE, 	False, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	True,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE_IF, 	False, NT_STATUS_OK },
		{ 6, 			        True,  NT_STATUS_INVALID_PARAMETER },
		{ 6, 	                        False, NT_STATUS_INVALID_PARAMETER },
	};

	printf("Checking RAW_OPEN_NTCREATEX\n");

	/* reasonable default parameters */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.access_mask = GENERIC_RIGHTS_FILE_ALL_ACCESS;
	io.ntcreatex.in.alloc_size = 1024*1024;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/* test the open disposition */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		if (open_funcs[i].with_file) {
			fnum = cli_open(cli->tree, fname, O_CREAT|O_RDWR|O_TRUNC, DENY_NONE);
			if (fnum == -1) {
				d_printf("Failed to create file %s - %s\n", fname, cli_errstr(cli->tree));
				ret = False;
				goto done;
			}
			cli_close(cli->tree, fnum);
		}
		io.ntcreatex.in.open_disposition = open_funcs[i].open_disp;
		status = smb_raw_open(cli->tree, mem_ctx, &io);
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			printf("(%d) incorrect status %s should be %s (i=%d with_file=%d open_disp=%d)\n", 
			       __LINE__, nt_errstr(status), nt_errstr(open_funcs[i].correct_status),
			       i, (int)open_funcs[i].with_file, (int)open_funcs[i].open_disp);
			ret = False;
		}
		if (NT_STATUS_IS_OK(status) || open_funcs[i].with_file) {
			cli_close(cli->tree, io.ntcreatex.out.fnum);
			cli_unlink(cli->tree, fname);
		}
	}

	/* basic field testing */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);

	/* check fields when the file already existed */
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);
	fnum = create_complex_file(cli, mem_ctx, fname);
	if (fnum == -1) {
		ret = False;
		goto done;
	}
	cli_close(cli->tree, fnum);

	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_EXISTED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);


	/* create a directory */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.access_mask = GENERIC_RIGHTS_FILE_ALL_ACCESS;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.fname = dname;
	fname = dname;

	cli_rmdir(cli->tree, fname);
	cli_unlink(cli->tree, fname);

	io.ntcreatex.in.access_mask = SEC_RIGHT_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.fnum;

	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);
	CHECK_VAL(io.ntcreatex.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.ntcreatex.out.create_time, create_time);
	CHECK_NTTIME(io.ntcreatex.out.access_time, access_time);
	CHECK_NTTIME(io.ntcreatex.out.write_time, write_time);
	CHECK_NTTIME(io.ntcreatex.out.change_time, change_time);
	CHECK_ALL_INFO(io.ntcreatex.out.attrib, attrib);
	CHECK_VAL(io.ntcreatex.out.attrib, FILE_ATTRIBUTE_DIRECTORY);
	CHECK_ALL_INFO(io.ntcreatex.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.ntcreatex.out.size, size);
	CHECK_ALL_INFO(io.ntcreatex.out.is_directory, directory);
	CHECK_VAL(io.ntcreatex.out.is_directory, 1);
	CHECK_VAL(io.ntcreatex.out.size, 0);
	CHECK_VAL(io.ntcreatex.out.alloc_size, 0);
	CHECK_VAL(io.ntcreatex.out.file_type, FILE_TYPE_DISK);
	cli_unlink(cli->tree, fname);
	

done:
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	return ret;
}

/*
  test RAW_OPEN_NTCREATEX with an already opened and byte range locked file

  I've got an application that does a similar sequence of ntcreate&x,
  locking&x and another ntcreate&x with
  open_disposition==NTCREATEX_DISP_OVERWRITE_IF. Windows 2003 allows the
  second open.
*/
static BOOL test_ntcreatex_brlocked(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io, io1;
	union smb_lock io2;
	struct smb_lock_entry lock[1];
	const char *fname = BASEDIR "\\torture_ntcreatex.txt";
	NTSTATUS status;
	BOOL ret = True;

	/* reasonable default parameters */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.access_mask = 0x2019f;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_IMPERSONATION;
	io.ntcreatex.in.security_flags = NTCREATEX_SECURITY_DYNAMIC |
		NTCREATEX_SECURITY_ALL;
	io.ntcreatex.in.fname = fname;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io2.lockx.level = RAW_LOCK_LOCKX;
	io2.lockx.in.fnum = io.ntcreatex.out.fnum;
	io2.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io2.lockx.in.timeout = 0;
	io2.lockx.in.ulock_cnt = 0;
	io2.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 0;
	lock[0].count = 0x1;
	io2.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io2);
	CHECK_STATUS(status, NT_STATUS_OK);

	io1.generic.level = RAW_OPEN_NTCREATEX;
	io1.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io1.ntcreatex.in.root_fid = 0;
	io1.ntcreatex.in.access_mask = 0x20196;
	io1.ntcreatex.in.alloc_size = 0;
	io1.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io1.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io1.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io1.ntcreatex.in.create_options = 0;
	io1.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_IMPERSONATION;
	io1.ntcreatex.in.security_flags = NTCREATEX_SECURITY_DYNAMIC |
		NTCREATEX_SECURITY_ALL;
	io1.ntcreatex.in.fname = fname;

	status = smb_raw_open(cli->tree, mem_ctx, &io1);
	CHECK_STATUS(status, NT_STATUS_OK);

 done:
	cli_close(cli->tree, io.ntcreatex.out.fnum);
	cli_close(cli->tree, io1.ntcreatex.out.fnum);
	cli_unlink(cli->tree, fname);
	return ret;
}

/*
  test RAW_OPEN_MKNEW
*/
static BOOL test_mknew(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	const char *fname = BASEDIR "\\torture_mknew.txt";
	NTSTATUS status;
	int fnum;
	BOOL ret = True;
	time_t basetime = (time(NULL) + 3600*24*3) & ~1;
	union smb_fileinfo finfo;

	printf("Checking RAW_OPEN_MKNEW\n");

	io.mknew.level = RAW_OPEN_MKNEW;
	io.mknew.in.attrib = 0;
	io.mknew.in.write_time = 0;
	io.mknew.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.mknew.out.fnum;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_COLLISION);

	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	/* make sure write_time works */
	io.mknew.in.write_time = basetime;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.mknew.out.fnum;
	CHECK_TIME(basetime, write_time);

	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	/* make sure file_attrs works */
	io.mknew.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.mknew.out.fnum;
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE, attrib);
	
done:
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	return ret;
}


/*
  test RAW_OPEN_CREATE
*/
static BOOL test_create(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	const char *fname = BASEDIR "\\torture_create.txt";
	NTSTATUS status;
	int fnum;
	BOOL ret = True;
	time_t basetime = (time(NULL) + 3600*24*3) & ~1;
	union smb_fileinfo finfo;

	printf("Checking RAW_OPEN_CREATE\n");

	io.create.level = RAW_OPEN_CREATE;
	io.create.in.attrib = 0;
	io.create.in.write_time = 0;
	io.create.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.create.out.fnum;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	cli_close(cli->tree, io.create.out.fnum);
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	/* make sure write_time works */
	io.create.in.write_time = basetime;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.create.out.fnum;
	CHECK_TIME(basetime, write_time);

	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	/* make sure file_attrs works */
	io.create.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.create.out.fnum;
	CHECK_ALL_INFO(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE, attrib);
	
done:
	cli_close(cli->tree, fnum);
	cli_unlink(cli->tree, fname);

	return ret;
}


/*
  test RAW_OPEN_CTEMP
*/
static BOOL test_ctemp(struct cli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	NTSTATUS status;
	int fnum;
	BOOL ret = True;
	time_t basetime = (time(NULL) + 3600*24*3) & ~1;
	union smb_fileinfo finfo;
	const char *name, *fname = NULL;

	printf("Checking RAW_OPEN_CTEMP\n");

	io.ctemp.level = RAW_OPEN_CTEMP;
	io.ctemp.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	io.ctemp.in.write_time = basetime;
	io.ctemp.in.directory = BASEDIR;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ctemp.out.fnum;

	name = io.ctemp.out.name;

	finfo.generic.level = RAW_FILEINFO_NAME_INFO;
	finfo.generic.in.fnum = fnum;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	fname = finfo.name_info.out.fname.s;
	d_printf("ctemp name=%s  real name=%s\n", name, fname);

	CHECK_TIME(basetime, write_time);

done:
	cli_close(cli->tree, fnum);
	if (fname) {
		cli_unlink(cli->tree, fname);
	}

	return ret;
}

/* basic testing of all RAW_OPEN_* calls 
*/
BOOL torture_raw_open(int dummy)
{
	struct cli_state *cli;
	BOOL ret = True;
	TALLOC_CTX *mem_ctx;

	if (!torture_open_connection(&cli)) {
		return False;
	}

	mem_ctx = talloc_init("torture_raw_open");

	if (cli_deltree(cli->tree, BASEDIR) == -1) {
		printf("Failed to clean " BASEDIR "\n");
		return False;
	}
	if (NT_STATUS_IS_ERR(cli_mkdir(cli->tree, BASEDIR))) {
		printf("Failed to create " BASEDIR " - %s\n", cli_errstr(cli->tree));
		return False;
	}

	if (!test_ntcreatex_brlocked(cli, mem_ctx)) {
		return False;
	}

	if (!test_open(cli, mem_ctx)) {
		ret = False;
	}

	if (!test_openx(cli, mem_ctx)) {
		ret = False;
	}

	if (!test_ntcreatex(cli, mem_ctx)) {
		ret = False;
	}

	if (!test_t2open(cli, mem_ctx)) {
		ret = False;
	}

	if (!test_mknew(cli, mem_ctx)) {
		ret = False;
	}

	if (!test_create(cli, mem_ctx)) {
		ret = False;
	}

	if (!test_ctemp(cli, mem_ctx)) {
		ret = False;
	}

	smb_raw_exit(cli->session);
	cli_deltree(cli->tree, BASEDIR);

	torture_close_connection(cli);
	talloc_destroy(mem_ctx);
	return ret;
}
