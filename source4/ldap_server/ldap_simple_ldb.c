/* 
   Unix SMB/CIFS implementation.
   LDAP server SIMPLE LDB implementation
   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Simo Sorce 2004
   
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
#include "ldap_parse.h"

/* TODO: samdb_context is not a pulblic struct */
struct samdb_context {
	struct ldb_context *ldb;
	struct samdb_context **static_ptr;
};


#define ALLOC_CHECK(ptr) do {\
	if (!(ptr)) {\
		return NT_STATUS_NO_MEMORY;\
	}\
} while(0)


static NTSTATUS sldb_Search(struct ldapsrv_partition *partition, struct ldapsrv_call *call,
				     struct ldap_SearchRequest *r)
{
	NTSTATUS status;
	void *local_ctx;
	struct ldap_dn *basedn;
	struct ldap_Result *done;
	struct ldap_SearchResEntry *ent;
	struct ldapsrv_reply *ent_r, *done_r;
	int result = 80;
	struct samdb_context *samdb;
	struct ldb_message **res;
	int i, j, y, count;
	enum ldb_scope scope = LDB_SCOPE_DEFAULT;
	const char **attrs = NULL;
	const char *errstr;

	local_ctx = talloc_named(call, 0, "sldb_Search local memory context");
	ALLOC_CHECK(local_ctx);

	samdb = samdb_connect(local_ctx);
	ALLOC_CHECK(samdb);

	basedn = ldap_parse_dn(local_ctx, r->basedn);
	ALLOC_CHECK(basedn);

	DEBUG(10, ("sldb_Search: basedn: [%s]\n", basedn->dn));
	DEBUG(10, ("sldb_Search: filter: [%s]\n", r->filter));

	switch (r->scope) {
		case LDAP_SEARCH_SCOPE_BASE:
			DEBUG(10,("sldb_Search: scope: [BASE]\n"));
			scope = LDB_SCOPE_BASE;
			break;
		case LDAP_SEARCH_SCOPE_SINGLE:
			DEBUG(10,("sldb_Search: scope: [ONE]\n"));
			scope = LDB_SCOPE_ONELEVEL;
			break;
		case LDAP_SEARCH_SCOPE_SUB:
			DEBUG(10,("sldb_Search: scope: [SUB]\n"));
			scope = LDB_SCOPE_SUBTREE;
			break;
	}

	if (r->num_attributes >= 1) {
		attrs = talloc_array_p(samdb, const char *, r->num_attributes+1);
		ALLOC_CHECK(attrs);

		for (i=0; i < r->num_attributes; i++) {
			DEBUG(10,("sldb_Search: attrs: [%s]\n",r->attributes[i]));
			attrs[i] = r->attributes[i];
		}
		attrs[i] = NULL;
	}

	ldb_set_alloc(samdb->ldb, talloc_realloc_fn, samdb);
	count = ldb_search(samdb->ldb, basedn->dn, scope, r->filter, attrs, &res);

	for (i=0; i < count; i++) {
		ent_r = ldapsrv_init_reply(call, LDAP_TAG_SearchResultEntry);
		ALLOC_CHECK(ent_r);

		ent = &ent_r->msg.r.SearchResultEntry;
		ent->dn = talloc_steal(ent_r, res[i]->dn);
		ent->num_attributes = 0;
		ent->attributes = NULL;
		if (res[i]->num_elements == 0) {
			goto queue_reply;
		}
		ent->num_attributes = res[i]->num_elements;
		ent->attributes = talloc_array_p(ent_r, struct ldap_attribute, ent->num_attributes);
		ALLOC_CHECK(ent->attributes);
		for (j=0; j < ent->num_attributes; j++) {
			ent->attributes[j].name = talloc_steal(ent->attributes, res[i]->elements[j].name);
			ent->attributes[j].num_values = 0;
			ent->attributes[j].values = NULL;
			if (r->attributesonly && (res[i]->elements[j].num_values == 0)) {
				continue;
			}
			ent->attributes[j].num_values = res[i]->elements[j].num_values;
			ent->attributes[j].values = talloc_array_p(ent->attributes,
							DATA_BLOB, ent->attributes[j].num_values);
			ALLOC_CHECK(ent->attributes[j].values);
			for (y=0; y < ent->attributes[j].num_values; y++) {
				ent->attributes[j].values[y].length = res[i]->elements[j].values[y].length;
				ent->attributes[j].values[y].data = talloc_steal(ent->attributes[j].values,
									res[i]->elements[j].values[y].data);
			}
		}
queue_reply:
		status = ldapsrv_queue_reply(call, ent_r);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	done_r = ldapsrv_init_reply(call, LDAP_TAG_SearchResultDone);
	ALLOC_CHECK(done_r);

	if (count > 0) {
		DEBUG(10,("sldb_Search: results: [%d]\n",count));
		result = 0;
		errstr = NULL;
	} else if (count == 0) {
		DEBUG(10,("sldb_Search: no results\n"));
		result = 32;
		errstr = talloc_strdup(done_r, ldb_errstring(samdb->ldb));
	} else if (count == -1) {
		DEBUG(10,("sldb_Search: error\n"));
		result = 1;
		errstr = talloc_strdup(done_r, ldb_errstring(samdb->ldb));
	}

	done = &done_r->msg.r.SearchResultDone;
	done->resultcode = result;
	done->dn = NULL;
	done->errormessage = errstr;
	done->referral = NULL;

	talloc_free(local_ctx);

	return ldapsrv_queue_reply(call, done_r);
}

static NTSTATUS sldb_Add(struct ldapsrv_partition *partition, struct ldapsrv_call *call,
				     struct ldap_AddRequest *r)
{
	void *local_ctx;
	struct ldap_dn *ldn;
	struct ldap_Result *add_result;
	struct ldapsrv_reply *add_reply;
	int ldb_ret;
	struct samdb_context *samdb;
	struct ldb_message *msg;
	int result = LDAP_SUCCESS;
	const char *errstr = NULL;
	int i,j;

	local_ctx = talloc_named(call, 0, "sldb_Add local memory context");
	ALLOC_CHECK(local_ctx);

	samdb = samdb_connect(local_ctx);
	ALLOC_CHECK(samdb);

	ldn = ldap_parse_dn(local_ctx, r->dn);
	if (!ldn) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(10, ("sldb_add: dn: [%s]\n", ldn->dn));

	msg = talloc_p(local_ctx, struct ldb_message);
	ALLOC_CHECK(msg);

	msg->dn = ldn->dn;
	msg->private_data = NULL;
	msg->num_elements = 0;
	msg->elements = NULL;

	if (r->num_attributes > 0) {
		msg->num_elements = r->num_attributes;
		msg->elements = talloc_array_p(msg, struct ldb_message_element, msg->num_elements);
		ALLOC_CHECK(msg->elements);

		for (i=0; i < msg->num_elements; i++) {
			msg->elements[i].name = discard_const_p(char, r->attributes[i].name);
			msg->elements[i].flags = 0;
			msg->elements[i].num_values = 0;
			msg->elements[i].values = NULL;
			
			if (r->attributes[i].num_values > 0) {
				msg->elements[i].num_values = r->attributes[i].num_values;
				msg->elements[i].values = talloc_array_p(msg, struct ldb_val, msg->elements[i].num_values);
				ALLOC_CHECK(msg->elements[i].values);

				for (j=0; j < msg->elements[i].num_values; j++) {
					if (!(r->attributes[i].values[j].length > 0)) {
						result = 80;
						goto invalid_input;
					}
					msg->elements[i].values[j].length = r->attributes[i].values[j].length;
					msg->elements[i].values[j].data = r->attributes[i].values[j].data;			
				}
			} else {
				result = 80;
				goto invalid_input;
			}
		}
	} else {
		result = 80;
		goto invalid_input;
	}

invalid_input:

	add_reply = ldapsrv_init_reply(call, LDAP_TAG_AddResponse);
	ALLOC_CHECK(add_reply);

	add_result = &add_reply->msg.r.AddResponse;
	add_result->dn = talloc_steal(add_reply, ldn->dn);

	if (result == LDAP_SUCCESS) {
		ldb_set_alloc(samdb->ldb, talloc_realloc_fn, samdb);
		ldb_ret = ldb_add(samdb->ldb, msg);
		if (ldb_ret == 0) {
			result = LDAP_SUCCESS;
			errstr = NULL;
		} else {
			/* currently we have no way to tell if there was an internal ldb error
		 	 * or if the object was not found, return the most probable error
		 	 */
			result = 1;
			errstr = talloc_strdup(add_reply, ldb_errstring(samdb->ldb));
		}
	} else {
		errstr = talloc_strdup(add_reply, "invalid input data");
	}

	add_result->resultcode = result;
	add_result->errormessage = errstr;
	add_result->referral = NULL;

	talloc_free(local_ctx);

	return ldapsrv_queue_reply(call, add_reply);
}

static NTSTATUS sldb_Del(struct ldapsrv_partition *partition, struct ldapsrv_call *call,
				     struct ldap_DelRequest *r)
{
	void *local_ctx;
	struct ldap_dn *ldn;
	struct ldap_Result *del_result;
	struct ldapsrv_reply *del_reply;
	int ldb_ret;
	struct samdb_context *samdb;
	const char *errstr = NULL;
	int result = LDAP_SUCCESS;

	local_ctx = talloc_named(call, 0, "sldb_Del local memory context");
	ALLOC_CHECK(local_ctx);

	samdb = samdb_connect(local_ctx);
	ldn = ldap_parse_dn(local_ctx, r->dn);
	ALLOC_CHECK(ldn);

	DEBUG(10, ("sldb_Del: dn: [%s]\n", ldn->dn));

	ldb_set_alloc(samdb->ldb, talloc_realloc_fn, samdb);
	ldb_ret = ldb_delete(samdb->ldb, ldn->dn);

	del_reply = ldapsrv_init_reply(call, LDAP_TAG_DelResponse);
	ALLOC_CHECK(del_reply);

	del_result = &del_reply->msg.r.DelResponse;
	del_result->dn = talloc_steal(del_reply, ldn->dn);

	if (ldb_ret == 0) {
		result = LDAP_SUCCESS;
		errstr = NULL;
	} else {
		/* currently we have no way to tell if there was an internal ldb error
		 * or if the object was not found, return the most probable error
		 */
		result = LDAP_NO_SUCH_OBJECT;
		errstr = talloc_strdup(del_reply, ldb_errstring(samdb->ldb));
	}

	del_result->resultcode = result;
	del_result->errormessage = errstr;
	del_result->referral = NULL;

	talloc_free(local_ctx);

	return ldapsrv_queue_reply(call, del_reply);
}

static NTSTATUS sldb_Modify(struct ldapsrv_partition *partition, struct ldapsrv_call *call,
				     struct ldap_ModifyRequest *r)
{
	void *local_ctx;
	struct ldap_dn *ldn;
	struct ldap_Result *modify_result;
	struct ldapsrv_reply *modify_reply;
	int ldb_ret;
	struct samdb_context *samdb;
	struct ldb_message *msg;
	int result = LDAP_SUCCESS;
	const char *errstr = NULL;
	int i,j;

	local_ctx = talloc_named(call, 0, "sldb_Modify local memory context");
	ALLOC_CHECK(local_ctx);

	samdb = samdb_connect(local_ctx);
	ALLOC_CHECK(samdb);

	ldn = ldap_parse_dn(local_ctx, r->dn);
	ALLOC_CHECK(ldn);

	DEBUG(10, ("sldb_modify: dn: [%s]\n", ldn->dn));

	msg = talloc_p(local_ctx, struct ldb_message);
	ALLOC_CHECK(msg);

	msg->dn = ldn->dn;
	msg->private_data = NULL;
	msg->num_elements = 0;
	msg->elements = NULL;

	if (r->num_mods > 0) {
		msg->num_elements = r->num_mods;
		msg->elements = talloc_array_p(msg, struct ldb_message_element, r->num_mods);
		ALLOC_CHECK(msg->elements);

		for (i=0; i < msg->num_elements; i++) {
			msg->elements[i].name = discard_const_p(char, r->mods[i].attrib.name);
			msg->elements[i].num_values = 0;
			msg->elements[i].values = NULL;

			switch (r->mods[i].type) {
			default:
				result = 2;
				goto invalid_input;
			case LDAP_MODIFY_ADD:
				msg->elements[i].flags = LDB_FLAG_MOD_ADD;
				break;
			case LDAP_MODIFY_DELETE:
				msg->elements[i].flags = LDB_FLAG_MOD_DELETE;
				break;
			case LDAP_MODIFY_REPLACE:
				msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
				break;
			}

			if (r->mods[i].attrib.num_values > 0) {
				msg->elements[i].num_values = r->mods[i].attrib.num_values;
				msg->elements[i].values = talloc_array_p(msg, struct ldb_val, msg->elements[i].num_values);
				ALLOC_CHECK(msg->elements[i].values);

				for (j=0; j < msg->elements[i].num_values; j++) {
					if (!(r->mods[i].attrib.values[j].length > 0)) {
						result = 80;
						goto invalid_input;
					}
					msg->elements[i].values[j].length = r->mods[i].attrib.values[j].length;
					msg->elements[i].values[j].data = r->mods[i].attrib.values[j].data;			
				}
			} else {
				/* TODO: test what we should do here 
				 *
				 *       LDAP_MODIFY_DELETE is ok to pass here
				 */
			}
		}
	} else {
		result = 80;
		goto invalid_input;
	}

invalid_input:

	modify_reply = ldapsrv_init_reply(call, LDAP_TAG_ModifyResponse);
	ALLOC_CHECK(modify_reply);

	modify_result = &modify_reply->msg.r.AddResponse;
	modify_result->dn = talloc_steal(modify_reply, ldn->dn);

	if (result == LDAP_SUCCESS) {
		ldb_set_alloc(samdb->ldb, talloc_realloc_fn, samdb);
		ldb_ret = ldb_modify(samdb->ldb, msg);
		if (ldb_ret == 0) {
			result = LDAP_SUCCESS;
			errstr = NULL;
		} else {
			/* currently we have no way to tell if there was an internal ldb error
		 	 * or if the object was not found, return the most probable error
		 	 */
			result = 1;
			errstr = talloc_strdup(modify_reply, ldb_errstring(samdb->ldb));
		}
	} else {
		errstr = talloc_strdup(modify_reply, "invalid input data");
	}

	modify_result->resultcode = result;
	modify_result->errormessage = errstr;
	modify_result->referral = NULL;

	talloc_free(local_ctx);

	return ldapsrv_queue_reply(call, modify_reply);
}

static NTSTATUS sldb_Compare(struct ldapsrv_partition *partition, struct ldapsrv_call *call,
				     struct ldap_CompareRequest *r)
{
	void *local_ctx;
	struct ldap_dn *ldn;
	struct ldap_Result *compare;
	struct ldapsrv_reply *compare_r;
	int result = 80;
	struct samdb_context *samdb;
	struct ldb_message **res;
	const char *attrs[1];
	const char *errstr;
	const char *dn;
	const char *filter;
	int count;

	local_ctx = talloc_named(call, 0, "sldb_Compare local_memory_context");
	ALLOC_CHECK(local_ctx);

	samdb = samdb_connect(local_ctx);
	ALLOC_CHECK(samdb);

	ldn = ldap_parse_dn(local_ctx, r->dn);
	ALLOC_CHECK(ldn);

	DEBUG(10, ("sldb_Compare: dn: [%s]\n", ldn->dn));
	filter = talloc_asprintf(local_ctx, "(%s=%*s)", r->attribute, r->value.length, r->value.data);
	ALLOC_CHECK(filter);

	DEBUGADD(10, ("sldb_Compare: attribute: [%s]\n", filter));

	attrs[0] = NULL;

	ldb_set_alloc(samdb->ldb, talloc_realloc_fn, samdb);
	count = ldb_search(samdb->ldb, dn, LDB_SCOPE_BASE, filter, attrs, &res);

	compare_r = ldapsrv_init_reply(call, LDAP_TAG_CompareResponse);
	ALLOC_CHECK(compare_r);

	if (count == 1) {
		DEBUG(10,("sldb_Compare: matched\n"));
		result = 6;
		errstr = NULL;
	} else if (count == 0) {
		DEBUG(10,("sldb_Compare: doesn't matched\n"));
		result = 5;
		errstr = NULL;
	} else if (count > 1) {
		result = 80;
		errstr = talloc_strdup(compare_r, "too many objects match");
		DEBUG(10,("sldb_Compare: %d results: %s\n", count, errstr));
	} else if (count == -1) {
		result = 1;
		errstr = talloc_strdup(compare_r, ldb_errstring(samdb->ldb));
		DEBUG(10,("sldb_Compare: error: %s\n", errstr));
	}

	compare = &compare_r->msg.r.CompareResponse;
	compare->resultcode = result;
	compare->dn = NULL;
	compare->errormessage = errstr;
	compare->referral = NULL;

	talloc_free(local_ctx);

	return ldapsrv_queue_reply(call, compare_r);
}

static const struct ldapsrv_partition_ops sldb_ops = {
	.Search		= sldb_Search,
	.Add		= sldb_Add,
	.Del		= sldb_Del,
	.Modify		= sldb_Modify,
	.Compare	= sldb_Compare
};

const struct ldapsrv_partition_ops *ldapsrv_get_sldb_partition_ops(void)
{
	return &sldb_ops;
}
