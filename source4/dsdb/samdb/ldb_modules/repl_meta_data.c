/* 
   ldb database library

   Copyright (C) Simo Sorce  2004-2006
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher 2007

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
 *  Component: ldb repl_meta_data module
 *
 *  Description: - add a unique objectGUID onto every new record,
 *               - handle whenCreated, whenChanged timestamps
 *               - handle uSNCreated, uSNChanged numbers
 *               - handle replPropertyMetaData attribute
 *
 *  Author: Simo Sorce
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/ldb/include/ldb_private.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"

struct replmd_replicated_request {
	struct ldb_module *module;
	struct ldb_handle *handle;
	struct ldb_request *orig_req;

	struct dsdb_extended_replicated_objects *objs;

	uint32_t index_current;

	struct {
		TALLOC_CTX *mem_ctx;
		struct ldb_request *search_req;
		struct ldb_message *search_msg;
		int search_ret;
		struct ldb_request *change_req;
		int change_ret;
	} sub;
};

static struct replmd_replicated_request *replmd_replicated_init_handle(struct ldb_module *module,
								       struct ldb_request *req,
								       struct dsdb_extended_replicated_objects *objs)
{
	struct replmd_replicated_request *ar;
	struct ldb_handle *h;

	h = talloc_zero(req, struct ldb_handle);
	if (h == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		return NULL;
	}

	h->module	= module;
	h->state	= LDB_ASYNC_PENDING;
	h->status	= LDB_SUCCESS;

	ar = talloc_zero(h, struct replmd_replicated_request);
	if (ar == NULL) {
		ldb_set_errstring(module->ldb, "Out of Memory");
		talloc_free(h);
		return NULL;
	}

	h->private_data	= ar;

	ar->module	= module;
	ar->handle	= h;
	ar->orig_req	= req;
	ar->objs	= objs;

	req->handle = h;

	return ar;
}

static struct ldb_message_element *replmd_find_attribute(const struct ldb_message *msg, const char *name)
{
	int i;

	for (i = 0; i < msg->num_elements; i++) {
		if (ldb_attr_cmp(name, msg->elements[i].name) == 0) {
			return &msg->elements[i];
		}
	}

	return NULL;
}

/*
  add a time element to a record
*/
static int add_time_element(struct ldb_message *msg, const char *attr, time_t t)
{
	struct ldb_message_element *el;
	char *s;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return 0;
	}

	s = ldb_timestring(msg, t);
	if (s == NULL) {
		return -1;
	}

	if (ldb_msg_add_string(msg, attr, s) != 0) {
		return -1;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return 0;
}

/*
  add a uint64_t element to a record
*/
static int add_uint64_element(struct ldb_message *msg, const char *attr, uint64_t v)
{
	struct ldb_message_element *el;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return 0;
	}

	if (ldb_msg_add_fmt(msg, attr, "%llu", (unsigned long long)v) != 0) {
		return -1;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return 0;
}

static int replmd_add_replicated(struct ldb_module *module, struct ldb_request *req, struct ldb_control *ctrl)
{
	struct ldb_control **saved_ctrls;
	int ret;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "replmd_add_replicated\n");

	if (!save_controls(ctrl, req, &saved_ctrls)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_next_request(module, req);
	req->controls = saved_ctrls;

	return ret;
}

static int replmd_add_originating(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_request *down_req;
	struct ldb_message_element *attribute;
	struct ldb_message *msg;
	struct ldb_val v;
	struct GUID guid;
	uint64_t seq_num;
	NTSTATUS nt_status;
	int ret;
	time_t t = time(NULL);

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "replmd_add_originating\n");

	if ((attribute = replmd_find_attribute(req->op.add.message, "objectGUID")) != NULL ) {
		return ldb_next_request(module, req);
	}

	down_req = talloc(req, struct ldb_request);
	if (down_req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*down_req = *req;

	/* we have to copy the message as the caller might have it as a const */
	down_req->op.add.message = msg = ldb_msg_copy_shallow(down_req, req->op.add.message);
	if (msg == NULL) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* a new GUID */
	guid = GUID_random();

	nt_status = ndr_push_struct_blob(&v, msg, &guid, 
					 (ndr_push_flags_fn_t)ndr_push_GUID);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_msg_add_value(msg, "objectGUID", &v, NULL);
	if (ret) {
		talloc_free(down_req);
		return ret;
	}
	
	if (add_time_element(msg, "whenCreated", t) != 0 ||
	    add_time_element(msg, "whenChanged", t) != 0) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(module->ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret == LDB_SUCCESS) {
		if (add_uint64_element(msg, "uSNCreated", seq_num) != 0 ||
		    add_uint64_element(msg, "uSNChanged", seq_num) != 0) {
			talloc_free(down_req);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ldb_set_timeout_from_prev_req(module->ldb, req, down_req);

	/* go on with the call chain */
	ret = ldb_next_request(module, down_req);

	/* do not free down_req as the call results may be linked to it,
	 * it will be freed when the upper level request get freed */
	if (ret == LDB_SUCCESS) {
		req->handle = down_req->handle;
	}

	return ret;
}

static int replmd_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_control *ctrl;

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	ctrl = get_control_from_list(req->controls, DSDB_CONTROL_REPLICATED_OBJECT_OID);
	if (ctrl) {
		/* handle replicated objects different */
		return replmd_add_replicated(module, req, ctrl);
	}

	return replmd_add_originating(module, req);
}

static int replmd_modify_replicated(struct ldb_module *module, struct ldb_request *req, struct ldb_control *ctrl)
{
	struct ldb_control **saved_ctrls;
	int ret;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "replmd_modify_replicated\n");

	if (!save_controls(ctrl, req, &saved_ctrls)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_next_request(module, req);
	req->controls = saved_ctrls;

	return ret;
}

static int replmd_modify_originating(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_request *down_req;
	struct ldb_message *msg;
	int ret;
	time_t t = time(NULL);
	uint64_t seq_num;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "replmd_modify_originating\n");

	down_req = talloc(req, struct ldb_request);
	if (down_req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*down_req = *req;

	/* we have to copy the message as the caller might have it as a const */
	down_req->op.mod.message = msg = ldb_msg_copy_shallow(down_req, req->op.mod.message);
	if (msg == NULL) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (add_time_element(msg, "whenChanged", t) != 0) {
		talloc_free(down_req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(module->ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret == LDB_SUCCESS) {
		if (add_uint64_element(msg, "uSNChanged", seq_num) != 0) {
			talloc_free(down_req);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ldb_set_timeout_from_prev_req(module->ldb, req, down_req);

	/* go on with the call chain */
	ret = ldb_next_request(module, down_req);

	/* do not free down_req as the call results may be linked to it,
	 * it will be freed when the upper level request get freed */
	if (ret == LDB_SUCCESS) {
		req->handle = down_req->handle;
	}

	return ret;
}

static int replmd_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_control *ctrl;

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	ctrl = get_control_from_list(req->controls, DSDB_CONTROL_REPLICATED_OBJECT_OID);
	if (ctrl) {
		/* handle replicated objects different */
		return replmd_modify_replicated(module, req, ctrl);
	}

	return replmd_modify_originating(module, req);
}

static int replmd_replicated_request_reply_helper(struct replmd_replicated_request *ar, int ret)
{
	struct ldb_reply *ares = NULL;

	ar->handle->status = ret;
	ar->handle->state = LDB_ASYNC_DONE;

	if (!ar->orig_req->callback) {
		return LDB_SUCCESS;
	}
	
	/* we're done and need to report the success to the caller */
	ares = talloc_zero(ar, struct ldb_reply);
	if (!ares) {
		ar->handle->status = LDB_ERR_OPERATIONS_ERROR;
		ar->handle->state = LDB_ASYNC_DONE;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ares->type	= LDB_REPLY_EXTENDED;
	ares->response	= NULL;

	return ar->orig_req->callback(ar->module->ldb, ar->orig_req->context, ares);
}

static int replmd_replicated_request_done(struct replmd_replicated_request *ar)
{
	return replmd_replicated_request_reply_helper(ar, LDB_SUCCESS);
}

static int replmd_replicated_request_error(struct replmd_replicated_request *ar, int ret)
{
	return replmd_replicated_request_reply_helper(ar, ret);
}

static int replmd_replicated_request_werror(struct replmd_replicated_request *ar, WERROR status)
{
	int ret = LDB_ERR_OTHER;
	/* TODO: do some error mapping */
	return replmd_replicated_request_reply_helper(ar, ret);
}

static int replmd_replicated_apply_next(struct replmd_replicated_request *ar);

static int replmd_replicated_apply_add_callback(struct ldb_context *ldb,
						void *private_data,
						struct ldb_reply *ares)
{
#ifdef REPLMD_FULL_ASYNC /* TODO: active this code when ldb support full async code */ 
	struct replmd_replicated_request *ar = talloc_get_type(private_data,
					       struct replmd_replicated_request);

	ar->sub.change_ret = ldb_wait(ar->sub.search_req->handle, LDB_WAIT_ALL);
	if (ar->sub.change_ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ar->sub.change_ret);
	}

	talloc_free(ar->sub.mem_ctx);
	ZERO_STRUCT(ar->sub);

	ar->index_current++;

	return replmd_replicated_apply_next(ar);
#else
	return LDB_SUCCESS;
#endif
}

static int replmd_replicated_apply_add(struct replmd_replicated_request *ar)
{
	NTSTATUS nt_status;
	struct ldb_message *msg;
	struct replPropertyMetaDataBlob *md;
	struct ldb_val md_value;
	uint32_t i;
	uint64_t seq_num;
	int ret;

	msg = ar->objs->objects[ar->index_current].msg;
	md = ar->objs->objects[ar->index_current].meta_data;

	ret = ldb_sequence_number(ar->module->ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = samdb_msg_add_uint64(ar->module->ldb, msg, msg, "uSNCreated", seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = samdb_msg_add_uint64(ar->module->ldb, msg, msg, "uSNChanged", seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	md = ar->objs->objects[ar->index_current].meta_data;
	for (i=0; i < md->ctr.ctr1.count; i++) {
		md->ctr.ctr1.array[i].local_usn = seq_num;
	}
	nt_status = ndr_push_struct_blob(&md_value, msg, md,
					 (ndr_push_flags_fn_t)ndr_push_replPropertyMetaDataBlob);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
	}
	ret = ldb_msg_add_value(msg, "replPropertyMetaData", &md_value, NULL);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = ldb_build_add_req(&ar->sub.change_req,
				ar->module->ldb,
				ar->sub.mem_ctx,
				msg,
				NULL,
				ar,
				replmd_replicated_apply_add_callback);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

#ifdef REPLMD_FULL_ASYNC /* TODO: active this code when ldb support full async code */ 
	return ldb_next_request(ar->module, ar->sub.change_req);
#else
	ret = ldb_next_request(ar->module, ar->sub.change_req);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	ar->sub.change_ret = ldb_wait(ar->sub.search_req->handle, LDB_WAIT_ALL);
	if (ar->sub.change_ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ar->sub.change_ret);
	}

	talloc_free(ar->sub.mem_ctx);
	ZERO_STRUCT(ar->sub);

	ar->index_current++;

	return LDB_SUCCESS;
#endif
}

static int replmd_replicated_apply_merge(struct replmd_replicated_request *ar)
{
#ifdef REPLMD_FULL_ASYNC /* TODO: active this code when ldb support full async code */ 
#error sorry replmd_replicated_apply_merge not implemented
#else
	ldb_debug(ar->module->ldb, LDB_DEBUG_FATAL,
		  "replmd_replicated_apply_merge: ignore [%u]\n",
		  ar->index_current);

	talloc_free(ar->sub.mem_ctx);
	ZERO_STRUCT(ar->sub);

	ar->index_current++;

	return LDB_SUCCESS;
#endif
}

static int replmd_replicated_apply_search_callback(struct ldb_context *ldb,
						   void *private_data,
						   struct ldb_reply *ares)
{
	struct replmd_replicated_request *ar = talloc_get_type(private_data,
					       struct replmd_replicated_request);
	bool is_done = false;

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		ar->sub.search_msg = talloc_steal(ar->sub.mem_ctx, ares->message);
		break;
	case LDB_REPLY_REFERRAL:
		/* we ignore referrals */
		break;
	case LDB_REPLY_EXTENDED:
	case LDB_REPLY_DONE:
		is_done = true;
	}

	talloc_free(ares);

#ifdef REPLMD_FULL_ASYNC /* TODO: active this code when ldb support full async code */ 
	if (is_done) {
		ar->sub.search_ret = ldb_wait(ar->sub.search_req->handle, LDB_WAIT_ALL);
		if (ar->sub.search_ret != LDB_SUCCESS) {
			return replmd_replicated_request_error(ar, ar->sub.search_ret);
		}
		if (ar->sub.search_msg) {
			return replmd_replicated_apply_merge(ar);
		}
		return replmd_replicated_apply_add(ar);
	}
#endif
	return LDB_SUCCESS;
}

static int replmd_replicated_apply_search(struct replmd_replicated_request *ar)
{
	int ret;
	char *tmp_str;
	char *filter;

	tmp_str = ldb_binary_encode(ar->sub.mem_ctx, ar->objs->objects[ar->index_current].guid_value);
	if (!tmp_str) return replmd_replicated_request_werror(ar, WERR_NOMEM);

	filter = talloc_asprintf(ar->sub.mem_ctx, "(objectGUID=%s)", tmp_str);
	if (!filter) return replmd_replicated_request_werror(ar, WERR_NOMEM);
	talloc_free(tmp_str);

	ret = ldb_build_search_req(&ar->sub.search_req,
				   ar->module->ldb,
				   ar->sub.mem_ctx,
				   ar->objs->partition_dn,
				   LDB_SCOPE_SUBTREE,
				   filter,
				   NULL,
				   NULL,
				   ar,
				   replmd_replicated_apply_search_callback);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

#ifdef REPLMD_FULL_ASYNC /* TODO: active this code when ldb support full async code */ 
	return ldb_next_request(ar->module, ar->sub.search_req);
#else
	ret = ldb_next_request(ar->module, ar->sub.search_req);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	ar->sub.search_ret = ldb_wait(ar->sub.search_req->handle, LDB_WAIT_ALL);
	if (ar->sub.search_ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ar->sub.search_ret);
	}
	if (ar->sub.search_msg) {
		return replmd_replicated_apply_merge(ar);
	}

	return replmd_replicated_apply_add(ar);
#endif
}

static int replmd_replicated_apply_next(struct replmd_replicated_request *ar)
{
	if (ar->index_current >= ar->objs->num_objects) {
		return replmd_replicated_request_done(ar);
	}

	ar->sub.mem_ctx = talloc_new(ar);
	if (!ar->sub.mem_ctx) return replmd_replicated_request_werror(ar, WERR_NOMEM);

	return replmd_replicated_apply_search(ar);
}

static int replmd_extended_replicated_objects(struct ldb_module *module, struct ldb_request *req)
{
	struct dsdb_extended_replicated_objects *objs;
	struct replmd_replicated_request *ar;

	ldb_debug(module->ldb, LDB_DEBUG_TRACE, "replmd_extended_replicated_objects\n");

	objs = talloc_get_type(req->op.extended.data, struct dsdb_extended_replicated_objects);
	if (!objs) {
		return LDB_ERR_PROTOCOL_ERROR;
	}

	ar = replmd_replicated_init_handle(module, req, objs);
	if (!ar) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

#ifdef REPLMD_FULL_ASYNC /* TODO: active this code when ldb support full async code */ 
	return replmd_replicated_apply_next(ar);
#else
	while (req->handle->state != LDB_ASYNC_DONE) {
		replmd_replicated_apply_next(ar);
	}

	return LDB_SUCCESS;
#endif
}

static int replmd_extended(struct ldb_module *module, struct ldb_request *req)
{
	if (strcmp(req->op.extended.oid, DSDB_EXTENDED_REPLICATED_OBJECTS_OID) == 0) {
		return replmd_extended_replicated_objects(module, req);
	}

	return ldb_next_request(module, req);
}

static int replmd_wait_none(struct ldb_handle *handle) {
	struct replmd_replicated_request *ar;
    
	if (!handle || !handle->private_data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ar = talloc_get_type(handle->private_data, struct replmd_replicated_request);
	if (!ar) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* we do only sync calls */
	if (handle->state != LDB_ASYNC_DONE) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return handle->status;
}

static int replmd_wait_all(struct ldb_handle *handle) {

	int ret;

	while (handle->state != LDB_ASYNC_DONE) {
		ret = replmd_wait_none(handle);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return handle->status;
}

static int replmd_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	if (type == LDB_WAIT_ALL) {
		return replmd_wait_all(handle);
	} else {
		return replmd_wait_none(handle);
	}
}

static const struct ldb_module_ops replmd_ops = {
	.name          = "repl_meta_data",
	.add           = replmd_add,
	.modify        = replmd_modify,
	.extended      = replmd_extended,
	.wait          = replmd_wait
};

int repl_meta_data_module_init(void)
{
	return ldb_register_module(&replmd_ops);
}
