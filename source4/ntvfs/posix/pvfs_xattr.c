/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - xattr support

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
#include "system/filesys.h"
#include "vfs_posix.h"
#include "librpc/gen_ndr/ndr_xattr.h"

/*
  pull a xattr as a blob, from either a file or a file descriptor
*/
static NTSTATUS pull_xattr_blob(TALLOC_CTX *mem_ctx,
				const char *attr_name, 
				const char *fname, 
				int fd, 
				size_t estimated_size,
				DATA_BLOB *blob)
{
#if HAVE_XATTR_SUPPORT
	int ret;

	*blob = data_blob_talloc(mem_ctx, NULL, estimated_size);
	if (blob->data == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

again:
	if (fd != -1) {
		ret = fgetxattr(fd, attr_name, blob->data, estimated_size);
	} else {
		ret = getxattr(fname, attr_name, blob->data, estimated_size);
	}
	if (ret == -1 && errno == ERANGE) {
		estimated_size *= 2;
		blob->data = talloc_realloc(mem_ctx, blob->data, estimated_size);
		if (blob->data == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		blob->length = estimated_size;
		goto again;
	}

	if (ret == -1) {
		data_blob_free(blob);
		return map_nt_error_from_unix(errno);
	}

	blob->length = ret;

	return NT_STATUS_OK;
#else
	return NT_STATUS_NOT_SUPPORTED;
#endif
}

/*
  push a xattr as a blob, from either a file or a file descriptor
*/
static NTSTATUS push_xattr_blob(const char *attr_name, 
				const char *fname, 
				int fd, 
				const DATA_BLOB *blob)
{
#if HAVE_XATTR_SUPPORT
	int ret;

	if (fd != -1) {
		ret = fsetxattr(fd, attr_name, blob->data, blob->length, 0);
	} else {
		ret = setxattr(fname, attr_name, blob->data, blob->length, 0);
	}
	if (ret == -1) {
		return map_nt_error_from_unix(errno);
	}

	return NT_STATUS_OK;
#else
	return NT_STATUS_NOT_SUPPORTED;
#endif
}

/*
  load a NDR structure from a xattr
*/
static NTSTATUS pvfs_xattr_ndr_load(TALLOC_CTX *mem_ctx,
				    const char *fname, int fd, const char *attr_name,
				    void *p, ndr_pull_flags_fn_t pull_fn)
{
	NTSTATUS status;
	DATA_BLOB blob;

	status = pull_xattr_blob(mem_ctx, attr_name, fname, 
				 fd, XATTR_DOSATTRIB_ESTIMATED_SIZE, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* pull the blob */
	status = ndr_pull_struct_blob(&blob, mem_ctx, p, pull_fn);

	data_blob_free(&blob);

	return status;
}

/*
  save a NDR structure into a xattr
*/
static NTSTATUS pvfs_xattr_ndr_save(const char *fname, int fd, const char *attr_name, 
				    void *p, ndr_push_flags_fn_t push_fn)
{
	TALLOC_CTX *mem_ctx = talloc(NULL, 0);
	DATA_BLOB blob;
	NTSTATUS status;

	status = ndr_push_struct_blob(&blob, mem_ctx, p, (ndr_push_flags_fn_t)push_fn);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return status;
	}

	status = push_xattr_blob(attr_name, fname, fd, &blob);
	talloc_free(mem_ctx);

	return status;
}


/*
  fill in file attributes from extended attributes
*/
NTSTATUS pvfs_dosattrib_load(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd)
{
	NTSTATUS status;
	struct xattr_DosAttrib attrib;
	TALLOC_CTX *mem_ctx = talloc(name, 0);
	struct xattr_DosInfo1 *info1;

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}

	status = pvfs_xattr_ndr_load(mem_ctx, name->full_name, fd, XATTR_DOSATTRIB_NAME,
				     &attrib, (ndr_pull_flags_fn_t)ndr_pull_xattr_DosAttrib);

	/* if the filesystem doesn't support them, then tell pvfs not to try again */
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
		DEBUG(5,("pvfs_xattr: xattr not supported in filesystem\n"));
		pvfs->flags &= ~PVFS_FLAG_XATTR_ENABLE;
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}

	/* not having a DosAttrib is not an error */
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return status;
	}

	switch (attrib.version) {
	case 1:
		info1 = &attrib.info.info1;
		name->dos.attrib = info1->attrib;
		name->dos.ea_size = info1->ea_size;
		if (name->st.st_size == info1->size) {
			name->dos.alloc_size = info1->alloc_size;
		}
		if (info1->create_time != 0) {
			name->dos.create_time = info1->create_time;
		}
		if (info1->change_time != 0) {
			name->dos.change_time = info1->change_time;
		}
		break;

	default:
		DEBUG(0,("ERROR: Unsupported xattr DosAttrib version %d on '%s'\n",
			 attrib.version, name->full_name));
		talloc_free(mem_ctx);
		return NT_STATUS_INVALID_LEVEL;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}


/*
  save the file attribute into the xattr
*/
NTSTATUS pvfs_dosattrib_save(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd)
{
	struct xattr_DosAttrib attrib;
	struct xattr_DosInfo1 *info1;

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}

	attrib.version = 1;
	info1 = &attrib.info.info1;

	info1->attrib      = name->dos.attrib;
	info1->ea_size     = name->dos.ea_size;
	info1->size        = name->st.st_size;
	info1->alloc_size  = name->dos.alloc_size;
	info1->create_time = name->dos.create_time;
	info1->change_time = name->dos.change_time;

	return pvfs_xattr_ndr_save(name->full_name, fd, XATTR_DOSATTRIB_NAME, &attrib, 
				   (ndr_push_flags_fn_t)ndr_push_xattr_DosAttrib);
}


/*
  load the set of DOS EAs
*/
NTSTATUS pvfs_doseas_load(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
			  struct xattr_DosEAs *eas)
{
	NTSTATUS status;
	ZERO_STRUCTP(eas);
	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}
	status = pvfs_xattr_ndr_load(eas, name->full_name, fd, XATTR_DOSEAS_NAME,
				     eas, (ndr_pull_flags_fn_t)ndr_pull_xattr_DosEAs);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return NT_STATUS_OK;
	}
	return status;
}

/*
  save the set of DOS EAs
*/
NTSTATUS pvfs_doseas_save(struct pvfs_state *pvfs, struct pvfs_filename *name, int fd,
			  struct xattr_DosEAs *eas)
{
	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_OK;
	}
	return pvfs_xattr_ndr_save(name->full_name, fd, XATTR_DOSEAS_NAME, eas, 
				   (ndr_push_flags_fn_t)ndr_push_xattr_DosEAs);
}
