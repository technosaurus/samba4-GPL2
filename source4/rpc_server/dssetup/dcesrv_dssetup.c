/* 
   Unix SMB/CIFS implementation.

   endpoint server for the dssetup pipe

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

#include "includes.h"
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_dssetup.h"
#include "rpc_server/common/common.h"


/* 
  ds_RolerGetPrimaryDomainInformation 
*/
static WERROR ds_RolerGetPrimaryDomainInformation(struct dcesrv_call_state *dce_call, 
						  TALLOC_CTX *mem_ctx,
						  struct ds_RolerGetPrimaryDomainInformation *r)
{
	WERROR err = WERR_OK;
	void *sam_ctx;
	const char * const attrs[] = { "dnsDomain", "objectGUID", "name", NULL };
	int ret;
	struct ldb_message **res;

	sam_ctx = samdb_connect(mem_ctx);
	if (sam_ctx == NULL) {
		return WERR_SERVER_UNAVAILABLE;
	}

	ret = samdb_search(sam_ctx, mem_ctx, NULL, &res, attrs,
			   "(&(objectClass=domainDNS)(!(objectClass=builtinDomain)))");
	if (ret != 1) {
		return WERR_SERVER_UNAVAILABLE;
	}

	switch (r->in.level) {
	case 1:
		r->out.info = talloc_p(mem_ctx, union ds_DomainInformation);
		if (r->out.info == NULL) {
			return WERR_NOMEM;
		}
		r->out.info->basic.role = 5;
		r->out.info->basic.flags = 0x01000003;
		r->out.info->basic.domain = samdb_result_string(res[0], "name", NULL);
		r->out.info->basic.dns_domain = samdb_result_string(res[0], "dnsDomain", NULL);
		r->out.info->basic.forest = samdb_result_string(res[0], "dnsDomain", NULL);
		r->out.info->basic.domain_guid = samdb_result_guid(res[0], "objectGUID");
		break;

	default:
		err = WERR_INVALID_PARAM;
		break;
	}

	return err;
}


/* 
  ds_RolerDnsNameToFlatName 
*/
static WERROR ds_RolerDnsNameToFlatName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerDnsNameToFlatName *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerDcAsDc 
*/
static WERROR ds_RolerDcAsDc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerDcAsDc *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerDcAsReplica 
*/
static WERROR ds_RolerDcAsReplica(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerDcAsReplica *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerDemoteDc 
*/
static WERROR ds_RolerDemoteDc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerDemoteDc *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerGetDcOperationProgress 
*/
static WERROR ds_RolerGetDcOperationProgress(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerGetDcOperationProgress *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerGetDcOperationResults 
*/
static WERROR ds_RolerGetDcOperationResults(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerGetDcOperationResults *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerCancel 
*/
static WERROR ds_RolerCancel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerCancel *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerServerSaveStateForUpgrade 
*/
static WERROR ds_RolerServerSaveStateForUpgrade(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerServerSaveStateForUpgrade *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerUpgradeDownlevelServer 
*/
static WERROR ds_RolerUpgradeDownlevelServer(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerUpgradeDownlevelServer *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_RolerAbortDownlevelServerUpgrade 
*/
static WERROR ds_RolerAbortDownlevelServerUpgrade(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_RolerAbortDownlevelServerUpgrade *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_dssetup_s.c"
