/* 
   Unix SMB/CIFS implementation.
   SMB torture tester utility functions
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
#include "smb.h"
#include "lib/cmdline/popt_common.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/ioctl.h"
#include "libcli/libcli.h"
#include "system/filesys.h"
#include "system/shmem.h"
#include "system/wait.h"
#include "system/time.h"
#include "torture/torture.h"


/**
  setup a directory ready for a test
*/
_PUBLIC_ BOOL torture_setup_dir(struct smbcli_state *cli, const char *dname)
{
	smb_raw_exit(cli->session);
	if (smbcli_deltree(cli->tree, dname) == -1 ||
	    NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, dname))) {
		printf("Unable to setup %s - %s\n", dname, smbcli_errstr(cli->tree));
		return False;
	}
	return True;
}

/*
  create a directory, returning a handle to it
*/
NTSTATUS create_directory_handle(struct smbcli_tree *tree, const char *dname, int *fnum)
{
	NTSTATUS status;
	union smb_open io;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("create_directory_handle");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = dname;

	status = smb_raw_open(tree, mem_ctx, &io);
	talloc_free(mem_ctx);

	if (NT_STATUS_IS_OK(status)) {
		*fnum = io.ntcreatex.out.file.fnum;
	}

	return status;
}


/**
  sometimes we need a fairly complex file to work with, so we can test
  all possible attributes. 
*/
_PUBLIC_ int create_complex_file(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *fname)
{
	int fnum;
	char buf[7] = "abc";
	union smb_setfileinfo setfile;
	union smb_fileinfo fileinfo;
	time_t t = (time(NULL) & ~1);
	NTSTATUS status;

	smbcli_unlink(cli->tree, fname);
	fnum = smbcli_nt_create_full(cli->tree, fname, 0, 
				     SEC_RIGHTS_FILE_ALL,
				     FILE_ATTRIBUTE_NORMAL,
				     NTCREATEX_SHARE_ACCESS_DELETE|
				     NTCREATEX_SHARE_ACCESS_READ|
				     NTCREATEX_SHARE_ACCESS_WRITE, 
				     NTCREATEX_DISP_OVERWRITE_IF,
				     0, 0);
	if (fnum == -1) return -1;

	smbcli_write(cli->tree, fnum, 0, buf, 0, sizeof(buf));

	if (strchr(fname, ':') == NULL) {
		/* setup some EAs */
		setfile.generic.level = RAW_SFILEINFO_EA_SET;
		setfile.generic.in.file.fnum = fnum;
		setfile.ea_set.in.num_eas = 2;	
		setfile.ea_set.in.eas = talloc_array(mem_ctx, struct ea_struct, 2);
		setfile.ea_set.in.eas[0].flags = 0;
		setfile.ea_set.in.eas[0].name.s = "EAONE";
		setfile.ea_set.in.eas[0].value = data_blob_talloc(mem_ctx, "VALUE1", 6);
		setfile.ea_set.in.eas[1].flags = 0;
		setfile.ea_set.in.eas[1].name.s = "SECONDEA";
		setfile.ea_set.in.eas[1].value = data_blob_talloc(mem_ctx, "ValueTwo", 8);
		status = smb_raw_setfileinfo(cli->tree, &setfile);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to setup EAs\n");
		}
	}

	/* make sure all the timestamps aren't the same, and are also 
	   in different DST zones*/
	setfile.generic.level = RAW_SFILEINFO_SETATTRE;
	setfile.generic.in.file.fnum = fnum;

	setfile.setattre.in.create_time = t + 9*30*24*60*60;
	setfile.setattre.in.access_time = t + 6*30*24*60*60;
	setfile.setattre.in.write_time  = t + 3*30*24*60*60;

	status = smb_raw_setfileinfo(cli->tree, &setfile);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to setup file times - %s\n", nt_errstr(status));
	}

	/* make sure all the timestamps aren't the same */
	fileinfo.generic.level = RAW_FILEINFO_GETATTRE;
	fileinfo.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &fileinfo);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to query file times - %s\n", nt_errstr(status));
	}

	if (setfile.setattre.in.create_time != fileinfo.getattre.out.create_time) {
		printf("create_time not setup correctly\n");
	}
	if (setfile.setattre.in.access_time != fileinfo.getattre.out.access_time) {
		printf("access_time not setup correctly\n");
	}
	if (setfile.setattre.in.write_time != fileinfo.getattre.out.write_time) {
		printf("write_time not setup correctly\n");
	}

	return fnum;
}


/*
  sometimes we need a fairly complex directory to work with, so we can test
  all possible attributes. 
*/
int create_complex_dir(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *dname)
{
	int fnum;
	union smb_setfileinfo setfile;
	union smb_fileinfo fileinfo;
	time_t t = (time(NULL) & ~1);
	NTSTATUS status;

	smbcli_deltree(cli->tree, dname);
	fnum = smbcli_nt_create_full(cli->tree, dname, 0, 
				     SEC_RIGHTS_DIR_ALL,
				     FILE_ATTRIBUTE_DIRECTORY,
				     NTCREATEX_SHARE_ACCESS_READ|
				     NTCREATEX_SHARE_ACCESS_WRITE, 
				     NTCREATEX_DISP_OPEN_IF,
				     NTCREATEX_OPTIONS_DIRECTORY, 0);
	if (fnum == -1) return -1;

	if (strchr(dname, ':') == NULL) {
		/* setup some EAs */
		setfile.generic.level = RAW_SFILEINFO_EA_SET;
		setfile.generic.in.file.fnum = fnum;
		setfile.ea_set.in.num_eas = 2;	
		setfile.ea_set.in.eas = talloc_array(mem_ctx, struct ea_struct, 2);
		setfile.ea_set.in.eas[0].flags = 0;
		setfile.ea_set.in.eas[0].name.s = "EAONE";
		setfile.ea_set.in.eas[0].value = data_blob_talloc(mem_ctx, "VALUE1", 6);
		setfile.ea_set.in.eas[1].flags = 0;
		setfile.ea_set.in.eas[1].name.s = "SECONDEA";
		setfile.ea_set.in.eas[1].value = data_blob_talloc(mem_ctx, "ValueTwo", 8);
		status = smb_raw_setfileinfo(cli->tree, &setfile);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to setup EAs\n");
		}
	}

	/* make sure all the timestamps aren't the same, and are also 
	   in different DST zones*/
	setfile.generic.level = RAW_SFILEINFO_SETATTRE;
	setfile.generic.in.file.fnum = fnum;

	setfile.setattre.in.create_time = t + 9*30*24*60*60;
	setfile.setattre.in.access_time = t + 6*30*24*60*60;
	setfile.setattre.in.write_time  = t + 3*30*24*60*60;

	status = smb_raw_setfileinfo(cli->tree, &setfile);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to setup file times - %s\n", nt_errstr(status));
	}

	/* make sure all the timestamps aren't the same */
	fileinfo.generic.level = RAW_FILEINFO_GETATTRE;
	fileinfo.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &fileinfo);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to query file times - %s\n", nt_errstr(status));
	}

	if (setfile.setattre.in.create_time != fileinfo.getattre.out.create_time) {
		printf("create_time not setup correctly\n");
	}
	if (setfile.setattre.in.access_time != fileinfo.getattre.out.access_time) {
		printf("access_time not setup correctly\n");
	}
	if (setfile.setattre.in.write_time != fileinfo.getattre.out.write_time) {
		printf("write_time not setup correctly\n");
	}

	return fnum;
}



/* return a pointer to a anonymous shared memory segment of size "size"
   which will persist across fork() but will disappear when all processes
   exit 

   The memory is not zeroed 

   This function uses system5 shared memory. It takes advantage of a property
   that the memory is not destroyed if it is attached when the id is removed
   */
void *shm_setup(int size)
{
	int shmid;
	void *ret;

	shmid = shmget(IPC_PRIVATE, size, SHM_R | SHM_W);
	if (shmid == -1) {
		printf("can't get shared memory\n");
		exit(1);
	}
	ret = (void *)shmat(shmid, 0, 0);
	if (!ret || ret == (void *)-1) {
		printf("can't attach to shared memory\n");
		return NULL;
	}
	/* the following releases the ipc, but note that this process
	   and all its children will still have access to the memory, its
	   just that the shmid is no longer valid for other shm calls. This
	   means we don't leave behind lots of shm segments after we exit 

	   See Stevens "advanced programming in unix env" for details
	   */
	shmctl(shmid, IPC_RMID, 0);
	
	return ret;
}


/*
  check that a wire string matches the flags specified 
  not 100% accurate, but close enough for testing
*/
BOOL wire_bad_flags(struct smb_wire_string *str, int flags, struct smbcli_state *cli)
{
	BOOL server_unicode;
	int len;
	if (!str || !str->s) return True;
	len = strlen(str->s);
	if (flags & STR_TERMINATE) len++;

	server_unicode = (cli->transport->negotiate.capabilities&CAP_UNICODE)?True:False;
	if (getenv("CLI_FORCE_ASCII") || !lp_unicode()) {
		server_unicode = False;
	}

	if ((flags & STR_UNICODE) || server_unicode) {
		len *= 2;
	} else if (flags & STR_TERMINATE_ASCII) {
		len++;
	}
	if (str->private_length != len) {
		printf("Expected wire_length %d but got %d for '%s'\n", 
		       len, str->private_length, str->s);
		return True;
	}
	return False;
}

/*
  dump a all_info QFILEINFO structure
*/
void dump_all_info(TALLOC_CTX *mem_ctx, union smb_fileinfo *finfo)
{
	d_printf("\tcreate_time:    %s\n", nt_time_string(mem_ctx, finfo->all_info.out.create_time));
	d_printf("\taccess_time:    %s\n", nt_time_string(mem_ctx, finfo->all_info.out.access_time));
	d_printf("\twrite_time:     %s\n", nt_time_string(mem_ctx, finfo->all_info.out.write_time));
	d_printf("\tchange_time:    %s\n", nt_time_string(mem_ctx, finfo->all_info.out.change_time));
	d_printf("\tattrib:         0x%x\n", finfo->all_info.out.attrib);
	d_printf("\talloc_size:     %llu\n", (long long)finfo->all_info.out.alloc_size);
	d_printf("\tsize:           %llu\n", (long long)finfo->all_info.out.size);
	d_printf("\tnlink:          %u\n", finfo->all_info.out.nlink);
	d_printf("\tdelete_pending: %u\n", finfo->all_info.out.delete_pending);
	d_printf("\tdirectory:      %u\n", finfo->all_info.out.directory);
	d_printf("\tea_size:        %u\n", finfo->all_info.out.ea_size);
	d_printf("\tfname:          '%s'\n", finfo->all_info.out.fname.s);
}

/*
  dump file infor by name
*/
void torture_all_info(struct smbcli_tree *tree, const char *fname)
{
	TALLOC_CTX *mem_ctx = talloc_init("%s", fname);
	union smb_fileinfo finfo;
	NTSTATUS status;

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(tree, mem_ctx, &finfo);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s - %s\n", fname, nt_errstr(status));
		return;
	}

	d_printf("%s:\n", fname);
	dump_all_info(mem_ctx, &finfo);
	talloc_free(mem_ctx);
}


/*
  set a attribute on a file
*/
BOOL torture_set_file_attribute(struct smbcli_tree *tree, const char *fname, uint16_t attrib)
{
	union smb_setfileinfo sfinfo;
	NTSTATUS status;

	ZERO_STRUCT(sfinfo.basic_info.in);
	sfinfo.basic_info.level = RAW_SFILEINFO_BASIC_INFORMATION;
	sfinfo.basic_info.in.file.path = fname;
	sfinfo.basic_info.in.attrib = attrib;
	status = smb_raw_setpathinfo(tree, &sfinfo);
	return NT_STATUS_IS_OK(status);
}


/*
  set a file descriptor as sparse
*/
NTSTATUS torture_set_sparse(struct smbcli_tree *tree, int fnum)
{
	union smb_ioctl nt;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("torture_set_sparse");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	nt.ntioctl.level = RAW_IOCTL_NTIOCTL;
	nt.ntioctl.in.function = FSCTL_SET_SPARSE;
	nt.ntioctl.in.file.fnum = fnum;
	nt.ntioctl.in.fsctl = True;
	nt.ntioctl.in.filter = 0;
	nt.ntioctl.in.max_data = 0;
	nt.ntioctl.in.blob = data_blob(NULL, 0);

	status = smb_raw_ioctl(tree, mem_ctx, &nt);

	talloc_free(mem_ctx);

	return status;
}

/*
  check that an EA has the right value 
*/
NTSTATUS torture_check_ea(struct smbcli_state *cli, 
			  const char *fname, const char *eaname, const char *value)
{
	union smb_fileinfo info;
	NTSTATUS status;
	struct ea_name ea;
	TALLOC_CTX *mem_ctx = talloc_new(cli);

	info.ea_list.level = RAW_FILEINFO_EA_LIST;
	info.ea_list.in.file.path = fname;
	info.ea_list.in.num_names = 1;
	info.ea_list.in.ea_names = &ea;

	ea.name.s = eaname;

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &info);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return status;
	}

	if (info.ea_list.out.num_eas != 1) {
		printf("Expected 1 ea in ea_list\n");
		talloc_free(mem_ctx);
		return NT_STATUS_EA_CORRUPT_ERROR;
	}

	if (strcasecmp_m(eaname, info.ea_list.out.eas[0].name.s) != 0) {
		printf("Expected ea '%s' not '%s' in ea_list\n",
		       eaname, info.ea_list.out.eas[0].name.s);
		talloc_free(mem_ctx);
		return NT_STATUS_EA_CORRUPT_ERROR;
	}

	if (value == NULL) {
		if (info.ea_list.out.eas[0].value.length != 0) {
			printf("Expected zero length ea for %s\n", eaname);
			talloc_free(mem_ctx);
			return NT_STATUS_EA_CORRUPT_ERROR;
		}
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}

	if (strlen(value) == info.ea_list.out.eas[0].value.length &&
	    memcmp(value, info.ea_list.out.eas[0].value.data,
		   info.ea_list.out.eas[0].value.length) == 0) {
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}

	printf("Expected value '%s' not '%*.*s' for ea %s\n",
	       value, 
	       (int)info.ea_list.out.eas[0].value.length,
	       (int)info.ea_list.out.eas[0].value.length,
	       info.ea_list.out.eas[0].value.data,
	       eaname);

	talloc_free(mem_ctx);

	return NT_STATUS_EA_CORRUPT_ERROR;
}

BOOL torture_open_connection_share(TALLOC_CTX *mem_ctx,
				   struct smbcli_state **c, 
				   const char *hostname, 
				   const char *sharename,
				   struct event_context *ev)
{
	NTSTATUS status;

	status = smbcli_full_connection(mem_ctx, c, hostname, 
					sharename, NULL,
					cmdline_credentials, ev);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to open connection - %s\n", nt_errstr(status));
		return False;
	}

	(*c)->transport->options.use_oplocks = use_oplocks;
	(*c)->transport->options.use_level2_oplocks = use_level_II_oplocks;

	return True;
}

_PUBLIC_ BOOL torture_open_connection(struct smbcli_state **c)
{
	const char *host = lp_parm_string(-1, "torture", "host");
	const char *share = lp_parm_string(-1, "torture", "share");

	return torture_open_connection_share(NULL, c, host, share, NULL);
}

_PUBLIC_ BOOL torture_open_connection_ev(struct smbcli_state **c, 
					 struct event_context *ev)
{
	const char *host = lp_parm_string(-1, "torture", "host");
	const char *share = lp_parm_string(-1, "torture", "share");

	return torture_open_connection_share(NULL, c, host, share, ev);
}



_PUBLIC_ BOOL torture_close_connection(struct smbcli_state *c)
{
	BOOL ret = True;
	if (!c) return True;
	if (NT_STATUS_IS_ERR(smbcli_tdis(c))) {
		printf("tdis failed (%s)\n", smbcli_errstr(c->tree));
		ret = False;
	}
	talloc_free(c);
	return ret;
}


/* check if the server produced the expected error code */
_PUBLIC_ BOOL check_error(const char *location, struct smbcli_state *c, 
		 uint8_t eclass, uint32_t ecode, NTSTATUS nterr)
{
	NTSTATUS status;
	
	status = smbcli_nt_error(c->tree);
	if (NT_STATUS_IS_DOS(status)) {
		int class, num;
		class = NT_STATUS_DOS_CLASS(status);
		num = NT_STATUS_DOS_CODE(status);
                if (eclass != class || ecode != num) {
                        printf("unexpected error code %s\n", nt_errstr(status));
                        printf(" expected %s or %s (at %s)\n", 
			       nt_errstr(NT_STATUS_DOS(eclass, ecode)), 
                               nt_errstr(nterr), location);
                        return False;
                }
        } else {
                if (!NT_STATUS_EQUAL(nterr, status)) {
                        printf("unexpected error code %s\n", nt_errstr(status));
                        printf(" expected %s (at %s)\n", nt_errstr(nterr), location);
                        return False;
                }
        }

	return True;
}

static struct smbcli_state *current_cli;
static int procnum; /* records process count number when forking */

static void sigcont(int sig)
{
}

double torture_create_procs(BOOL (*fn)(struct smbcli_state *, int), BOOL *result)
{
	int i, status;
	volatile pid_t *child_status;
	volatile BOOL *child_status_out;
	int synccount;
	int tries = 8;
	double start_time_limit = 10 + (torture_nprocs * 1.5);
	char **unc_list = NULL;
	const char *p;
	int num_unc_names = 0;
	struct timeval tv;

	*result = True;

	synccount = 0;

	signal(SIGCONT, sigcont);

	child_status = (volatile pid_t *)shm_setup(sizeof(pid_t)*torture_nprocs);
	if (!child_status) {
		printf("Failed to setup shared memory\n");
		return -1;
	}

	child_status_out = (volatile BOOL *)shm_setup(sizeof(BOOL)*torture_nprocs);
	if (!child_status_out) {
		printf("Failed to setup result status shared memory\n");
		return -1;
	}

	p = lp_parm_string(-1, "torture", "unclist");
	if (p) {
		unc_list = file_lines_load(p, &num_unc_names, NULL);
		if (!unc_list || num_unc_names <= 0) {
			printf("Failed to load unc names list from '%s'\n", p);
			exit(1);
		}
	}

	for (i = 0; i < torture_nprocs; i++) {
		child_status[i] = 0;
		child_status_out[i] = True;
	}

	tv = timeval_current();

	for (i=0;i<torture_nprocs;i++) {
		procnum = i;
		if (fork() == 0) {
			char *myname;
			char *hostname=NULL, *sharename;

			pid_t mypid = getpid();
			srandom(((int)mypid) ^ ((int)time(NULL)));

			asprintf(&myname, "CLIENT%d", i);
			lp_set_cmdline("netbios name", myname);
			free(myname);


			if (unc_list) {
				if (!smbcli_parse_unc(unc_list[i % num_unc_names],
						      NULL, &hostname, &sharename)) {
					printf("Failed to parse UNC name %s\n",
					       unc_list[i % num_unc_names]);
					exit(1);
				}
			}

			while (1) {
				if (hostname) {
					if (torture_open_connection_share(NULL,
									  &current_cli,
									  hostname, 
									  sharename,
									  NULL)) {
						break;
					}
				} else if (torture_open_connection(&current_cli)) {
						break;
				}
				if (tries-- == 0) {
					printf("pid %d failed to start\n", (int)getpid());
					_exit(1);
				}
				msleep(100);	
			}

			child_status[i] = getpid();

			pause();

			if (child_status[i]) {
				printf("Child %d failed to start!\n", i);
				child_status_out[i] = 1;
				_exit(1);
			}

			child_status_out[i] = fn(current_cli, i);
			_exit(0);
		}
	}

	do {
		synccount = 0;
		for (i=0;i<torture_nprocs;i++) {
			if (child_status[i]) synccount++;
		}
		if (synccount == torture_nprocs) break;
		msleep(100);
	} while (timeval_elapsed(&tv) < start_time_limit);

	if (synccount != torture_nprocs) {
		printf("FAILED TO START %d CLIENTS (started %d)\n", torture_nprocs, synccount);
		*result = False;
		return timeval_elapsed(&tv);
	}

	printf("Starting %d clients\n", torture_nprocs);

	/* start the client load */
	tv = timeval_current();
	for (i=0;i<torture_nprocs;i++) {
		child_status[i] = 0;
	}

	printf("%d clients started\n", torture_nprocs);

	kill(0, SIGCONT);

	for (i=0;i<torture_nprocs;i++) {
		int ret;
		while ((ret=waitpid(0, &status, 0)) == -1 && errno == EINTR) /* noop */ ;
		if (ret == -1 || WEXITSTATUS(status) != 0) {
			*result = False;
		}
	}

	printf("\n");
	
	for (i=0;i<torture_nprocs;i++) {
		if (!child_status_out[i]) {
			*result = False;
		}
	}
	return timeval_elapsed(&tv);
}

static BOOL wrap_old_torture_multifn(struct torture_context *torture,
								const void *_fn)
{
	BOOL (*fn)(struct smbcli_state *, int ) = _fn;
	BOOL result;

	torture_create_procs(fn, &result);

	return result;
}

_PUBLIC_ NTSTATUS register_torture_multi_op(const char *name, 
											BOOL (*multi_fn)(struct smbcli_state *, int ))
{
	struct torture_suite *suite;

	suite = torture_suite_create(talloc_autofree_context(), name);

	torture_suite_add_simple_tcase(suite, name, 
								   wrap_old_torture_multifn,
								   multi_fn);
	torture_register_suite(suite);

	return NT_STATUS_OK;
}

