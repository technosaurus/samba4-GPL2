/* 
   Unix SMB/CIFS implementation.
   Registry interface
   Copyright (C) Jelmer Vernooij  2004.
   
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

static char *reg_path_to_ldb(TALLOC_CTX *mem_ctx, const char *path, const char *add)
{
	char *ret = talloc_strdup(mem_ctx, "");
	char *mypath = strdup(path);
	char *end = mypath, *begin;

	if(add) 
		ret = talloc_asprintf_append(ret, "%s", add);

	while(end) {
		char *keyname;
		begin = strrchr(end, '\\');

		if(begin) keyname = begin + 1;
		else keyname = mypath;

		if(strlen(keyname))
			ret = talloc_asprintf_append(ret, "key=%s,", keyname);
			
		if(begin) {
			begin[0] = '\0';
			end = begin-1;
		} else {
			end = NULL;
		}
	}

	SAFE_FREE(mypath);

	ret[strlen(ret)-1] = '\0';
	
	return ret;
}


static int ldb_close_registry(void *data) 
{
	ldb_close((struct ldb_context *)data);
	return 0;
}


/* 
 * Saves the dn as private_data for every key/val
 */

static WERROR ldb_open_hive(TALLOC_CTX *mem_ctx, struct registry_hive *hive, struct registry_key **k)
{
	struct ldb_context *c;

	if (!hive->location) return WERR_INVALID_PARAM;
	c = ldb_connect(hive->location, 0, NULL);

	ldb_set_debug_stderr(c);


	if(!c) return WERR_FOOBAR;

	hive->backend_data = c;
	talloc_set_destructor(c, ldb_close_registry);
	
	return WERR_OK;
}

static WERROR ldb_add_key(TALLOC_CTX *mem_ctx, struct registry_key *p, const char *name, uint32_t access_mask, SEC_DESC *sec, struct registry_key **new)
{
	return WERR_NOT_SUPPORTED;	
}

#if 0
FIXME
static WERROR ldb_fetch_subkeys(struct registry_key *k, int *count, struct registry_key ***subkeys)
{
	struct ldb_context *c = k->hive->backend_data;
	int ret, i, j;
	struct ldb_message **msg;

	ret = ldb_search(c, (char *)k->backend_data, LDB_SCOPE_ONELEVEL, "(key=*)", NULL,&msg);

	if(ret < 0) {
		DEBUG(0, ("Error getting subkeys for '%s': %s\n", (char *)k->backend_data, ldb_errstring(c)));
		return WERR_FOOBAR;
	}

	*subkeys = talloc_array_p(k->mem_ctx, struct registry_key *, ret);
	j = 0;
	for(i = 0; i < ret; i++) {
		struct ldb_message_element *el;
		char *name;
		el = ldb_msg_find_element(msg[i], "key");

		name = el->values[0].data;

		/* Dirty hack to circumvent ldb_tdb bug */
		if(k->backend_data && !strcmp(msg[i]->dn, (char *)k->backend_data)) continue;
			
		(*subkeys)[j] = reg_key_new_rel(name, k, NULL);
		(*subkeys)[j]->backend_data = talloc_strdup((*subkeys)[j]->mem_ctx, msg[i]->dn);
		j++;
	}
	*count = j;

	ldb_search_free(c, msg);
	return WERR_OK;
}

static WERROR ldb_fetch_values(struct registry_key *k, int *count, REG_VAL ***values)
{
	struct ldb_context *c = k->hive->backend_data;
	int ret, i, j;
	struct ldb_message **msg;

	ret = ldb_search(c, (char *)k->backend_data, LDB_SCOPE_ONELEVEL, "(value=*)", NULL,&msg);

	if(ret < 0) {
		DEBUG(0, ("Error getting values for '%s': %s\n", (char *)k->backend_data, ldb_errstring(c)));
		return WERR_FOOBAR;
	}

	*values = talloc_array_p(k->mem_ctx, REG_VAL *, ret);
	j = 0;
	for(i = 0; i < ret; i++) {
		struct ldb_message_element *el;
		char *name;
		el = ldb_msg_find_element(msg[i], "key");

		name = el->values[0].data;

		/* Dirty hack to circumvent ldb_tdb bug */
		if(k->backend_data && !strcmp(msg[i]->dn, (char *)k->backend_data)) continue;
			
		(*values)[j] = reg_val_new(k, NULL);
		(*values)[j]->backend_data = talloc_strdup((*values)[j]->mem_ctx, msg[i]->dn);
		j++;
	}
	*count = j;

	ldb_search_free(c, msg);
	return WERR_OK;
}

#endif

static WERROR ldb_open_key(TALLOC_CTX *mem_ctx, struct registry_hive *h, const char *name, struct registry_key **key)
{
	struct ldb_context *c = h->backend_data;
	struct ldb_message **msg;
	char *ldap_path;
	int ret;
	ldap_path = reg_path_to_ldb(mem_ctx, name, NULL);
	
	ret = ldb_search(c, ldap_path, LDB_SCOPE_BASE, "*", NULL,&msg);

	if(ret == 0) {
		return WERR_NO_MORE_ITEMS;
	} else if(ret < 0) {
		DEBUG(0, ("Error opening key '%s': %s\n", ldap_path, ldb_errstring(c)));
		return WERR_FOOBAR;
	}

	*key = talloc_p(mem_ctx, struct registry_key);
	/* FIXME */

	ldb_search_free(c, msg);

	return WERR_OK;
}

static struct registry_operations reg_backend_ldb = {
	.name = "ldb",
	.open_hive = ldb_open_hive,
	.open_key = ldb_open_key,
/*	.fetch_subkeys = ldb_fetch_subkeys,
	.fetch_values = ldb_fetch_values,*/
	.add_key = ldb_add_key,
};

NTSTATUS registry_ldb_init(void)
{
	return register_backend("registry", &reg_backend_ldb);
}
