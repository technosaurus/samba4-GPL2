/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - open and close

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
  find open file handle given fnum
*/
struct pvfs_file *pvfs_find_fd(struct pvfs_state *pvfs,
			       struct smbsrv_request *req, uint16_t fnum)
{
	struct pvfs_file *f;

	f = idr_find(pvfs->idtree_fnum, fnum);
	if (f == NULL) {
		return NULL;
	}

	if (req->session != f->session) {
		DEBUG(2,("pvfs_find_fd: attempt to use wrong session for fnum %d\n", 
			 fnum));
		return NULL;
	}

	return f;
}


/*
  cleanup a open directory handle
*/
static int pvfs_dir_fd_destructor(void *p)
{
	struct pvfs_file *f = p;
	DLIST_REMOVE(f->pvfs->open_files, f);
	idr_remove(f->pvfs->idtree_fnum, f->fnum);
	return 0;
}


/*
  open a directory
*/
static NTSTATUS pvfs_open_directory(struct pvfs_state *pvfs, 
				    struct smbsrv_request *req, 
				    struct pvfs_filename *name, 
				    union smb_open *io)
{
	struct pvfs_file *f;
	int fnum;
	NTSTATUS status;
	uint32_t create_action;

	/* if the client says it must be a directory, and it isn't,
	   then fail */
	if (name->exists && !(name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY)) {
		return NT_STATUS_NOT_A_DIRECTORY;
	}

	switch (io->generic.in.open_disposition) {
	case NTCREATEX_DISP_OPEN_IF:
		break;

	case NTCREATEX_DISP_OPEN:
		if (!name->exists) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		break;

	case NTCREATEX_DISP_CREATE:
		if (name->exists) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
		break;

	case NTCREATEX_DISP_OVERWRITE_IF:
	case NTCREATEX_DISP_OVERWRITE:
	case NTCREATEX_DISP_SUPERSEDE:
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	f = talloc_p(req, struct pvfs_file);
	if (f == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	f->fnum = fnum;
	f->fd = -1;
	f->name = talloc_steal(f, name);
	f->session = req->session;
	f->smbpid = req->smbpid;
	f->pvfs = pvfs;
	f->pending_list = NULL;
	f->lock_count = 0;
	f->locking_key = data_blob(NULL, 0);
	f->create_options = io->generic.in.create_options;
	f->share_access = io->generic.in.share_access;

	fnum = idr_get_new(pvfs->idtree_fnum, f, UINT16_MAX);
	if (fnum == -1) {
		talloc_free(f);
		return NT_STATUS_TOO_MANY_OPENED_FILES;
	}

	DLIST_ADD(pvfs->open_files, f);

	/* TODO: should we check in the opendb? Do directory opens 
	   follow the share_access rules? */


	/* setup a destructor to avoid leaks on abnormal termination */
	talloc_set_destructor(f, pvfs_dir_fd_destructor);

	if (!name->exists) {
		if (mkdir(name->full_name, 0755) == -1) {
			return pvfs_map_errno(pvfs,errno);
		}
		status = pvfs_resolve_name(pvfs, req, io->ntcreatex.in.fname,
					   PVFS_RESOLVE_NO_WILDCARD, &name);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		create_action = NTCREATEX_ACTION_CREATED;
	} else {
		create_action = NTCREATEX_ACTION_EXISTED;
	}

	if (!name->exists) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* the open succeeded, keep this handle permanently */
	talloc_steal(pvfs, f);

	io->generic.out.oplock_level  = NO_OPLOCK;
	io->generic.out.fnum          = f->fnum;
	io->generic.out.create_action = create_action;
	io->generic.out.create_time   = name->dos.create_time;
	io->generic.out.access_time   = name->dos.access_time;
	io->generic.out.write_time    = name->dos.write_time;
	io->generic.out.change_time   = name->dos.change_time;
	io->generic.out.attrib        = name->dos.attrib;
	io->generic.out.alloc_size    = 0;
	io->generic.out.size          = 0;
	io->generic.out.file_type     = FILE_TYPE_DISK;
	io->generic.out.ipc_state     = 0;
	io->generic.out.is_directory  = 1;

	return NT_STATUS_OK;
}


/*
  by using a destructor we make sure that abnormal cleanup will not 
  leak file descriptors (assuming at least the top level pointer is freed, which
  will cascade down to here)
*/
static int pvfs_fd_destructor(void *p)
{
	struct pvfs_file *f = p;
	struct odb_lock *lck;
	NTSTATUS status;

	DLIST_REMOVE(f->pvfs->open_files, f);

	pvfs_lock_close(f->pvfs, f);

	if (f->fd != -1) {
		close(f->fd);
		f->fd = -1;
	}

	idr_remove(f->pvfs->idtree_fnum, f->fnum);

	lck = odb_lock(f, f->pvfs->odb_context, &f->locking_key);
	if (lck == NULL) {
		DEBUG(0,("Unabled to lock opendb for close\n"));
		return 0;
	}

	status = odb_close_file(lck, f->fnum);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unabled to remove opendb entry for '%s' - %s\n", 
			 f->name->full_name, nt_errstr(status)));
	}

	return 0;
}


/*
  form the lock context used for byte range locking and opendb
  locking. Note that we must zero here to take account of
  possible padding on some architectures
*/
static NTSTATUS pvfs_locking_key(struct pvfs_filename *name, 
				 TALLOC_CTX *mem_ctx, DATA_BLOB *key)
{
	struct {
		dev_t device;
		ino_t inode;
	} lock_context;
	ZERO_STRUCT(lock_context);

	lock_context.device = name->st.st_dev;
	lock_context.inode = name->st.st_ino;

	*key = data_blob_talloc(mem_ctx, &lock_context, sizeof(lock_context));
	if (key->data == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	
	return NT_STATUS_OK;
}


/*
  create a new file
*/
static NTSTATUS pvfs_create_file(struct pvfs_state *pvfs, 
				 struct smbsrv_request *req, 
				 struct pvfs_filename *name, 
				 union smb_open *io)
{
	struct pvfs_file *f;
	NTSTATUS status;
	int flags, fnum, fd;
	struct odb_lock *lck;
	uint32_t create_options = io->generic.in.create_options;
	uint32_t share_access = io->generic.in.share_access;
	uint32_t access_mask = io->generic.in.access_mask;
	
	flags = O_RDWR;

	f = talloc_p(req, struct pvfs_file);
	if (f == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	fnum = idr_get_new(pvfs->idtree_fnum, f, UINT16_MAX);
	if (fnum == -1) {
		return NT_STATUS_TOO_MANY_OPENED_FILES;
	}

	/* create the file */
	fd = open(name->full_name, flags | O_CREAT | O_EXCL, 0644);
	if (fd == -1) {
		idr_remove(pvfs->idtree_fnum, fnum);
		return pvfs_map_errno(pvfs, errno);
	}

	/* re-resolve the open fd */
	status = pvfs_resolve_name_fd(pvfs, fd, name);
	if (!NT_STATUS_IS_OK(status)) {
		idr_remove(pvfs->idtree_fnum, fnum);
		close(fd);
		return status;
	}

	/* form the lock context used for byte range locking and
	   opendb locking */
	status = pvfs_locking_key(name, f, &f->locking_key);
	if (!NT_STATUS_IS_OK(status)) {
		idr_remove(pvfs->idtree_fnum, fnum);
		close(fd);
		return status;
	}

	/* grab a lock on the open file record */
	lck = odb_lock(req, pvfs->odb_context, &f->locking_key);
	if (lck == NULL) {
		DEBUG(0,("pvfs_open: failed to lock file '%s' in opendb\n",
			 name->full_name));
		/* we were supposed to do a blocking lock, so something
		   is badly wrong! */
		idr_remove(pvfs->idtree_fnum, fnum);
		close(fd);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = odb_open_file(lck, fnum, share_access, create_options, access_mask);
	if (!NT_STATUS_IS_OK(status)) {
		/* bad news, we must have hit a race */
		idr_remove(pvfs->idtree_fnum, fnum);
		close(fd);
		return status;
	}

	f->fnum = fnum;
	f->fd = fd;
	f->name = talloc_steal(f, name);
	f->session = req->session;
	f->smbpid = req->smbpid;
	f->pvfs = pvfs;
	f->pending_list = NULL;
	f->lock_count = 0;
	f->create_options = io->generic.in.create_options;
	f->share_access = io->generic.in.share_access;
	f->access_mask = io->generic.in.access_mask;

	DLIST_ADD(pvfs->open_files, f);

	/* setup a destructor to avoid file descriptor leaks on
	   abnormal termination */
	talloc_set_destructor(f, pvfs_fd_destructor);

	io->generic.out.oplock_level  = NO_OPLOCK;
	io->generic.out.fnum          = f->fnum;
	io->generic.out.create_action = NTCREATEX_ACTION_CREATED;
	io->generic.out.create_time   = name->dos.create_time;
	io->generic.out.access_time   = name->dos.access_time;
	io->generic.out.write_time    = name->dos.write_time;
	io->generic.out.change_time   = name->dos.change_time;
	io->generic.out.attrib        = name->dos.attrib;
	io->generic.out.alloc_size    = name->dos.alloc_size;
	io->generic.out.size          = name->st.st_size;
	io->generic.out.file_type     = FILE_TYPE_DISK;
	io->generic.out.ipc_state     = 0;
	io->generic.out.is_directory  = 0;

	/* success - keep the file handle */
	talloc_steal(pvfs, f);

	return NT_STATUS_OK;
}


/*
  open a file
*/
NTSTATUS pvfs_open(struct ntvfs_module_context *ntvfs,
		   struct smbsrv_request *req, union smb_open *io)
{
	struct pvfs_state *pvfs = ntvfs->private_data;
	int fd, flags;
	struct pvfs_filename *name;
	struct pvfs_file *f;
	NTSTATUS status;
	int fnum;
	struct odb_lock *lck;
	uint32_t create_options;
	uint32_t share_access;
	uint32_t access_mask;

	/* use the generic mapping code to avoid implementing all the
	   different open calls. This won't allow openx to work
	   perfectly as the mapping code has no way of knowing if two
	   opens are on the same connection, so this will need to
	   change eventually */	   
	if (io->generic.level != RAW_OPEN_GENERIC) {
		return ntvfs_map_open(req, io, ntvfs);
	}

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, io->ntcreatex.in.fname,
				   PVFS_RESOLVE_NO_WILDCARD, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* directory opens are handled separately */
	if ((name->exists && (name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY)) ||
	    (io->generic.in.create_options & NTCREATEX_OPTIONS_DIRECTORY)) {
		return pvfs_open_directory(pvfs, req, name, io);
	}


	switch (io->generic.in.open_disposition) {
	case NTCREATEX_DISP_SUPERSEDE:
		if (!name->exists) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		flags = O_TRUNC;
		break;

	case NTCREATEX_DISP_OVERWRITE_IF:
		flags = O_TRUNC;
		break;

	case NTCREATEX_DISP_OPEN:
		if (!name->exists) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		flags = 0;
		break;

	case NTCREATEX_DISP_OVERWRITE:
		if (!name->exists) {
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}
		flags = O_TRUNC;
		break;

	case NTCREATEX_DISP_CREATE:
		if (name->exists) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
		flags = 0;
		break;

	case NTCREATEX_DISP_OPEN_IF:
		flags = 0;
		break;

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	flags |= O_RDWR;

	/* handle creating a new file separately */
	if (!name->exists) {
		status = pvfs_create_file(pvfs, req, name, io);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_COLLISION)) {
			return status;
		}

		/* we've hit a race - the file was created during this call */
		if (io->generic.in.open_disposition == NTCREATEX_DISP_CREATE) {
			return status;
		}

		/* try re-resolving the name */
		status = pvfs_resolve_name(pvfs, req, io->ntcreatex.in.fname,
					   PVFS_RESOLVE_NO_WILDCARD, &name);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		/* fall through to a normal open */
	}

	f = talloc_p(req, struct pvfs_file);
	if (f == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* allocate a fnum */
	fnum = idr_get_new(pvfs->idtree_fnum, f, UINT16_MAX);
	if (fnum == -1) {
		return NT_STATUS_TOO_MANY_OPENED_FILES;
	}

	/* form the lock context used for byte range locking and
	   opendb locking */
	status = pvfs_locking_key(name, f, &f->locking_key);
	if (!NT_STATUS_IS_OK(status)) {
		idr_remove(pvfs->idtree_fnum, fnum);
		return status;
	}

	/* get a lock on this file before the actual open */
	lck = odb_lock(req, pvfs->odb_context, &f->locking_key);
	if (lck == NULL) {
		DEBUG(0,("pvfs_open: failed to lock file '%s' in opendb\n",
			 name->full_name));
		/* we were supposed to do a blocking lock, so something
		   is badly wrong! */
		idr_remove(pvfs->idtree_fnum, fnum);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	create_options = io->generic.in.create_options;
	share_access   = io->generic.in.share_access;
	access_mask    = io->generic.in.access_mask;

	/* see if we are allowed to open at the same time as existing opens */
	status = odb_open_file(lck, fnum, share_access, create_options, access_mask);
	if (!NT_STATUS_IS_OK(status)) {
		idr_remove(pvfs->idtree_fnum, fnum);
		return status;
	}

	/* do the actual open */
	fd = open(name->full_name, flags);
	if (fd == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	/* re-resolve the open fd */
	status = pvfs_resolve_name_fd(pvfs, fd, name);
	if (!NT_STATUS_IS_OK(status)) {
		close(fd);
		idr_remove(pvfs->idtree_fnum, fnum);
		return status;
	}

	f->fnum = fnum;
	f->fd = fd;
	f->name = talloc_steal(f, name);
	f->session = req->session;
	f->smbpid = req->smbpid;
	f->pvfs = pvfs;
	f->pending_list = NULL;
	f->lock_count = 0;
	f->create_options = io->generic.in.create_options;
	f->share_access = io->generic.in.share_access;
	f->access_mask = io->generic.in.access_mask;

	DLIST_ADD(pvfs->open_files, f);

	/* setup a destructor to avoid file descriptor leaks on
	   abnormal termination */
	talloc_set_destructor(f, pvfs_fd_destructor);

	io->generic.out.oplock_level  = NO_OPLOCK;
	io->generic.out.fnum          = f->fnum;
	io->generic.out.create_action = NTCREATEX_ACTION_EXISTED;
	io->generic.out.create_time   = name->dos.create_time;
	io->generic.out.access_time   = name->dos.access_time;
	io->generic.out.write_time    = name->dos.write_time;
	io->generic.out.change_time   = name->dos.change_time;
	io->generic.out.attrib        = name->dos.attrib;
	io->generic.out.alloc_size    = name->dos.alloc_size;
	io->generic.out.size          = name->st.st_size;
	io->generic.out.file_type     = FILE_TYPE_DISK;
	io->generic.out.ipc_state     = 0;
	io->generic.out.is_directory  = 0;

	/* success - keep the file handle */
	talloc_steal(pvfs, f);

	return NT_STATUS_OK;
}


/*
  close a file
*/
NTSTATUS pvfs_close(struct ntvfs_module_context *ntvfs,
		    struct smbsrv_request *req, union smb_close *io)
{
	struct pvfs_state *pvfs = ntvfs->private_data;
	struct pvfs_file *f;
	NTSTATUS status;

	if (io->generic.level != RAW_CLOSE_CLOSE) {
		return ntvfs_map_close(req, io, ntvfs);
	}

	f = pvfs_find_fd(pvfs, req, io->close.in.fnum);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (f->fd != -1 && 
	    close(f->fd) == -1) {
		status = pvfs_map_errno(pvfs, errno);
	} else {
		status = NT_STATUS_OK;
	}
	f->fd = -1;

	/* the destructor takes care of the rest */
	talloc_free(f);

	return status;
}


/*
  logoff - close all file descriptors open by a vuid
*/
NTSTATUS pvfs_logoff(struct ntvfs_module_context *ntvfs,
		     struct smbsrv_request *req)
{
	struct pvfs_state *pvfs = ntvfs->private_data;
	struct pvfs_file *f, *next;

	for (f=pvfs->open_files;f;f=next) {
		next = f->next;
		if (f->session == req->session) {
			DLIST_REMOVE(pvfs->open_files, f);
			talloc_free(f);
		}
	}

	return NT_STATUS_OK;
}


/*
  exit - close files for the current pid
*/
NTSTATUS pvfs_exit(struct ntvfs_module_context *ntvfs,
		   struct smbsrv_request *req)
{
	struct pvfs_state *pvfs = ntvfs->private_data;
	struct pvfs_file *f, *next;

	for (f=pvfs->open_files;f;f=next) {
		next = f->next;
		if (f->smbpid == req->smbpid) {
			DLIST_REMOVE(pvfs->open_files, f);
			talloc_free(f);
		}
	}

	return NT_STATUS_OK;
}
