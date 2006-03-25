/* 
   Unix SMB/CIFS implementation.

   test suite for delayed write update 

   Copyright (C) Volker Lendecke 2004
   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Jeremy Allison 2004
   
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

#define BASEDIR "\\delaywrite"

static BOOL test_delayed_write_update(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_fileinfo finfo1, finfo2;
	const char *fname = BASEDIR "\\torture_file.txt";
	NTSTATUS status;
	int fnum1 = -1;
	BOOL ret = True;
	ssize_t written;
	time_t t;

	printf("Testing delayed update of write time\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum1 == -1) {
		printf("Failed to open %s\n", fname);
		return False;
	}

	finfo1.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo1.basic_info.in.file.fnum = fnum1;
	finfo2 = finfo1;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo1);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		return False;
	}
	
	printf("Initial write time %s\n", 
	       nt_time_string(mem_ctx, finfo1.basic_info.out.write_time));

	/* 3 second delay to ensure we get past any 2 second time
	   granularity (older systems may have that) */
	sleep(3);

	written =  smbcli_write(cli->tree, fnum1, 0, "x", 0, 1);

	if (written != 1) {
		printf("write failed - wrote %d bytes (%s)\n", 
		       (int)written, __location__);
		return False;
	}

	t = time(NULL);

	while (time(NULL) < t+120) {
		status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo2);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
			ret = False;
			break;
		}
		printf("write time %s\n", 
		       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
		if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
			printf("Server updated write_time after %d seconds\n",
			       (int)(time(NULL) - t));
			break;
		}
		sleep(1);
		fflush(stdout);
	}
	
	if (finfo1.basic_info.out.write_time == finfo2.basic_info.out.write_time) {
		printf("Server did not update write time?!\n");
		ret = False;
	}


	if (fnum1 != -1)
		smbcli_close(cli->tree, fnum1);
	smbcli_unlink(cli->tree, fname);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/* 
 * Do as above, but using 2 connections.
 */

static BOOL test_delayed_write_update2(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	struct smbcli_state *cli2=NULL;
	union smb_fileinfo finfo1, finfo2;
	const char *fname = BASEDIR "\\torture_file.txt";
	NTSTATUS status;
	int fnum1 = -1;
	int fnum2 = -1;
	BOOL ret = True;
	ssize_t written;
	time_t t;
	union smb_flush flsh;

	printf("Testing delayed update of write time using 2 connections\n");

	if (!torture_open_connection(&cli2)) {
		return False;
	}

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum1 == -1) {
		printf("Failed to open %s\n", fname);
		return False;
	}

	finfo1.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo1.basic_info.in.file.fnum = fnum1;
	finfo2 = finfo1;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo1);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		return False;
	}
	
	printf("Initial write time %s\n", 
	       nt_time_string(mem_ctx, finfo1.basic_info.out.write_time));

	/* 3 second delay to ensure we get past any 2 second time
	   granularity (older systems may have that) */
	sleep(3);

	{
		/* Try using setfileinfo instead of write to update write time. */
		union smb_setfileinfo sfinfo;
		time_t t_set = time(NULL);
		sfinfo.basic_info.level = RAW_SFILEINFO_BASIC_INFO;
		sfinfo.basic_info.in.file.fnum = fnum1;
		sfinfo.basic_info.in.create_time = finfo1.basic_info.out.create_time;
		sfinfo.basic_info.in.access_time = finfo1.basic_info.out.access_time;

		/* I tried this with both + and - ve to see if it makes a different.
		   It doesn't - once the filetime is set via setfileinfo it stays that way. */
#if 1
		unix_to_nt_time(&sfinfo.basic_info.in.write_time, t_set - 30000);
#else
		unix_to_nt_time(&sfinfo.basic_info.in.write_time, t_set + 30000);
#endif
		sfinfo.basic_info.in.change_time = finfo1.basic_info.out.change_time;
		sfinfo.basic_info.in.attrib = finfo1.basic_info.out.attrib;

		status = smb_raw_setfileinfo(cli->tree, &sfinfo);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("sfileinfo failed: %s\n", nt_errstr(status)));
			return False;
		}
	}

	t = time(NULL);

	while (time(NULL) < t+120) {
		finfo2.basic_info.in.file.path = fname;
	
		status = smb_raw_pathinfo(cli2->tree, mem_ctx, &finfo2);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
			ret = False;
			break;
		}
		printf("write time %s\n", 
		       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
		if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
			printf("Server updated write_time after %d seconds\n",
			       (int)(time(NULL) - t));
			break;
		}
		sleep(1);
		fflush(stdout);
	}
	
	if (finfo1.basic_info.out.write_time == finfo2.basic_info.out.write_time) {
		printf("Server did not update write time?!\n");
		ret = False;
	}

	/* Now try a write to see if the write time gets reset. */

	finfo1.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo1.basic_info.in.file.fnum = fnum1;
	finfo2 = finfo1;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo1);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		return False;
	}
	
	printf("Modified write time %s\n", 
	       nt_time_string(mem_ctx, finfo1.basic_info.out.write_time));


	printf("Doing a 10 byte write to extend the file and see if this changes the last write time.\n");

	written =  smbcli_write(cli->tree, fnum1, 0, "0123456789", 1, 10);

	if (written != 10) {
		printf("write failed - wrote %d bytes (%s)\n", 
		       (int)written, __location__);
		return False;
	}

	/* Just to prove to tridge that the an smbflush has no effect on
	   the write time :-). The setfileinfo IS STICKY. JRA. */

	printf("Doing flush after write\n");

	flsh.flush.in.file.fnum = fnum1;
	status = smb_raw_flush(cli->tree, &flsh);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("smbflush failed: %s\n", nt_errstr(status)));
		return False;
	}

	t = time(NULL);

	/* Once the time was set using setfileinfo then it stays set - writes
	   don't have any effect. But make sure. */

	while (time(NULL) < t+15) {
		status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo2);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
			ret = False;
			break;
		}
		printf("write time %s\n", 
		       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
		if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
			printf("Server updated write_time after %d seconds\n",
			       (int)(time(NULL) - t));
			break;
		}
		sleep(1);
		fflush(stdout);
	}
	
	if (finfo1.basic_info.out.write_time == finfo2.basic_info.out.write_time) {
		printf("Server did not update write time\n");
	}

	fnum2 = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	if (fnum2 == -1) {
		printf("Failed to open %s\n", fname);
		return False;
	}
	
	printf("Doing a 10 byte write to extend the file via second fd and see if this changes the last write time.\n");

	written =  smbcli_write(cli->tree, fnum2, 0, "0123456789", 11, 10);

	if (written != 10) {
		printf("write failed - wrote %d bytes (%s)\n", 
		       (int)written, __location__);
		return False;
	}

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo2);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		return False;
	}
	printf("write time %s\n", 
	       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
	if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
		printf("Server updated write_time\n");
	}

	printf("Closing the first fd to see if write time updated.\n");
	smbcli_close(cli->tree, fnum1);
	fnum1 = -1;

	printf("Doing a 10 byte write to extend the file via second fd and see if this changes the last write time.\n");

	written =  smbcli_write(cli->tree, fnum2, 0, "0123456789", 21, 10);

	if (written != 10) {
		printf("write failed - wrote %d bytes (%s)\n", 
		       (int)written, __location__);
		return False;
	}

	finfo1.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo1.basic_info.in.file.fnum = fnum2;
	finfo2 = finfo1;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo2);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		return False;
	}
	printf("write time %s\n", 
	       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
	if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
		printf("Server updated write_time\n");
	}

	t = time(NULL);

	/* Once the time was set using setfileinfo then it stays set - writes
	   don't have any effect. But make sure. */

	while (time(NULL) < t+15) {
		status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo2);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
			ret = False;
			break;
		}
		printf("write time %s\n", 
		       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
		if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
			printf("Server updated write_time after %d seconds\n",
			       (int)(time(NULL) - t));
			break;
		}
		sleep(1);
		fflush(stdout);
	}
	
	if (finfo1.basic_info.out.write_time == finfo2.basic_info.out.write_time) {
		printf("Server did not update write time\n");
	}

	printf("Closing both fd's to see if write time updated.\n");

	smbcli_close(cli->tree, fnum2);
	fnum2 = -1;

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	if (fnum1 == -1) {
		printf("Failed to open %s\n", fname);
		return False;
	}

	finfo1.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo1.basic_info.in.file.fnum = fnum1;
	finfo2 = finfo1;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo1);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		return False;
	}
	
	printf("Second open initial write time %s\n", 
	       nt_time_string(mem_ctx, finfo1.basic_info.out.write_time));

	sleep(10);
	printf("Doing a 10 byte write to extend the file to see if this changes the last write time.\n");

	written =  smbcli_write(cli->tree, fnum1, 0, "0123456789", 31, 10);

	if (written != 10) {
		printf("write failed - wrote %d bytes (%s)\n", 
		       (int)written, __location__);
		return False;
	}

	finfo1.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo1.basic_info.in.file.fnum = fnum1;
	finfo2 = finfo1;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo2);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		return False;
	}
	printf("write time %s\n", 
	       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
	if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
		printf("Server updated write_time\n");
	}

	t = time(NULL);

	/* Once the time was set using setfileinfo then it stays set - writes
	   don't have any effect. But make sure. */

	while (time(NULL) < t+15) {
		status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo2);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
			ret = False;
			break;
		}
		printf("write time %s\n", 
		       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
		if (finfo1.basic_info.out.write_time != finfo2.basic_info.out.write_time) {
			printf("Server updated write_time after %d seconds\n",
			       (int)(time(NULL) - t));
			break;
		}
		sleep(1);
		fflush(stdout);
	}
	
	if (finfo1.basic_info.out.write_time == finfo2.basic_info.out.write_time) {
		printf("Server did not update write time\n");
	}


	/* One more test to do. We should read the filetime via findfirst on the
	   second connection to ensure it's the same. This is very easy for a Windows
	   server but a bastard to get right on a POSIX server. JRA. */

	if (cli2 != NULL) {
		torture_close_connection(cli2);
	}
	if (fnum1 != -1)
		smbcli_close(cli->tree, fnum1);
	smbcli_unlink(cli->tree, fname);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/* Windows does obviously not update the stat info during a write call. I
 * *think* this is the problem causing a spurious Excel 2003 on XP error
 * message when saving a file. Excel does a setfileinfo, writes, and then does
 * a getpath(!)info. Or so... For Samba sometimes it displays an error message
 * that the file might have been changed in between. What i've been able to
 * trace down is that this happens if the getpathinfo after the write shows a
 * different last write time than the setfileinfo showed. This is really
 * nasty....
 */

static BOOL test_finfo_after_write(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_fileinfo finfo1, finfo2;
	const char *fname = BASEDIR "\\torture_file.txt";
	NTSTATUS status;
	int fnum1 = -1;
	int fnum2;
	BOOL ret = True;
	ssize_t written;
	struct smbcli_state *cli2=NULL;

	printf("Testing finfo update on close\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return False;
	}

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum1 == -1) {
		ret = False;
		goto done;
	}

	finfo1.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo1.basic_info.in.file.fnum = fnum1;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo1);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		ret = False;
		goto done;
	}

	msleep(1000);

	written =  smbcli_write(cli->tree, fnum1, 0, "x", 0, 1);

	if (written != 1) {
		printf("(%s) written gave %d - should have been 1\n", 
		       __location__, (int)written);
		ret = False;
		goto done;
	}

	if (!torture_open_connection(&cli2)) {
		return False;
	}

	fnum2 = smbcli_open(cli2->tree, fname, O_RDWR, DENY_NONE);
	if (fnum2 == -1) {
		printf("(%s) failed to open 2nd time - %s\n", 
		       __location__, smbcli_errstr(cli2->tree));
		ret = False;
		goto done;
	}
	
	written =  smbcli_write(cli2->tree, fnum2, 0, "x", 0, 1);
	
	if (written != 1) {
		printf("(%s) written gave %d - should have been 1\n", 
		       __location__, (int)written);
		ret = False;
		goto done;
	}
	
	finfo2.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo2.basic_info.in.file.path = fname;
	
	status = smb_raw_pathinfo(cli2->tree, mem_ctx, &finfo2);
	
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("(%s) fileinfo failed: %s\n", 
			  __location__, nt_errstr(status)));
		ret = False;
		goto done;
	}
	
	if (finfo1.basic_info.out.create_time !=
	    finfo2.basic_info.out.create_time) {
		printf("(%s) create_time changed\n", __location__);
		ret = False;
		goto done;
	}
	
	if (finfo1.basic_info.out.access_time !=
	    finfo2.basic_info.out.access_time) {
		printf("(%s) access_time changed\n", __location__);
		ret = False;
		goto done;
	}
	
	if (finfo1.basic_info.out.write_time !=
	    finfo2.basic_info.out.write_time) {
		printf("(%s) write_time changed\n", __location__);
		printf("write time conn 1 = %s, conn 2 = %s\n", 
		       nt_time_string(mem_ctx, finfo1.basic_info.out.write_time),
		       nt_time_string(mem_ctx, finfo2.basic_info.out.write_time));
		ret = False;
		goto done;
	}
	
	if (finfo1.basic_info.out.change_time !=
	    finfo2.basic_info.out.change_time) {
		printf("(%s) change_time changed\n", __location__);
		ret = False;
		goto done;
	}
	
	/* One of the two following calls updates the qpathinfo. */
	
	/* If you had skipped the smbcli_write on fnum2, it would
	 * *not* have updated the stat on disk */
	
	smbcli_close(cli2->tree, fnum2);
	torture_close_connection(cli2);
	cli2 = NULL;

	/* This call is only for the people looking at ethereal :-) */
	finfo2.basic_info.level = RAW_FILEINFO_BASIC_INFO;
	finfo2.basic_info.in.file.path = fname;

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo2);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("fileinfo failed: %s\n", nt_errstr(status)));
		ret = False;
		goto done;
	}

 done:
	if (fnum1 != -1)
		smbcli_close(cli->tree, fnum1);
	smbcli_unlink(cli->tree, fname);
	smbcli_deltree(cli->tree, BASEDIR);
	if (cli2 != NULL) {
		torture_close_connection(cli2);
	}

	return ret;
}


/* 
   testing of delayed update of write_time
*/
BOOL torture_delay_write(struct torture_context *torture)
{
	struct smbcli_state *cli;
	BOOL ret = True;
	TALLOC_CTX *mem_ctx;

	if (!torture_open_connection(&cli)) {
		return False;
	}

	mem_ctx = talloc_init("torture_delay_write");

	ret &= test_finfo_after_write(cli, mem_ctx);
	ret &= test_delayed_write_update(cli, mem_ctx);
	ret &= test_delayed_write_update2(cli, mem_ctx);

	torture_close_connection(cli);
	talloc_free(mem_ctx);
	return ret;
}
