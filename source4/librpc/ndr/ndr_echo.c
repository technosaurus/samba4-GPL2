/* parser auto-generated by pidl */

#include "includes.h"

NTSTATUS ndr_push_echo_AddOne(struct ndr_push *ndr, struct echo_AddOne *r)
{
	NDR_CHECK(ndr_push_uint32(ndr, *r->in.v));

	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_echo_AddOne(struct ndr_pull *ndr, struct echo_AddOne *r)
{
	NDR_CHECK(ndr_pull_uint32(ndr, r->out.v));

	return NT_STATUS_OK;
}

NTSTATUS ndr_push_echo_EchoData(struct ndr_push *ndr, struct echo_EchoData *r)
{
	NDR_CHECK(ndr_push_uint32(ndr, r->in.len));
	if (r->in.data) {
		NDR_CHECK(ndr_push_array_uint8(ndr, r->in.data, r->in.len));
	}

	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_echo_EchoData(struct ndr_pull *ndr, struct echo_EchoData *r)
{
	if (r->out.data) {
		NDR_CHECK(ndr_pull_array_uint8(ndr, r->out.data, r->in.len));
	}

	return NT_STATUS_OK;
}

NTSTATUS ndr_push_echo_SinkData(struct ndr_push *ndr, struct echo_SinkData *r)
{
	NDR_CHECK(ndr_push_uint32(ndr, r->in.len));
	if (r->in.data) {
		NDR_CHECK(ndr_push_array_uint8(ndr, r->in.data, r->in.len));
	}

	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_echo_SinkData(struct ndr_pull *ndr, struct echo_SinkData *r)
{

	return NT_STATUS_OK;
}

NTSTATUS ndr_push_echo_SourceData(struct ndr_push *ndr, struct echo_SourceData *r)
{
	NDR_CHECK(ndr_push_uint32(ndr, r->in.len));

	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_echo_SourceData(struct ndr_pull *ndr, struct echo_SourceData *r)
{
	if (r->out.data) {
		NDR_CHECK(ndr_pull_array_uint8(ndr, r->out.data, r->in.len));
	}

	return NT_STATUS_OK;
}

