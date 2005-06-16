/* 
   Unix SMB/CIFS implementation.
   simple registry frontend
   
   Copyright (C) Jelmer Vernooij 2004

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
#include "dynconfig.h"
#include "registry.h"
#include "lib/cmdline/popt_common.h"

static void print_tree(int l, struct registry_key *p, int fullpath, int novals)
{
	struct registry_key *subkey;
	struct registry_value *value;
	WERROR error;
	int i;
	TALLOC_CTX *mem_ctx;

	for(i = 0; i < l; i++) putchar(' ');
	
	/* Hive name */
	if(p->hive->root == p) {
		if(p->hive->root->name) printf("%s\n", p->hive->root->name); else printf("<No Name>\n");
	} else {
		if(!p->name) printf("<No Name>\n");
		if(fullpath) printf("%s\n", p->path);
		else printf("%s\n", p->name);
	}

	mem_ctx = talloc_init("print_tree");
	for(i = 0; W_ERROR_IS_OK(error = reg_key_get_subkey_by_index(mem_ctx, p, i, &subkey)); i++) {
		print_tree(l+1, subkey, fullpath, novals);
	}
	talloc_free(mem_ctx);

	if(!W_ERROR_EQUAL(error, WERR_NO_MORE_ITEMS)) {
		DEBUG(0, ("Error occured while fetching subkeys for '%s': %s\n", p->path, win_errstr(error)));
	}

	if(!novals) {
		mem_ctx = talloc_init("print_tree");
		for(i = 0; W_ERROR_IS_OK(error = reg_key_get_value_by_index(mem_ctx, p, i, &value)); i++) {
			int j;
			char *desc;
			for(j = 0; j < l+1; j++) putchar(' ');
			desc = reg_val_description(mem_ctx, value);
			printf("%s\n", desc);
		}
		talloc_free(mem_ctx);

		if(!W_ERROR_EQUAL(error, WERR_NO_MORE_ITEMS)) {
			DEBUG(0, ("Error occured while fetching values for '%s': %s\n", p->path, win_errstr(error)));
		}
	}
}

 int main(int argc, char **argv)
{
	int opt, i;
	const char *backend = NULL;
	const char *remote = NULL;
	poptContext pc;
	struct registry_context *h = NULL;
	struct registry_key *root = NULL;
	WERROR error;
	int fullpath = 0, no_values = 0;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"backend", 'b', POPT_ARG_STRING, &backend, 0, "backend to use", NULL},
		{"fullpath", 'f', POPT_ARG_NONE, &fullpath, 0, "show full paths", NULL},
		{"remote", 'R', POPT_ARG_STRING, &remote, 0, "connect to specified remote server", NULL },
		{"no-values", 'V', POPT_ARG_NONE, &no_values, 0, "don't show values", NULL},
		POPT_COMMON_SAMBA	
		POPT_COMMON_CREDENTIALS	
		POPT_TABLEEND
	};

	regtree_init_subsystems;

	pc = poptGetContext(argv[0], argc, (const char **) argv, long_options,0);
	
	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	if (remote) {
		error = reg_open_remote(&h, cmdline_credentials, remote, NULL);
	} else if (backend) {
	    error = reg_open_hive(NULL, backend, poptGetArg(pc), NULL, &root);
	} else {
		error = reg_open_local (&h);
	}

	if(!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Unable to open '%s' with backend '%s':%s \n", poptGetArg(pc), backend, win_errstr(error));
		return 1;
	}
	poptFreeContext(pc);

	error = WERR_OK;
	
	if (!h) {
		print_tree(0, root, fullpath, no_values);
	} else {
		for(i = HKEY_CLASSES_ROOT; i < HKEY_PERFORMANCE_NLSTEXT; i++) {
			error = reg_get_predefined_key(h, i, &root);
			if (!W_ERROR_IS_OK(error)) {
				fprintf(stderr, "Skipping %s\n", reg_get_predef_name(i));
				continue;
			}
			print_tree(0, root, fullpath, no_values);
		}
	}

	return 0;
}
