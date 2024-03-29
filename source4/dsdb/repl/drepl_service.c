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

#include "includes.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/events/events.h"
#include "lib/messaging/irpc.h"
#include "dsdb/repl/drepl_service.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"

static WERROR dreplsrv_init_creds(struct dreplsrv_service *service)
{
	NTSTATUS status;

	status = auth_system_session_info(service, &service->system_session_info);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return WERR_OK;
}

static WERROR dreplsrv_connect_samdb(struct dreplsrv_service *service)
{
	const struct GUID *ntds_guid;
	struct drsuapi_DsBindInfo28 *bind_info28;

	service->samdb = samdb_connect(service, service->system_session_info);
	if (!service->samdb) {
		return WERR_DS_SERVICE_UNAVAILABLE;
	}

	ntds_guid = samdb_ntds_objectGUID(service->samdb);
	if (!ntds_guid) {
		return WERR_DS_SERVICE_UNAVAILABLE;
	}

	service->ntds_guid = *ntds_guid;

	bind_info28				= &service->bind_info28;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_BASE;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ASYNC_REPLICATION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_REMOVEAPI;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_MOVEREQ_V2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHG_COMPRESS;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V1;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_RESTORE_USN_OPTIMIZATION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_KCC_EXECUTE;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRY_V2;
#if 0
	if (s->domain_behavior_version == 2) {
		/* TODO: find out how this is really triggered! */
		bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_LINKED_VALUE_REPLICATION;
	}
#endif
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_INSTANCE_TYPE_NOT_REQ_ON_MOD;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_CRYPTO_BIND;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_REPL_INFO;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V01;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_TRANSITIVE_MEMBERSHIP;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADD_SID_HISTORY;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_00100000;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_MEMBERSHIPS2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V6;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_NONDOMAIN_NCS;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V5;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V6;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRYREPLY_V3;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V7;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_VERIFY_OBJECT;
#if 0 /* we don't support XPRESS compression yet */
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_XPRESS_COMPRESS;
#endif
	/* TODO: fill in site_guid */
	bind_info28->site_guid			= GUID_zero();
	/* TODO: find out how this is really triggered! */
	bind_info28->u1				= 0;
	bind_info28->repl_epoch			= 0;

	return WERR_OK;
}

/*
  startup the dsdb replicator service task
*/
static void dreplsrv_task_init(struct task_server *task)
{
	WERROR status;
	struct dreplsrv_service *service;
	uint32_t periodic_startup_interval;

	switch (lp_server_role()) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "dreplsrv: no DSDB replication required in standalone configuration");
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "dreplsrv: no DSDB replication required in domain member configuration");
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want DSDB replication */
		break;
	}

	task_server_set_title(task, "task[dreplsrv]");

	service = talloc_zero(task, struct dreplsrv_service);
	if (!service) {
		task_server_terminate(task, "dreplsrv_task_init: out of memory");
		return;
	}
	service->task		= task;
	service->startup_time	= timeval_current();
	task->private		= service;

	status = dreplsrv_init_creds(service);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to obtain server credentials: %s\n",
				      win_errstr(status)));
		return;
	}

	status = dreplsrv_connect_samdb(service);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to connect to local samdb: %s\n",
				      win_errstr(status)));
		return;
	}

	status = dreplsrv_load_partitions(service);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to load partitions: %s\n",
				      win_errstr(status)));
		return;
	}

	periodic_startup_interval	= lp_parm_int(-1, "dreplsrv", "periodic_startup_interval", 15); /* in seconds */
	service->periodic.interval	= lp_parm_int(-1, "dreplsrv", "periodic_interval", 300); /* in seconds */

	status = dreplsrv_periodic_schedule(service, periodic_startup_interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to periodic schedule: %s\n",
				      win_errstr(status)));
		return;
	}

	irpc_add_name(task->msg_ctx, "dreplsrv");
}

/*
  initialise the dsdb replicator service
 */
static NTSTATUS dreplsrv_init(struct event_context *event_ctx, const struct model_ops *model_ops)
{
	return task_server_startup(event_ctx, model_ops, dreplsrv_task_init);
}

/*
  register ourselves as a available server
*/
NTSTATUS server_service_drepl_init(void)
{
	return register_server_service("drepl", dreplsrv_init);
}
