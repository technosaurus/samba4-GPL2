/* 
   Unix SMB/CIFS implementation.

   interface functions for the sam database

   Copyright (C) Andrew Tridgell 2004
   
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

#ifndef __SAMDB_H__
#define __SAMDB_H__

struct auth_session_info;
struct dsdb_control_current_partition;
struct dsdb_extended_replicated_object;
struct dsdb_extended_replicated_objects;

#include "librpc/gen_ndr/security.h"
#include "lib/ldb/include/ldb.h"
#include "librpc/gen_ndr/samr.h"
#include "librpc/gen_ndr/drsuapi.h"
#include "librpc/gen_ndr/drsblobs.h"
#include "dsdb/schema/schema.h"
#include "dsdb/samdb/samdb_proto.h"

#define DSDB_CONTROL_CURRENT_PARTITION_OID "1.3.6.1.4.1.7165.4.3.2"
struct dsdb_control_current_partition {
	/* 
	 * this is the version of the dsdb_control_current_partition
	 * version 0: initial implementation
	 */
#define DSDB_CONTROL_CURRENT_PARTITION_VERSION 0
	uint32_t version;

	struct ldb_dn *dn;

	const char *backend;

	struct ldb_module *module;
};

#define DSDB_EXTENDED_REPLICATED_OBJECTS_OID "1.3.6.1.4.1.7165.4.4.1"
struct dsdb_extended_replicated_object {
	struct ldb_message *msg;
	struct ldb_val guid_value;
	const char *when_changed;
	struct replPropertyMetaDataBlob *meta_data;
};

struct dsdb_extended_replicated_objects {
	/* 
	 * this is the version of the dsdb_extended_replicated_objects
	 * version 0: initial implementation
	 */
#define DSDB_EXTENDED_REPLICATED_OBJECTS_VERSION 0
	uint32_t version;

	struct ldb_dn *partition_dn;

	const struct repsFromTo1 *source_dsa;
	const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector;

	uint32_t num_objects;
	struct dsdb_extended_replicated_object *objects;
};

struct dsdb_schema_fsmo {
	bool we_are_master;
	struct ldb_dn *master_dn;
};

struct dsdb_naming_fsmo {
	bool we_are_master;
	struct ldb_dn *master_dn;
};

struct dsdb_pdc_fsmo {
	bool we_are_master;
	struct ldb_dn *master_dn;
};

#endif /* __SAMDB_H__ */
