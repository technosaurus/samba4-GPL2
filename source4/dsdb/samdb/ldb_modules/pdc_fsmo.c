/* 
   Unix SMB/CIFS mplementation.

   The module that handles the PDC FSMO Role Owner checkings
   
   Copyright (C) Stefan Metzmacher 2007
    
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
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/ldb/include/ldb_private.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "lib/util/dlinklist.h"

static int pdc_fsmo_init(struct ldb_module *module)
{
	TALLOC_CTX *mem_ctx;
	struct ldb_dn *pdc_dn;
	struct dsdb_pdc_fsmo *pdc_fsmo;
	struct ldb_result *pdc_res;
	int ret;
	static const char *pdc_attrs[] = {
		"fSMORoleOwner",
		NULL
	};

	mem_ctx = talloc_new(module);
	if (!mem_ctx) {
		ldb_oom(module->ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	pdc_dn = samdb_base_dn(module->ldb);
	if (!pdc_dn) {
		ldb_debug(module->ldb, LDB_DEBUG_WARNING,
			  "pdc_fsmo_init: no domain dn present: (skip loading of domain details)\n");
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	}

	pdc_fsmo = talloc_zero(mem_ctx, struct dsdb_pdc_fsmo);
	if (!pdc_fsmo) {
		ldb_oom(module->ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	module->private_data = pdc_fsmo;

	ret = ldb_search(module->ldb, pdc_dn,
			 LDB_SCOPE_BASE,
			 NULL, pdc_attrs,
			 &pdc_res);
	if (ret != LDB_SUCCESS) {
		ldb_debug_set(module->ldb, LDB_DEBUG_FATAL,
			      "pdc_fsmo_init: failed to search the domain object: %d:%s\n",
			      ret, ldb_strerror(ret));
		talloc_free(mem_ctx);
		return ret;
	}
	talloc_steal(mem_ctx, pdc_res);
	if (pdc_res->count == 0) {
		ldb_debug(module->ldb, LDB_DEBUG_WARNING,
			  "pdc_fsmo_init: no domain object present: (skip loading of domain details)\n");
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	} else if (pdc_res->count > 1) {
		ldb_debug_set(module->ldb, LDB_DEBUG_FATAL,
			      "pdc_fsmo_init: [%u] domain objects found on a base search\n",
			      pdc_res->count);
		talloc_free(mem_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	pdc_fsmo->master_dn = ldb_msg_find_attr_as_dn(module->ldb, mem_ctx, pdc_res->msgs[0], "fSMORoleOwner");
	if (ldb_dn_compare(samdb_ntds_settings_dn(module->ldb), pdc_fsmo->master_dn) == 0) {
		pdc_fsmo->we_are_master = true;
	} else {
		pdc_fsmo->we_are_master = false;
	}

	if (ldb_set_opaque(module->ldb, "dsdb_pdc_fsmo", pdc_fsmo) != LDB_SUCCESS) {
		ldb_oom(module->ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	talloc_steal(module, pdc_fsmo);

	ldb_debug(module->ldb, LDB_DEBUG_TRACE,
			  "pdc_fsmo_init: we are master: %s\n",
			  (pdc_fsmo->we_are_master?"yes":"no"));

	talloc_free(mem_ctx);
	return ldb_next_init(module);
}

static const struct ldb_module_ops pdc_fsmo_ops = {
	.name		= "pdc_fsmo",
	.init_context	= pdc_fsmo_init
};

int pdc_fsmo_module_init(void)
{
	return ldb_register_module(&pdc_fsmo_ops);
}
