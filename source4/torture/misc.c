/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-2003
   Copyright (C) Jelmer Vernooij 2006
   
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
#include "system/time.h"
#include "system/wait.h"
#include "system/filesys.h"
#include "libcli/raw/ioctl.h"
#include "libcli/libcli.h"
#include "lib/events/events.h"
#include "libcli/resolve/resolve.h"
#include "auth/credentials/credentials.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "torture/torture.h"
#include "torture/util.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/composite/composite.h"

extern struct cli_credentials *cmdline_credentials;
void benchrw_callback(struct smbcli_request *req);
enum benchrw_stage {
	START,
	OPEN_CONNECTION,
	CLEANUP_TESTDIR,
	MK_TESTDIR,
	OPEN_FILE,
	INITIAL_WRITE,
	READ_WRITE_DATA,
	MAX_OPS_REACHED,
	ERROR,
	CLOSE_FILE,
	CLEANUP,
	FINISHED
};

struct benchrw_state{
		char *dname;
		char *fname;
		uint16_t fnum;
		int nr;
		struct smbcli_tree	*cli;		
		uint8_t *buffer;
		int writecnt;
		int readcnt;
		int completed;
		TALLOC_CTX *mem_ctx;
		void *req_params;
		enum benchrw_stage mode;
		struct params{
			struct unclist{
				const char *host;
				const char *share;
			} **unc;
			const char *workgroup;
			int retry;
			unsigned int writeblocks;
			unsigned int blocksize;
			unsigned int writeratio;
		} *lp_params;
	};
	
static BOOL wait_lock(struct smbcli_state *c, int fnum, uint32_t offset, uint32_t len)
{
	while (NT_STATUS_IS_ERR(smbcli_lock(c->tree, fnum, offset, len, -1, WRITE_LOCK))) {
		if (!check_error(__location__, c, ERRDOS, ERRlock, NT_STATUS_LOCK_NOT_GRANTED)) return False;
	}
	return True;
}


static BOOL rw_torture(struct smbcli_state *c)
{
	const char *lockfname = "\\torture.lck";
	char *fname;
	int fnum;
	int fnum2;
	pid_t pid2, pid = getpid();
	int i, j;
	uint8_t buf[1024];
	BOOL correct = True;

	fnum2 = smbcli_open(c->tree, lockfname, O_RDWR | O_CREAT | O_EXCL, 
			 DENY_NONE);
	if (fnum2 == -1)
		fnum2 = smbcli_open(c->tree, lockfname, O_RDWR, DENY_NONE);
	if (fnum2 == -1) {
		printf("open of %s failed (%s)\n", lockfname, smbcli_errstr(c->tree));
		return False;
	}


	for (i=0;i<torture_numops;i++) {
		uint_t n = (uint_t)random()%10;
		if (i % 10 == 0) {
			printf("%d\r", i); fflush(stdout);
		}
		asprintf(&fname, "\\torture.%u", n);

		if (!wait_lock(c, fnum2, n*sizeof(int), sizeof(int))) {
			return False;
		}

		fnum = smbcli_open(c->tree, fname, O_RDWR | O_CREAT | O_TRUNC, DENY_ALL);
		if (fnum == -1) {
			printf("open failed (%s)\n", smbcli_errstr(c->tree));
			correct = False;
			break;
		}

		if (smbcli_write(c->tree, fnum, 0, &pid, 0, sizeof(pid)) != sizeof(pid)) {
			printf("write failed (%s)\n", smbcli_errstr(c->tree));
			correct = False;
		}

		for (j=0;j<50;j++) {
			if (smbcli_write(c->tree, fnum, 0, buf, 
				      sizeof(pid)+(j*sizeof(buf)), 
				      sizeof(buf)) != sizeof(buf)) {
				printf("write failed (%s)\n", smbcli_errstr(c->tree));
				correct = False;
			}
		}

		pid2 = 0;

		if (smbcli_read(c->tree, fnum, &pid2, 0, sizeof(pid)) != sizeof(pid)) {
			printf("read failed (%s)\n", smbcli_errstr(c->tree));
			correct = False;
		}

		if (pid2 != pid) {
			printf("data corruption!\n");
			correct = False;
		}

		if (NT_STATUS_IS_ERR(smbcli_close(c->tree, fnum))) {
			printf("close failed (%s)\n", smbcli_errstr(c->tree));
			correct = False;
		}

		if (NT_STATUS_IS_ERR(smbcli_unlink(c->tree, fname))) {
			printf("unlink failed (%s)\n", smbcli_errstr(c->tree));
			correct = False;
		}

		if (NT_STATUS_IS_ERR(smbcli_unlock(c->tree, fnum2, n*sizeof(int), sizeof(int)))) {
			printf("unlock failed (%s)\n", smbcli_errstr(c->tree));
			correct = False;
		}
		free(fname);
	}

	smbcli_close(c->tree, fnum2);
	smbcli_unlink(c->tree, lockfname);

	printf("%d\n", i);

	return correct;
}

static BOOL run_torture(struct smbcli_state *cli, int dummy)
{
    BOOL ret;

	ret = rw_torture(cli);
	
	if (!torture_close_connection(cli)) {
		ret = False;
	}

	return ret;
}


/*
  see how many RPC pipes we can open at once
*/
static BOOL run_pipe_number(struct torture_context *torture)
{
	struct smbcli_state *cli1;
	const char *pipe_name = "\\WKSSVC";
	int fnum;
	int num_pipes = 0;

	printf("starting pipenumber test\n");
	if (!torture_open_connection(&cli1, 0)) {
		return False;
	}

	while(1) {
		fnum = smbcli_nt_create_full(cli1->tree, pipe_name, 0, SEC_FILE_READ_DATA, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE, NTCREATEX_DISP_OPEN_IF, 0, 0);

		if (fnum == -1) {
			printf("Open of pipe %s failed with error (%s)\n", pipe_name, smbcli_errstr(cli1->tree));
			break;
		}
		num_pipes++;
		printf("%d\r", num_pipes);
		fflush(stdout);
	}

	printf("pipe_number test - we can open %d %s pipes.\n", num_pipes, pipe_name );
	torture_close_connection(cli1);
	return True;
}




/*
  open N connections to the server and just hold them open
  used for testing performance when there are N idle users
  already connected
 */
 static BOOL torture_holdcon(struct torture_context *torture)
{
	int i;
	struct smbcli_state **cli;
	int num_dead = 0;

	printf("Opening %d connections\n", torture_numops);
	
	cli = malloc_array_p(struct smbcli_state *, torture_numops);

	for (i=0;i<torture_numops;i++) {
		if (!torture_open_connection(&cli[i], i)) {
			return False;
		}
		printf("opened %d connections\r", i);
		fflush(stdout);
	}

	printf("\nStarting pings\n");

	while (1) {
		for (i=0;i<torture_numops;i++) {
			NTSTATUS status;
			if (cli[i]) {
				status = smbcli_chkpath(cli[i]->tree, "\\");
				if (!NT_STATUS_IS_OK(status)) {
					printf("Connection %d is dead\n", i);
					cli[i] = NULL;
					num_dead++;
				}
				usleep(100);
			}
		}

		if (num_dead == torture_numops) {
			printf("All connections dead - finishing\n");
			break;
		}

		printf(".");
		fflush(stdout);
	}

	return True;
}

/*
test how many open files this server supports on the one socket
*/
static BOOL run_maxfidtest(struct smbcli_state *cli, int dummy)
{
#define MAXFID_TEMPLATE "\\maxfid\\fid%d\\maxfid.%d.%d"
	char *fname;
	int fnums[0x11000], i;
	int retries=4, maxfid;
	BOOL correct = True;

	if (retries <= 0) {
		printf("failed to connect\n");
		return False;
	}

	if (smbcli_deltree(cli->tree, "\\maxfid") == -1) {
		printf("Failed to deltree \\maxfid - %s\n",
		       smbcli_errstr(cli->tree));
		return False;
	}
	if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, "\\maxfid"))) {
		printf("Failed to mkdir \\maxfid, error=%s\n", 
		       smbcli_errstr(cli->tree));
		return False;
	}

	printf("Testing maximum number of open files\n");

	for (i=0; i<0x11000; i++) {
		if (i % 1000 == 0) {
			asprintf(&fname, "\\maxfid\\fid%d", i/1000);
			if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, fname))) {
				printf("Failed to mkdir %s, error=%s\n", 
				       fname, smbcli_errstr(cli->tree));
				return False;
			}
			free(fname);
		}
		asprintf(&fname, MAXFID_TEMPLATE, i/1000, i,(int)getpid());
		if ((fnums[i] = smbcli_open(cli->tree, fname, 
					O_RDWR|O_CREAT|O_TRUNC, DENY_NONE)) ==
		    -1) {
			printf("open of %s failed (%s)\n", 
			       fname, smbcli_errstr(cli->tree));
			printf("maximum fnum is %d\n", i);
			break;
		}
		free(fname);
		printf("%6d\r", i);
	}
	printf("%6d\n", i);
	i--;

	maxfid = i;

	printf("cleaning up\n");
	for (i=0;i<maxfid/2;i++) {
		asprintf(&fname, MAXFID_TEMPLATE, i/1000, i,(int)getpid());
		if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnums[i]))) {
			printf("Close of fnum %d failed - %s\n", fnums[i], smbcli_errstr(cli->tree));
		}
		if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, fname))) {
			printf("unlink of %s failed (%s)\n", 
			       fname, smbcli_errstr(cli->tree));
			correct = False;
		}
		free(fname);

		asprintf(&fname, MAXFID_TEMPLATE, (maxfid-i)/1000, maxfid-i,(int)getpid());
		if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnums[maxfid-i]))) {
			printf("Close of fnum %d failed - %s\n", fnums[maxfid-i], smbcli_errstr(cli->tree));
		}
		if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, fname))) {
			printf("unlink of %s failed (%s)\n", 
			       fname, smbcli_errstr(cli->tree));
			correct = False;
		}
		free(fname);

		printf("%6d %6d\r", i, maxfid-i);
	}
	printf("%6d\n", 0);

	if (smbcli_deltree(cli->tree, "\\maxfid") == -1) {
		printf("Failed to deltree \\maxfid - %s\n",
		       smbcli_errstr(cli->tree));
		return False;
	}

	printf("maxfid test finished\n");
	if (!torture_close_connection(cli)) {
		correct = False;
	}
	return correct;
#undef MAXFID_TEMPLATE
}



/*
  sees what IOCTLs are supported
 */
static BOOL torture_ioctl_test(struct torture_context *torture)
{
	struct smbcli_state *cli;
	uint16_t device, function;
	int fnum;
	const char *fname = "\\ioctl.dat";
	NTSTATUS status;
	union smb_ioctl parms;
	TALLOC_CTX *mem_ctx;

	if (!torture_open_connection(&cli, 0)) {
		return False;
	}

	mem_ctx = talloc_named_const(torture, 0, "ioctl_test");

	printf("starting ioctl test\n");

	smbcli_unlink(cli->tree, fname);

	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	if (fnum == -1) {
		printf("open of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
		return False;
	}

	parms.ioctl.level = RAW_IOCTL_IOCTL;
	parms.ioctl.in.file.fnum = fnum;
	parms.ioctl.in.request = IOCTL_QUERY_JOB_INFO;
	status = smb_raw_ioctl(cli->tree, mem_ctx, &parms);
	printf("ioctl job info: %s\n", smbcli_errstr(cli->tree));

	for (device=0;device<0x100;device++) {
		printf("testing device=0x%x\n", device);
		for (function=0;function<0x100;function++) {
			parms.ioctl.in.request = (device << 16) | function;
			status = smb_raw_ioctl(cli->tree, mem_ctx, &parms);

			if (NT_STATUS_IS_OK(status)) {
				printf("ioctl device=0x%x function=0x%x OK : %d bytes\n", 
					device, function, (int)parms.ioctl.out.blob.length);
			}
		}
	}

	if (!torture_close_connection(cli)) {
		return False;
	}

	return True;
}

/* 
 	init params using lp_parm_xxx 
 	return number of unclist entries
*/
int init_benchrw_params(TALLOC_CTX *mem_ctx,struct params *lpar)
{
	char **unc_list = NULL;
	int num_unc_names = 0, conn_index=0, empty_lines=0;
	const char *p;
	lpar->retry = lp_parm_int(-1, "torture", "retry",3);
	lpar->blocksize = lp_parm_int(-1, "torture", "blocksize",65535);
	lpar->writeblocks = lp_parm_int(-1, "torture", "writeblocks",15);
	lpar->writeratio = lp_parm_int(-1, "torture", "writeratio",5);
	lpar->workgroup = lp_workgroup();
	
	p = lp_parm_string(-1, "torture", "unclist");
	if (p) {
		char *h, *s;
		unc_list = file_lines_load(p, &num_unc_names, NULL);
		if (!unc_list || num_unc_names <= 0) {
			printf("Failed to load unc names list from '%s'\n", p);
			exit(1);
		}
		
		lpar->unc = talloc_array(mem_ctx, struct unclist *, (num_unc_names-empty_lines));
		for(conn_index = 0; conn_index < num_unc_names; conn_index++) {
			/* ignore empty lines */
			if(strlen(unc_list[conn_index % num_unc_names])==0){
				empty_lines++;
				continue;
			}
			if (!smbcli_parse_unc(unc_list[conn_index % num_unc_names],
					      NULL, &h, &s)) {
				printf("Failed to parse UNC name %s\n",
			       unc_list[conn_index % num_unc_names]);
				exit(1);
			}
		lpar->unc[conn_index-empty_lines] = talloc(mem_ctx,struct unclist);
		lpar->unc[conn_index-empty_lines]->host = h;
		lpar->unc[conn_index-empty_lines]->share = s;	
		}
		return num_unc_names-empty_lines;
	}else{
		lpar->unc = talloc_array(mem_ctx, struct unclist *, 1);
		lpar->unc[0] = talloc(mem_ctx,struct unclist);
		lpar->unc[0]->host  = lp_parm_string(-1, "torture", "host");
		lpar->unc[0]->share = lp_parm_string(-1, "torture", "share");
		return 1;
	}
}

/*
 Called when the reads & writes are finished. closes the file.
*/
NTSTATUS benchrw_close(struct smbcli_request *req,
				struct benchrw_state *state)
{
	union smb_close close_parms;
	
	NT_STATUS_NOT_OK_RETURN(req->status);
	
	printf("Close file %d (%d)\n",state->nr,state->fnum);
	close_parms.close.level = RAW_CLOSE_CLOSE;
	close_parms.close.in.file.fnum = state->fnum ;
	close_parms.close.in.write_time = 0;
	state->mode=CLOSE_FILE;
	
	req = smb_raw_close_send(state->cli, &close_parms);
	NT_STATUS_HAVE_NO_MEMORY(req);
	/*register the callback function!*/
	req->async.fn = benchrw_callback;
	req->async.private = state;
	
	return NT_STATUS_OK;
}

/*
 Called when the initial write is completed is done. write or read a file.
*/
NTSTATUS benchrw_readwrite(struct smbcli_request *req,
					struct benchrw_state *state)
{
	union smb_read	rd;
	union smb_write	wr;
	
	NT_STATUS_NOT_OK_RETURN(req->status);
	
	state->completed++;
	/*rotate between writes and reads*/
	if( state->completed % state->lp_params->writeratio == 0){
		printf("Callback WRITE file:%d (%d/%d)\n",
				state->nr,state->completed,torture_numops);
		wr.generic.level = RAW_WRITE_WRITEX  ;
		wr.writex.in.file.fnum  = state->fnum ;
		wr.writex.in.offset     = 0;
		wr.writex.in.wmode	= 0		;
		wr.writex.in.remaining  = 0;
		wr.writex.in.count      = state->lp_params->blocksize;
		wr.writex.in.data       = state->buffer;
		state->readcnt=0;
		req = smb_raw_write_send(state->cli,&wr);
	}else{
		printf("Callback READ file:%d (%d/%d) Offset:%d\n",
				state->nr,state->completed,torture_numops,
				(state->readcnt*state->lp_params->blocksize));
		rd.generic.level = RAW_READ_READ    ;
		rd.read.in.file.fnum	= state->fnum 	;
		rd.read.in.offset	= state->readcnt * 
					state->lp_params->blocksize;
		rd.read.in.count	= state->lp_params->blocksize;
		rd.read.in.remaining	= 0	;
		rd.read.out.data	= state->buffer;
		if(state->readcnt < state->lp_params->writeblocks){
			state->readcnt++;	
		}else{
			/*start reading from beginn of file*/
			state->readcnt=0;
		}
		req = smb_raw_read_send(state->cli,&rd);
	}
	NT_STATUS_HAVE_NO_MEMORY(req);
	/*register the callback function!*/
	req->async.fn = benchrw_callback;
	req->async.private = state;
	
	return NT_STATUS_OK;
}

/*
 Called when the open is done. writes to the file.
*/
NTSTATUS benchrw_open(struct smbcli_request *req,
				struct benchrw_state *state)
{
	union smb_write	wr;
	if(state->mode == OPEN_FILE){
		NTSTATUS status;
		status = smb_raw_open_recv(req,state->mem_ctx,(
					union smb_open*)state->req_params);
		NT_STATUS_NOT_OK_RETURN(status);
	
		state->fnum = ((union smb_open*)state->req_params)
						->openx.out.file.fnum;
		printf("File opened (%d)\n",state->fnum);
		state->mode=INITIAL_WRITE;
	}
		
	printf("Write initial test file:%d (%d/%d)\n",state->nr,
		(state->writecnt+1)*state->lp_params->blocksize,
		(state->lp_params->writeblocks*state->lp_params->blocksize));
	wr.generic.level = RAW_WRITE_WRITEX  ;
	wr.writex.in.file.fnum  = state->fnum ;
	wr.writex.in.offset     = state->writecnt * 
					state->lp_params->blocksize;
	wr.writex.in.wmode	= 0		;
	wr.writex.in.remaining  = (state->lp_params->writeblocks *
						state->lp_params->blocksize)-
						((state->writecnt+1)*state->
						lp_params->blocksize);
	wr.writex.in.count      = state->lp_params->blocksize;
	wr.writex.in.data       = state->buffer;
	state->writecnt++;
	if(state->writecnt == state->lp_params->writeblocks){
		state->mode=READ_WRITE_DATA;
	}
	req = smb_raw_write_send(state->cli,&wr);
	NT_STATUS_HAVE_NO_MEMORY(req);
	
	/*register the callback function!*/
	req->async.fn = benchrw_callback;
	req->async.private = state;
	return NT_STATUS_OK;
} 

/*
 Called when the mkdir is done. Opens a file.
*/
NTSTATUS benchrw_mkdir(struct smbcli_request *req,
				struct benchrw_state *state)
{
	union smb_open *open_parms;	
	uint8_t *writedata;	
		
	NT_STATUS_NOT_OK_RETURN(req->status);
	
	/* open/create the files */
	printf("Open File %d/%d\n",state->nr+1,torture_nprocs);
	open_parms=talloc_zero(state->mem_ctx, union smb_open);
	NT_STATUS_HAVE_NO_MEMORY(open_parms);
	open_parms->openx.level = RAW_OPEN_OPENX;
	open_parms->openx.in.flags = 0;
	open_parms->openx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	open_parms->openx.in.search_attrs = 
			FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	open_parms->openx.in.file_attrs = 0;
	open_parms->openx.in.write_time = 0;
	open_parms->openx.in.open_func = OPENX_OPEN_FUNC_CREATE;
	open_parms->openx.in.size = 0;
	open_parms->openx.in.timeout = 0;
	open_parms->openx.in.fname = state->fname;
		
	writedata = talloc_size(state->mem_ctx,state->lp_params->blocksize);
	NT_STATUS_HAVE_NO_MEMORY(writedata);
	generate_random_buffer(writedata,state->lp_params->blocksize);
	state->buffer=writedata;
	state->writecnt=1;
	state->readcnt=0;
	state->req_params=open_parms;		
	state->mode=OPEN_FILE;	
			
	req = smb_raw_open_send(state->cli,open_parms);
	NT_STATUS_HAVE_NO_MEMORY(req);
	
	/*register the callback function!*/
	req->async.fn = benchrw_callback;
	req->async.private = state;
		
	return NT_STATUS_OK;
}

/*
 handler for completion of a sub-request of the bench-rw test
*/
void benchrw_callback(struct smbcli_request *req)
{
	struct benchrw_state *state = req->async.private;
	
	/*dont send new requests when torture_numops is reached*/
	if(state->completed >= torture_numops){
		state->completed=0;
		state->mode=MAX_OPS_REACHED;
	}
	
	switch (state->mode) {
	
	case MK_TESTDIR:
		if (!NT_STATUS_IS_OK(benchrw_mkdir(req,state))) {			
			printf("Failed to create the test directory - %s\n", 
							nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		break;	
	case OPEN_FILE:
	case INITIAL_WRITE:
		if (!NT_STATUS_IS_OK(benchrw_open(req,state))){
			printf("Failed to open/write the file - %s\n", 
							nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		break;
	case READ_WRITE_DATA:
		if (!NT_STATUS_IS_OK(benchrw_readwrite(req,state))){
			printf("Failed to read/write the file - %s\n", 
							nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		break;
	case MAX_OPS_REACHED:
		if (!NT_STATUS_IS_OK(benchrw_close(req,state))){
			printf("Failed to read/write/close the file - %s\n", 
							nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		break;
	case CLOSE_FILE:
		printf("File %d closed\n",state->nr);
		if (!NT_STATUS_IS_OK(req->status)) {
			printf("Failed to close the file - %s\n",
							nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		state->mode=CLEANUP;
		return;	
	default:
		break;
	}
	
}

/* open connection async callback function*/
void async_open_callback(struct composite_context *con)
{
	struct benchrw_state *state = con->async.private_data;
	int retry = state->lp_params->retry;
	 	
	if (NT_STATUS_IS_OK(con->status)) {
		state->cli=((struct smb_composite_connect*)
					state->req_params)->out.tree;
		state->mode=CLEANUP_TESTDIR;
	}else{
		if(state->writecnt < retry){
			printf("Failed to open connection:%d, Retry (%d/%d)\n",
					state->nr,state->writecnt,retry);
			state->writecnt++;
			state->mode=START;
			usleep(1000);	
		}else{
			printf("Failed to open connection (%d) - %s\n",
					state->nr, nt_errstr(con->status));
			state->mode=ERROR;
		}
		return;
	}	
}

/*
 establishs a smbcli_tree from scratch (async)
*/
struct composite_context *torture_connect_async(
				struct smb_composite_connect *smb,
				TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				const char *host,
				const char *share,
				const char *workgroup)
{
	printf("Open Connection to %s/%s\n",host,share);
	smb->in.dest_host=talloc_strdup(mem_ctx,host);
	smb->in.service=talloc_strdup(mem_ctx,share);
	smb->in.port=0;
	smb->in.called_name = strupper_talloc(mem_ctx, host);
	smb->in.service_type=NULL;
	smb->in.credentials=cmdline_credentials;
	smb->in.fallback_to_anonymous=False;
	smb->in.workgroup=workgroup;
	
	return smb_composite_connect_send(smb,mem_ctx,ev);
}

static BOOL run_benchrw(struct torture_context *torture)
{
	struct smb_composite_connect *smb_con;
	const char *fname = "\\rwtest.dat";
	struct smbcli_request *req;
	struct benchrw_state **state;
	int i , num_unc_names;
	TALLOC_CTX *mem_ctx;
	struct event_context 	*ev	;	
	struct composite_context *req1;
	struct params lpparams;
	union smb_mkdir parms;
	int finished = 0;
	BOOL success=True;
	
	printf("Start BENCH-READWRITE num_ops=%d num_nprocs=%d\n",
		torture_numops,torture_nprocs);

	/*init talloc context*/
	mem_ctx = talloc_named_const(torture, 0, "bench-readwrite");
	ev = event_context_init(mem_ctx);
	state = talloc_array(mem_ctx, struct benchrw_state *, torture_nprocs);

	/* init params using lp_parm_xxx */
	num_unc_names = init_benchrw_params(mem_ctx,&lpparams);
	
	/* init private data structs*/
	for(i = 0; i<torture_nprocs;i++){
		state[i]=talloc(mem_ctx,struct benchrw_state);
		state[i]->completed=0;
		state[i]->lp_params=&lpparams;
		state[i]->nr=i;
		state[i]->dname=talloc_asprintf(mem_ctx,"benchrw%d",i);
		state[i]->fname=talloc_asprintf(mem_ctx,"%s%s",
								state[i]->dname,fname);	
		state[i]->mode=START;
		state[i]->writecnt=0;
	}
	
	printf("Starting async requests\n");	
	while(finished != torture_nprocs){
		finished=0;
		for(i = 0; i<torture_nprocs;i++){
			switch (state[i]->mode){
			/*open multiple connections with the same userid */
			case START:
				smb_con = talloc(mem_ctx,struct smb_composite_connect) ;
				state[i]->req_params=smb_con; 
				state[i]->mode=OPEN_CONNECTION;
				req1 = torture_connect_async(smb_con,
									mem_ctx,ev,
									lpparams.unc[i % num_unc_names]->host,
									lpparams.unc[i % num_unc_names]->share,
									lpparams.workgroup);
				/* register callback fn + private data */
				req1->async.fn = async_open_callback;
				req1->async.private_data=state[i];
				break;
			/*setup test dirs (sync)*/
			case CLEANUP_TESTDIR:
				printf("Setup test dir %d\n",i);
				smb_raw_exit(state[i]->cli->session);
				if (smbcli_deltree(state[i]->cli, 
						state[i]->dname) == -1) {
					printf("Unable to delete %s - %s\n", 
						state[i]->dname,
						smbcli_errstr(state[i]->cli));
					state[i]->mode=ERROR;
					break;
				}
				state[i]->mode=MK_TESTDIR;
				parms.mkdir.level = RAW_MKDIR_MKDIR;
				parms.mkdir.in.path = state[i]->dname;
				req = smb_raw_mkdir_send(state[i]->cli,&parms);
				/* register callback fn + private data */
				req->async.fn = benchrw_callback;
				req->async.private=state[i];
				break;
			/* error occured , finish */
			case ERROR:
				finished++;
				success=False;
				break;
			/* cleanup , close connection */
			case CLEANUP:
				printf("Deleting test dir %s %d/%d\n",state[i]->dname,
						i+1,torture_nprocs);
				smbcli_deltree(state[i]->cli,state[i]->dname);
				if (NT_STATUS_IS_ERR(smb_tree_disconnect(
										state[i]->cli))) {
					printf("ERROR: Tree disconnect failed");
					state[i]->mode=ERROR;
					break;
				}
				state[i]->mode=FINISHED;
			case FINISHED:
				finished++;
				break;
			default:
				event_loop_once(ev);
			}
		}
	}
				
	printf("BENCH-READWRITE done. Closing connections.\n");
	
	/*free all allocated memory*/
	talloc_free(mem_ctx);

	return success;	
}


NTSTATUS torture_misc_init(void)
{
	register_torture_op("BENCH-HOLDCON", torture_holdcon);
	register_torture_op("SCAN-PIPE_NUMBER", run_pipe_number);
	register_torture_op("SCAN-IOCTL", torture_ioctl_test);
	register_torture_op("BENCH-READWRITE", run_benchrw);
	register_torture_multi_op("BENCH-TORTURE", run_torture);
	register_torture_multi_op("SCAN-MAXFID", run_maxfidtest);

	return NT_STATUS_OK;
}
