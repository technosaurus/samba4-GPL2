/* dcerpc client calls generated by pidl */

#include "includes.h"


NTSTATUS dcerpc_wks_QueryInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct wks_QueryInfo *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(wks_QueryInfo, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_QUERYINFO, mem_ctx,
				    (ndr_push_fn_t) ndr_push_wks_QueryInfo,
				    (ndr_pull_fn_t) ndr_pull_wks_QueryInfo,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(wks_QueryInfo, r);		
	}

	return status;
}

NTSTATUS dcerpc_wks_SetInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct wks_SetInfo *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(wks_SetInfo, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_SETINFO, mem_ctx,
				    (ndr_push_fn_t) ndr_push_wks_SetInfo,
				    (ndr_pull_fn_t) ndr_pull_wks_SetInfo,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(wks_SetInfo, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRWKSTAUSERENUM(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRWKSTAUSERENUM *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRWKSTAUSERENUM, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRWKSTAUSERENUM, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRWKSTAUSERENUM,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRWKSTAUSERENUM,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRWKSTAUSERENUM, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRWKSTAUSERGETINFO(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRWKSTAUSERGETINFO *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRWKSTAUSERGETINFO, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRWKSTAUSERGETINFO, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRWKSTAUSERGETINFO,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRWKSTAUSERGETINFO,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRWKSTAUSERGETINFO, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRWKSTAUSERSETINFO(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRWKSTAUSERSETINFO *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRWKSTAUSERSETINFO, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRWKSTAUSERSETINFO, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRWKSTAUSERSETINFO,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRWKSTAUSERSETINFO,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRWKSTAUSERSETINFO, r);		
	}

	return status;
}

NTSTATUS dcerpc_wks_TransportEnum(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct wks_TransportEnum *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(wks_TransportEnum, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_TRANSPORTENUM, mem_ctx,
				    (ndr_push_fn_t) ndr_push_wks_TransportEnum,
				    (ndr_pull_fn_t) ndr_pull_wks_TransportEnum,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(wks_TransportEnum, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRWKSTATRANSPORTADD(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRWKSTATRANSPORTADD *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRWKSTATRANSPORTADD, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRWKSTATRANSPORTADD, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRWKSTATRANSPORTADD,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRWKSTATRANSPORTADD,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRWKSTATRANSPORTADD, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRWKSTATRANSPORTDEL(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRWKSTATRANSPORTDEL *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRWKSTATRANSPORTDEL, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRWKSTATRANSPORTDEL, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRWKSTATRANSPORTDEL,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRWKSTATRANSPORTDEL,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRWKSTATRANSPORTDEL, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRUSEADD(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRUSEADD *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRUSEADD, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRUSEADD, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRUSEADD,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRUSEADD,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRUSEADD, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRUSEGETINFO(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRUSEGETINFO *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRUSEGETINFO, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRUSEGETINFO, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRUSEGETINFO,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRUSEGETINFO,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRUSEGETINFO, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRUSEDEL(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRUSEDEL *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRUSEDEL, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRUSEDEL, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRUSEDEL,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRUSEDEL,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRUSEDEL, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRUSEENUM(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRUSEENUM *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRUSEENUM, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRUSEENUM, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRUSEENUM,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRUSEENUM,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRUSEENUM, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRMESSAGEBUFFERSEND(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRMESSAGEBUFFERSEND *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRMESSAGEBUFFERSEND, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRMESSAGEBUFFERSEND, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRMESSAGEBUFFERSEND,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRMESSAGEBUFFERSEND,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRMESSAGEBUFFERSEND, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRWORKSTATIONSTATISTICSGET(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRWORKSTATIONSTATISTICSGET *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRWORKSTATIONSTATISTICSGET, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRWORKSTATIONSTATISTICSGET, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRWORKSTATIONSTATISTICSGET,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRWORKSTATIONSTATISTICSGET,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRWORKSTATIONSTATISTICSGET, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRLOGONDOMAINNAMEADD(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRLOGONDOMAINNAMEADD *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRLOGONDOMAINNAMEADD, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRLOGONDOMAINNAMEADD, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRLOGONDOMAINNAMEADD,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRLOGONDOMAINNAMEADD,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRLOGONDOMAINNAMEADD, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRLOGONDOMAINNAMEDEL(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRLOGONDOMAINNAMEDEL *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRLOGONDOMAINNAMEDEL, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRLOGONDOMAINNAMEDEL, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRLOGONDOMAINNAMEDEL,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRLOGONDOMAINNAMEDEL,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRLOGONDOMAINNAMEDEL, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRJOINDOMAIN(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRJOINDOMAIN *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRJOINDOMAIN, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRJOINDOMAIN, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRJOINDOMAIN,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRJOINDOMAIN,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRJOINDOMAIN, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRUNJOINDOMAIN(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRUNJOINDOMAIN *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRUNJOINDOMAIN, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRUNJOINDOMAIN, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRUNJOINDOMAIN,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRUNJOINDOMAIN,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRUNJOINDOMAIN, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRRENAMEMACHINEINDOMAIN(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRRENAMEMACHINEINDOMAIN *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRRENAMEMACHINEINDOMAIN, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRRENAMEMACHINEINDOMAIN, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRRENAMEMACHINEINDOMAIN,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRRENAMEMACHINEINDOMAIN,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRRENAMEMACHINEINDOMAIN, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRVALIDATENAME(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRVALIDATENAME *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRVALIDATENAME, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRVALIDATENAME, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRVALIDATENAME,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRVALIDATENAME,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRVALIDATENAME, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRGETJOININFORMATION(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRGETJOININFORMATION *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRGETJOININFORMATION, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRGETJOININFORMATION, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRGETJOININFORMATION,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRGETJOININFORMATION,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRGETJOININFORMATION, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRGETJOINABLEOUS(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRGETJOINABLEOUS *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRGETJOINABLEOUS, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRGETJOINABLEOUS, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRGETJOINABLEOUS,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRGETJOINABLEOUS,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRGETJOINABLEOUS, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRJOINDOMAIN2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRJOINDOMAIN2 *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRJOINDOMAIN2, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRJOINDOMAIN2, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRJOINDOMAIN2,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRJOINDOMAIN2,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRJOINDOMAIN2, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRUNJOINDOMAIN2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRUNJOINDOMAIN2 *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRUNJOINDOMAIN2, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRUNJOINDOMAIN2, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRUNJOINDOMAIN2,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRUNJOINDOMAIN2,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRUNJOINDOMAIN2, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRRENAMEMACHINEINDOMAIN2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRRENAMEMACHINEINDOMAIN2 *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRRENAMEMACHINEINDOMAIN2, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRRENAMEMACHINEINDOMAIN2, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRRENAMEMACHINEINDOMAIN2,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRRENAMEMACHINEINDOMAIN2,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRRENAMEMACHINEINDOMAIN2, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRVALIDATENAME2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRVALIDATENAME2 *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRVALIDATENAME2, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRVALIDATENAME2, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRVALIDATENAME2,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRVALIDATENAME2,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRVALIDATENAME2, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRGETJOINABLEOUS2(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRGETJOINABLEOUS2 *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRGETJOINABLEOUS2, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRGETJOINABLEOUS2, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRGETJOINABLEOUS2,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRGETJOINABLEOUS2,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRGETJOINABLEOUS2, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRADDALTERNATECOMPUTERNAME(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRADDALTERNATECOMPUTERNAME *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRADDALTERNATECOMPUTERNAME, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRADDALTERNATECOMPUTERNAME, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRADDALTERNATECOMPUTERNAME,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRADDALTERNATECOMPUTERNAME,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRADDALTERNATECOMPUTERNAME, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRREMOVEALTERNATECOMPUTERNAME(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRREMOVEALTERNATECOMPUTERNAME *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRREMOVEALTERNATECOMPUTERNAME, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRREMOVEALTERNATECOMPUTERNAME, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRREMOVEALTERNATECOMPUTERNAME,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRREMOVEALTERNATECOMPUTERNAME,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRREMOVEALTERNATECOMPUTERNAME, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRSETPRIMARYCOMPUTERNAME(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRSETPRIMARYCOMPUTERNAME *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRSETPRIMARYCOMPUTERNAME, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRSETPRIMARYCOMPUTERNAME, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRSETPRIMARYCOMPUTERNAME,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRSETPRIMARYCOMPUTERNAME,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRSETPRIMARYCOMPUTERNAME, r);		
	}

	return status;
}

NTSTATUS dcerpc_WKS_NETRENUMERATECOMPUTERNAMES(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct WKS_NETRENUMERATECOMPUTERNAMES *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(WKS_NETRENUMERATECOMPUTERNAMES, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_WKS_NETRENUMERATECOMPUTERNAMES, mem_ctx,
				    (ndr_push_fn_t) ndr_push_WKS_NETRENUMERATECOMPUTERNAMES,
				    (ndr_pull_fn_t) ndr_pull_WKS_NETRENUMERATECOMPUTERNAMES,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(WKS_NETRENUMERATECOMPUTERNAMES, r);		
	}

	return status;
}
