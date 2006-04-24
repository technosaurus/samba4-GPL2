/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend

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
/*
  this implements most of the POSIX NTVFS backend
  This is the default backend
*/

#include "includes.h"
#include "vfs_posix.h"
#include "librpc/gen_ndr/security.h"
#include "lib/tdb/include/tdb.h"
#include "db_wrap.h"
#include "libcli/security/security.h"
#include "lib/events/events.h"


/*
  setup config options for a posix share
*/
static void pvfs_setup_options(struct pvfs_state *pvfs)
{
	int snum = pvfs->ntvfs->ctx->config.snum;
	const char *eadb;

	if (lp_map_hidden(snum))     pvfs->flags |= PVFS_FLAG_MAP_HIDDEN;
	if (lp_map_archive(snum))    pvfs->flags |= PVFS_FLAG_MAP_ARCHIVE;
	if (lp_map_system(snum))     pvfs->flags |= PVFS_FLAG_MAP_SYSTEM;
	if (lp_readonly(snum))       pvfs->flags |= PVFS_FLAG_READONLY;
	if (lp_strict_sync(snum))    pvfs->flags |= PVFS_FLAG_STRICT_SYNC;
	if (lp_strict_locking(snum)) pvfs->flags |= PVFS_FLAG_STRICT_LOCKING;
	if (lp_ci_filesystem(snum))  pvfs->flags |= PVFS_FLAG_CI_FILESYSTEM;

	if (lp_parm_bool(snum, "posix", "fakeoplocks", False)) {
		pvfs->flags |= PVFS_FLAG_FAKE_OPLOCKS;
	}

	/* this must be a power of 2 */
	pvfs->alloc_size_rounding = lp_parm_int(snum, 
						"posix", "allocationrounding", 512);

	pvfs->search.inactivity_time = lp_parm_int(snum, 
						   "posix", "searchinactivity", 300);

#if HAVE_XATTR_SUPPORT
	if (lp_parm_bool(snum, "posix", "xattr", True)) pvfs->flags |= PVFS_FLAG_XATTR_ENABLE;
#endif

	pvfs->sharing_violation_delay = lp_parm_int(snum, "posix", "sharedelay", 1000000);

	pvfs->share_name = talloc_strdup(pvfs, lp_servicename(snum));

	pvfs->fs_attribs = 
		FS_ATTR_CASE_SENSITIVE_SEARCH | 
		FS_ATTR_CASE_PRESERVED_NAMES |
		FS_ATTR_UNICODE_ON_DISK |
		FS_ATTR_SPARSE_FILES;

	/* allow xattrs to be stored in a external tdb */
	eadb = lp_parm_string(snum, "posix", "eadb");
	if (eadb != NULL) {
		pvfs->ea_db = tdb_wrap_open(pvfs, eadb, 50000,  
					    TDB_DEFAULT, O_RDWR|O_CREAT, 0600);
		if (pvfs->ea_db != NULL) {
			pvfs->flags |= PVFS_FLAG_XATTR_ENABLE;
		} else {
			DEBUG(0,("Failed to open eadb '%s' - %s\n",
				 eadb, strerror(errno)));
			pvfs->flags &= ~PVFS_FLAG_XATTR_ENABLE;
		}
	}

	if (pvfs->flags & PVFS_FLAG_XATTR_ENABLE) {
		pvfs->fs_attribs |= FS_ATTR_NAMED_STREAMS;
	}
	if (pvfs->flags & PVFS_FLAG_XATTR_ENABLE) {
		pvfs->fs_attribs |= FS_ATTR_PERSISTANT_ACLS;
	}

	pvfs->sid_cache.creator_owner = dom_sid_parse_talloc(pvfs, SID_CREATOR_OWNER);
	pvfs->sid_cache.creator_group = dom_sid_parse_talloc(pvfs, SID_CREATOR_GROUP);

	/* check if the system really supports xattrs */
	if (pvfs->flags & PVFS_FLAG_XATTR_ENABLE) {
		pvfs_xattr_probe(pvfs);
	}
}

static int pvfs_state_destructor(void *ptr)
{
	struct pvfs_state *pvfs = talloc_get_type(ptr, struct pvfs_state);
	struct pvfs_file *f, *fn;
	struct pvfs_search_state *s, *sn;

	/* 
	 * make sure we cleanup files and searches before anything else
	 * because there destructors need to acess the pvfs_state struct
	 */
	for (f=pvfs->files.list; f; f=fn) {
		fn = f->next;
		talloc_free(f);
	}

	for (s=pvfs->search.list; s; s=sn) {
		sn = s->next;
		talloc_free(s);
	}

	return 0;
}

/*
  connect to a share - used when a tree_connect operation comes
  in. For a disk based backend we needs to ensure that the base
  directory exists (tho it doesn't need to be accessible by the user,
  that comes later)
*/
static NTSTATUS pvfs_connect(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, const char *sharename)
{
	struct pvfs_state *pvfs;
	struct stat st;
	char *base_directory;
	NTSTATUS status;

	pvfs = talloc_zero(ntvfs, struct pvfs_state);
	NT_STATUS_HAVE_NO_MEMORY(pvfs);

	/* for simplicity of path construction, remove any trailing slash now */
	base_directory = talloc_strdup(pvfs, lp_pathname(ntvfs->ctx->config.snum));
	NT_STATUS_HAVE_NO_MEMORY(base_directory);
	if (strcmp(base_directory, "/") != 0) {
		trim_string(base_directory, NULL, "/");
	}

	pvfs->ntvfs = ntvfs;
	pvfs->base_directory = base_directory;

	/* the directory must exist. Note that we deliberately don't
	   check that it is readable */
	if (stat(pvfs->base_directory, &st) != 0 || !S_ISDIR(st.st_mode)) {
		DEBUG(0,("pvfs_connect: '%s' is not a directory, when connecting to [%s]\n", 
			 pvfs->base_directory, sharename));
		return NT_STATUS_BAD_NETWORK_NAME;
	}

	ntvfs->ctx->fs_type = talloc_strdup(ntvfs->ctx, "NTFS");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->fs_type);

	ntvfs->ctx->dev_type = talloc_strdup(ntvfs->ctx, "A:");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->dev_type);

	ntvfs->private_data = pvfs;

	pvfs->brl_context = brl_init(pvfs, 
				     pvfs->ntvfs->ctx->server_id,  
				     pvfs->ntvfs->ctx->config.snum,
				     pvfs->ntvfs->ctx->msg_ctx);
	if (pvfs->brl_context == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	pvfs->odb_context = odb_init(pvfs, pvfs->ntvfs->ctx);
	if (pvfs->odb_context == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* allow this to be NULL - we just disable change notify */
	pvfs->notify_context = notify_init(pvfs, 
					   pvfs->ntvfs->ctx->server_id,  
					   pvfs->ntvfs->ctx->msg_ctx, 
					   event_context_find(pvfs),
					   pvfs->ntvfs->ctx->config.snum);

	pvfs->sidmap = sidmap_open(pvfs);
	if (pvfs->sidmap == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* allocate the fnum id -> ptr tree */
	pvfs->files.idtree = idr_init(pvfs);
	NT_STATUS_HAVE_NO_MEMORY(pvfs->files.idtree);

	/* allocate the search handle -> ptr tree */
	pvfs->search.idtree = idr_init(pvfs);
	NT_STATUS_HAVE_NO_MEMORY(pvfs->search.idtree);

	status = pvfs_mangle_init(pvfs);
	NT_STATUS_NOT_OK_RETURN(status);

	pvfs_setup_options(pvfs);

	talloc_set_destructor(pvfs, pvfs_state_destructor);

#ifdef SIGXFSZ
	/* who had the stupid idea to generate a signal on a large
	   file write instead of just failing it!? */
	BlockSignals(True, SIGXFSZ);
#endif

	return NT_STATUS_OK;
}

/*
  disconnect from a share
*/
static NTSTATUS pvfs_disconnect(struct ntvfs_module_context *ntvfs)
{
	return NT_STATUS_OK;
}

/*
  check if a directory exists
*/
static NTSTATUS pvfs_chkpath(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req,
			     union smb_chkpath *cp)
{
	struct pvfs_state *pvfs = ntvfs->private_data;
	struct pvfs_filename *name;
	NTSTATUS status;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, cp->chkpath.in.path, 0, &name);
	NT_STATUS_NOT_OK_RETURN(status);

	if (!name->exists) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (!S_ISDIR(name->st.st_mode)) {
		return NT_STATUS_NOT_A_DIRECTORY;
	}

	return NT_STATUS_OK;
}

/*
  copy a set of files
*/
static NTSTATUS pvfs_copy(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, struct smb_copy *cp)
{
	DEBUG(0,("pvfs_copy not implemented\n"));
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  return print queue info
*/
static NTSTATUS pvfs_lpq(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req, union smb_lpq *lpq)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/* SMBtrans - not used on file shares */
static NTSTATUS pvfs_trans(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, struct smb_trans2 *trans2)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  initialialise the POSIX disk backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_posix_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;
	NTVFS_CURRENT_CRITICAL_SIZES(vers);

	ZERO_STRUCT(ops);

	ops.type = NTVFS_DISK;
	
	/* fill in all the operations */
	ops.connect = pvfs_connect;
	ops.disconnect = pvfs_disconnect;
	ops.unlink = pvfs_unlink;
	ops.chkpath = pvfs_chkpath;
	ops.qpathinfo = pvfs_qpathinfo;
	ops.setpathinfo = pvfs_setpathinfo;
	ops.open = pvfs_open;
	ops.mkdir = pvfs_mkdir;
	ops.rmdir = pvfs_rmdir;
	ops.rename = pvfs_rename;
	ops.copy = pvfs_copy;
	ops.ioctl = pvfs_ioctl;
	ops.read = pvfs_read;
	ops.write = pvfs_write;
	ops.seek = pvfs_seek;
	ops.flush = pvfs_flush;	
	ops.close = pvfs_close;
	ops.exit = pvfs_exit;
	ops.lock = pvfs_lock;
	ops.setfileinfo = pvfs_setfileinfo;
	ops.qfileinfo = pvfs_qfileinfo;
	ops.fsinfo = pvfs_fsinfo;
	ops.lpq = pvfs_lpq;
	ops.search_first = pvfs_search_first;
	ops.search_next = pvfs_search_next;
	ops.search_close = pvfs_search_close;
	ops.trans = pvfs_trans;
	ops.logoff = pvfs_logoff;
	ops.async_setup = pvfs_async_setup;
	ops.cancel = pvfs_cancel;
	ops.notify = pvfs_notify;

	/* register ourselves with the NTVFS subsystem. We register
	   under the name 'default' as we wish to be the default
	   backend, and also register as 'posix' */
	ops.name = "default";
	ret = ntvfs_register(&ops, &vers);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register POSIX backend as '%s'!\n", ops.name));
	}

	ops.name = "posix";
	ret = ntvfs_register(&ops, &vers);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register POSIX backend as '%s'!\n", ops.name));
	}

	if (NT_STATUS_IS_OK(ret)) {
		ret = ntvfs_common_init();
	}

	return ret;
}
