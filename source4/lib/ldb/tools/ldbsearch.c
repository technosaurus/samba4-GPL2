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
 *  Component: ldbsearch
 *
 *  Description: utility for ldb search - modelled on ldapsearch
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/includes.h"
#include "ldb/tools/cmdline.h"

static void usage(void)
{
	printf("Usage: ldbsearch <options> <expression> <attrs...>\n");
	printf("Options:\n");
	printf("  -H ldb_url       choose the database (or $LDB_URL)\n");
	printf("  -s base|sub|one  choose search scope\n");
	printf("  -b basedn        choose baseDN\n");
	printf("  -i               read search expressions from stdin\n");
        printf("  -S               sort returned attributes\n");
	printf("  -o options       pass options like modules to activate\n");
	printf("              e.g: -o modules:timestamps\n");
	exit(1);
}

static int do_compare_msg(struct ldb_message **el1,
			  struct ldb_message **el2,
			  void *opaque)
{
	struct ldb_context *ldb = talloc_get_type(opaque, struct ldb_context);
	return ldb_dn_compare(ldb, (*el1)->dn, (*el2)->dn);
}

struct async_context {
	struct ldb_control **req_ctrls;

	int sort;
	int num_stored;
	struct ldb_message **store;
	char **refs_store;

	int entries;
	int refs;

	int pending;
	int status;
};

static int store_message(struct ldb_message *msg, struct async_context *actx) {

	actx->store = talloc_realloc(actx, actx->store, struct ldb_message *, actx->num_stored + 2);
	if (!actx->store) {
		fprintf(stderr, "talloc_realloc failed while storing messages\n");
		return -1;
	}

	actx->store[actx->num_stored] = talloc_steal(actx->store, msg);
	if (!actx->store[actx->num_stored]) {
		fprintf(stderr, "talloc_steal failed while storing messages\n");
		return -1;
	}

	actx->num_stored++;
	actx->store[actx->num_stored] = NULL;

	return 0;
}

static int store_referral(char *referral, struct async_context *actx) {

	actx->refs_store = talloc_realloc(actx, actx->refs_store, char *, actx->refs + 2);
	if (!actx->refs_store) {
		fprintf(stderr, "talloc_realloc failed while storing referrals\n");
		return -1;
	}

	actx->refs_store[actx->refs] = talloc_steal(actx->refs_store, referral);
	if (!actx->refs_store[actx->refs]) {
		fprintf(stderr, "talloc_steal failed while storing referrals\n");
		return -1;
	}

	actx->refs++;
	actx->refs_store[actx->refs] = NULL;

	return 0;
}

static int display_message(struct ldb_context *ldb, struct ldb_message *msg, struct async_context *actx) {
	struct ldb_ldif ldif;

	actx->entries++;
	printf("# record %d\n", actx->entries);

	ldif.changetype = LDB_CHANGETYPE_NONE;
	ldif.msg = msg;

       	if (actx->sort) {
	/*
	 * Ensure attributes are always returned in the same
	 * order.  For testing, this makes comparison of old
	 * vs. new much easier.
	 */
        	ldb_msg_sort_elements(ldif.msg);
       	}

	ldb_ldif_write_file(ldb, stdout, &ldif);

	return 0;
}

static int display_referral(char *referral, struct async_context *actx) {

	actx->refs++;
	printf("# Referral\nref: %s\n\n", referral);

	return 0;
}

static int search_callback(struct ldb_context *ldb, void *context, struct ldb_async_result *ares)
{
	struct async_context *actx = talloc_get_type(context, struct async_context);
	int ret;
	
	switch (ares->type) {

	case LDB_REPLY_ENTRY:
		if (actx->sort) {
			ret = store_message(ares->message, actx);
		} else {
			ret = display_message(ldb, ares->message, actx);
		}
		break;

	case LDB_REPLY_REFERRAL:
		if (actx->sort) {
			ret = store_referral(ares->referral, actx);
		} else {
			ret = display_referral(ares->referral, actx);
		}
		break;

	case LDB_REPLY_DONE:
		if (ares->controls) {
			if (handle_controls_reply(ares->controls, actx->req_ctrls) == 1)
				actx->pending = 1;
		}
		ret = 0;
		break;
		
	default:
		fprintf(stderr, "unknown Reply Type\n");
		return LDB_ERR_OTHER;
	}

	if (talloc_free(ares) == -1) {
		fprintf(stderr, "talloc_free failed\n");
		actx->pending = 0;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ret) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

static int do_search(struct ldb_context *ldb,
		     const struct ldb_dn *basedn,
		     struct ldb_cmdline *options,
		     const char *expression,
		     const char * const *attrs)
{
	struct ldb_request *req;
	struct async_context *actx;
	int ret;

	req = talloc(ldb, struct ldb_request);
	if (!req) return -1;
	
	actx = talloc(req, struct async_context);
	if (!actx) return -1;

	actx->sort = options->sorted;
	actx->num_stored = 0;
	actx->store = NULL;
	actx->req_ctrls = parse_controls(ldb, options->controls);
	if (options->controls != NULL &&  actx->req_ctrls== NULL) return -1;
	actx->entries = 0;
	actx->refs = 0;

	req->operation = LDB_ASYNC_SEARCH;
	req->op.search.base = basedn;
	req->op.search.scope = options->scope;
	req->op.search.tree = ldb_parse_tree(ldb, expression);
	if (req->op.search.tree == NULL) return -1;
	req->op.search.attrs = attrs;
	req->controls = actx->req_ctrls;
	req->async.context = actx;
	req->async.callback = &search_callback;
	req->async.timeout = 3600; /* TODO: make this settable by command line */

again:
	actx->pending = 0;

	ret = ldb_request(ldb, req);
	if (ret != LDB_SUCCESS) {
		printf("search failed - %s\n", ldb_errstring(ldb));
		return -1;
	}

	ret = ldb_async_wait(req->async.handle, LDB_WAIT_ALL);
       	if (ret != LDB_SUCCESS) {
		printf("search error - %s\n", ldb_errstring(ldb));
		return -1;
	}

	if (actx->pending)
		goto again;

	if (actx->sort && actx->num_stored != 0) {
		int i;

		ldb_qsort(actx->store, ret, sizeof(struct ldb_message *),
			  ldb, (ldb_qsort_cmp_fn_t)do_compare_msg);

		if (ret != 0) {
			fprintf(stderr, "An error occurred while sorting messages\n");
			exit(1);
		}

		for (i = 0; i < actx->num_stored; i++) {
			display_message(ldb, actx->store[i], actx);
		}

		for (i = 0; i < actx->refs; i++) {
			display_referral(actx->refs_store[i], actx);
		}
	}

	printf("# returned %d records\n# %d entries\n# %d referrals\n",
		actx->entries + actx->refs, actx->entries, actx->refs);

	talloc_free(req);

	return 0;
}

int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	struct ldb_dn *basedn = NULL;
	const char * const * attrs = NULL;
	struct ldb_cmdline *options;
	int ret = -1;
	const char *expression = "(|(objectClass=*)(distinguishedName=*))";

	ldb_global_init();

	ldb = ldb_init(NULL);

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	/* the check for '=' is for compatibility with ldapsearch */
	if (!options->interactive &&
	    options->argc > 0 && 
	    strchr(options->argv[0], '=')) {
		expression = options->argv[0];
		options->argv++;
		options->argc--;
	}

	if (options->argc > 0) {
		attrs = (const char * const *)(options->argv);
	}

	if (options->basedn != NULL) {
		basedn = ldb_dn_explode(ldb, options->basedn);
		if (basedn == NULL) {
			fprintf(stderr, "Invalid Base DN format\n");
			exit(1);
		}
	}

	if (options->interactive) {
		char line[1024];
		while (fgets(line, sizeof(line), stdin)) {
			if (do_search(ldb, basedn, options, line, attrs) == -1) {
				ret = -1;
			}
		}
	} else {
		ret = do_search(ldb, basedn, options, expression, attrs);
	}

	talloc_free(ldb);
	return ret;
}
