/* 
   ldb database library

   Copyright (C) Simo Sorce  2006
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2006

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
 *  Component: objectClass sorting module
 *
 *  Description: sort the objectClass attribute into the class hierarchy
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb/include/includes.h"

struct oc_async_context {

	enum oc_step {OC_DO_REQ, OC_SEARCH_SELF, OC_DO_MOD} step;

	struct ldb_module *module;
	struct ldb_request *orig_req;

	struct ldb_request *down_req;

	struct ldb_request *search_req;
	struct ldb_async_result *search_res;

	struct ldb_request *mod_req;
};

static struct ldb_async_handle *oc_init_handle(struct ldb_request *req, struct ldb_module *module)
{
	struct oc_async_context *ac;
	struct ldb_async_handle *h;

	h = talloc_zero(req, struct ldb_async_handle);
	if (h == NULL) {
		ldb_set_errstring(module->ldb, talloc_asprintf(module, "Out of Memory"));
		return NULL;
	}

	h->module = module;

	ac = talloc_zero(h, struct oc_async_context);
	if (ac == NULL) {
		ldb_set_errstring(module->ldb, talloc_asprintf(module, "Out of Memory"));
		talloc_free(h);
		return NULL;
	}

	h->private_data = (void *)ac;

	h->state = LDB_ASYNC_INIT;
	h->status = LDB_SUCCESS;

	ac->module = module;
	ac->orig_req = req;

	return h;
}

static int objectclass_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_async_handle *h;
	struct oc_async_context *ac;
	struct ldb_message_element *objectClassAttr;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "objectclass_add\n");

	if (ldb_dn_is_special(req->op.add.message->dn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}
	
	objectClassAttr = ldb_msg_find_element(req->op.add.message, "objectClass");

	/* If no part of this add has an objectClass, then we don't
	 * need to make any changes. cn=rootdse doesn't have an objectClass */
	if (!objectClassAttr) {
		return ldb_next_request(module, req);
	}

	h = oc_init_handle(req, module);
	if (!h) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct oc_async_context);

	/* return or own handle to deal with this call */
	req->async.handle = h;

	/* prepare the first operation */
	ac->down_req = talloc_zero(ac, struct ldb_request);
	if (ac->down_req == NULL) {
		ldb_set_errstring(module->ldb, talloc_asprintf(module->ldb, "Out of memory!"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*(ac->down_req) = *req; /* copy the request */

	ac->down_req->async.context = NULL;
	ac->down_req->async.callback = NULL;
	ldb_set_timeout_from_prev_req(module->ldb, req, ac->down_req);

	ac->step = OC_DO_REQ;

	return ldb_next_request(module, ac->down_req);
}

static int objectclass_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_async_handle *h;
	struct oc_async_context *ac;
	struct ldb_message_element *objectClassAttr;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "objectclass_modify\n");

	if (ldb_dn_is_special(req->op.mod.message->dn)) { /* do not manipulate our control entries */
		return ldb_next_request(module, req);
	}
	
	objectClassAttr = ldb_msg_find_element(req->op.mod.message, "objectClass");

	/* If no part of this touches the objectClass, then we don't
	 * need to make any changes.  */
	/* If the only operation is the deletion of the objectClass then go on */
	if (!objectClassAttr) {
		return ldb_next_request(module, req);
	}

	h = oc_init_handle(req, module);
	if (!h) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct oc_async_context);

	/* return or own handle to deal with this call */
	req->async.handle = h;

	/* prepare the first operation */
	ac->down_req = talloc_zero(ac, struct ldb_request);
	if (ac->down_req == NULL) {
		ldb_set_errstring(module->ldb, talloc_asprintf(module->ldb, "Out of memory!"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*(ac->down_req) = *req; /* copy the request */

	ac->down_req->async.context = NULL;
	ac->down_req->async.callback = NULL;
	ldb_set_timeout_from_prev_req(module->ldb, req, ac->down_req);

	ac->step = OC_DO_REQ;

	return ldb_next_request(module, ac->down_req);
}

static int get_self_callback(struct ldb_context *ldb, void *context, struct ldb_async_result *ares)
{
	struct oc_async_context *ac;

	if (!context || !ares) {
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "NULL Context or Result in callback"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac = talloc_get_type(context, struct oc_async_context);

	/* we are interested only in the single reply (base search) we receive here */
	if (ares->type == LDB_REPLY_ENTRY) {
		if (ac->search_res != NULL) {
			ldb_set_errstring(ldb, talloc_asprintf(ldb, "Too many results"));
			talloc_free(ares);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ac->search_res = talloc_steal(ac, ares);
	} else {
		talloc_free(ares);
	}

	return LDB_SUCCESS;
}

static int objectclass_search_self(struct ldb_async_handle *h) {

	struct oc_async_context *ac;
	static const char * const attrs[] = { "objectClass", NULL };

	ac = talloc_get_type(h->private_data, struct oc_async_context);

	/* prepare the search operation */
	ac->search_req = talloc_zero(ac, struct ldb_request);
	if (ac->search_req == NULL) {
		ldb_debug(ac->module->ldb, LDB_DEBUG_ERROR, "Out of Memory!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->search_req->operation = LDB_SEARCH;
	ac->search_req->op.search.base = ac->orig_req->op.mod.message->dn;
	ac->search_req->op.search.scope = LDB_SCOPE_BASE;
	ac->search_req->op.search.tree = ldb_parse_tree(ac->module->ldb, NULL);
	if (ac->search_req->op.search.tree == NULL) {
		ldb_set_errstring(ac->module->ldb, talloc_asprintf(ac, "objectclass: Internal error producing null search"));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->search_req->op.search.attrs = attrs;
	ac->search_req->controls = NULL;
	ac->search_req->async.context = ac;
	ac->search_req->async.callback = get_self_callback;
	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->search_req);

	ac->step = OC_SEARCH_SELF;

	return ldb_next_request(ac->module, ac->search_req);
}

static int objectclass_do_mod(struct ldb_async_handle *h) {

	struct oc_async_context *ac;
	struct ldb_message_element *objectclass_element;
	struct ldb_message *msg;
	TALLOC_CTX *mem_ctx;
	struct class_list {
		struct class_list *prev, *next;
		const char *objectclass;
	};
	struct class_list *sorted = NULL, *parent_class = NULL,
		*subclass = NULL, *unsorted = NULL, *current, *poss_subclass;
	int i;
	int layer;
	int ret;
      
	ac = talloc_get_type(h->private_data, struct oc_async_context);

	mem_ctx = talloc_new(ac);
	if (mem_ctx == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->mod_req = talloc(ac, struct ldb_request);
	if (ac->mod_req == NULL) {
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->mod_req->operation = LDB_MODIFY;
	ac->mod_req->controls = NULL;
	ac->mod_req->async.context = ac;
	ac->mod_req->async.callback = NULL;
	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->mod_req);
	
	/* use a new message structure */
	ac->mod_req->op.mod.message = msg = ldb_msg_new(ac->mod_req);
	if (msg == NULL) {
		ldb_set_errstring(ac->module->ldb, talloc_asprintf(ac, "objectclass: could not create new modify msg"));
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* modify dn */
	msg->dn = ac->orig_req->op.mod.message->dn;

	/* This is now the objectClass list from the database */
	objectclass_element = ldb_msg_find_element(ac->search_res->message, 
						   "objectClass");
	if (!objectclass_element) {
		/* Perhaps the above was a remove?  Move along now, nothing to see here */
		talloc_free(mem_ctx);
		return LDB_SUCCESS;
	}
	
	/* DESIGN:
	 *
	 * We work on 4 different 'bins' (implemented here as linked lists):
	 *
	 * * sorted:       the eventual list, in the order we wish to push
	 *                 into the database.  This is the only ordered list.
	 *
	 * * parent_class: The current parent class 'bin' we are
	 *                 trying to find subclasses for
	 *
	 * * subclass:     The subclasses we have found so far
	 *
	 * * unsorted:     The remaining objectClasses
	 *
	 * The process is a matter of filtering objectClasses up from
	 * unsorted into sorted.  Order is irrelevent in the later 3 'bins'.
	 * 
	 * We start with 'top' (found and promoted to parent_class
	 * initially).  Then we find (in unsorted) all the direct
	 * subclasses of 'top'.  parent_classes is concatenated onto
	 * the end of 'sorted', and subclass becomes the list in
	 * parent_class.
	 *
	 * We then repeat, until we find no more subclasses.  Any left
	 * over classes are added to the end.
	 *
	 */

	/* Firstly, dump all the objectClass elements into the
	 * unsorted bin, except for 'top', which is special */
	for (i=0; i < objectclass_element->num_values; i++) {
		current = talloc(mem_ctx, struct class_list);
		if (!current) {
			ldb_set_errstring(ac->module->ldb, talloc_asprintf(ac, "objectclass: out of memory allocating objectclass list"));
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		current->objectclass = (const char *)objectclass_element->values[i].data;

		/* this is the root of the tree.  We will start
		 * looking for subclasses from here */
		if (ldb_attr_cmp("top", current->objectclass) == 0) {
			DLIST_ADD(parent_class, current);
		} else {
			DLIST_ADD(unsorted, current);
		}
	}

	/* DEBUGGING aid:  how many layers are we down now? */
	layer = 0;
	do {
		layer++;
		/* Find all the subclasses of classes in the
		 * parent_classes.  Push them onto the subclass list */

		/* Ensure we don't bother if there are no unsorted entries left */
		for (current = parent_class; unsorted && current; current = current->next) {
			const char **subclasses = ldb_subclass_list(ac->module->ldb, current->objectclass);

			/* Walk the list of possible subclasses in unsorted */
			for (poss_subclass = unsorted; poss_subclass; ) {
				struct class_list *next;
				
				/* Save the next pointer, as the DLIST_ macros will change poss_subclass->next */
				next = poss_subclass->next;

				for (i = 0; subclasses && subclasses[i]; i++) {
					if (ldb_attr_cmp(poss_subclass->objectclass, subclasses[i]) == 0) {
						DLIST_REMOVE(unsorted, poss_subclass);
						DLIST_ADD(subclass, poss_subclass);

						break;
					}
				}
				poss_subclass = next;
			}
		}

		/* Now push the parent_classes as sorted, we are done with
		these.  Add to the END of the list by concatenation */
		DLIST_CONCATENATE(sorted, parent_class, struct class_list *);

		/* and now find subclasses of these */
		parent_class = subclass;
		subclass = NULL;

		/* If we didn't find any subclasses we will fall out
		 * the bottom here */
	} while (parent_class);

	/* This shouldn't happen, and would break MMC, but we can't
	 * afford to loose objectClasses.  Perhaps there was no 'top',
	 * or some other schema error? 
	 *
	 * Detecting schema errors is the job of the schema module, so
	 * at this layer we just try not to loose data
 	 */
	DLIST_CONCATENATE(sorted, unsorted, struct class_list *);

	/* We must completely replace the existing objectClass entry.
	 * We could do a constrained add/del, but we are meant to be
	 * in a transaction... */

	ret = ldb_msg_add_empty(msg, "objectClass", LDB_FLAG_MOD_REPLACE);
	if (ret != LDB_SUCCESS) {
		ldb_set_errstring(ac->module->ldb, talloc_asprintf(ac, "objectclass: could not clear objectclass in modify msg"));
		talloc_free(mem_ctx);
		return ret;
	}
	
	/* Move from the linked list back into an ldb msg */
	for (current = sorted; current; current = current->next) {
		ret = ldb_msg_add_string(msg, "objectClass", current->objectclass);
		if (ret != LDB_SUCCESS) {
			ldb_set_errstring(ac->module->ldb, talloc_asprintf(ac, "objectclass: could not re-add sorted objectclass to modify msg"));
			talloc_free(mem_ctx);
			return ret;
		}
	}

	ret = ldb_msg_sanity_check(ac->module->ldb, msg);
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}


	h->state = LDB_ASYNC_INIT;
	h->status = LDB_SUCCESS;

	ac->step = OC_DO_MOD;

	talloc_free(mem_ctx);
	/* perform the search */
	return ldb_next_request(ac->module, ac->mod_req);
}

static int oc_async_wait(struct ldb_async_handle *handle) {
	struct oc_async_context *ac;
	int ret;
    
	if (!handle || !handle->private_data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	handle->state = LDB_ASYNC_PENDING;
	handle->status = LDB_SUCCESS;

	ac = talloc_get_type(handle->private_data, struct oc_async_context);

	switch (ac->step) {
	case OC_DO_REQ:
		ret = ldb_async_wait(ac->down_req->async.handle, LDB_WAIT_NONE);

		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (ac->down_req->async.handle->status != LDB_SUCCESS) {
			handle->status = ac->down_req->async.handle->status;
			goto done;
		}

		if (ac->down_req->async.handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}

		/* mods done, go on */
		return objectclass_search_self(handle);

	case OC_SEARCH_SELF:
		ret = ldb_async_wait(ac->search_req->async.handle, LDB_WAIT_NONE);

		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (ac->search_req->async.handle->status != LDB_SUCCESS) {
			handle->status = ac->search_req->async.handle->status;
			goto done;
		}

		if (ac->search_req->async.handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}

		/* self search done, go on */
		return objectclass_do_mod(handle);

	case OC_DO_MOD:
		ret = ldb_async_wait(ac->mod_req->async.handle, LDB_WAIT_NONE);

		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (ac->mod_req->async.handle->status != LDB_SUCCESS) {
			handle->status = ac->mod_req->async.handle->status;
			goto done;
		}

		if (ac->mod_req->async.handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}

		break;
		
	default:
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	ret = LDB_SUCCESS;

done:
	handle->state = LDB_ASYNC_DONE;
	return ret;
}

static int oc_async_wait_all(struct ldb_async_handle *handle) {

	int ret;

	while (handle->state != LDB_ASYNC_DONE) {
		ret = oc_async_wait(handle);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return handle->status;
}

static int objectclass_async_wait(struct ldb_async_handle *handle, enum ldb_async_wait_type type)
{
	if (type == LDB_WAIT_ALL) {
		return oc_async_wait_all(handle);
	} else {
		return oc_async_wait(handle);
	}
}

static const struct ldb_module_ops objectclass_ops = {
	.name		   = "objectclass",
	.add           = objectclass_add,
	.modify        = objectclass_modify,
	.async_wait    = objectclass_async_wait
};

int ldb_objectclass_init(void)
{
	return ldb_register_module(&objectclass_ops);
}

