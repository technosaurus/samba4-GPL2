/* 
   samdb proxy module

   Copyright (C) Andrew Tridgell 2005

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
  ldb proxy module. At startup this looks for a record like this:

   dn=@PROXYINFO
   url=destination url
   olddn = basedn to proxy in upstream server
   newdn = basedn in local server
   username = username to connect to upstream
   password = password for upstream

   NOTE: this module is a complete hack at this stage. I am committing it just
   so others can know how I am investigating mmc support
   
 */

#include "includes.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_private.h"
#include "lib/cmdline/popt_common.h"

struct proxy_data {
	struct ldb_context *upstream;
	struct ldb_dn *olddn;
	struct ldb_dn *newdn;
	const char **oldstr;
	const char **newstr;
};


/*
  load the @PROXYINFO record
*/
static int load_proxy_info(struct ldb_module *module)
{
	struct proxy_data *proxy = talloc_get_type(module->private_data, struct proxy_data);
	struct ldb_dn *dn;
	struct ldb_message **msg;
	int res;
	const char *olddn, *newdn, *url, *username, *password, *oldstr, *newstr;
	struct cli_credentials *creds;
	

	/* see if we have already loaded it */
	if (proxy->upstream != NULL) {
		return 0;
	}

	dn = ldb_dn_explode(proxy, "@PROXYINFO");
	if (dn == NULL) {
		goto failed;
	}
	res = ldb_search(module->ldb, dn, LDB_SCOPE_BASE, NULL, NULL, &msg);
	talloc_free(dn);
	if (res != 1) {
		ldb_debug(module->ldb, LDB_DEBUG_FATAL, "Can't find @PROXYINFO\n");
		goto failed;
	}

	url      = ldb_msg_find_string(msg[0], "url", NULL);
	olddn    = ldb_msg_find_string(msg[0], "olddn", NULL);
	newdn    = ldb_msg_find_string(msg[0], "newdn", NULL);
	username = ldb_msg_find_string(msg[0], "username", NULL);
	password = ldb_msg_find_string(msg[0], "password", NULL);
	oldstr   = ldb_msg_find_string(msg[0], "oldstr", NULL);
	newstr   = ldb_msg_find_string(msg[0], "newstr", NULL);

	if (url == NULL || olddn == NULL || newdn == NULL || username == NULL || password == NULL) {
		ldb_debug(module->ldb, LDB_DEBUG_FATAL, "Need url, olddn, newdn, oldstr, newstr, username and password in @PROXYINFO\n");
		goto failed;
	}

	proxy->olddn = ldb_dn_explode(proxy, olddn);
	if (proxy->olddn == NULL) {
		ldb_debug(module->ldb, LDB_DEBUG_FATAL, "Failed to explode olddn '%s'\n", olddn);
		goto failed;
	}
	
	proxy->newdn = ldb_dn_explode(proxy, newdn);
	if (proxy->newdn == NULL) {
		ldb_debug(module->ldb, LDB_DEBUG_FATAL, "Failed to explode newdn '%s'\n", newdn);
		goto failed;
	}

	proxy->upstream = ldb_init(proxy);
	if (proxy->upstream == NULL) {
		ldb_oom(module->ldb);
		goto failed;
	}

	proxy->oldstr = str_list_make(proxy, oldstr, ", ");
	if (proxy->oldstr == NULL) {
		ldb_oom(module->ldb);
		goto failed;
	}

	proxy->newstr = str_list_make(proxy, newstr, ", ");
	if (proxy->newstr == NULL) {
		ldb_oom(module->ldb);
		goto failed;
	}

	/* setup credentials for connection */
	creds = cli_credentials_init(proxy->upstream);
	if (creds == NULL) {
		ldb_oom(module->ldb);
		goto failed;
	}
	cli_credentials_guess(creds);
	cli_credentials_set_username(creds, username, CRED_SPECIFIED);
	cli_credentials_set_password(creds, password, CRED_SPECIFIED);

	ldb_set_opaque(proxy->upstream, "credentials", creds);

	res = ldb_connect(proxy->upstream, url, 0, NULL);
	if (res != 0) {
		ldb_debug(module->ldb, LDB_DEBUG_FATAL, "proxy failed to connect to %s\n", url);
		goto failed;
	}

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "proxy connected to %s\n", url);

	talloc_free(msg);

	return 0;

failed:
	talloc_free(msg);
	talloc_free(proxy->olddn);
	talloc_free(proxy->newdn);
	talloc_free(proxy->upstream);
	proxy->upstream = NULL;
	return -1;
}


/*
  convert a binary blob
*/
static void proxy_convert_blob(TALLOC_CTX *mem_ctx, struct ldb_val *v,
			       const char *oldstr, const char *newstr)
{
	int len1, len2, len3;
	uint8_t *olddata = v->data;
	char *p = strcasestr((char *)v->data, oldstr);

	len1 = (p - (char *)v->data);
	len2 = strlen(newstr);
	len3 = v->length - (p+strlen(oldstr) - (char *)v->data);
	v->length = len1+len2+len3;
	v->data = talloc_size(mem_ctx, v->length);
	memcpy(v->data, olddata, len1);
	memcpy(v->data+len1, newstr, len2);
	memcpy(v->data+len1+len2, olddata + len1 + strlen(oldstr), len3);
}

/*
  convert a returned value
*/
static void proxy_convert_value(struct ldb_module *module, struct ldb_message *msg, struct ldb_val *v)
{
	struct proxy_data *proxy = talloc_get_type(module->private_data, struct proxy_data);
	int i;
	for (i=0;proxy->oldstr[i];i++) {
		char *p = strcasestr((char *)v->data, proxy->oldstr[i]);
		if (p == NULL) continue;
		proxy_convert_blob(msg, v, proxy->oldstr[i], proxy->newstr[i]);
	}
}


/*
  convert a returned value
*/
static struct ldb_parse_tree *proxy_convert_tree(struct ldb_module *module, 
						 struct ldb_parse_tree *tree)
{
	struct proxy_data *proxy = talloc_get_type(module->private_data, struct proxy_data);
	int i;
	char *expression = ldb_filter_from_tree(module, tree);
	for (i=0;proxy->newstr[i];i++) {
		struct ldb_val v;
		char *p = strcasestr(expression, proxy->newstr[i]);
		if (p == NULL) continue;
		v.data = (uint8_t *)expression;
		v.length = strlen(expression)+1;
		proxy_convert_blob(module, &v, proxy->newstr[i], proxy->oldstr[i]);
		return ldb_parse_tree(module, (const char *)v.data);
	}
	return tree;
}



/*
  convert a returned record
*/
static void proxy_convert_record(struct ldb_module *module, struct ldb_message *msg)
{
	struct proxy_data *proxy = talloc_get_type(module->private_data, struct proxy_data);
	int attr, v;
	
	/* fix the message DN */
	if (ldb_dn_compare_base(module->ldb, proxy->olddn, msg->dn) == 0) {
		struct ldb_dn *newdn = ldb_dn_copy(msg, msg->dn);
		newdn->comp_num -= proxy->olddn->comp_num;
		msg->dn = ldb_dn_compose(msg, newdn, proxy->newdn);
	}

	/* fix any attributes */
	for (attr=0;attr<msg->num_elements;attr++) {
		for (v=0;v<msg->elements[attr].num_values;v++) {
			proxy_convert_value(module, msg, &msg->elements[attr].values[v]);
		}
	}

	/* fix any DN components */
	for (attr=0;attr<msg->num_elements;attr++) {
		for (v=0;v<msg->elements[attr].num_values;v++) {
			proxy_convert_value(module, msg, &msg->elements[attr].values[v]);
		}
	}
}

/* search */
static int proxy_search_bytree(struct ldb_module *module, const struct ldb_dn *base,
			       enum ldb_scope scope, struct ldb_parse_tree *tree,
			       const char * const *attrs, struct ldb_message ***res)
{
	struct proxy_data *proxy = talloc_get_type(module->private_data, struct proxy_data);
	struct ldb_dn *newbase;
	int ret, i;

	if (base == NULL || (base->comp_num == 1 && base->components[0].name[0] == '@')) {
		goto passthru;
	}

	if (load_proxy_info(module) != 0) {
		return -1;
	}

	/* see if the dn is within olddn */
	if (ldb_dn_compare_base(module->ldb, proxy->newdn, base) != 0) {
		goto passthru;
	}

	tree = proxy_convert_tree(module, tree);

	/* convert the basedn of this search */
	newbase = ldb_dn_copy(proxy, base);
	if (newbase == NULL) {
		goto failed;
	}
	newbase->comp_num -= proxy->newdn->comp_num;
	newbase = ldb_dn_compose(proxy, newbase, proxy->olddn);

	ldb_debug(module->ldb, LDB_DEBUG_FATAL, "proxying: '%s' with dn '%s' \n", 
		  ldb_filter_from_tree(proxy, tree), ldb_dn_linearize(proxy, newbase));
	for (i=0;attrs && attrs[i];i++) {
		ldb_debug(module->ldb, LDB_DEBUG_FATAL, "attr: '%s'\n", attrs[i]);
	}
	
	ret = ldb_search_bytree(proxy->upstream, newbase, scope, tree, attrs, res);
	if (ret == -1) {
		ldb_set_errstring(module, talloc_strdup(module, ldb_errstring(proxy->upstream)));
		return -1;
	}

	for (i=0;i<ret;i++) {
		struct ldb_ldif ldif;
		printf("# record %d\n", i+1);
		
		proxy_convert_record(module, (*res)[i]);

		ldif.changetype = LDB_CHANGETYPE_NONE;
		ldif.msg = (*res)[i];
	}

	return ret;

failed:
	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "proxy failed for %s\n", 
		  ldb_dn_linearize(proxy, base));

passthru:
	return ldb_next_search_bytree(module, base, scope, tree, attrs, res); 
}


static const struct ldb_module_ops proxy_ops = {
	.name		   = "proxy",
	.search_bytree     = proxy_search_bytree
};

#ifdef HAVE_DLOPEN_DISABLED
struct ldb_module *init_module(struct ldb_context *ldb, const char *options[])
#else
struct ldb_module *proxy_module_init(struct ldb_context *ldb, const char *options[])
#endif
{
	struct ldb_module *ctx;

	ctx = talloc(ldb, struct ldb_module);
	if (!ctx)
		return NULL;

	ctx->ldb = ldb;
	ctx->prev = ctx->next = NULL;
	ctx->ops = &proxy_ops;

	ctx->private_data = talloc_zero(ctx, struct proxy_data);
	if (ctx->private_data == NULL) {
		return NULL;
	}

	return ctx;
}
