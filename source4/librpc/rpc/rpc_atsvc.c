/* dcerpc client calls generated by pidl */

#include "includes.h"


NTSTATUS dcerpc_atsvc_JobAdd(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct atsvc_JobAdd *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(atsvc_JobAdd, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_ATSVC_JOBADD, mem_ctx,
				    (ndr_push_fn_t) ndr_push_atsvc_JobAdd,
				    (ndr_pull_fn_t) ndr_pull_atsvc_JobAdd,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(atsvc_JobAdd, r);		
	}
	if (NT_STATUS_IS_OK(status)) status = r->out.result;

	return status;
}

NTSTATUS dcerpc_atsvc_JobDel(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct atsvc_JobDel *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(atsvc_JobDel, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_ATSVC_JOBDEL, mem_ctx,
				    (ndr_push_fn_t) ndr_push_atsvc_JobDel,
				    (ndr_pull_fn_t) ndr_pull_atsvc_JobDel,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(atsvc_JobDel, r);		
	}
	if (NT_STATUS_IS_OK(status)) status = r->out.result;

	return status;
}

NTSTATUS dcerpc_atsvc_JobEnum(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct atsvc_JobEnum *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(atsvc_JobEnum, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_ATSVC_JOBENUM, mem_ctx,
				    (ndr_push_fn_t) ndr_push_atsvc_JobEnum,
				    (ndr_pull_fn_t) ndr_pull_atsvc_JobEnum,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(atsvc_JobEnum, r);		
	}
	if (NT_STATUS_IS_OK(status)) status = r->out.result;

	return status;
}

NTSTATUS dcerpc_atsvc_JobGetInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct atsvc_JobGetInfo *r)
{
	NTSTATUS status;

        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG(atsvc_JobGetInfo, r);		
	}

	status = dcerpc_ndr_request(p, DCERPC_ATSVC_JOBGETINFO, mem_ctx,
				    (ndr_push_fn_t) ndr_push_atsvc_JobGetInfo,
				    (ndr_pull_fn_t) ndr_pull_atsvc_JobGetInfo,
				    r);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG(atsvc_JobGetInfo, r);		
	}
	if (NT_STATUS_IS_OK(status)) status = r->out.result;

	return status;
}
