/* 
   Unix SMB/CIFS implementation.
   useful function for deleting a whole directory tree
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

struct delete_state {
	struct cli_tree *tree;
	int total_deleted;
	BOOL failed;
};

/* 
   callback function for torture_deltree() 
*/
static void delete_fn(file_info *finfo, const char *name, void *state)
{
	struct delete_state *dstate = state;
	char *s, *n;
	if (strcmp(finfo->name, ".") == 0 ||
	    strcmp(finfo->name, "..") == 0) return;

	n = strdup(name);
	n[strlen(n)-1] = 0;
	asprintf(&s, "%s%s", n, finfo->name);

	if (finfo->mode & FILE_ATTRIBUTE_READONLY) {
		if (NT_STATUS_IS_ERR(cli_setatr(dstate->tree, s, 0, 0))) {
			DEBUG(2,("Failed to remove READONLY on %s - %s\n",
				 s, cli_errstr(dstate->tree)));			
		}
	}

	if (finfo->mode & FILE_ATTRIBUTE_DIRECTORY) {
		char *s2;
		asprintf(&s2, "%s\\*", s);
		cli_unlink(dstate->tree, s2);
		cli_list(dstate->tree, s2, 
			 FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, 
			 delete_fn, state);
		free(s2);
		if (NT_STATUS_IS_ERR(cli_rmdir(dstate->tree, s))) {
			DEBUG(2,("Failed to delete %s - %s\n", 
				 s, cli_errstr(dstate->tree)));
			dstate->failed = True;
		}
		dstate->total_deleted++;
	} else {
		if (NT_STATUS_IS_ERR(cli_unlink(dstate->tree, s))) {
			DEBUG(2,("Failed to delete %s - %s\n", 
				 s, cli_errstr(dstate->tree)));
			dstate->failed = True;
		}
		dstate->total_deleted++;
	}
	free(s);
	free(n);
}

/* 
   recursively descend a tree deleting all files
   returns the number of files deleted, or -1 on error
*/
int cli_deltree(struct cli_tree *tree, const char *dname)
{
	char *mask;
	struct delete_state dstate;

	dstate.tree = tree;
	dstate.total_deleted = 0;
	dstate.failed = False;

	/* it might be a file */
	if (NT_STATUS_IS_OK(cli_unlink(tree, dname))) {
		return 1;
	}
	if (NT_STATUS_EQUAL(cli_nt_error(tree), NT_STATUS_OBJECT_NAME_NOT_FOUND) ||
	    NT_STATUS_EQUAL(cli_nt_error(tree), NT_STATUS_OBJECT_PATH_NOT_FOUND) ||
	    NT_STATUS_EQUAL(cli_nt_error(tree), NT_STATUS_NO_SUCH_FILE)) {
		return 0;
	}

	asprintf(&mask, "%s\\*", dname);
	cli_unlink(dstate.tree, mask);
	cli_list(dstate.tree, mask, 
		 FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, 
		 delete_fn, &dstate);
	free(mask);
	if (NT_STATUS_IS_ERR(cli_rmdir(dstate.tree, dname))) {
		DEBUG(2,("Failed to delete %s - %s\n", 
			 dname, cli_errstr(dstate.tree)));
		return -1;
	}
	dstate.total_deleted++;

	if (dstate.failed) {
		return -1;
	}

	return dstate.total_deleted;
}
