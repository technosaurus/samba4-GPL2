/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - read

   Copyright (C) Andrew Tridgell 2004

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

#include "include/includes.h"
#include "vfs_posix.h"

/*
  read from a file
*/
NTSTATUS pvfs_read(struct ntvfs_module_context *ntvfs,
		   struct smbsrv_request *req, union smb_read *rd)
{
	struct pvfs_state *pvfs = ntvfs->private_data;
	ssize_t ret;
	struct pvfs_file *f;
	NTSTATUS status;

	if (rd->generic.level != RAW_READ_READX) {
		return NT_STATUS_NOT_SUPPORTED;
	}


	f = pvfs_find_fd(pvfs, req, rd->readx.in.fnum);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	status = pvfs_check_lock(pvfs, f, req->smbpid, 
				 rd->readx.in.offset,
				 rd->readx.in.maxcnt,
				 READ_LOCK);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ret = pread(f->fd, 
		    rd->readx.out.data, 
		    rd->readx.in.maxcnt,
		    rd->readx.in.offset);
	if (ret == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	rd->readx.out.nread = ret;
	rd->readx.out.remaining = 0; /* should fill this in? */
	rd->readx.out.compaction_mode = 0; 

	return NT_STATUS_OK;
}
