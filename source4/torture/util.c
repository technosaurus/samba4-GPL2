/* 
   Unix SMB/CIFS implementation.
   SMB torture tester utility functions
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
#include "system/filesys.h"
#include "system/wait.h"
#include "torture/torture.h"

/**
 create a temporary directory.
*/
_PUBLIC_ NTSTATUS torture_temp_dir(TALLOC_CTX *mem_ctx, char **tempdir)
{
	*tempdir = talloc_strdup(mem_ctx, "torture-tmp.XXXXXX");

	if (mkdtemp(*tempdir) == NULL)
		return NT_STATUS_UNSUCCESSFUL;

	return NT_STATUS_OK;
}

/**
  check if 2 NTTIMEs are equal.
*/
BOOL nt_time_equal(NTTIME *t1, NTTIME *t2)
{
	return *t1 == *t2;
}

/**
 * Provision a Samba installation using @param setupdir_script and start smbd.
 */
NTSTATUS torture_setup_server(TALLOC_CTX *mem_ctx, 
							  const char *setupdir_script,
							  const char *smbd_path,
							  pid_t *smbd_pid)
{
	char *tempdir;
	NTSTATUS status;
	pid_t pid;
	int child_status;
	char *configfile, *configparam;
	pid_t closed_pid;

	*smbd_pid = -1;

	status = torture_temp_dir(mem_ctx, &tempdir);
	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	if ((pid = fork()) == 0) {
		execl(setupdir_script, setupdir_script, tempdir, NULL);
		exit(1);
	} else if (pid == -1) {
		DEBUG(0, ("Unable to fork()\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	closed_pid = waitpid(pid, &child_status, 0);

	if (closed_pid == -1) {
		DEBUG(1, ("Error waiting for child"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	SMB_ASSERT(closed_pid == pid);

	if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
		DEBUG(1, ("Invalid return code from setup script %s: %d\n", 
				  setupdir_script,
				  WEXITSTATUS(child_status)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	configfile = talloc_asprintf(mem_ctx, "%s/etc/smb.conf", 
								 tempdir);
	if (!file_exist(configfile)) {
		DEBUG(1, ("Setup script didn't create %s\n", configfile));
		return NT_STATUS_UNSUCCESSFUL;
	}

	configparam = talloc_asprintf(mem_ctx, "--configfile=%s", configfile);
	talloc_free(configfile);

	if ((pid = fork()) == 0) {
		execl(smbd_path, smbd_path, "-i", "--model=single", 
						configparam, NULL);
		exit(1);
	} else if (pid == -1) {
		DEBUG(0, ("Unable to fork()\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	*smbd_pid = pid;

	return NT_STATUS_OK;
}
