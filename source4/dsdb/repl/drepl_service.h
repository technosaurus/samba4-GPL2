/* 
   Unix SMB/CIFS mplementation.
   DSDB replication service
   
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

#ifndef _DSDB_REPL_DREPL_SERVICE_H_
#define _DSDB_REPL_DREPL_SERVICE_H_

#include "librpc/gen_ndr/ndr_drsuapi_c.h"

struct dreplsrv_service;
struct dreplsrv_partition;

struct dreplsrv_drsuapi_connection {
	/*
	 * this pipe pointer is also the indicator
	 * for a valid connection
	 */
	struct dcerpc_pipe *pipe;

	DATA_BLOB gensec_skey;
	struct drsuapi_DsBindInfo28 remote_info28;
	struct policy_handle bind_handle;
};

struct dreplsrv_out_connection {
	struct dreplsrv_out_connection *prev, *next;

	struct dreplsrv_service *service;

	/*
	 * the binding for the outgoing connection
	 */
	struct dcerpc_binding *binding;

	/* the out going connection to the source dsa */
	struct dreplsrv_drsuapi_connection *drsuapi;
};

struct dreplsrv_partition_source_dsa {
	struct dreplsrv_partition_source_dsa *prev, *next;

	struct dreplsrv_partition *partition;

	/*
	 * the cached repsFrom value for this source dsa
	 *
	 * it needs to be updated after each DsGetNCChanges() call
	 * to the source dsa
	 *
	 * repsFrom1 == &_repsFromBlob.ctr.ctr1
	 */
	struct repsFromToBlob _repsFromBlob;
	struct repsFromTo1 *repsFrom1;

	/* the reference to the source_dsa and its outgoing connection */
	struct dreplsrv_out_connection *conn;
};

struct dreplsrv_partition {
	struct dreplsrv_partition *prev, *next;

	struct dreplsrv_service *service;

	/* the dn of the partition */
	struct ldb_dn *dn;
	struct drsuapi_DsReplicaObjectIdentifier nc;

	/* 
	 * uptodate vector needs to be updated before and after each DsGetNCChanges() call
	 *
	 * - before: we need to use our own invocationId together with our highestCommitedUsn
	 * - after: we need to merge in the remote uptodatevector, to avoid reading it again
	 */
	struct replUpToDateVectorCtr2 uptodatevector;
	struct drsuapi_DsReplicaCursorCtrEx uptodatevector_ex;

	/*
	 * a linked list of all source dsa's we replicate from
	 */
	struct dreplsrv_partition_source_dsa *sources;
};

struct dreplsrv_out_operation {
	struct dreplsrv_out_operation *prev, *next;

	struct dreplsrv_service *service;

	struct dreplsrv_partition_source_dsa *source_dsa;

	struct composite_context *creq;
};

struct dreplsrv_service {
	/* the whole drepl service is in one task */
	struct task_server *task;

	/* the time the service was started */
	struct timeval startup_time;

	/* 
	 * system session info
	 * with machine account credentials
	 */
	struct auth_session_info *system_session_info;

	/*
	 * a connection to the local samdb
	 */
	struct ldb_context *samdb;

	/* the guid of our NTDS Settings object, which never changes! */
	struct GUID ntds_guid;
	/*
	 * the struct holds the values used for outgoing DsBind() calls,
	 * so that we need to set them up only once
	 */
	struct drsuapi_DsBindInfo28 bind_info28;

	/* some stuff for periodic processing */
	struct {
		/*
		 * the interval between to periodic runs
		 */
		uint32_t interval;

		/*
		 * the timestamp for the next event,
		 * this is the timstamp passed to event_add_timed()
		 */
		struct timeval next_event;

		/* here we have a reference to the timed event the schedules the periodic stuff */
		struct timed_event *te;
	} periodic;

	/*
	 * the list of partitions we need to replicate
	 */
	struct dreplsrv_partition *partitions;

	/*
	 * the list of cached connections
	 */
	struct dreplsrv_out_connection *connections;

	struct {	
		/* the pointer to the current active operation */
		struct dreplsrv_out_operation *current;

		/* the list of pending operations */
		struct dreplsrv_out_operation *pending;
	} ops;
};

#include "dsdb/repl/drepl_out_helpers.h"
#include "dsdb/repl/drepl_service_proto.h"

#endif /* _DSDB_REPL_DREPL_SERVICE_H_ */
