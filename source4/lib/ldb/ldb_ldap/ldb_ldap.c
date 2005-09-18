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
 *  Component: ldb ldap backend
 *
 *  Description: core files for LDAP backend
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_private.h"
#include "ldb/ldb_ldap/ldb_ldap.h"

/*
  rename a record
*/
static int lldb_rename(struct ldb_module *module, const struct ldb_dn *olddn, const struct ldb_dn *newdn)
{
	TALLOC_CTX *local_ctx;
	struct lldb_private *lldb = module->private_data;
	int ret = 0;
	char *old_dn;
	char *newrdn;
	const char *parentdn = "";

	/* ignore ltdb specials */
	if (ldb_dn_is_special(olddn) || ldb_dn_is_special(newdn)) {
		return 0;
	}

	local_ctx = talloc_named(lldb, 0, "lldb_rename local context");
	if (local_ctx == NULL) {
		return -1;
	}

	old_dn = ldb_dn_linearize(local_ctx, olddn);
	if (old_dn == NULL) {
		goto failed;
	}

	newrdn = talloc_asprintf(lldb, "%s=%s",
				      newdn->components[0].name,
				      ldb_dn_escape_value(lldb, newdn->components[0].value));
	if (!newrdn) {
		goto failed;
	}

	parentdn = ldb_dn_linearize(lldb, ldb_dn_get_parent(lldb, newdn));
	if (!parentdn) {
		goto failed;
	}

	lldb->last_rc = ldap_rename_s(lldb->ldap, old_dn, newrdn, parentdn, 1, NULL, NULL);
	if (lldb->last_rc != LDAP_SUCCESS) {
		ldb_set_errstring(module, talloc_strdup(module, ldap_err2string(lldb->last_rc)));
		ret = -1;
	}

	talloc_free(local_ctx);
	return ret;

failed:
	talloc_free(local_ctx);
	return -1;
}

/*
  delete a record
*/
static int lldb_delete(struct ldb_module *module, const struct ldb_dn *edn)
{
	struct lldb_private *lldb = module->private_data;
	char *dn;
	int ret = 0;

	/* ignore ltdb specials */
	if (ldb_dn_is_special(edn)) {
		return 0;
	}

	dn = ldb_dn_linearize(lldb, edn);

	lldb->last_rc = ldap_delete_s(lldb->ldap, dn);
	if (lldb->last_rc != LDAP_SUCCESS) {
		ldb_set_errstring(module, talloc_strdup(module, ldap_err2string(lldb->last_rc)));
		ret = -1;
	}

	talloc_free(dn);
	return ret;
}

/*
  add a single set of ldap message values to a ldb_message
*/
static int lldb_add_msg_attr(struct ldb_context *ldb,
			     struct ldb_message *msg, 
			     const char *attr, struct berval **bval)
{
	int count, i;
	struct ldb_message_element *el;

	count = ldap_count_values_len(bval);

	if (count <= 0) {
		return -1;
	}

	el = talloc_realloc(msg, msg->elements, struct ldb_message_element, 
			      msg->num_elements + 1);
	if (!el) {
		errno = ENOMEM;
		return -1;
	}

	msg->elements = el;

	el = &msg->elements[msg->num_elements];

	el->name = talloc_strdup(msg->elements, attr);
	if (!el->name) {
		errno = ENOMEM;
		return -1;
	}
	el->flags = 0;

	el->num_values = 0;
	el->values = talloc_array(msg->elements, struct ldb_val, count);
	if (!el->values) {
		errno = ENOMEM;
		return -1;
	}

	for (i=0;i<count;i++) {
		el->values[i].data = talloc_memdup(el->values, bval[i]->bv_val, bval[i]->bv_len);
		if (!el->values[i].data) {
			return -1;
		}
		el->values[i].length = bval[i]->bv_len;
		el->num_values++;
	}

	msg->num_elements++;

	return 0;
}

/*
  search for matching records
*/
static int lldb_search(struct ldb_module *module, const struct ldb_dn *base,
		       enum ldb_scope scope, const char *expression,
		       const char * const *attrs, struct ldb_message ***res)
{
	struct ldb_context *ldb = module->ldb;
	struct lldb_private *lldb = module->private_data;
	int count, msg_count;
	char *search_base;
	LDAPMessage *ldapres, *msg;

	search_base = ldb_dn_linearize(ldb, base);
	if (base == NULL) {
		search_base = talloc_strdup(ldb, "");
	}
	if (search_base == NULL) {
		return -1;
	}

	if (expression == NULL || expression[0] == '\0') {
		expression = "objectClass=*";
	}

	lldb->last_rc = ldap_search_s(lldb->ldap, search_base, (int)scope, 
				      expression, 
				      discard_const_p(char *, attrs), 
				      0, &ldapres);
	talloc_free(search_base);
	if (lldb->last_rc != LDAP_SUCCESS) {
		ldb_set_errstring(module, talloc_strdup(module, ldap_err2string(lldb->last_rc)));
		return -1;
	}

	count = ldap_count_entries(lldb->ldap, ldapres);
	if (count == -1 || count == 0) {
		ldap_msgfree(ldapres);
		return count;
	}

	(*res) = talloc_array(lldb, struct ldb_message *, count+1);
	if (! *res) {
		ldap_msgfree(ldapres);
		errno = ENOMEM;
		return -1;
	}

	(*res)[0] = NULL;

	msg_count = 0;

	/* loop over all messages */
	for (msg=ldap_first_entry(lldb->ldap, ldapres); 
	     msg; 
	     msg=ldap_next_entry(lldb->ldap, msg)) {
		BerElement *berptr = NULL;
		char *attr, *dn;

		if (msg_count == count) {
			/* hmm, got too many? */
			ldb_debug(ldb, LDB_DEBUG_FATAL, "Fatal: ldap message count inconsistent\n");
			break;
		}

		(*res)[msg_count] = talloc(*res, struct ldb_message);
		if (!(*res)[msg_count]) {
			goto failed;
		}
		(*res)[msg_count+1] = NULL;

		dn = ldap_get_dn(lldb->ldap, msg);
		if (!dn) {
			goto failed;
		}

		(*res)[msg_count]->dn = ldb_dn_explode((*res)[msg_count], dn);
		ldap_memfree(dn);
		if (!(*res)[msg_count]->dn) {
			goto failed;
		}


		(*res)[msg_count]->num_elements = 0;
		(*res)[msg_count]->elements = NULL;
		(*res)[msg_count]->private_data = NULL;

		/* loop over all attributes */
		for (attr=ldap_first_attribute(lldb->ldap, msg, &berptr);
		     attr;
		     attr=ldap_next_attribute(lldb->ldap, msg, berptr)) {
			struct berval **bval;
			bval = ldap_get_values_len(lldb->ldap, msg, attr);

			if (bval) {
				lldb_add_msg_attr(ldb, (*res)[msg_count], attr, bval);
				ldap_value_free_len(bval);
			}					  
			
			ldap_memfree(attr);
		}
		if (berptr) ber_free(berptr, 0);

		msg_count++;
	}

	ldap_msgfree(ldapres);

	return msg_count;

failed:
	if (*res) talloc_free(*res);
	return -1;
}


/*
  search for matching records using a ldb_parse_tree
*/
static int lldb_search_bytree(struct ldb_module *module, const struct ldb_dn *base,
			      enum ldb_scope scope, struct ldb_parse_tree *tree,
			      const char * const *attrs, struct ldb_message ***res)
{
	struct lldb_private *lldb = module->private_data;
	char *expression;
	int ret;

	expression = ldb_filter_from_tree(lldb, tree);
	if (expression == NULL) {
		return -1;
	}
	ret = lldb_search(module, base, scope, expression, attrs, res);
	talloc_free(expression);
	return ret;
}


/*
  convert a ldb_message structure to a list of LDAPMod structures
  ready for ldap_add() or ldap_modify()
*/
static LDAPMod **lldb_msg_to_mods(struct ldb_context *ldb,
				  const struct ldb_message *msg, int use_flags)
{
	LDAPMod **mods;
	unsigned int i, j;
	int num_mods = 0;

	/* allocate maximum number of elements needed */
	mods = talloc_array(ldb, LDAPMod *, msg->num_elements+1);
	if (!mods) {
		errno = ENOMEM;
		return NULL;
	}
	mods[0] = NULL;

	for (i=0;i<msg->num_elements;i++) {
		const struct ldb_message_element *el = &msg->elements[i];

		mods[num_mods] = talloc(ldb, LDAPMod);
		if (!mods[num_mods]) {
			goto failed;
		}
		mods[num_mods+1] = NULL;
		mods[num_mods]->mod_op = LDAP_MOD_BVALUES;
		if (use_flags) {
			switch (el->flags & LDB_FLAG_MOD_MASK) {
			case LDB_FLAG_MOD_ADD:
				mods[num_mods]->mod_op |= LDAP_MOD_ADD;
				break;
			case LDB_FLAG_MOD_DELETE:
				mods[num_mods]->mod_op |= LDAP_MOD_DELETE;
				break;
			case LDB_FLAG_MOD_REPLACE:
				mods[num_mods]->mod_op |= LDAP_MOD_REPLACE;
				break;
			}
		}
		mods[num_mods]->mod_type = discard_const_p(char, el->name);
		mods[num_mods]->mod_vals.modv_bvals = talloc_array(mods[num_mods], 
								     struct berval *,
								     1+el->num_values);
		if (!mods[num_mods]->mod_vals.modv_bvals) {
			goto failed;
		}

		for (j=0;j<el->num_values;j++) {
			mods[num_mods]->mod_vals.modv_bvals[j] = talloc(mods[num_mods]->mod_vals.modv_bvals,
									  struct berval);
			if (!mods[num_mods]->mod_vals.modv_bvals[j]) {
				goto failed;
			}
			mods[num_mods]->mod_vals.modv_bvals[j]->bv_val = el->values[j].data;
			mods[num_mods]->mod_vals.modv_bvals[j]->bv_len = el->values[j].length;
		}
		mods[num_mods]->mod_vals.modv_bvals[j] = NULL;
		num_mods++;
	}

	return mods;

failed:
	talloc_free(mods);
	return NULL;
}


/*
  add a record
*/
static int lldb_add(struct ldb_module *module, const struct ldb_message *msg)
{
	struct ldb_context *ldb = module->ldb;
	struct lldb_private *lldb = module->private_data;
	LDAPMod **mods;
	char *dn;
	int ret = 0;

	/* ignore ltdb specials */
	if (ldb_dn_is_special(msg->dn)) {
		return 0;
	}

	mods = lldb_msg_to_mods(ldb, msg, 0);
	if (mods == NULL) {
		return -1;
	}

	dn = ldb_dn_linearize(mods, msg->dn);
	if (dn == NULL) {
		talloc_free(mods);
		return -1;
	}

	lldb->last_rc = ldap_add_s(lldb->ldap, dn, mods);
	if (lldb->last_rc != LDAP_SUCCESS) {
		ldb_set_errstring(module, talloc_strdup(module, ldap_err2string(lldb->last_rc)));
		ret = -1;
	}

	talloc_free(mods);

	return ret;
}


/*
  modify a record
*/
static int lldb_modify(struct ldb_module *module, const struct ldb_message *msg)
{
	struct ldb_context *ldb = module->ldb;
	struct lldb_private *lldb = module->private_data;
	LDAPMod **mods;
	char *dn;
	int ret = 0;

	/* ignore ltdb specials */
	if (ldb_dn_is_special(msg->dn)) {
		return 0;
	}

	mods = lldb_msg_to_mods(ldb, msg, 1);
	if (mods == NULL) {
		return -1;
	}

	dn = ldb_dn_linearize(mods, msg->dn);
	if (dn == NULL) {
		talloc_free(mods);
		return -1;
	}

	lldb->last_rc = ldap_modify_s(lldb->ldap, dn, mods);
	if (lldb->last_rc != LDAP_SUCCESS) {
		ldb_set_errstring(module, talloc_strdup(module, ldap_err2string(lldb->last_rc)));
		ret = -1;
	}

	talloc_free(mods);

	return ret;
}

static int lldb_start_trans(struct ldb_module *module)
{
	/* TODO implement a local transaction mechanism here */

	return 0;
}

static int lldb_end_trans(struct ldb_module *module, int status)
{
	/* TODO implement a local transaction mechanism here */

	return status;
}

static const struct ldb_module_ops lldb_ops = {
	.name              = "ldap",
	.search            = lldb_search,
	.search_bytree     = lldb_search_bytree,
	.add_record        = lldb_add,
	.modify_record     = lldb_modify,
	.delete_record     = lldb_delete,
	.rename_record     = lldb_rename,
	.start_transaction = lldb_start_trans,
	.end_transaction   = lldb_end_trans
};


static int lldb_destructor(void *p)
{
	struct lldb_private *lldb = p;
	ldap_unbind(lldb->ldap);
	return 0;
}

/*
  connect to the database
*/
int lldb_connect(struct ldb_context *ldb,
		 const char *url, 
		 unsigned int flags, 
		 const char *options[])
{
	struct lldb_private *lldb = NULL;
	int version = 3;

	lldb = talloc(ldb, struct lldb_private);
	if (!lldb) {
		ldb_oom(ldb);
		goto failed;
	}

	lldb->ldap = NULL;
	lldb->options = NULL;

	lldb->last_rc = ldap_initialize(&lldb->ldap, url);
	if (lldb->last_rc != LDAP_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "ldap_initialize failed for URL '%s' - %s\n",
			  url, ldap_err2string(lldb->last_rc));
		goto failed;
	}

	talloc_set_destructor(lldb, lldb_destructor);

	lldb->last_rc = ldap_set_option(lldb->ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
	if (lldb->last_rc != LDAP_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "ldap_set_option failed - %s\n",
			  ldap_err2string(lldb->last_rc));
	}

	ldb->modules = talloc(ldb, struct ldb_module);
	if (!ldb->modules) {
		ldb_oom(ldb);
		goto failed;
	}
	ldb->modules->ldb = ldb;
	ldb->modules->prev = ldb->modules->next = NULL;
	ldb->modules->private_data = lldb;
	ldb->modules->ops = &lldb_ops;

	return 0;

failed:
	talloc_free(lldb);
	return -1;
}

