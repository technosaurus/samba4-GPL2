/* 
   Unix SMB/CIFS implementation.

   endpoint server for the wkssvc pipe

   Copyright (C) Stefan (metze) Metzmacher 2004
   
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
#include "rpc_server/common/common.h"

/* 
  wkssvc_NetWkstaGetInfo 
*/
static WERROR wkssvc_NetWkstaGetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct wkssvc_NetWkstaGetInfo *r)
{
	struct dcesrv_context *dce_ctx = dce_call->conn->dce_ctx;

	/* NOTE: win2k3 ignores r->in.server_name completly so we do --metze */

	switch(r->in.level) {
	case 100: {
			r->out.info.info100 = talloc_p(mem_ctx, struct wkssvc_NetWkstaInfo100);
			WERR_TALLOC_CHECK(r->out.info.info100);

			r->out.info.info100->platform_id = dcesrv_common_get_platform_id(mem_ctx, dce_ctx);
			r->out.info.info100->server = dcesrv_common_get_server_name(mem_ctx, dce_ctx);
			r->out.info.info100->domain = dcesrv_common_get_domain_name(mem_ctx, dce_ctx);
			r->out.info.info100->ver_major = dcesrv_common_get_version_major(mem_ctx, dce_ctx);
			r->out.info.info100->ver_minor = dcesrv_common_get_version_minor(mem_ctx, dce_ctx);
			break;
		}
	case 101: {
			r->out.info.info101 = talloc_p(mem_ctx, struct wkssvc_NetWkstaInfo101);
			WERR_TALLOC_CHECK(r->out.info.info101);

			r->out.info.info101->platform_id = dcesrv_common_get_platform_id(mem_ctx, dce_ctx);
			r->out.info.info101->server = dcesrv_common_get_server_name(mem_ctx, dce_ctx);
			r->out.info.info101->domain = dcesrv_common_get_domain_name(mem_ctx, dce_ctx);
			r->out.info.info101->ver_major = dcesrv_common_get_version_major(mem_ctx, dce_ctx);
			r->out.info.info101->ver_minor = dcesrv_common_get_version_minor(mem_ctx, dce_ctx);
			r->out.info.info101->lan_root = dcesrv_common_get_lan_root(mem_ctx, dce_ctx);
			break;
		}
	case 102: {
			r->out.info.info102 = NULL;

			return WERR_ACCESS_DENIED;
		}
	case 502: {	
			r->out.info.info502 = NULL;

			return WERR_ACCESS_DENIED;
		}
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_OK;
}


/* 
  wkssvc_NetWkstaSetInfo 
*/
static WERROR wkssvc_NetWkstaSetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct wkssvc_NetWkstaSetInfo *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRWKSTAUSERENUM 
*/
static WERROR WKSSVC_NETRWKSTAUSERENUM(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRWKSTAUSERENUM *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRWKSTAUSERGETINFO 
*/
static WERROR WKSSVC_NETRWKSTAUSERGETINFO(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRWKSTAUSERGETINFO *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRWKSTAUSERSETINFO 
*/
static WERROR WKSSVC_NETRWKSTAUSERSETINFO(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRWKSTAUSERSETINFO *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  wkssvc_NetWkstaTransportEnum 
*/
static WERROR wkssvc_NetWkstaTransportEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct wkssvc_NetWkstaTransportEnum *r)
{
	r->out.level = r->in.level;
	r->out.totalentries = 0;
	r->out.resume_handle = NULL;

	switch (r->in.level) {
	case 0:
		r->out.ctr.ctr0 = talloc_p(mem_ctx, struct wkssvc_NetWkstaTransportCtr0);
		WERR_TALLOC_CHECK(r->out.ctr.ctr0);

		r->out.ctr.ctr0->count = 0;
		r->out.ctr.ctr0->array = NULL;

		return WERR_NOT_SUPPORTED;

	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_OK;
}


/* 
  WKSSVC_NETRWKSTATRANSPORTADD 
*/
static WERROR WKSSVC_NETRWKSTATRANSPORTADD(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRWKSTATRANSPORTADD *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRWKSTATRANSPORTDEL 
*/
static WERROR WKSSVC_NETRWKSTATRANSPORTDEL(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRWKSTATRANSPORTDEL *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRUSEADD 
*/
static WERROR WKSSVC_NETRUSEADD(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRUSEADD *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRUSEGETINFO 
*/
static WERROR WKSSVC_NETRUSEGETINFO(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRUSEGETINFO *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRUSEDEL 
*/
static WERROR WKSSVC_NETRUSEDEL(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRUSEDEL *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRUSEENUM 
*/
static WERROR WKSSVC_NETRUSEENUM(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRUSEENUM *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRMESSAGEBUFFERSEND 
*/
static WERROR WKSSVC_NETRMESSAGEBUFFERSEND(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRMESSAGEBUFFERSEND *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRWORKSTATIONSTATISTICSGET 
*/
static WERROR WKSSVC_NETRWORKSTATIONSTATISTICSGET(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRWORKSTATIONSTATISTICSGET *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRLOGONDOMAINNAMEADD 
*/
static WERROR WKSSVC_NETRLOGONDOMAINNAMEADD(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRLOGONDOMAINNAMEADD *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRLOGONDOMAINNAMEDEL 
*/
static WERROR WKSSVC_NETRLOGONDOMAINNAMEDEL(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRLOGONDOMAINNAMEDEL *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRJOINDOMAIN 
*/
static WERROR WKSSVC_NETRJOINDOMAIN(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRJOINDOMAIN *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRUNJOINDOMAIN 
*/
static WERROR WKSSVC_NETRUNJOINDOMAIN(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRUNJOINDOMAIN *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRRENAMEMACHINEINDOMAIN 
*/
static WERROR WKSSVC_NETRRENAMEMACHINEINDOMAIN(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRRENAMEMACHINEINDOMAIN *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRVALIDATENAME 
*/
static WERROR WKSSVC_NETRVALIDATENAME(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRVALIDATENAME *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRGETJOININFORMATION 
*/
static WERROR WKSSVC_NETRGETJOININFORMATION(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRGETJOININFORMATION *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRGETJOINABLEOUS 
*/
static WERROR WKSSVC_NETRGETJOINABLEOUS(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRGETJOINABLEOUS *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRJOINDOMAIN2 
*/
static WERROR WKSSVC_NETRJOINDOMAIN2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRJOINDOMAIN2 *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRUNJOINDOMAIN2 
*/
static WERROR WKSSVC_NETRUNJOINDOMAIN2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRUNJOINDOMAIN2 *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRRENAMEMACHINEINDOMAIN2 
*/
static WERROR WKSSVC_NETRRENAMEMACHINEINDOMAIN2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRRENAMEMACHINEINDOMAIN2 *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRVALIDATENAME2 
*/
static WERROR WKSSVC_NETRVALIDATENAME2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRVALIDATENAME2 *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRGETJOINABLEOUS2 
*/
static WERROR WKSSVC_NETRGETJOINABLEOUS2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRGETJOINABLEOUS2 *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRADDALTERNATECOMPUTERNAME 
*/
static WERROR WKSSVC_NETRADDALTERNATECOMPUTERNAME(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRADDALTERNATECOMPUTERNAME *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRREMOVEALTERNATECOMPUTERNAME 
*/
static WERROR WKSSVC_NETRREMOVEALTERNATECOMPUTERNAME(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRREMOVEALTERNATECOMPUTERNAME *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRSETPRIMARYCOMPUTERNAME 
*/
static WERROR WKSSVC_NETRSETPRIMARYCOMPUTERNAME(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRSETPRIMARYCOMPUTERNAME *r)
{
	return WERR_NOT_SUPPORTED;
}


/* 
  WKSSVC_NETRENUMERATECOMPUTERNAMES 
*/
static WERROR WKSSVC_NETRENUMERATECOMPUTERNAMES(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct WKSSVC_NETRENUMERATECOMPUTERNAMES *r)
{
	return WERR_NOT_SUPPORTED;
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_wkssvc_s.c"
