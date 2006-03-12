/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - seek

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

#include "includes.h"
#include "vfs_posix.h"

/*
  seek in a file
*/
NTSTATUS pvfs_seek(struct ntvfs_module_context *ntvfs,
		   struct ntvfs_request *req,
		   union smb_seek *io)
{
	struct pvfs_state *pvfs = ntvfs->private_data;
	struct pvfs_file *f;
	struct pvfs_file_handle *h;
	NTSTATUS status;

	f = pvfs_find_fd(pvfs, req, io->lseek.in.file.fnum);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}
	h = f->handle;

	status = NT_STATUS_OK;

	switch (io->lseek.in.mode) {
	case SEEK_MODE_START:
		h->seek_offset = io->lseek.in.offset;
		break;

	case SEEK_MODE_CURRENT:
		h->seek_offset += io->lseek.in.offset;
		break;

	case SEEK_MODE_END:
		status = pvfs_resolve_name_fd(pvfs, h->fd, h->name);
		h->seek_offset = h->name->st.st_size + io->lseek.in.offset;
		break;
	}

	io->lseek.out.offset = h->seek_offset;

	return status;
}

