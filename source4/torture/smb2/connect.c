/* 
   Unix SMB/CIFS implementation.

   test suite for SMB2 connection operations

   Copyright (C) Andrew Tridgell 2005
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "lib/cmdline/popt_common.h"
#include "lib/events/events.h"

/*
  send a close
*/
static NTSTATUS torture_smb2_close(struct smb2_tree *tree, struct smb2_handle handle)
{
	struct smb2_close io;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);

	ZERO_STRUCT(io);
	io.in.flags       = SMB2_CLOSE_FLAGS_FULL_INFORMATION;
	io.in.handle   = handle;
	status = smb2_close(tree, &io);
	if (!NT_STATUS_IS_OK(status)) {
		printf("close failed - %s\n", nt_errstr(status));
		return status;
	}

	if (DEBUGLVL(1)) {
		printf("Close gave:\n");
		printf("create_time     = %s\n", nt_time_string(tmp_ctx, io.out.create_time));
		printf("access_time     = %s\n", nt_time_string(tmp_ctx, io.out.access_time));
		printf("write_time      = %s\n", nt_time_string(tmp_ctx, io.out.write_time));
		printf("change_time     = %s\n", nt_time_string(tmp_ctx, io.out.change_time));
		printf("alloc_size      = %lld\n", io.out.alloc_size);
		printf("size            = %lld\n", io.out.size);
		printf("file_attr       = 0x%x\n", io.out.file_attr);
	}

	talloc_free(tmp_ctx);
	
	return status;
}


/*
  test writing
*/
static NTSTATUS torture_smb2_write(struct smb2_tree *tree, struct smb2_handle handle)
{
	struct smb2_write w;
	struct smb2_read r;
	NTSTATUS status;
	DATA_BLOB data;
	int i;
	
	if (lp_parm_bool(-1, "torture", "dangerous", False)) {
		data = data_blob_talloc(tree, NULL, 160000);
	} else {
		data = data_blob_talloc(tree, NULL, 120000);
	}
	for (i=0;i<data.length;i++) {
		data.data[i] = i;
	}

	ZERO_STRUCT(w);
	w.in.offset      = 0;
	w.in.handle      = handle;
	w.in.data        = data;

	status = smb2_write(tree, &w);
	if (!NT_STATUS_IS_OK(status)) {
		printf("write failed - %s\n", nt_errstr(status));
		return status;
	}

	torture_smb2_all_info(tree, handle);

	status = smb2_write(tree, &w);
	if (!NT_STATUS_IS_OK(status)) {
		printf("write failed - %s\n", nt_errstr(status));
		return status;
	}

	torture_smb2_all_info(tree, handle);

	ZERO_STRUCT(r);
	r.in.length      = data.length;
	r.in.offset      = 0;
	r.in.handle      = handle;

	status = smb2_read(tree, tree, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("read failed - %s\n", nt_errstr(status));
		return status;
	}

	if (data.length != r.out.data.length ||
	    memcmp(data.data, r.out.data.data, data.length) != 0) {
		printf("read data mismatch\n");
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return status;
}


/*
  send a create
*/
static struct smb2_handle torture_smb2_create(struct smb2_tree *tree, 
					      const char *fname)
{
	struct smb2_create io;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);

	ZERO_STRUCT(io);
	io.in.oplock_flags = 0;
	io.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.in.file_attr   = FILE_ATTRIBUTE_NORMAL;
	io.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = NTCREATEX_OPTIONS_WRITE_THROUGH;
	io.in.fname = fname;

	status = smb2_create(tree, tmp_ctx, &io);
	if (!NT_STATUS_IS_OK(status)) {
		printf("create1 failed - %s\n", nt_errstr(status));
		return io.out.handle;
	}

	if (DEBUGLVL(1)) {
		printf("Open gave:\n");
		printf("oplock_flags    = 0x%x\n", io.out.oplock_flags);
		printf("create_action   = 0x%x\n", io.out.create_action);
		printf("create_time     = %s\n", nt_time_string(tmp_ctx, io.out.create_time));
		printf("access_time     = %s\n", nt_time_string(tmp_ctx, io.out.access_time));
		printf("write_time      = %s\n", nt_time_string(tmp_ctx, io.out.write_time));
		printf("change_time     = %s\n", nt_time_string(tmp_ctx, io.out.change_time));
		printf("alloc_size      = %lld\n", io.out.alloc_size);
		printf("size            = %lld\n", io.out.size);
		printf("file_attr       = 0x%x\n", io.out.file_attr);
		printf("handle          = %016llx%016llx\n", 
		       io.out.handle.data[0], 
		       io.out.handle.data[1]);
	}

	talloc_free(tmp_ctx);
	
	return io.out.handle;
}


/* 
   basic testing of SMB2 connection calls
*/
BOOL torture_smb2_connect(void)
{
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	struct smb2_tree *tree;
	struct smb2_handle h1, h2;
	NTSTATUS status;

	if (!torture_smb2_connection(mem_ctx, &tree)) {
		return False;
	}

	h1 = torture_smb2_create(tree, "test9.dat");
	h2 = torture_smb2_create(tree, "test9.dat");
//	h2 = torture_smb2_create(tree, "test9test9test9t9.dat");
	torture_smb2_write(tree, h1);
	torture_smb2_close(tree, h1);
	torture_smb2_close(tree, h2);

	status = smb2_logoff(tree);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Logoff failed - %s\n", nt_errstr(status));
		return False;
	}

	status = smb2_logoff(tree);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_USER_SESSION_DELETED)) {
		printf("Logoff should have disabled session - %s\n", nt_errstr(status));
		return False;
	}

	talloc_free(mem_ctx);

	return True;
}
