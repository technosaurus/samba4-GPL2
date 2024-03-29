/* 
   Unix SMB/CIFS mplementation.

   The Module that loads some DSDB related things
   into memory. E.g. it loads the dsdb_schema struture
   
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

static int dsdb_cache_init(struct ldb_module *module)
{
	/* TODO: load the schema */
	return ldb_next_init(module);
}

static const struct ldb_module_ops dsdb_cache_ops = {
	.name		= "dsdb_cache",
	.init_context	= dsdb_cache_init
};

int dsdb_cache_module_init(void)
{
	return ldb_register_module(&dsdb_cache_ops);
}
