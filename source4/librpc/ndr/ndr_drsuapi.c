/* 
   Unix SMB/CIFS implementation.

   routines for printing some linked list structs in DRSUAPI

   Copyright (C) Stefan (metze) Metzmacher 2005

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
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "libcli/util/asn_1.h"

void ndr_print_drsuapi_DsReplicaObjectListItem(struct ndr_print *ndr, const char *name, 
					       const struct drsuapi_DsReplicaObjectListItem *r)
{
	ndr_print_struct(ndr, name, "drsuapi_DsReplicaObjectListItem");
	ndr->depth++;
	ndr_print_ptr(ndr, "next_object", r->next_object);
	ndr_print_drsuapi_DsReplicaObject(ndr, "object", &r->object);
	ndr->depth--;
	if (r->next_object) {
		ndr_print_drsuapi_DsReplicaObjectListItem(ndr, "next_object", r->next_object);
	}
}

void ndr_print_drsuapi_DsReplicaObjectListItemEx(struct ndr_print *ndr, const char *name, const struct drsuapi_DsReplicaObjectListItemEx *r)
{
	ndr_print_struct(ndr, name, "drsuapi_DsReplicaObjectListItemEx");
	ndr->depth++;
	ndr_print_ptr(ndr, "next_object", r->next_object);
	ndr_print_drsuapi_DsReplicaObject(ndr, "object", &r->object);
	ndr_print_uint32(ndr, "unknown1", r->unknown1);
	ndr_print_ptr(ndr, "parent_object_guid", r->parent_object_guid);
	ndr->depth++;
	if (r->parent_object_guid) {
		ndr_print_GUID(ndr, "parent_object_guid", r->parent_object_guid);
	}
	ndr->depth--;
	ndr_print_ptr(ndr, "meta_data_ctr", r->meta_data_ctr);
	ndr->depth++;
	if (r->meta_data_ctr) {
		ndr_print_drsuapi_DsReplicaMetaDataCtr(ndr, "meta_data_ctr", r->meta_data_ctr);
	}
	ndr->depth--;
	ndr->depth--;
	if (r->next_object) {
		ndr_print_drsuapi_DsReplicaObjectListItemEx(ndr, "next_object", r->next_object);
	}
}

#define _OID_PUSH_CHECK(call) do { \
	BOOL _status; \
	_status = call; \
	if (_status != True) { \
		return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT, "OID Conversion Error: %s\n", __location__); \
	} \
} while (0)

#define _OID_PULL_CHECK(call) do { \
	BOOL _status; \
	_status = call; \
	if (_status != True) { \
		return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "OID Conversion Error: %s\n", __location__); \
	} \
} while (0)

NTSTATUS ndr_push_drsuapi_DsReplicaOID(struct ndr_push *ndr, int ndr_flags, const struct drsuapi_DsReplicaOID *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, ndr_size_drsuapi_DsReplicaOID_oid(r->oid, 0)));
		NDR_CHECK(ndr_push_unique_ptr(ndr, r->oid));
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->oid) {
			DATA_BLOB blob;

			if (strncasecmp("ff", r->oid, 2) == 0) {
				blob = strhex_to_data_blob(r->oid);
				if (!blob.data) {
					return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT,
							      "HEX String Conversion Error: %s\n",
							      __location__);
				}
			} else {
				_OID_PUSH_CHECK(ber_write_OID_String(&blob, r->oid));
			}
			talloc_steal(ndr, blob.data);

			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, blob.length));
			NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, blob.data, blob.length));
		}
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_drsuapi_DsReplicaOID(struct ndr_pull *ndr, int ndr_flags, struct drsuapi_DsReplicaOID *r)
{
	uint32_t _ptr_oid;
	TALLOC_CTX *_mem_save_oid_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->__ndr_size));
		if (r->__ndr_size < 0 || r->__ndr_size > 10000) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_oid));
		if (_ptr_oid) {
			NDR_PULL_ALLOC(ndr, r->oid);
		} else {
			r->oid = NULL;
		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->oid) {
			DATA_BLOB _oid_array;
			const char *_oid;

			_mem_save_oid_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, ndr, 0);
			NDR_CHECK(ndr_pull_array_size(ndr, &r->oid));
			_oid_array.length = ndr_get_array_size(ndr, &r->oid);
			NDR_PULL_ALLOC_N(ndr, _oid_array.data, _oid_array.length);
			NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, _oid_array.data, _oid_array.length));
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_oid_0, 0);

			if (_oid_array.length && _oid_array.data[0] == 0xFF) {
				_oid = data_blob_hex_string(ndr, &_oid_array);
				NT_STATUS_HAVE_NO_MEMORY(_oid);
			} else {
				_OID_PULL_CHECK(ber_read_OID_String(r->oid, _oid_array, &_oid));
			}
			data_blob_free(&_oid_array);
			talloc_steal(r->oid, _oid);
			r->oid = _oid;
		}
		if (r->oid) {
			NDR_CHECK(ndr_check_array_size(ndr, (void*)&r->oid, r->__ndr_size));
		}
	}
	return NT_STATUS_OK;
}

size_t ndr_size_drsuapi_DsReplicaOID_oid(const char *oid, int flags)
{
	DATA_BLOB _blob;
	size_t ret = 0;

	if (!oid) return 0;

	if (strncasecmp("ff", oid, 2) == 0) {
		_blob = strhex_to_data_blob(oid);
		if (_blob.data) {
			ret = _blob.length;
		}
	} else {
		if (ber_write_OID_String(&_blob, oid)) {
			ret = _blob.length;
		}
	}
	data_blob_free(&_blob);
	return ret;
}
