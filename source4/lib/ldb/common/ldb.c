/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 *  Name: ldb
 *
 *  Component: ldb core API
 *
 *  Description: core API routines interfacing to ldb backends
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_errors.h"
#include "ldb/include/ldb_private.h"

/* 
   initialise a ldb context
   The mem_ctx is optional
*/
struct ldb_context *ldb_init(void *mem_ctx)
{
	struct ldb_context *ldb = talloc_zero(mem_ctx, struct ldb_context);
	int ret;

	ret = ldb_setup_wellknown_attributes(ldb);
	if (ret != 0) {
		talloc_free(ldb);
		return NULL;
	}

	return ldb;
}

/* 
 connect to a database. The URL can either be one of the following forms
   ldb://path
   ldapi://path

   flags is made up of LDB_FLG_*

   the options are passed uninterpreted to the backend, and are
   backend specific
*/
int ldb_connect(struct ldb_context *ldb, const char *url, unsigned int flags, const char *options[])
{
	int ret;

	if (strncmp(url, "tdb:", 4) == 0 ||
	    strchr(url, ':') == NULL) {
		ret = ltdb_connect(ldb, url, flags, options);
	}

#if HAVE_ILDAP
	else if (strncmp(url, "ldap", 4) == 0) {
		ret = ildb_connect(ldb, url, flags, options);
	}
#elif HAVE_LDAP
	else if (strncmp(url, "ldap", 4) == 0) {
		ret = lldb_connect(ldb, url, flags, options);
	}
#endif
#if HAVE_SQLITE3
	else if (strncmp(url, "sqlite:", 7) == 0) {
                ret = lsqlite3_connect(ldb, url, flags, options);
	}
#endif
	else {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Unable to find backend for '%s'\n", url);
		return LDB_ERR_OTHER;
	}

	if (ret != LDB_ERR_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "Failed to connect to '%s'\n", url);
		return ret;
	}

	if (ldb_load_modules(ldb, options) != LDB_ERR_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Unable to load modules for '%s'\n", url);
		return LDB_ERR_OTHER;
	}

	return LDB_ERR_SUCCESS;
}

/*
  start a transaction
*/
static int ldb_start_trans(struct ldb_context *ldb)
{
        return ldb->modules->ops->start_transaction(ldb->modules);
}

/*
  end a transaction
*/
static int ldb_end_trans(struct ldb_context *ldb, int status)
{
        return ldb->modules->ops->end_transaction(ldb->modules, status);
}

/*
  search the database given a LDAP-like search expression

  return the number of records found, or -1 on error

  Use talloc_free to free the ldb_message returned in 'res'

*/
int ldb_search(struct ldb_context *ldb, 
	       const struct ldb_dn *base,
	       enum ldb_scope scope,
	       const char *expression,
	       const char * const *attrs, struct ldb_message ***res)
{
	return ldb->modules->ops->search(ldb->modules, base, scope, expression, attrs, res);
}

/*
  search the database given a LDAP-like search expression

  return the number of records found, or -1 on error

  Use talloc_free to free the ldb_message returned in 'res'

*/
int ldb_search_bytree(struct ldb_context *ldb, 
		      const struct ldb_dn *base,
		      enum ldb_scope scope,
		      struct ldb_parse_tree *tree,
		      const char * const *attrs, struct ldb_message ***res)
{
	return ldb->modules->ops->search_bytree(ldb->modules, base, scope, tree, attrs, res);
}

/*
  add a record to the database. Will fail if a record with the given class and key
  already exists
*/
int ldb_add(struct ldb_context *ldb, 
	    const struct ldb_message *message)
{
	int status;


	status = ldb_msg_sanity_check(message);
	if (status != LDB_ERR_SUCCESS) return status;

	status = ldb_start_trans(ldb);
	if (status != LDB_ERR_SUCCESS) return status;

	status = ldb->modules->ops->add_record(ldb->modules, message);
	return ldb_end_trans(ldb, status);
}

/*
  modify the specified attributes of a record
*/
int ldb_modify(struct ldb_context *ldb, 
	       const struct ldb_message *message)
{
	int status;

	status = ldb_msg_sanity_check(message);
	if (status != LDB_ERR_SUCCESS) return status;

	status = ldb_start_trans(ldb);
	if (status != LDB_ERR_SUCCESS) return status;

	status = ldb->modules->ops->modify_record(ldb->modules, message);
	return ldb_end_trans(ldb, status);
}


/*
  delete a record from the database
*/
int ldb_delete(struct ldb_context *ldb, const struct ldb_dn *dn)
{
	int status = ldb_start_trans(ldb);
	if (status != LDB_ERR_SUCCESS) return status;

	status = ldb->modules->ops->delete_record(ldb->modules, dn);
	return ldb_end_trans(ldb, status);
}

/*
  rename a record in the database
*/
int ldb_rename(struct ldb_context *ldb, const struct ldb_dn *olddn, const struct ldb_dn *newdn)
{
	int status = ldb_start_trans(ldb);
	if (status != LDB_ERR_SUCCESS) return status;

	status = ldb->modules->ops->rename_record(ldb->modules, olddn, newdn);
	return ldb_end_trans(ldb, status);
}

/*
  return extended error information 
*/
const char *ldb_errstring(struct ldb_context *ldb)
{
	if (ldb->modules == NULL) {
		return "ldb not connected";
	}
	return ldb->modules->ops->errstring(ldb->modules);
}


/*
  set backend specific opaque parameters
*/
int ldb_set_opaque(struct ldb_context *ldb, const char *name, void *value)
{
	struct ldb_opaque *o = talloc(ldb, struct ldb_opaque);
	if (o == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OTHER;
	}
	o->next = ldb->opaque;
	o->name = name;
	o->value = value;
	ldb->opaque = o;
	return LDB_ERR_SUCCESS;
}

/*
  get a previously set opaque value
*/
void *ldb_get_opaque(struct ldb_context *ldb, const char *name)
{
	struct ldb_opaque *o;
	for (o=ldb->opaque;o;o=o->next) {
		if (strcmp(o->name, name) == 0) {
			return o->value;
		}
	}
	return NULL;
}
