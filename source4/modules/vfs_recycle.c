/*
 * Recycle bin VFS module for Samba.
 *
 * Copyright (C) 2001, Brandon Stone, Amherst College, <bbstone@amherst.edu>.
 * Copyright (C) 2002, Jeremy Allison - modified to make a VFS module.
 * Copyright (C) 2002, Alexander Bokovoy - cascaded VFS adoption,
 * Copyright (C) 2002, Juergen Hasch - added some options.
 * Copyright (C) 2002, Simo Sorce
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"

#define ALLOC_CHECK(ptr, label) do { if ((ptr) == NULL) { DEBUG(0, ("recycle.bin: out of memory!\n")); errno = ENOMEM; goto label; } } while(0)

static int vfs_recycle_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_recycle_debug_level

static const char *delimiter = "|";		/* delimiter for options */

/* One per connection */

typedef struct recycle_bin_struct
{
	TALLOC_CTX *mem_ctx;
	char	*repository;		/* name of the recycle bin directory */
	BOOL	keep_dir_tree;		/* keep directory structure of deleted file in recycle bin */
	BOOL	versions;		/* create versions of deleted files with identical name */
	BOOL	touch;			/* touch access date of deleted file */
	char	*exclude;		/* which files to exclude */
	char	*exclude_dir;		/* which directories to exclude */
	char	*noversions;		/* which files to exclude from versioning */
	SMB_OFF_T maxsize;		/* maximum file size to be saved */
} recycle_bin_struct;

typedef struct recycle_bin_connections {
	int conn;
	recycle_bin_struct *data;
	struct recycle_bin_connections *next;
} recycle_bin_connections;

typedef struct recycle_bin_private_data {
	TALLOC_CTX *mem_ctx;
	recycle_bin_connections *conns;
} recycle_bin_private_data;

struct smb_vfs_handle_struct *recycle_bin_private_handle;

/* VFS operations */
static struct vfs_ops default_vfs_ops;   /* For passthrough operation */

static int recycle_connect(struct tcon_context *conn, const char *service, const char *user);
static void recycle_disconnect(struct tcon_context *conn);
static int recycle_unlink(struct tcon_context *, const char *);

#define VFS_OP(x) ((void *) x)

static vfs_op_tuple recycle_ops[] = {

	/* Disk operations */
	{VFS_OP(recycle_connect),	SMB_VFS_OP_CONNECT,	SMB_VFS_LAYER_TRANSPARENT},
	{VFS_OP(recycle_disconnect),	SMB_VFS_OP_DISCONNECT,	SMB_VFS_LAYER_TRANSPARENT},

	/* File operations */
	{VFS_OP(recycle_unlink),	SMB_VFS_OP_UNLINK,	SMB_VFS_LAYER_TRANSPARENT},

	{NULL,				SMB_VFS_OP_NOOP,	SMB_VFS_LAYER_NOOP}
};

/**
 * VFS initialisation function.
 *
 * @retval initialised vfs_op_tuple array
 **/
vfs_op_tuple *vfs_init(int *vfs_version, struct vfs_ops *def_vfs_ops,
			struct smb_vfs_handle_struct *vfs_handle)
{
	TALLOC_CTX *mem_ctx = NULL;

	DEBUG(10, ("Initializing VFS module recycle\n"));
	*vfs_version = SMB_VFS_INTERFACE_VERSION;
	memcpy(&default_vfs_ops, def_vfs_ops, sizeof(struct vfs_ops));
	vfs_recycle_debug_level = debug_add_class("vfs_recycle_bin");
	if (vfs_recycle_debug_level == -1) {
		vfs_recycle_debug_level = DBGC_VFS;
		DEBUG(0, ("vfs_recycle: Couldn't register custom debugging class!\n"));
	} else {
		DEBUG(0, ("vfs_recycle: Debug class number of 'vfs_recycle': %d\n", vfs_recycle_debug_level));
	}

	recycle_bin_private_handle = vfs_handle;
	if (!(mem_ctx = talloc_init("recycle bin data"))) {
		DEBUG(0, ("Failed to allocate memory in VFS module recycle_bin\n"));
		return NULL;
	}

	recycle_bin_private_handle->data = talloc(mem_ctx, sizeof(recycle_bin_private_data));
	if (recycle_bin_private_handle->data == NULL) {
		DEBUG(0, ("Failed to allocate memory in VFS module recycle_bin\n"));
		return NULL;
	}
	((recycle_bin_private_data *)(recycle_bin_private_handle->data))->mem_ctx = mem_ctx;
	((recycle_bin_private_data *)(recycle_bin_private_handle->data))->conns = NULL;

	return recycle_ops;
}

/**
 * VFS finalization function.
 *
 **/
void vfs_done(void)
{
	recycle_bin_private_data *recdata;
	recycle_bin_connections *recconn;

	DEBUG(10, ("Unloading/Cleaning VFS module recycle bin\n"));

	if (recycle_bin_private_handle)
		recdata = (recycle_bin_private_data *)(recycle_bin_private_handle->data);
	else {
		DEBUG(0, ("Recycle bin not initialized!\n"));
		return;
	}

	if (recdata) {
		if (recdata->conns) {
			recconn = recdata->conns;
			while (recconn) {
				talloc_destroy(recconn->data->mem_ctx);
				recconn = recconn->next;
			}
		}
		if (recdata->mem_ctx) {
			talloc_destroy(recdata->mem_ctx);
		}
		recdata = NULL;
	}
}

static int recycle_connect(struct tcon_context *conn, const char *service, const char *user)
{
	TALLOC_CTX *ctx = NULL;
	recycle_bin_struct *recbin;
	recycle_bin_connections *recconn;
	recycle_bin_connections *recconnbase;
	recycle_bin_private_data *recdata;
	char *tmp_str;

	DEBUG(10, ("Called for service %s (%d) as user %s\n", service, SNUM(conn), user));

	if (recycle_bin_private_handle)
		recdata = (recycle_bin_private_data *)(recycle_bin_private_handle->data);
	else {
		DEBUG(0, ("Recycle bin not initialized!\n"));
		return -1;
	}

	if (!(ctx = talloc_init("recycle bin connection"))) {
		DEBUG(0, ("Failed to allocate memory in VFS module recycle_bin\n"));
		return -1;
	}

	recbin = talloc(ctx, sizeof(recycle_bin_struct));
	if (recbin == NULL) {
		DEBUG(0, ("Failed to allocate memory in VFS module recycle_bin\n"));
		return -1;
	}
	recbin->mem_ctx = ctx;

	/* Set defaults */
	recbin->repository = talloc_strdup(recbin->mem_ctx, ".recycle");
	ALLOC_CHECK(recbin->repository, error);
	recbin->keep_dir_tree = False;
	recbin->versions = False;
	recbin->touch = False;
	recbin->exclude = "";
	recbin->exclude_dir = "";
	recbin->noversions = "";
	recbin->maxsize = 0;

	/* parse configuration options */
	if ((tmp_str = lp_parm_string(SNUM(conn), "vfs_recycle_bin", "repository")) != NULL) {
		recbin->repository = talloc_sub_conn(recbin->mem_ctx, conn, tmp_str);
		ALLOC_CHECK(recbin->repository, error);
		trim_string(recbin->repository, "/", "/");
		DEBUG(5, ("recycle.bin: repository = %s\n", recbin->repository));
	}
	
	recbin->keep_dir_tree = lp_parm_bool(SNUM(conn), "vfs_recycle_bin", "keeptree", False);
	DEBUG(5, ("recycle.bin: keeptree = %d\n", recbin->keep_dir_tree));
	
	recbin->versions = lp_parm_bool(SNUM(conn), "vfs_recycle_bin", "versions", False);
	DEBUG(5, ("recycle.bin: versions = %d\n", recbin->versions));
	
	recbin->touch = lp_parm_bool(SNUM(conn), "vfs_recycle_bin", "touch", False);
	DEBUG(5, ("recycle.bin: touch = %d\n", recbin->touch));

	recbin->maxsize = lp_parm_ulong(SNUM(conn), "vfs_recycle_bin", "maxsize");
	if (recbin->maxsize == 0) {
		recbin->maxsize = -1;
		DEBUG(5, ("recycle.bin: maxsize = -infinite-\n"));
	} else {
		DEBUG(5, ("recycle.bin: maxsize = %ld\n", (long int)recbin->maxsize));
	}

	if ((tmp_str = lp_parm_string(SNUM(conn), "vfs_recycle_bin", "exclude")) != NULL) {
		recbin->exclude = talloc_strdup(recbin->mem_ctx, tmp_str);
		ALLOC_CHECK(recbin->exclude, error);
		DEBUG(5, ("recycle.bin: exclude = %s\n", recbin->exclude));
	}
	if ((tmp_str = lp_parm_string(SNUM(conn), "vfs_recycle_bin", "exclude_dir")) != NULL) {
		recbin->exclude_dir = talloc_strdup(recbin->mem_ctx, tmp_str);
		ALLOC_CHECK(recbin->exclude_dir, error);
		DEBUG(5, ("recycle.bin: exclude_dir = %s\n", recbin->exclude_dir));
	}
	if ((tmp_str = lp_parm_string(SNUM(conn), "vfs_recycle_bin", "noversions")) != NULL) {
		recbin->noversions = talloc_strdup(recbin->mem_ctx, tmp_str);
		ALLOC_CHECK(recbin->noversions, error);
		DEBUG(5, ("recycle.bin: noversions = %s\n", recbin->noversions));
	}

	recconn = talloc(recdata->mem_ctx, sizeof(recycle_bin_connections));
	if (recconn == NULL) {
		DEBUG(0, ("Failed to allocate memory in VFS module recycle_bin\n"));
		goto error;
	}
	recconn->conn = SNUM(conn);
	recconn->data = recbin;
	recconn->next = NULL;
	if (recdata->conns) {
		recconnbase = recdata->conns;
		while (recconnbase->next != NULL) recconnbase = recconnbase->next;
		recconnbase->next = recconn;
	} else {
		recdata->conns = recconn;
	}
	return default_vfs_ops.connect(conn, service, user);

error:
	talloc_destroy(ctx);
	return -1;
}

static void recycle_disconnect(struct tcon_context *conn)
{
	recycle_bin_private_data *recdata;
	recycle_bin_connections *recconn;

	DEBUG(10, ("Disconnecting VFS module recycle bin\n"));

	if (recycle_bin_private_handle)
		recdata = (recycle_bin_private_data *)(recycle_bin_private_handle->data);
	else {
		DEBUG(0, ("Recycle bin not initialized!\n"));
		return;
	}

	if (recdata) {
		if (recdata->conns) {
			if (recdata->conns->conn == SNUM(conn)) {
				talloc_destroy(recdata->conns->data->mem_ctx);
				recdata->conns = recdata->conns->next;
			} else {
				recconn = recdata->conns;
				while (recconn->next) {
					if (recconn->next->conn == SNUM(conn)) {
						talloc_destroy(recconn->next->data->mem_ctx);
						recconn->next = recconn->next->next;
						break;
					}
					recconn = recconn->next;
				}
			}
		}
	}
	default_vfs_ops.disconnect(conn);
}

static BOOL recycle_directory_exist(struct tcon_context *conn, const char *dname)
{
	SMB_STRUCT_STAT st;

	if (default_vfs_ops.stat(conn, dname, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			return True;
		}
	}

	return False;
}

static BOOL recycle_file_exist(struct tcon_context *conn, const char *fname)
{
	SMB_STRUCT_STAT st;

	if (default_vfs_ops.stat(conn, fname, &st) == 0) {
		if (S_ISREG(st.st_mode)) {
			return True;
		}
	}

	return False;
}

/**
 * Return file size
 * @param conn connection
 * @param fname file name
 * @return size in bytes
 **/
static SMB_OFF_T recycle_get_file_size(struct tcon_context *conn, const char *fname)
{
	SMB_STRUCT_STAT st;
	if (default_vfs_ops.stat(conn, fname, &st) != 0) {
		DEBUG(0,("recycle.bin: stat for %s returned %s\n", fname, strerror(errno)));
		return (SMB_OFF_T)0;
	}
	return(st.st_size);
}

/**
 * Create directory tree
 * @param conn connection
 * @param dname Directory tree to be created
 * @return Returns True for success
 **/
static BOOL recycle_create_dir(struct tcon_context *conn, const char *dname)
{
	int len;
	mode_t mode;
	char *new_dir = NULL;
	char *tmp_str = NULL;
	char *token;
	char *tok_str;
	BOOL ret = False;

	mode = S_IREAD | S_IWRITE | S_IEXEC;

	tmp_str = strdup(dname);
	ALLOC_CHECK(tmp_str, done);
	tok_str = tmp_str;

	len = strlen(dname);
	new_dir = (char *)malloc(len + 1);
	ALLOC_CHECK(new_dir, done);
	*new_dir = '\0';

	/* Create directory tree if neccessary */
	for(token = strtok(tok_str, "/"); token; token = strtok(NULL, "/")) {
		safe_strcat(new_dir, token, len);
		if (recycle_directory_exist(conn, new_dir))
			DEBUG(10, ("recycle.bin: dir %s already exists\n", new_dir));
		else {
			DEBUG(5, ("recycle.bin: creating new dir %s\n", new_dir));
			if (default_vfs_ops.mkdir(conn, new_dir, mode) != 0) {
				DEBUG(1,("recycle.bin: mkdir failed for %s with error: %s\n", new_dir, strerror(errno)));
				ret = False;
				goto done;
			}
		}
		safe_strcat(new_dir, "/", len);
		}

	ret = True;
done:
	SAFE_FREE(tmp_str);
	SAFE_FREE(new_dir);
	return ret;
}

/**
 * Check if needle is contained exactly in haystack
 * @param haystack list of parameters separated by delimimiter character
 * @param needle string to be matched exactly to haystack
 * @return True if found
 **/
static BOOL checkparam(const char *haystack, const char *needle)
{
	char *token;
	char *tok_str;
	char *tmp_str;
	BOOL ret = False;

	if (haystack == NULL || strlen(haystack) == 0 || needle == NULL || strlen(needle) == 0) {
		return False;
	}

	tmp_str = strdup(haystack);
	ALLOC_CHECK(tmp_str, done);
	token = tok_str = tmp_str;

	for(token = strtok(tok_str, delimiter); token; token = strtok(NULL, delimiter)) {
		if(strcmp(token, needle) == 0) {
			ret = True;
			goto done;
		}
	}
done:
	SAFE_FREE(tmp_str);
	return ret;
}

/**
 * Check if needle is contained in haystack, * and ? patterns are resolved
 * @param haystack list of parameters separated by delimimiter character
 * @param needle string to be matched exectly to haystack including pattern matching
 * @return True if found
 **/
static BOOL matchparam(const char *haystack, const char *needle)
{
	char *token;
	char *tok_str;
	char *tmp_str;
	BOOL ret = False;

	if (haystack == NULL || strlen(haystack) == 0 || needle == NULL || strlen(needle) == 0) {
		return False;
	}

	tmp_str = strdup(haystack);
	ALLOC_CHECK(tmp_str, done);
	token = tok_str = tmp_str;

	for(token = strtok(tok_str, delimiter); token; token = strtok(NULL, delimiter)) {
		if (!unix_wild_match(token, needle)) {
			ret = True;
			goto done;
		}
	}
done:
	SAFE_FREE(tmp_str);
	return ret;
}

/**
 * Touch access date
 **/
static void recycle_touch(struct tcon_context *conn, const char *fname)
{
	SMB_STRUCT_STAT st;
	struct utimbuf tb;
	time_t currtime;

	if (default_vfs_ops.stat(conn, fname, &st) != 0) {
		DEBUG(0,("recycle.bin: stat for %s returned %s\n", fname, strerror(errno)));
		return;
	}
	currtime = time(&currtime);
	tb.actime = currtime;
	tb.modtime = st.st_mtime;

	if (default_vfs_ops.utime(conn, fname, &tb) == -1 )
		DEBUG(0, ("recycle.bin: touching %s failed, reason = %s\n", fname, strerror(errno)));
	}

/**
 * Check if file should be recycled
 **/
static int recycle_unlink(struct tcon_context *conn, const char *file_name)
{
	recycle_bin_private_data *recdata;
	recycle_bin_connections *recconn;
	recycle_bin_struct *recbin;
	char *path_name = NULL;
       	char *temp_name = NULL;
	char *final_name = NULL;
	const char *base;
	int i;
/*	SMB_BIG_UINT dfree, dsize, bsize;	*/
	SMB_OFF_T file_size; /* space_avail;	*/
	BOOL exist;
	int rc = -1;

	recbin = NULL;
	if (recycle_bin_private_handle) {
		recdata = (recycle_bin_private_data *)(recycle_bin_private_handle->data);
		if (recdata) {
			if (recdata->conns) {
				recconn = recdata->conns;
				while (recconn && recconn->conn != SNUM(conn)) recconn = recconn->next;
				if (recconn != NULL) {
					recbin = recconn->data;
				}
			}
		}
	}
	if (recbin == NULL) {
		DEBUG(0, ("Recycle bin not initialized!\n"));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}

	if(!recbin->repository || *(recbin->repository) == '\0') {
		DEBUG(3, ("Recycle path not set, purging %s...\n", file_name));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}

	/* we don't recycle the recycle bin... */
	if (strncmp(file_name, recbin->repository, strlen(recbin->repository)) == 0) {
		DEBUG(3, ("File is within recycling bin, unlinking ...\n"));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}

	file_size = recycle_get_file_size(conn, file_name);
	/* it is wrong to purge filenames only because they are empty imho
	 *   --- simo
	 *
	if(fsize == 0) {
		DEBUG(3, ("File %s is empty, purging...\n", file_name));
		rc = default_vfs_ops.unlink(conn,file_name);
		goto done;
	}
	 */

	/* FIXME: this is wrong, we should check the hole size of the recycle bin is
	 * not greater then maxsize, not the size of the single file, also it is better
	 * to remove older files
	 */
	if(recbin->maxsize > 0 && file_size > recbin->maxsize) {
		DEBUG(3, ("File %s exceeds maximum recycle size, purging... \n", file_name));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}

	/* FIXME: this is wrong: moving files with rename does not change the disk space
	 * allocation
	 *
	space_avail = default_vfs_ops.disk_free(conn, ".", True, &bsize, &dfree, &dsize) * 1024L;
	DEBUG(5, ("space_avail = %Lu, file_size = %Lu\n", space_avail, file_size));
	if(space_avail < file_size) {
		DEBUG(3, ("Not enough diskspace, purging file %s\n", file_name));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}
	 */

	/* extract filename and path */
	path_name = (char *)malloc(PATH_MAX);
	ALLOC_CHECK(path_name, done);
	*path_name = '\0';
	safe_strcpy(path_name, file_name, PATH_MAX - 1);
	base = strrchr(path_name, '/');
	if (base == NULL) {
		base = file_name;
		safe_strcpy(path_name, "/", PATH_MAX - 1);
	}
	else {
		base++;
	}

	DEBUG(10, ("recycle.bin: fname = %s\n", file_name));	/* original filename with path */
	DEBUG(10, ("recycle.bin: fpath = %s\n", path_name));	/* original path */
	DEBUG(10, ("recycle.bin: base = %s\n", base));		/* filename without path */

	if (matchparam(recbin->exclude, base)) {
		DEBUG(3, ("recycle.bin: file %s is excluded \n", base));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}

	/* FIXME: this check will fail if we have more than one level of directories,
	 * we shoud check for every level 1, 1/2, 1/2/3, 1/2/3/4 .... 
	 * 	---simo
	 */
	if (checkparam(recbin->exclude_dir, path_name)) {
		DEBUG(3, ("recycle.bin: directory %s is excluded \n", path_name));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}

	temp_name = (char *)strdup(recbin->repository);
	ALLOC_CHECK(temp_name, done);

	/* see if we need to recreate the original directory structure in the recycle bin */
	if (recbin->keep_dir_tree == True) {
		safe_strcat(temp_name, "/", PATH_MAX - 1);
		safe_strcat(temp_name, path_name, PATH_MAX - 1);
	}

	exist = recycle_directory_exist(conn, temp_name);
	if (exist) {
		DEBUG(10, ("recycle.bin: Directory already exists\n"));
	} else {
		DEBUG(10, ("recycle.bin: Creating directory %s\n", temp_name));
		if (recycle_create_dir(conn, temp_name) == False) {
			DEBUG(3, ("Could not create directory, purging %s...\n", file_name));
			rc = default_vfs_ops.unlink(conn, file_name);
			goto done;
		}
	}

	final_name = NULL;
	asprintf(&final_name, "%s/%s", temp_name, base);
	ALLOC_CHECK(final_name, done);
	DEBUG(10, ("recycle.bin: recycled file name%s\n", temp_name));		/* new filename with path */

	/* check if we should delete file from recycle bin */
	if (recycle_file_exist(conn, final_name)) {
		if (recbin->versions == False || matchparam(recbin->noversions, base) == True) {
			DEBUG(3, ("recycle.bin: Removing old file %s from recycle bin\n", final_name));
			if (default_vfs_ops.unlink(conn, final_name) != 0) {
				DEBUG(1, ("recycle.bin: Error deleting old file: %s\n", strerror(errno)));
			}
		}
	}

	/* rename file we move to recycle bin */
	i = 1;
	while (recycle_file_exist(conn, final_name)) {
		snprintf(final_name, PATH_MAX, "%s/Copy #%d of %s", temp_name, i++, base);
	}

	DEBUG(10, ("recycle.bin: Moving %s to %s\n", file_name, final_name));
	rc = default_vfs_ops.rename(conn, file_name, final_name);
	if (rc != 0) {
		DEBUG(3, ("recycle.bin: Move error %d (%s), purging file %s (%s)\n", errno, strerror(errno), file_name, final_name));
		rc = default_vfs_ops.unlink(conn, file_name);
		goto done;
	}

	/* touch access date of moved file */
	if (recbin->touch == True )
		recycle_touch(conn, final_name);

done:
	SAFE_FREE(path_name);
	SAFE_FREE(temp_name);
	SAFE_FREE(final_name);
	return rc;
}
