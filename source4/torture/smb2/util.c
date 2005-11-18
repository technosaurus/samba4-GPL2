/* 
   Unix SMB/CIFS implementation.

   helper functions for SMB2 test suite

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
#include "system/time.h"


/*
  close a handle with SMB2
*/
NTSTATUS smb2_util_close(struct smb2_tree *tree, struct smb2_handle h)
{
	struct smb2_close c;

	ZERO_STRUCT(c);
	c.in.handle = h;

	return smb2_close(tree, &c);
}

/*
  unlink a file with SMB2
*/
NTSTATUS smb2_util_unlink(struct smb2_tree *tree, const char *fname)
{
	struct smb2_create io;
	NTSTATUS status;

	ZERO_STRUCT(io);
	io.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.in.file_attr   = FILE_ATTRIBUTE_NORMAL;
	io.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	io.in.fname = fname;

	status = smb2_create(tree, tree, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		return NT_STATUS_OK;
	}
	NT_STATUS_NOT_OK_RETURN(status);

	return smb2_util_close(tree, io.out.handle);
}

/*
  write to a file on SMB2
*/
NTSTATUS smb2_util_write(struct smb2_tree *tree,
			 struct smb2_handle handle, 
			 const void *buf, off_t offset, size_t size)
{
	struct smb2_write w;

	ZERO_STRUCT(w);
	w.in.offset      = offset;
	w.in.handle      = handle;
	w.in.data        = data_blob_const(buf, size);

	return smb2_write(tree, &w);
}

/*
  create a complex file using the SMB2 protocol
*/
NTSTATUS smb2_create_complex_file(struct smb2_tree *tree, const char *fname, struct smb2_handle *handle)
{
	TALLOC_CTX *tmp_ctx = talloc_new(tree);
	char buf[7] = "abc";
	struct smb2_create io;
	union smb_setfileinfo setfile;
	union smb_fileinfo fileinfo;
	time_t t = (time(NULL) & ~1);
	NTSTATUS status;

	ZERO_STRUCT(io);
	io.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.in.file_attr   = FILE_ATTRIBUTE_NORMAL;
	io.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = 0;
	io.in.fname = fname;

	io.in.sd = security_descriptor_create(tmp_ctx,
					      NULL, NULL,
					      SID_NT_AUTHENTICATED_USERS,
					      SEC_ACE_TYPE_ACCESS_ALLOWED,
					      SEC_RIGHTS_FILE_ALL | SEC_STD_ALL,
					      0,
					      SID_WORLD,
					      SEC_ACE_TYPE_ACCESS_ALLOWED,
					      SEC_RIGHTS_FILE_READ | SEC_STD_ALL,
					      0,
					      NULL);

	if (strchr(fname, ':') == NULL) {
		/* setup some EAs */
		io.in.eas.num_eas = 2;
		io.in.eas.eas = talloc_array(tmp_ctx, struct ea_struct, 2);
		io.in.eas.eas[0].flags = 0;
		io.in.eas.eas[0].name.s = "EAONE";
		io.in.eas.eas[0].value = data_blob_talloc(tmp_ctx, "VALUE1", 6);
		io.in.eas.eas[1].flags = 0;
		io.in.eas.eas[1].name.s = "SECONDEA";
		io.in.eas.eas[1].value = data_blob_talloc(tmp_ctx, "ValueTwo", 8);
	}

	status = smb2_create(tree, tmp_ctx, &io);
	talloc_free(tmp_ctx);
	NT_STATUS_NOT_OK_RETURN(status);

	*handle = io.out.handle;

	status = smb2_util_write(tree, *handle, buf, 0, sizeof(buf));
	NT_STATUS_NOT_OK_RETURN(status);

	/* make sure all the timestamps aren't the same, and are also 
	   in different DST zones*/
	setfile.generic.level = RAW_SFILEINFO_BASIC_INFORMATION;
	setfile.generic.file.handle = *handle;

	setfile.basic_info.in.create_time = t +  9*30*24*60*60;
	setfile.basic_info.in.access_time = t +  6*30*24*60*60;
	setfile.basic_info.in.write_time  = t +  3*30*24*60*60;
	setfile.basic_info.in.change_time = t +  1*30*24*60*60;
	setfile.basic_info.in.attrib      = FILE_ATTRIBUTE_NORMAL;

	status = smb2_setinfo_file(tree, &setfile);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to setup file times - %s\n", nt_errstr(status));
	}

	/* make sure all the timestamps aren't the same */
	fileinfo.generic.level = RAW_FILEINFO_BASIC_INFORMATION;
	fileinfo.generic.in.handle = *handle;

	status = smb2_getinfo_file(tree, tree, &fileinfo);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to query file times - %s\n", nt_errstr(status));
	}

	if (setfile.basic_info.in.create_time != fileinfo.basic_info.out.create_time) {
		printf("create_time not setup correctly\n");
	}
	if (setfile.basic_info.in.access_time != fileinfo.basic_info.out.access_time) {
		printf("access_time not setup correctly\n");
	}
	if (setfile.basic_info.in.write_time != fileinfo.basic_info.out.write_time) {
		printf("write_time not setup correctly\n");
	}
	
	return NT_STATUS_OK;
}

/*
  show lots of information about a file
*/
void torture_smb2_all_info(struct smb2_tree *tree, struct smb2_handle handle)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);
	union smb_fileinfo io;

	io.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	io.generic.in.handle = handle;

	status = smb2_getinfo_file(tree, tmp_ctx, &io);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("getinfo failed - %s\n", nt_errstr(status)));
		talloc_free(tmp_ctx);
		return;
	}

	d_printf("\tcreate_time:    %s\n", nt_time_string(tmp_ctx, io.all_info2.out.create_time));
	d_printf("\taccess_time:    %s\n", nt_time_string(tmp_ctx, io.all_info2.out.access_time));
	d_printf("\twrite_time:     %s\n", nt_time_string(tmp_ctx, io.all_info2.out.write_time));
	d_printf("\tchange_time:    %s\n", nt_time_string(tmp_ctx, io.all_info2.out.change_time));
	d_printf("\tattrib:         0x%x\n", io.all_info2.out.attrib);
	d_printf("\tunknown1:       0x%x\n", io.all_info2.out.unknown1);
	d_printf("\talloc_size:     %llu\n", (uint64_t)io.all_info2.out.alloc_size);
	d_printf("\tsize:           %llu\n", (uint64_t)io.all_info2.out.size);
	d_printf("\tnlink:          %u\n", io.all_info2.out.nlink);
	d_printf("\tdelete_pending: %u\n", io.all_info2.out.delete_pending);
	d_printf("\tdirectory:      %u\n", io.all_info2.out.directory);
	d_printf("\tfile_id:        %llu\n", io.all_info2.out.file_id);
	d_printf("\tea_size:        %u\n", io.all_info2.out.ea_size);
	d_printf("\taccess_mask:    0x%08x\n", io.all_info2.out.access_mask);
	d_printf("\tunknown2:       0x%llx\n", io.all_info2.out.unknown2);
	d_printf("\tunknown3:       0x%llx\n", io.all_info2.out.unknown3);
	d_printf("\tfname:          '%s'\n", io.all_info2.out.fname.s);

	/* short name, if any */
	io.generic.level = RAW_FILEINFO_ALT_NAME_INFORMATION;
	status = smb2_getinfo_file(tree, tmp_ctx, &io);
	if (NT_STATUS_IS_OK(status)) {
		d_printf("\tshort name:     '%s'\n", io.alt_name_info.out.fname.s);
	}

	/* the EAs, if any */
	io.generic.level = RAW_FILEINFO_SMB2_ALL_EAS;
	status = smb2_getinfo_file(tree, tmp_ctx, &io);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		for (i=0;i<io.all_eas.out.num_eas;i++) {
			d_printf("\tEA[%d] flags=%d len=%d '%s'\n", i,
				 io.all_eas.out.eas[i].flags,
				 (int)io.all_eas.out.eas[i].value.length,
				 io.all_eas.out.eas[i].name.s);
		}
	}

	/* streams, if available */
	io.generic.level = RAW_FILEINFO_STREAM_INFORMATION;
	status = smb2_getinfo_file(tree, tmp_ctx, &io);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		for (i=0;i<io.stream_info.out.num_streams;i++) {
			d_printf("\tstream %d:\n", i);
			d_printf("\t\tsize       %ld\n", 
				 (long)io.stream_info.out.streams[i].size);
			d_printf("\t\talloc size %ld\n", 
				 (long)io.stream_info.out.streams[i].alloc_size);
			d_printf("\t\tname       %s\n", io.stream_info.out.streams[i].stream_name.s);
		}
	}	

	talloc_free(tmp_ctx);	
}


/*
  open a smb2 connection
*/
BOOL torture_smb2_connection(TALLOC_CTX *mem_ctx, struct smb2_tree **tree)
{
	NTSTATUS status;
	const char *host = lp_parm_string(-1, "torture", "host");
	const char *share = lp_parm_string(-1, "torture", "share");
	struct cli_credentials *credentials = cmdline_credentials;

	status = smb2_connect(mem_ctx, host, share, credentials, tree, 
			      event_context_find(mem_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to connect to SMB2 share \\\\%s\\%s - %s\n",
		       host, share, nt_errstr(status));
		return False;
	}
	return True;
}


/*
  create and return a handle to a test file
*/
NTSTATUS torture_smb2_testfile(struct smb2_tree *tree, const char *fname, 
			       struct smb2_handle *handle)
{
	struct smb2_create io;
	struct smb2_read r;
	NTSTATUS status;

	ZERO_STRUCT(io);
	io.in.oplock_flags = 0;
	io.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.in.file_attr   = FILE_ATTRIBUTE_NORMAL;
	io.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = 0;
	io.in.fname = fname;

	status = smb2_create(tree, tree, &io);
	NT_STATUS_NOT_OK_RETURN(status);

	*handle = io.out.handle;

	ZERO_STRUCT(r);
	r.in.length      = 5;
	r.in.offset      = 0;
	r.in.handle      = *handle;

	smb2_read(tree, tree, &r);

	return NT_STATUS_OK;
}

/*
  create and return a handle to a test directory
*/
NTSTATUS torture_smb2_testdir(struct smb2_tree *tree, const char *fname, 
			      struct smb2_handle *handle)
{
	struct smb2_create io;
	NTSTATUS status;

	ZERO_STRUCT(io);
	io.in.oplock_flags = 0;
	io.in.access_mask = SEC_RIGHTS_DIR_ALL;
	io.in.file_attr   = FILE_ATTRIBUTE_DIRECTORY;
	io.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.in.share_access = NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE;
	io.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.in.fname = fname;

	status = smb2_create(tree, tree, &io);
	NT_STATUS_NOT_OK_RETURN(status);

	*handle = io.out.handle;

	return NT_STATUS_OK;
}


/*
  create a complex file using the old SMB protocol, to make it easier to 
  find fields in SMB2 getinfo levels
*/
BOOL torture_setup_complex_file(const char *fname)
{
	struct smbcli_state *cli;
	int fnum;

	if (!torture_open_connection(&cli)) {
		return False;
	}

	fnum = create_complex_file(cli, cli, fname);

	if (DEBUGLVL(1)) {
		torture_all_info(cli->tree, fname);
	}
	
	talloc_free(cli);
	return fnum != -1;
}

/*
  create a complex directory using the old SMB protocol, to make it easier to 
  find fields in SMB2 getinfo levels
*/
BOOL torture_setup_complex_dir(const char *dname)
{
	struct smbcli_state *cli;
	int fnum;

	if (!torture_open_connection(&cli)) {
		return False;
	}

	fnum = create_complex_dir(cli, cli, dname);

	if (DEBUGLVL(1)) {
		torture_all_info(cli->tree, dname);
	}
	
	talloc_free(cli);
	return fnum != -1;
}
