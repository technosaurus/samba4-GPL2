/* 
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling basic types

   Copyright (C) Andrew Tridgell 2003
   
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
#include "system/network.h"

#define NDR_BE(ndr) (((ndr)->flags & (LIBNDR_FLAG_BIGENDIAN|LIBNDR_FLAG_LITTLE_ENDIAN)) == LIBNDR_FLAG_BIGENDIAN)
#define NDR_SVAL(ndr, ofs) (NDR_BE(ndr)?RSVAL(ndr->data,ofs):SVAL(ndr->data,ofs))
#define NDR_IVAL(ndr, ofs) (NDR_BE(ndr)?RIVAL(ndr->data,ofs):IVAL(ndr->data,ofs))
#define NDR_IVALS(ndr, ofs) (NDR_BE(ndr)?RIVALS(ndr->data,ofs):IVALS(ndr->data,ofs))
#define NDR_SSVAL(ndr, ofs, v) do { if (NDR_BE(ndr))  { RSSVAL(ndr->data,ofs,v); } else SSVAL(ndr->data,ofs,v); } while (0)
#define NDR_SIVAL(ndr, ofs, v) do { if (NDR_BE(ndr))  { RSIVAL(ndr->data,ofs,v); } else SIVAL(ndr->data,ofs,v); } while (0)
#define NDR_SIVALS(ndr, ofs, v) do { if (NDR_BE(ndr))  { RSIVALS(ndr->data,ofs,v); } else SIVALS(ndr->data,ofs,v); } while (0)


/*
  check for data leaks from the server by looking for non-zero pad bytes
  these could also indicate that real structure elements have been
  mistaken for padding in the IDL
*/
void ndr_check_padding(struct ndr_pull *ndr, size_t n)
{
	size_t ofs2 = (ndr->offset + (n-1)) & ~(n-1);
	int i;
	for (i=ndr->offset;i<ofs2;i++) {
		if (ndr->data[i] != 0) {
			break;
		}
	}
	if (i<ofs2) {
		DEBUG(0,("WARNING: Non-zero padding to %d: ", n));
		for (i=ndr->offset;i<ofs2;i++) {
			DEBUG(0,("%02x ", ndr->data[i]));
		}
		DEBUG(0,("\n"));
	}

}

/*
  parse a int8_t
*/
NTSTATUS ndr_pull_int8(struct ndr_pull *ndr, int ndr_flags, int8_t *v)
{
	NDR_PULL_NEED_BYTES(ndr, 1);
	*v = (int8_t)CVAL(ndr->data, ndr->offset);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  parse a uint8_t
*/
NTSTATUS ndr_pull_uint8(struct ndr_pull *ndr, int ndr_flags, uint8_t *v)
{
	NDR_PULL_NEED_BYTES(ndr, 1);
	*v = CVAL(ndr->data, ndr->offset);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  parse a int16_t
*/
NTSTATUS ndr_pull_int16(struct ndr_pull *ndr, int ndr_flags, int16_t *v)
{
	NDR_PULL_ALIGN(ndr, 2);
	NDR_PULL_NEED_BYTES(ndr, 2);
	*v = (uint16_t)NDR_SVAL(ndr, ndr->offset);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  parse a uint16_t
*/
NTSTATUS ndr_pull_uint16(struct ndr_pull *ndr, int ndr_flags, uint16_t *v)
{
	NDR_PULL_ALIGN(ndr, 2);
	NDR_PULL_NEED_BYTES(ndr, 2);
	*v = NDR_SVAL(ndr, ndr->offset);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  parse a int32_t
*/
NTSTATUS ndr_pull_int32(struct ndr_pull *ndr, int ndr_flags, int32_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 4);
	*v = NDR_IVALS(ndr, ndr->offset);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  parse a uint32_t
*/
NTSTATUS ndr_pull_uint32(struct ndr_pull *ndr, int ndr_flags, uint32_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 4);
	*v = NDR_IVAL(ndr, ndr->offset);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  parse a pointer referent identifier
*/
NTSTATUS ndr_pull_unique_ptr(struct ndr_pull *ndr, uint32_t *v)
{
	NTSTATUS status;
	status = ndr_pull_uint32(ndr, NDR_SCALARS, v);
	if (*v != 0) {
		ndr->ptr_count++;
	}
	return status;
}

/*
  parse a ref pointer referent identifier
*/
NTSTATUS ndr_pull_ref_ptr(struct ndr_pull *ndr, uint32_t *v)
{
	NTSTATUS status;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, v));
	/* ref pointers always point to data */
	*v = 1;
	return status;
}

/*
  parse a udlong
*/
NTSTATUS ndr_pull_udlong(struct ndr_pull *ndr, int ndr_flags, uint64_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 8);
	*v = NDR_IVAL(ndr, ndr->offset);
	*v |= (uint64_t)(NDR_IVAL(ndr, ndr->offset+4)) << 32;
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  parse a udlongr
*/
NTSTATUS ndr_pull_udlongr(struct ndr_pull *ndr, int ndr_flags, uint64_t *v)
{
	NDR_PULL_ALIGN(ndr, 4);
	NDR_PULL_NEED_BYTES(ndr, 8);
	*v = ((uint64_t)NDR_IVAL(ndr, ndr->offset)) << 32;
	*v |= NDR_IVAL(ndr, ndr->offset+4);
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  parse a dlong
*/
NTSTATUS ndr_pull_dlong(struct ndr_pull *ndr, int ndr_flags, int64_t *v)
{
	return ndr_pull_udlong(ndr, ndr_flags, (uint64_t *)v);
}

/*
  parse a hyper
*/
NTSTATUS ndr_pull_hyper(struct ndr_pull *ndr, int ndr_flags, uint64_t *v)
{
	NDR_PULL_ALIGN(ndr, 8);
	return ndr_pull_udlong(ndr, ndr_flags, v);
}

/*
  pull a NTSTATUS
*/
NTSTATUS ndr_pull_NTSTATUS(struct ndr_pull *ndr, int ndr_flags, NTSTATUS *status)
{
	uint32_t v;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &v));
	*status = NT_STATUS(v);
	return NT_STATUS_OK;
}

/*
  push a NTSTATUS
*/
NTSTATUS ndr_push_NTSTATUS(struct ndr_push *ndr, int ndr_flags, NTSTATUS status)
{
	return ndr_push_uint32(ndr, ndr_flags, NT_STATUS_V(status));
}

void ndr_print_NTSTATUS(struct ndr_print *ndr, const char *name, NTSTATUS r)
{
	ndr->print(ndr, "%-25s: %s", name, nt_errstr(r));
}

/*
  pull a WERROR
*/
NTSTATUS ndr_pull_WERROR(struct ndr_pull *ndr, int ndr_flags, WERROR *status)
{
	uint32_t v;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &v));
	*status = W_ERROR(v);
	return NT_STATUS_OK;
}

/*
  push a WERROR
*/
NTSTATUS ndr_push_WERROR(struct ndr_push *ndr, int ndr_flags, WERROR status)
{
	return ndr_push_uint32(ndr, NDR_SCALARS, W_ERROR_V(status));
}

void ndr_print_WERROR(struct ndr_print *ndr, const char *name, WERROR r)
{
	ndr->print(ndr, "%-25s: %s", name, win_errstr(r));
}

/*
  parse a set of bytes
*/
NTSTATUS ndr_pull_bytes(struct ndr_pull *ndr, uint8_t *data, uint32_t n)
{
	NDR_PULL_NEED_BYTES(ndr, n);
	memcpy(data, ndr->data + ndr->offset, n);
	ndr->offset += n;
	return NT_STATUS_OK;
}

/*
  pull an array of uint8
*/
NTSTATUS ndr_pull_array_uint8(struct ndr_pull *ndr, int ndr_flags, uint8_t *data, uint32_t n)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	return ndr_pull_bytes(ndr, data, n);
}

/*
  pull an array of uint16
*/
NTSTATUS ndr_pull_array_uint16(struct ndr_pull *ndr, int ndr_flags, uint16_t *data, uint32_t n)
{
	uint32_t i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &data[i]));
	}
	return NT_STATUS_OK;
}

/*
  pull a const array of uint32_t
*/
NTSTATUS ndr_pull_array_uint32(struct ndr_pull *ndr, int ndr_flags, uint32_t *data, uint32_t n)
{
	uint32_t i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &data[i]));
	}
	return NT_STATUS_OK;
}

/*
  pull a const array of hyper
*/
NTSTATUS ndr_pull_array_hyper(struct ndr_pull *ndr, int ndr_flags, uint64_t *data, uint32_t n)
{
	uint32_t i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_pull_hyper(ndr, NDR_SCALARS, &data[i]));
	}
	return NT_STATUS_OK;
}

/*
  pull a const array of WERROR
*/
NTSTATUS ndr_pull_array_WERROR(struct ndr_pull *ndr, int ndr_flags, WERROR *data, uint32_t n)
{
	uint32_t i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_pull_WERROR(ndr, NDR_SCALARS, &data[i]));
	}
	return NT_STATUS_OK;
}

/*
  push a int8_t
*/
NTSTATUS ndr_push_int8(struct ndr_push *ndr, int ndr_flags, int8_t v)
{
	NDR_PUSH_NEED_BYTES(ndr, 1);
	SCVAL(ndr->data, ndr->offset, (uint8_t)v);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  push a uint8_t
*/
NTSTATUS ndr_push_uint8(struct ndr_push *ndr, int ndr_flags, uint8_t v)
{
	NDR_PUSH_NEED_BYTES(ndr, 1);
	SCVAL(ndr->data, ndr->offset, v);
	ndr->offset += 1;
	return NT_STATUS_OK;
}

/*
  push a int16_t
*/
NTSTATUS ndr_push_int16(struct ndr_push *ndr, int ndr_flags, int16_t v)
{
	NDR_PUSH_ALIGN(ndr, 2);
	NDR_PUSH_NEED_BYTES(ndr, 2);
	NDR_SSVAL(ndr, ndr->offset, (uint16_t)v);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  push a uint16_t
*/
NTSTATUS ndr_push_uint16(struct ndr_push *ndr, int ndr_flags, uint16_t v)
{
	NDR_PUSH_ALIGN(ndr, 2);
	NDR_PUSH_NEED_BYTES(ndr, 2);
	NDR_SSVAL(ndr, ndr->offset, v);
	ndr->offset += 2;
	return NT_STATUS_OK;
}

/*
  push a int32_t
*/
NTSTATUS ndr_push_int32(struct ndr_push *ndr, int ndr_flags, int32_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 4);
	NDR_SIVALS(ndr, ndr->offset, v);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  push a uint32_t
*/
NTSTATUS ndr_push_uint32(struct ndr_push *ndr, int ndr_flags, uint32_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 4);
	NDR_SIVAL(ndr, ndr->offset, v);
	ndr->offset += 4;
	return NT_STATUS_OK;
}

/*
  push a udlong
*/
NTSTATUS ndr_push_udlong(struct ndr_push *ndr, int ndr_flags, uint64_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 8);
	NDR_SIVAL(ndr, ndr->offset, (v & 0xFFFFFFFF));
	NDR_SIVAL(ndr, ndr->offset+4, (v>>32));
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  push a udlongr
*/
NTSTATUS ndr_push_udlongr(struct ndr_push *ndr, int ndr_flags, uint64_t v)
{
	NDR_PUSH_ALIGN(ndr, 4);
	NDR_PUSH_NEED_BYTES(ndr, 8);
	NDR_SIVAL(ndr, ndr->offset+4, (v>>32));
	NDR_SIVAL(ndr, ndr->offset, (v & 0xFFFFFFFF));
	ndr->offset += 8;
	return NT_STATUS_OK;
}

/*
  push a dlong
*/
NTSTATUS ndr_push_dlong(struct ndr_push *ndr, int ndr_flags, int64_t v)
{
	return ndr_push_udlong(ndr, NDR_SCALARS, (uint64_t)v);
}

/*
  push a hyper
*/
NTSTATUS ndr_push_hyper(struct ndr_push *ndr, int ndr_flags, uint64_t v)
{
	NDR_PUSH_ALIGN(ndr, 8);
	return ndr_push_udlong(ndr, NDR_SCALARS, v);
}

NTSTATUS ndr_push_align(struct ndr_push *ndr, size_t size)
{
	NDR_PUSH_ALIGN(ndr, size);
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_align(struct ndr_pull *ndr, size_t size)
{
	NDR_PULL_ALIGN(ndr, size);
	return NT_STATUS_OK;
}

/*
  push some bytes
*/
NTSTATUS ndr_push_bytes(struct ndr_push *ndr, const uint8_t *data, uint32_t n)
{
	NDR_PUSH_NEED_BYTES(ndr, n);
	memcpy(ndr->data + ndr->offset, data, n);
	ndr->offset += n;
	return NT_STATUS_OK;
}

/*
  push some zero bytes
*/
NTSTATUS ndr_push_zero(struct ndr_push *ndr, uint32_t n)
{
	NDR_PUSH_NEED_BYTES(ndr, n);
	memset(ndr->data + ndr->offset, 0, n);
	ndr->offset += n;
	return NT_STATUS_OK;
}

/*
  push an array of uint8
*/
NTSTATUS ndr_push_array_uint8(struct ndr_push *ndr, int ndr_flags, const uint8_t *data, uint32_t n)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	return ndr_push_bytes(ndr, data, n);
}

/*
  push an array of uint16
*/
NTSTATUS ndr_push_array_uint16(struct ndr_push *ndr, int ndr_flags, const uint16_t *data, uint32_t n)
{
	int i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, data[i]));
	}
	return NT_STATUS_OK;
}

/*
  push an array of uint32_t
*/
NTSTATUS ndr_push_array_uint32(struct ndr_push *ndr, int ndr_flags, const uint32_t *data, uint32_t n)
{
	int i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, data[i]));
	}
	return NT_STATUS_OK;
}

/*
  push an array of hyper
*/
NTSTATUS ndr_push_array_hyper(struct ndr_push *ndr, int ndr_flags, const uint64_t *data, uint32_t n)
{
	int i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_push_hyper(ndr, NDR_SCALARS, data[i]));
	}
	return NT_STATUS_OK;
}

/*
  push an array of hyper
*/
NTSTATUS ndr_push_array_WERROR(struct ndr_push *ndr, int ndr_flags, const WERROR *data, uint32_t n)
{
	int i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	for (i=0;i<n;i++) {
		NDR_CHECK(ndr_push_WERROR(ndr, NDR_SCALARS, data[i]));
	}
	return NT_STATUS_OK;
}

/*
  save the current position
 */
void ndr_push_save(struct ndr_push *ndr, struct ndr_push_save *save)
{
	save->offset = ndr->offset;
}

/*
  restore the position
 */
void ndr_push_restore(struct ndr_push *ndr, struct ndr_push_save *save)
{
	ndr->offset = save->offset;
}

/*
  push a unique non-zero value if a pointer is non-NULL, otherwise 0
*/
NTSTATUS ndr_push_unique_ptr(struct ndr_push *ndr, const void *p)
{
	uint32_t ptr = 0;
	if (p) {
		ndr->ptr_count++;
		ptr = ndr->ptr_count;
	}
	return ndr_push_uint32(ndr, NDR_SCALARS, ptr);
}

/*
  push always a 0, if a pointer is NULL it's a fatal error
*/
NTSTATUS ndr_push_ref_ptr(struct ndr_push *ndr, const void *p)
{
	if (p == NULL) {
		return NT_STATUS_INVALID_PARAMETER_MIX;
	}
	return ndr_push_uint32(ndr, NDR_SCALARS, 0);
}

/*
  pull a general string from the wire
*/
NTSTATUS ndr_pull_string(struct ndr_pull *ndr, int ndr_flags, const char **s)
{
	char *as=NULL;
	uint32_t len1, ofs, len2;
	uint16_t len3;
	int ret;
	int chset = CH_UTF16;
	unsigned byte_mul = 2;
	unsigned flags = ndr->flags;
	unsigned c_len_term = 0;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	if (NDR_BE(ndr)) {
		chset = CH_UTF16BE;
	}

	if (flags & LIBNDR_FLAG_STR_ASCII) {
		chset = CH_DOS;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_ASCII;
	}

	if (flags & LIBNDR_FLAG_STR_UTF8) {
		chset = CH_UTF8;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_UTF8;
	}

	flags &= ~LIBNDR_FLAG_STR_CONFORMANT;
	if (flags & LIBNDR_FLAG_STR_CHARLEN) {
		c_len_term = 1;
		flags &= ~LIBNDR_FLAG_STR_CHARLEN;
	}

	switch (flags & LIBNDR_STRING_FLAGS) {
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4:
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len1));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &ofs));
		if (ofs != 0) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, "non-zero array offset with string flags 0x%x\n",
					      ndr->flags & LIBNDR_STRING_FLAGS);
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len2));
		if (len2 > len1) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, 
					      "Bad string lengths len1=%u ofs=%u len2=%u\n", 
					      len1, ofs, len2);
		}
		if (len2 == 0) {
			*s = talloc_strdup(ndr, "");
			break;
		}
		NDR_PULL_NEED_BYTES(ndr, (len2 + c_len_term)*byte_mul);
		ret = convert_string_talloc(ndr, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    (len2 + c_len_term)*byte_mul,
					    (void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len2 + c_len_term)*byte_mul));

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len2 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len2 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;

	case LIBNDR_FLAG_STR_SIZE4:
	case LIBNDR_FLAG_STR_SIZE4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len1));
		NDR_PULL_NEED_BYTES(ndr, (len1 + c_len_term)*byte_mul);
		if (len1 == 0) {
			*s = talloc_strdup(ndr, "");
			break;
		}
		ret = convert_string_talloc(ndr, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    (len1 + c_len_term)*byte_mul,
					    (void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len1 + c_len_term)*byte_mul));

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len1 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len1 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;

	case LIBNDR_FLAG_STR_LEN4:
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &ofs));
		if (ofs != 0) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, "non-zero array offset with string flags 0x%x\n",
					      ndr->flags & LIBNDR_STRING_FLAGS);
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &len1));
		NDR_PULL_NEED_BYTES(ndr, (len1 + c_len_term)*byte_mul);
		if (len1 == 0) {
			*s = talloc_strdup(ndr, "");
			break;
		}
		ret = convert_string_talloc(ndr, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    (len1 + c_len_term)*byte_mul,
					    (void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len1 + c_len_term)*byte_mul));

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len1 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len1 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;


	case LIBNDR_FLAG_STR_SIZE2:
	case LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &len3));
		NDR_PULL_NEED_BYTES(ndr, (len3 + c_len_term)*byte_mul);
		if (len3 == 0) {
			*s = talloc_strdup(ndr, "");
			break;
		}
		ret = convert_string_talloc(ndr, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    (len3 + c_len_term)*byte_mul,
					    (void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, (len3 + c_len_term)*byte_mul));

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < (len3 + c_len_term)) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == (len3 + c_len_term)) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;
		break;

	case LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM|LIBNDR_FLAG_STR_BYTESIZE:
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &len3));
		NDR_PULL_NEED_BYTES(ndr, len3);
		if (len3 == 0) {
			*s = talloc_strdup(ndr, "");
			break;
		}
		ret = convert_string_talloc(ndr, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len3,
					    (void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, len3));
		*s = as;
		break;

	case LIBNDR_FLAG_STR_NULLTERM:
		if (byte_mul == 1) {
			len1 = ascii_len_n((const char *)(ndr->data+ndr->offset), ndr->data_size - ndr->offset);
		} else {
			len1 = utf16_len_n(ndr->data+ndr->offset, ndr->data_size - ndr->offset);
		}
		ret = convert_string_talloc(ndr, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len1,
					    (void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, len1));
		*s = as;
		break;

	case LIBNDR_FLAG_STR_FIXLEN15:
	case LIBNDR_FLAG_STR_FIXLEN32:
		len1 = (flags & LIBNDR_FLAG_STR_FIXLEN32)?32:15;
		NDR_PULL_NEED_BYTES(ndr, len1*byte_mul);
		ret = convert_string_talloc(ndr, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len1*byte_mul,
					    (void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		NDR_CHECK(ndr_pull_advance(ndr, len1*byte_mul));
		*s = as;
		break;

	default:
		return ndr_pull_error(ndr, NDR_ERR_STRING, "Bad string flags 0x%x\n",
				      ndr->flags & LIBNDR_STRING_FLAGS);
	}

	return NT_STATUS_OK;
}


/*
  push a general string onto the wire
*/
NTSTATUS ndr_push_string(struct ndr_push *ndr, int ndr_flags, const char *s)
{
	ssize_t s_len, c_len, d_len;
	int ret;
	int chset = CH_UTF16;
	unsigned flags = ndr->flags;
	unsigned byte_mul = 2;
	unsigned c_len_term = 1;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	if (NDR_BE(ndr)) {
		chset = CH_UTF16BE;
	}
	
	s_len = s?strlen(s):0;
	c_len = s?strlen_m(s):0;

	if (flags & LIBNDR_FLAG_STR_ASCII) {
		chset = CH_DOS;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_ASCII;
	}

	if (flags & LIBNDR_FLAG_STR_UTF8) {
		chset = CH_UTF8;
		byte_mul = 1;
		flags &= ~LIBNDR_FLAG_STR_UTF8;
	}

	flags &= ~LIBNDR_FLAG_STR_CONFORMANT;

	if (flags & LIBNDR_FLAG_STR_CHARLEN) {
		c_len_term = 0;
		flags &= ~LIBNDR_FLAG_STR_CHARLEN;
	}

	switch (flags & LIBNDR_STRING_FLAGS) {
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len+c_len_term));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len+c_len_term));
		NDR_PUSH_NEED_BYTES(ndr, byte_mul*(c_len+1));
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len+1,
				     ndr->data+ndr->offset, 
				     byte_mul*(c_len+1));
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += byte_mul*(c_len+1);
		break;

	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len));
		NDR_PUSH_NEED_BYTES(ndr, c_len*byte_mul);
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len,
				     ndr->data+ndr->offset, c_len*byte_mul);
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += c_len*byte_mul;
		break;

	case LIBNDR_FLAG_STR_LEN4:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len + c_len_term));
		NDR_PUSH_NEED_BYTES(ndr, byte_mul*(c_len+1));
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len + 1,
				     ndr->data+ndr->offset, byte_mul*(c_len+1));
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += byte_mul*(c_len+1);
		break;

	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_NOTERM:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len));
		NDR_PUSH_NEED_BYTES(ndr, byte_mul*c_len);
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len,
				     ndr->data+ndr->offset, byte_mul*c_len);
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += byte_mul*c_len;
		break;

	case LIBNDR_FLAG_STR_SIZE4:
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, c_len + c_len_term));
		NDR_PUSH_NEED_BYTES(ndr, byte_mul*(c_len+1));
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len + 1,
				     ndr->data+ndr->offset, byte_mul*(c_len+1));
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += byte_mul*(c_len+1);
		break;

	case LIBNDR_FLAG_STR_SIZE2:
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, c_len + c_len_term));
		NDR_PUSH_NEED_BYTES(ndr, byte_mul*(c_len+1));
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len + 1,
				     ndr->data+ndr->offset, byte_mul*(c_len+1));
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += byte_mul*(c_len+1);
		break;

	case LIBNDR_FLAG_STR_NULLTERM:
		NDR_PUSH_NEED_BYTES(ndr, byte_mul*(c_len+1));
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len+1,
				     ndr->data+ndr->offset, byte_mul*(c_len+1));
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += byte_mul*(c_len+1);
		break;

	case LIBNDR_FLAG_STR_SIZE2|LIBNDR_FLAG_STR_NOTERM|LIBNDR_FLAG_STR_BYTESIZE:
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, c_len*byte_mul));
		NDR_PUSH_NEED_BYTES(ndr, c_len*byte_mul);
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len,
				     ndr->data+ndr->offset, c_len*byte_mul);
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += c_len*byte_mul;
		break;

	case LIBNDR_FLAG_STR_FIXLEN15:
	case LIBNDR_FLAG_STR_FIXLEN32:
		d_len = (flags & LIBNDR_FLAG_STR_FIXLEN32)?32:15;
		NDR_PUSH_NEED_BYTES(ndr, byte_mul*d_len);
		ret = convert_string(CH_UNIX, chset, 
				     s, s_len,
				     ndr->data+ndr->offset, byte_mul*d_len);
		if (ret == -1) {
			return ndr_push_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr->offset += byte_mul*d_len;
		break;

	default:
		return ndr_push_error(ndr, NDR_ERR_STRING, "Bad string flags 0x%x\n",
				      ndr->flags & LIBNDR_STRING_FLAGS);
	}

	return NT_STATUS_OK;
}

/*
  push a general string onto the wire
*/
size_t ndr_string_array_size(struct ndr_push *ndr, const char *s)
{
	size_t c_len;
	unsigned flags = ndr->flags;
	unsigned byte_mul = 2;
	unsigned c_len_term = 1;

	if (flags & LIBNDR_FLAG_STR_FIXLEN32) {
		return 32;
	}
	if (flags & LIBNDR_FLAG_STR_FIXLEN15) {
		return 15;
	}
	
	c_len = s?strlen_m(s):0;

	if (flags & (LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_UTF8)) {
		byte_mul = 1;
	}

	if (flags & LIBNDR_FLAG_STR_NOTERM) {
		c_len_term = 0;
	}

	c_len = c_len + c_len_term;

	if (flags & LIBNDR_FLAG_STR_BYTESIZE) {
		c_len = c_len * byte_mul;
	}

	return c_len;
}


/*
  push a NTTIME
*/
NTSTATUS ndr_push_NTTIME(struct ndr_push *ndr, int ndr_flags, NTTIME t)
{
	NDR_CHECK(ndr_push_udlong(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  pull a NTTIME
*/
NTSTATUS ndr_pull_NTTIME(struct ndr_pull *ndr, int ndr_flags, NTTIME *t)
{
	NDR_CHECK(ndr_pull_udlong(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  push a NTTIME
*/
NTSTATUS ndr_push_NTTIME_1sec(struct ndr_push *ndr, int ndr_flags, NTTIME t)
{
	t /= 10000000;
	NDR_CHECK(ndr_push_hyper(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  pull a NTTIME_1sec
*/
NTSTATUS ndr_pull_NTTIME_1sec(struct ndr_pull *ndr, int ndr_flags, NTTIME *t)
{
	NDR_CHECK(ndr_pull_hyper(ndr, ndr_flags, t));
	(*t) *= 10000000;
	return NT_STATUS_OK;
}

/*
  pull a NTTIME_hyper
*/
NTSTATUS ndr_pull_NTTIME_hyper(struct ndr_pull *ndr, int ndr_flags, NTTIME *t)
{
	NDR_CHECK(ndr_pull_hyper(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  push a NTTIME_hyper
*/
NTSTATUS ndr_push_NTTIME_hyper(struct ndr_push *ndr, int ndr_flags, NTTIME t)
{
	NDR_CHECK(ndr_push_hyper(ndr, ndr_flags, t));
	return NT_STATUS_OK;
}

/*
  push a time_t
*/
NTSTATUS ndr_push_time_t(struct ndr_push *ndr, int ndr_flags, time_t t)
{
	return ndr_push_uint32(ndr, ndr_flags, t);
}

/*
  pull a time_t
*/
NTSTATUS ndr_pull_time_t(struct ndr_pull *ndr, int ndr_flags, time_t *t)
{
	uint32_t tt;
	NDR_CHECK(ndr_pull_uint32(ndr, ndr_flags, &tt));
	*t = tt;
	return NT_STATUS_OK;
}


/*
  pull a ipv4address
*/
NTSTATUS ndr_pull_ipv4address(struct ndr_pull *ndr, int ndr_flags, const char **address)
{
	struct ipv4_addr in;
	NDR_CHECK(ndr_pull_uint32(ndr, ndr_flags, &in.addr));
	in.addr = htonl(in.addr);
	*address = talloc_strdup(ndr, sys_inet_ntoa(in));
	NT_STATUS_HAVE_NO_MEMORY(*address);
	return NT_STATUS_OK;
}

/*
  push a ipv4address
*/
NTSTATUS ndr_push_ipv4address(struct ndr_push *ndr, int ndr_flags, const char *address)
{
	uint32_t addr = interpret_addr(address);
	NDR_CHECK(ndr_push_uint32(ndr, ndr_flags, htonl(addr)));
	return NT_STATUS_OK;
}

/*
  print a ipv4address
*/
void ndr_print_ipv4address(struct ndr_print *ndr, const char *name, 
			   const char *address)
{
	ndr->print(ndr, "%-25s: %s", name, address);
}


void ndr_print_struct(struct ndr_print *ndr, const char *name, const char *type)
{
	ndr->print(ndr, "%s: struct %s", name, type);
}

void ndr_print_enum(struct ndr_print *ndr, const char *name, const char *type, 
		    const char *val, uint_t value)
{
	if (ndr->flags & LIBNDR_PRINT_ARRAY_HEX) {
		ndr->print(ndr, "%-25s: %s (0x%X)", name, val?val:"UNKNOWN_ENUM_VALUE", value);
	} else {
		ndr->print(ndr, "%-25s: %s (%d)", name, val?val:"UNKNOWN_ENUM_VALUE", value);
	}
}

void ndr_print_bitmap_flag(struct ndr_print *ndr, size_t size, const char *flag_name, uint_t flag, uint_t value)
{
	/* this is an attempt to support multi-bit bitmap masks */
	value &= flag;

	while (!(flag & 1)) {
		flag >>= 1;
		value >>= 1;
	}	
	if (flag == 1) {
		ndr->print(ndr, "   %d: %-25s", value, flag_name);
	} else {
		ndr->print(ndr, "0x%02x: %-25s (%d)", value, flag_name, value);
	}
}

void ndr_print_int8(struct ndr_print *ndr, const char *name, int8_t v)
{
	ndr->print(ndr, "%-25s: %d", name, v);
}

void ndr_print_uint8(struct ndr_print *ndr, const char *name, uint8_t v)
{
	ndr->print(ndr, "%-25s: 0x%02x (%u)", name, v, v);
}

void ndr_print_int16(struct ndr_print *ndr, const char *name, int16_t v)
{
	ndr->print(ndr, "%-25s: %d", name, v);
}

void ndr_print_uint16(struct ndr_print *ndr, const char *name, uint16_t v)
{
	ndr->print(ndr, "%-25s: 0x%04x (%u)", name, v, v);
}

void ndr_print_int32(struct ndr_print *ndr, const char *name, int32_t v)
{
	ndr->print(ndr, "%-25s: %d", name, v);
}

void ndr_print_uint32(struct ndr_print *ndr, const char *name, uint32_t v)
{
	ndr->print(ndr, "%-25s: 0x%08x (%u)", name, v, v);
}

void ndr_print_udlong(struct ndr_print *ndr, const char *name, uint64_t v)
{
	ndr->print(ndr, "%-25s: 0x%08x%08x (%llu)", name,
		   (uint32_t)(v >> 32),
		   (uint32_t)(v & 0xFFFFFFFF),
		   v);
}

void ndr_print_udlongr(struct ndr_print *ndr, const char *name, uint64_t v)
{
	ndr_print_udlong(ndr, name, v);
}

void ndr_print_dlong(struct ndr_print *ndr, const char *name, int64_t v)
{
	ndr->print(ndr, "%-25s: 0x%08x%08x (%lld)", name, 
		   (uint32_t)(v >> 32), 
		   (uint32_t)(v & 0xFFFFFFFF),
		   v);
}

void ndr_print_hyper(struct ndr_print *ndr, const char *name, uint64_t v)
{
	ndr_print_dlong(ndr, name, v);
}

void ndr_print_ptr(struct ndr_print *ndr, const char *name, const void *p)
{
	if (p) {
		ndr->print(ndr, "%-25s: *", name);
	} else {
		ndr->print(ndr, "%-25s: NULL", name);
	}
}

void ndr_print_string(struct ndr_print *ndr, const char *name, const char *s)
{
	if (s) {
		ndr->print(ndr, "%-25s: '%s'", name, s);
	} else {
		ndr->print(ndr, "%-25s: NULL", name);
	}
}

void ndr_print_NTTIME(struct ndr_print *ndr, const char *name, NTTIME t)
{
	ndr->print(ndr, "%-25s: %s", name, nt_time_string(ndr, t));
}

void ndr_print_NTTIME_1sec(struct ndr_print *ndr, const char *name, NTTIME t)
{
	/* this is a standard NTTIME here
	 * as it's already converted in the pull/push code
	 */
	ndr_print_NTTIME(ndr, name, t);
}

void ndr_print_NTTIME_hyper(struct ndr_print *ndr, const char *name, NTTIME t)
{
	ndr_print_NTTIME(ndr, name, t);
}

void ndr_print_time_t(struct ndr_print *ndr, const char *name, time_t t)
{
	if (t == (time_t)-1 || t == 0) {
		ndr->print(ndr, "%-25s: (time_t)%d", name, (int)t);
	} else {
		ndr->print(ndr, "%-25s: %s", name, timestring(ndr, t));
	}
}

void ndr_print_union(struct ndr_print *ndr, const char *name, int level, const char *type)
{
	ndr->print(ndr, "%-25s: union %s(case %d)", name, type, level);
}

void ndr_print_bad_level(struct ndr_print *ndr, const char *name, uint16_t level)
{
	ndr->print(ndr, "UNKNOWN LEVEL %u", level);
}

void ndr_print_array_WERROR(struct ndr_print *ndr, const char *name, 
			    const WERROR *data, uint32_t count)
{
	int i;

	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			ndr_print_WERROR(ndr, idx, data[i]);
			free(idx);
		}
	}
	ndr->depth--;	
}

void ndr_print_array_hyper(struct ndr_print *ndr, const char *name, 
			    const uint64_t *data, uint32_t count)
{
	int i;

	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			ndr_print_hyper(ndr, idx, data[i]);
			free(idx);
		}
	}
	ndr->depth--;	
}

void ndr_print_array_uint32(struct ndr_print *ndr, const char *name, 
			    const uint32_t *data, uint32_t count)
{
	int i;

	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			ndr_print_uint32(ndr, idx, data[i]);
			free(idx);
		}
	}
	ndr->depth--;	
}

void ndr_print_array_uint16(struct ndr_print *ndr, const char *name, 
			    const uint16_t *data, uint32_t count)
{
	int i;

	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			ndr_print_uint16(ndr, idx, data[i]);
			free(idx);
		}
	}
	ndr->depth--;	
}

void ndr_print_array_uint8(struct ndr_print *ndr, const char *name, 
			   const uint8_t *data, uint32_t count)
{
	int i;

	if (count <= 600 && (ndr->flags & LIBNDR_PRINT_ARRAY_HEX)) {
		char s[1202];
		for (i=0;i<count;i++) {
			snprintf(&s[i*2], 3, "%02x", data[i]);
		}
		s[i*2] = 0;
		ndr->print(ndr, "%-25s: %s", name, s);
		return;
	}

	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			ndr_print_uint8(ndr, idx, data[i]);
			free(idx);
		}
	}
	ndr->depth--;	
}

void ndr_print_DATA_BLOB(struct ndr_print *ndr, const char *name, DATA_BLOB r)
{
	ndr->print(ndr, "%-25s: DATA_BLOB length=%u", name, r.length);
	if (r.length) {
		dump_data(10, r.data, r.length);
	}
}


/*
  push a DATA_BLOB onto the wire. 
*/
NTSTATUS ndr_push_DATA_BLOB(struct ndr_push *ndr, int ndr_flags, DATA_BLOB blob)
{
	if (ndr->flags & LIBNDR_ALIGN_FLAGS) {
		if (ndr->flags & LIBNDR_FLAG_ALIGN2) {
			blob.length = NDR_ALIGN(ndr, 2);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN4) {
			blob.length = NDR_ALIGN(ndr, 4);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN8) {
			blob.length = NDR_ALIGN(ndr, 8);
		}
		NDR_PUSH_ALLOC_SIZE(ndr, blob.data, blob.length);
		data_blob_clear(&blob);
	} else if (!(ndr->flags & LIBNDR_FLAG_REMAINING)) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, blob.length));
	}
	NDR_CHECK(ndr_push_bytes(ndr, blob.data, blob.length));
	return NT_STATUS_OK;
}

/*
  pull a DATA_BLOB from the wire. 
*/
NTSTATUS ndr_pull_DATA_BLOB(struct ndr_pull *ndr, int ndr_flags, DATA_BLOB *blob)
{
	uint32_t length;

	if (ndr->flags & LIBNDR_ALIGN_FLAGS) {
		if (ndr->flags & LIBNDR_FLAG_ALIGN2) {
			length = NDR_ALIGN(ndr, 2);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN4) {
			length = NDR_ALIGN(ndr, 4);
		} else if (ndr->flags & LIBNDR_FLAG_ALIGN8) {
			length = NDR_ALIGN(ndr, 8);
		}
		if (ndr->data_size - ndr->offset < length) {
			length = ndr->data_size - ndr->offset;
		}
	} else if (ndr->flags & LIBNDR_FLAG_REMAINING) {
		length = ndr->data_size - ndr->offset;
	} else {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	}
	NDR_PULL_NEED_BYTES(ndr, length);
	*blob = data_blob_talloc(ndr, ndr->data+ndr->offset, length);
	ndr->offset += length;
	return NT_STATUS_OK;
}

uint32_t ndr_size_DATA_BLOB(int ret, const DATA_BLOB *data, int flags)
{
	return ret + data->length;
}

uint32_t ndr_size_string(int ret, const char * const* string, int flags) 
{
	/* FIXME: Is this correct for all strings ? */
	if(!(*string)) return ret;
	return ret+strlen(*string)+1;
}
