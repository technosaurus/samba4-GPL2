/* 
   Partitions ldb module

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006

   * NOTICE: this module is NOT released under the GNU LGPL license as
   * other ldb code. This module is release under the GNU GPL v2 or
   * later license.

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
 *  Name: ldb
 *
 *  Component: ldb partitions module
 *
 *  Description: Implement LDAP partitions
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb/include/includes.h"

struct partition {
	struct ldb_module *module;
	const char *backend;
	struct ldb_dn *dn;
};
struct partition_private_data {
	struct partition **partitions;
	struct ldb_dn **replicate;
};

struct partition_context {
	struct ldb_module *module;
	struct ldb_request *orig_req;

	struct ldb_request **down_req;
	int num_requests;
	int finished_requests;
};

static struct ldb_handle *partition_init_handle(struct ldb_request *req, struct ldb_module *module)
{
	struct partition_context *ac;
	struct ldb_handle *h;

	h = talloc_zero(req, struct ldb_handle);
	if (h == NULL) {
		ldb_set_errstring(module->ldb, talloc_asprintf(module, "Out of Memory"));
		return NULL;
	}

	h->module = module;

	ac = talloc_zero(h, struct partition_context);
	if (ac == NULL) {
		ldb_set_errstring(module->ldb, talloc_asprintf(module, "Out of Memory"));
		talloc_free(h);
		return NULL;
	}

	h->private_data = (void *)ac;

	ac->module = module;
	ac->orig_req = req;

	return h;
}

struct ldb_module *make_module_for_next_request(TALLOC_CTX *mem_ctx, 
						struct ldb_context *ldb,
						struct ldb_module *module) 
{
	struct ldb_module *current;
	static const struct ldb_module_ops ops; /* zero */
	current = talloc_zero(mem_ctx, struct ldb_module);
	if (current == NULL) {
		return module;
	}
	
	current->ldb = ldb;
	current->ops = &ops;
	current->prev = NULL;
	current->next = module;
	return current;
}

struct ldb_module *find_backend(struct ldb_module *module, struct ldb_request *req, const struct ldb_dn *dn)
{
	int i;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	/* Look at base DN */
	/* Figure out which partition it is under */
	/* Skip the lot if 'data' isn't here yet (initialistion) */
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		if (ldb_dn_compare_base(module->ldb, 
					data->partitions[i]->dn, 
					dn) == 0) {
			return make_module_for_next_request(req, module->ldb, data->partitions[i]->module);
		}
	}

	return module;
};


/*
  fire the caller's callback for every entry, but only send 'done' once.
*/
static int partition_search_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct partition_context *ac;

	if (!context || !ares) {
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "partition_search_callback: NULL Context or Result in 'search' callback"));
		goto error;
	}

	ac = talloc_get_type(context, struct partition_context);

	if (ares->type == LDB_REPLY_ENTRY) {
		return ac->orig_req->callback(ldb, ac->orig_req->context, ares);
	} else {
		ac->finished_requests++;
		if (ac->finished_requests == ac->num_requests) {
			return ac->orig_req->callback(ldb, ac->orig_req->context, ares);
		} else {
			talloc_free(ares);
			return LDB_SUCCESS;
		}
	}
error:
	talloc_free(ares);
	return LDB_ERR_OPERATIONS_ERROR;
}

/*
  only fire the 'last' callback, and only for START-TLS for now 
*/
static int partition_other_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct partition_context *ac;

	if (!context) {
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "partition_other_callback: NULL Context in 'other' callback"));
		goto error;
	}

	ac = talloc_get_type(context, struct partition_context);

	if (!ac->orig_req->callback) {
		talloc_free(ares);
		return LDB_SUCCESS;
	}

	if (!ares 
	    || (ares->type == LDB_REPLY_EXTENDED 
		&& strcmp(ares->response->oid, LDB_EXTENDED_START_TLS_OID))) {
		ac->finished_requests++;
		if (ac->finished_requests == ac->num_requests) {
			return ac->orig_req->callback(ldb, ac->orig_req->context, ares);
		}
		talloc_free(ares);
		return LDB_SUCCESS;
	}
	ldb_set_errstring(ldb, talloc_asprintf(ldb, "partition_other_callback: Unknown reply type, only supports START_TLS"));
error:
	talloc_free(ares);
	return LDB_ERR_OPERATIONS_ERROR;
}


static int partition_send_request(struct partition_context *ac, struct ldb_module *partition)
{
	int ret;
	struct ldb_module *next = make_module_for_next_request(ac->module, ac->module->ldb, partition);
	
	ac->down_req = talloc_realloc(ac, ac->down_req, 
					struct ldb_request *, ac->num_requests + 1);
	if (!ac->down_req) {
		ldb_set_errstring(ac->module->ldb, talloc_asprintf(ac->module->ldb, "Out of memory!"));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->down_req[ac->num_requests] = talloc(ac, struct ldb_request);
	if (ac->down_req[ac->num_requests] == NULL) {
		ldb_set_errstring(ac->module->ldb, talloc_asprintf(ac->module->ldb, "Out of memory!"));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	*ac->down_req[ac->num_requests] = *ac->orig_req; /* copy the request */
	
	if (ac->down_req[ac->num_requests]->operation == LDB_SEARCH) {
		ac->down_req[ac->num_requests]->callback = partition_search_callback;
		ac->down_req[ac->num_requests]->context = ac;
	} else {
		ac->down_req[ac->num_requests]->callback = partition_other_callback;
		ac->down_req[ac->num_requests]->context = ac;
	}

	/* Spray off search requests to all backends */
	ret = ldb_next_request(next, ac->down_req[ac->num_requests]); 
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	ac->num_requests++;
	return LDB_SUCCESS;
}

/* Send a request down to all the partitions */
static int partition_send_all(struct ldb_module *module, 
			      struct partition_context *ac, struct ldb_request *req) 
{
	int i;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	int ret = partition_send_request(ac, module->next);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		ret = partition_send_request(ac, data->partitions[i]->module);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return LDB_SUCCESS;
}

/* Figure out which backend a request needs to be aimed at.  Some
 * requests must be replicated to all backends */
static int partition_replicate(struct ldb_module *module, struct ldb_request *req, const struct ldb_dn *dn) 
{
	int i;
	struct ldb_module *backend;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	
	/* Is this a special DN, we need to replicate to every backend? */
	for (i=0; data->replicate && data->replicate[i]; i++) {
		if (ldb_dn_compare(module->ldb, 
				   data->replicate[i], 
				   dn) == 0) {
			struct ldb_handle *h;
			struct partition_context *ac;
			
			h = partition_init_handle(req, module);
			if (!h) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			/* return our own handle to deal with this call */
			req->handle = h;
			
			ac = talloc_get_type(h->private_data, struct partition_context);
			
			return partition_send_all(module, ac, req);
		}
	}

	/* Otherwise, we need to find the backend to fire it to */

	/* Find backend */
	backend = find_backend(module, req, dn);
	
	/* issue request */
	return ldb_next_request(backend, req);
	
}

/* search */
static int partition_search(struct ldb_module *module, struct ldb_request *req)
{
	/* Find backend */
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	/* issue request */

	/* (later) consider if we should be searching multiple
	 * partitions (for 'invisible' partition behaviour */
	if (ldb_get_opaque(module->ldb, "global_catalog")) {
		int ret, i;
		struct ldb_handle *h;
		struct partition_context *ac;
		
		h = partition_init_handle(req, module);
		if (!h) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		/* return our own handle to deal with this call */
		req->handle = h;
		
		ac = talloc_get_type(h->private_data, struct partition_context);
		
		for (i=0; data && data->partitions && data->partitions[i]; i++) {
			/* Find all partitions under the search base */
			if (ldb_dn_compare_base(module->ldb, 
						req->op.search.base,
						data->partitions[i]->dn) == 0) {
				ret = partition_send_request(ac, data->partitions[i]->module);
				if (ret != LDB_SUCCESS) {
					return ret;
				}
			}
		}

		/* Perhaps we didn't match any partitions.  Try the main partition, then all partitions */
		if (ac->num_requests == 0) {
			return partition_send_all(module, ac, req);
		}
		
		return LDB_SUCCESS;
	} else {
		struct ldb_module *backend = find_backend(module, req, req->op.search.base);
	
		return ldb_next_request(backend, req);
	}
}

/* add */
static int partition_add(struct ldb_module *module, struct ldb_request *req)
{
	return partition_replicate(module, req, req->op.add.message->dn);
}

/* modify */
static int partition_modify(struct ldb_module *module, struct ldb_request *req)
{
	return partition_replicate(module, req, req->op.mod.message->dn);
}

/* delete */
static int partition_delete(struct ldb_module *module, struct ldb_request *req)
{
	return partition_replicate(module, req, req->op.del.dn);
}

/* rename */
static int partition_rename(struct ldb_module *module, struct ldb_request *req)
{
	/* Find backend */
	struct ldb_module *backend = find_backend(module, req, req->op.rename.olddn);
	struct ldb_module *backend2 = find_backend(module, req, req->op.rename.newdn);

	if (backend->next != backend2->next) {
		return LDB_ERR_AFFECTS_MULTIPLE_DSAS;
	}

	return partition_replicate(module, req, req->op.rename.olddn);
}

/* start a transaction */
static int partition_start_trans(struct ldb_module *module)
{
	int i, ret;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	/* Look at base DN */
	/* Figure out which partition it is under */
	/* Skip the lot if 'data' isn't here yet (initialistion) */
	ret = ldb_next_start_trans(module);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next = make_module_for_next_request(module, module->ldb, data->partitions[i]->module);

		ret = ldb_next_start_trans(next);
		talloc_free(next);
		if (ret != LDB_SUCCESS) {
			/* Back it out, if it fails on one */
			for (i--; i >= 0; i--) {
				next = make_module_for_next_request(module, module->ldb, data->partitions[i]->module);
				ldb_next_del_trans(next);
				talloc_free(next);
			}
			return ret;
		}
	}
	return LDB_SUCCESS;
}

/* end a transaction */
static int partition_end_trans(struct ldb_module *module)
{
	int i, ret, ret2 = LDB_SUCCESS;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	ret = ldb_next_end_trans(module);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Look at base DN */
	/* Figure out which partition it is under */
	/* Skip the lot if 'data' isn't here yet (initialistion) */
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next = make_module_for_next_request(module, module->ldb, data->partitions[i]->module);
		
		ret = ldb_next_end_trans(next);
		talloc_free(next);
		if (ret != LDB_SUCCESS) {
			ret2 = ret;
		}
	}

	if (ret != LDB_SUCCESS) {
		/* Back it out, if it fails on one */
		for (i=0; data && data->partitions && data->partitions[i]; i++) {
			struct ldb_module *next = make_module_for_next_request(module, module->ldb, data->partitions[i]->module);
			ldb_next_del_trans(next);
			talloc_free(next);
		}
	}
	return ret;
}

/* delete a transaction */
static int partition_del_trans(struct ldb_module *module)
{
	int i, ret, ret2 = LDB_SUCCESS;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	ret = ldb_next_del_trans(module);
	if (ret != LDB_SUCCESS) {
		ret2 = ret;
	}

	/* Look at base DN */
	/* Figure out which partition it is under */
	/* Skip the lot if 'data' isn't here yet (initialistion) */
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next = make_module_for_next_request(module, module->ldb, data->partitions[i]->module);
		
		ret = ldb_next_del_trans(next);
		talloc_free(next);
		if (ret != LDB_SUCCESS) {
			ret2 = ret;
		}
	}
	return ret2;
}

static int partition_sequence_number(struct ldb_module *module, struct ldb_request *req)
{
	int i, ret;
	uint64_t seq_number = 0;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	ret = ldb_next_request(module, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	seq_number = seq_number + req->op.seq_num.seq_num;

	/* Look at base DN */
	/* Figure out which partition it is under */
	/* Skip the lot if 'data' isn't here yet (initialistion) */
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next = make_module_for_next_request(req, module->ldb, data->partitions[i]->module);
		
		ret = ldb_next_request(next, req);
		talloc_free(next);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
		seq_number = seq_number + req->op.seq_num.seq_num;
	}
	req->op.seq_num.seq_num = seq_number;
	return LDB_SUCCESS;
}

static int sort_compare(void *void1,
			void *void2, void *opaque)
{
	struct ldb_context *ldb = talloc_get_type(opaque, struct ldb_context);
	struct partition **pp1 = void1;
	struct partition **pp2 = void2;
	struct partition *partition1 = talloc_get_type(*pp1, struct partition);
	struct partition *partition2 = talloc_get_type(*pp2, struct partition);

	return ldb_dn_compare(ldb, partition1->dn, partition2->dn);
}

static int partition_init(struct ldb_module *module)
{
	int ret, i;
	TALLOC_CTX *mem_ctx = talloc_new(module);
	static const char *attrs[] = { "partition", "replicateEntries", NULL };
	struct ldb_result *res;
	struct ldb_message *msg;
	struct ldb_message_element *partition_attributes;
	struct ldb_message_element *replicate_attributes;

	struct partition_private_data *data;

	if (!mem_ctx) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	data = talloc(mem_ctx, struct partition_private_data);
	if (data == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_search(module->ldb, ldb_dn_explode(mem_ctx, "@PARTITION"),
			 LDB_SCOPE_BASE,
			 NULL, attrs,
			 &res);
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}
	talloc_steal(mem_ctx, res);
	if (res->count == 0) {
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	}

	if (res->count > 1) {
		talloc_free(mem_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	msg = res->msgs[0];

	partition_attributes = ldb_msg_find_element(msg, "partition");
	if (!partition_attributes) {
		ldb_set_errstring(module->ldb, 
				  talloc_asprintf(module, "partition_init: "
						  "no partitions specified"));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	data->partitions = talloc_array(data, struct partition *, partition_attributes->num_values + 1);
	if (!data->partitions) {
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	for (i=0; i < partition_attributes->num_values; i++) {
		char *base = talloc_strdup(data->partitions, (char *)partition_attributes->values[i].data);
		char *p = strchr(base, ':');
		if (!p) {
			ldb_set_errstring(module->ldb, 
					  talloc_asprintf(module, "partition_init: "
							  "invalid form for partition record (missing ':'): %s", base));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
		p[0] = '\0';
		p++;
		if (!p[0]) {
			ldb_set_errstring(module->ldb, 
					  talloc_asprintf(module, "partition_init: "
							  "invalid form for partition record (missing backend database): %s", base));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
		data->partitions[i] = talloc(data->partitions, struct partition);
		if (!data->partitions[i]) {
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		data->partitions[i]->dn = ldb_dn_explode(data->partitions[i], base);
		if (!data->partitions[i]->dn) {
			ldb_set_errstring(module->ldb, 
					  talloc_asprintf(module, "partition_init: "
							  "invalid DN in partition record: %s", base));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}

		data->partitions[i]->backend = private_path(data->partitions[i], p);
		ret = ldb_connect_backend(module->ldb, data->partitions[i]->backend, NULL, &data->partitions[i]->module);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	data->partitions[i] = NULL;

	/* sort these into order, most to least specific */
	ldb_qsort(data->partitions, partition_attributes->num_values, sizeof(*data->partitions), 
		  module->ldb, sort_compare);

	for (i=0; data->partitions[i]; i++) {
		struct ldb_request *req;
		req = talloc_zero(mem_ctx, struct ldb_request);
		if (req == NULL) {
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, "partition: Out of memory!\n");
			return LDB_ERR_OPERATIONS_ERROR;
		}
		
		req->operation = LDB_REQ_REGISTER_PARTITION;
		req->op.reg_partition.dn = data->partitions[i]->dn;
		
		ret = ldb_request(module->ldb, req);
		if (ret != LDB_SUCCESS) {
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, "partition: Unable to register partition with rootdse!\n");
			return LDB_ERR_OTHER;
		}
		talloc_free(req);
	}

	replicate_attributes = ldb_msg_find_element(msg, "replicateEntries");
	if (!replicate_attributes) {
		ldb_set_errstring(module->ldb, 
				  talloc_asprintf(module, "partition_init: "
						  "no entries to replicate specified"));
		data->replicate = NULL;
	} else {
		data->replicate = talloc_array(data, struct ldb_dn *, replicate_attributes->num_values + 1);
		if (!data->replicate) {
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		
		for (i=0; i < replicate_attributes->num_values; i++) {
			data->replicate[i] = ldb_dn_explode(data->replicate, replicate_attributes->values[i].data);
			if (!data->replicate[i]) {
				ldb_set_errstring(module->ldb, 
						  talloc_asprintf(module, "partition_init: "
								  "invalid DN in partition replicate record: %s", 
								  replicate_attributes->values[i].data));
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
		}
		data->replicate[i] = NULL;
	}

	module->private_data = data;
	talloc_steal(module, data);
	
	talloc_free(mem_ctx);
	return ldb_next_init(module);
}

static int partition_wait_none(struct ldb_handle *handle) {
	struct partition_context *ac;
	int ret;
	int i;
    
	if (!handle || !handle->private_data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	handle->state = LDB_ASYNC_PENDING;
	handle->status = LDB_SUCCESS;

	ac = talloc_get_type(handle->private_data, struct partition_context);

	for (i=0; i < ac->num_requests; i++) {
		ret = ldb_wait(ac->down_req[i]->handle, LDB_WAIT_NONE);
		
		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (ac->down_req[i]->handle->status != LDB_SUCCESS) {
			handle->status = ac->down_req[i]->handle->status;
			goto done;
		}
		
		if (ac->down_req[i]->handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}
	}

	ret = LDB_SUCCESS;

done:
	handle->state = LDB_ASYNC_DONE;
	return ret;
}


static int partition_wait_all(struct ldb_handle *handle) {

	int ret;

	while (handle->state != LDB_ASYNC_DONE) {
		ret = partition_wait_none(handle);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return handle->status;
}

static int partition_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	if (type == LDB_WAIT_ALL) {
		return partition_wait_all(handle);
	} else {
		return partition_wait_none(handle);
	}
}

static const struct ldb_module_ops partition_ops = {
	.name		   = "partition",
	.init_context	   = partition_init,
	.search            = partition_search,
	.add               = partition_add,
	.modify            = partition_modify,
	.del               = partition_delete,
	.rename            = partition_rename,
	.start_transaction = partition_start_trans,
	.end_transaction   = partition_end_trans,
	.del_transaction   = partition_del_trans,
	.sequence_number   = partition_sequence_number,
	.wait              = partition_wait
};

int ldb_partition_init(void)
{
	return ldb_register_module(&partition_ops);
}
