/* 
   Unix SMB/CIFS implementation.

   TDR (Trivial Data Representation) helper functions
     Based loosely on ndr.c by Andrew Tridgell.

   Copyright (C) Jelmer Vernooij 2005
   
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

#define TDR_BASE_MARSHALL_SIZE 1024

#define TDR_PUSH_NEED_BYTES(tdr, n) TDR_CHECK(tdr_push_expand(tdr, tdr->offset+(n)))

#define TDR_PULL_NEED_BYTES(tdr, n) do { \
	if ((n) > tdr->length || tdr->offset + (n) > tdr->length) { \
		return NT_STATUS_BUFFER_TOO_SMALL; \
	} \
} while(0)

#define TDR_BE(tdr) ((tdr)->flags & TDR_BIG_ENDIAN)

#define TDR_SVAL(tdr, ofs) (TDR_BE(tdr)?RSVAL(tdr->data,ofs):SVAL(tdr->data,ofs))
#define TDR_IVAL(tdr, ofs) (TDR_BE(tdr)?RIVAL(tdr->data,ofs):IVAL(tdr->data,ofs))
#define TDR_IVALS(tdr, ofs) (TDR_BE(tdr)?RIVALS(tdr->data,ofs):IVALS(tdr->data,ofs))
#define TDR_SSVAL(tdr, ofs, v) do { if (TDR_BE(tdr))  { RSSVAL(tdr->data,ofs,v); } else SSVAL(tdr->data,ofs,v); } while (0)
#define TDR_SIVAL(tdr, ofs, v) do { if (TDR_BE(tdr))  { RSIVAL(tdr->data,ofs,v); } else SIVAL(tdr->data,ofs,v); } while (0)
#define TDR_SIVALS(tdr, ofs, v) do { if (TDR_BE(tdr))  { RSIVALS(tdr->data,ofs,v); } else SIVALS(tdr->data,ofs,v); } while (0)

struct tdr_pull *tdr_pull_init(TALLOC_CTX *mem_ctx, DATA_BLOB *blob)
{
	struct tdr_pull *tdr = talloc_zero(mem_ctx, struct tdr_pull);
	tdr->data = blob->data;
	tdr->length = blob->length;
	return tdr;
}

struct tdr_push *tdr_push_init(TALLOC_CTX *mem_ctx)
{
	return talloc_zero(mem_ctx, struct tdr_push);
}

struct tdr_print *tdr_print_init(TALLOC_CTX *mem_ctx)
{
	return talloc_zero(mem_ctx, struct tdr_print);
}

/*
  expand the available space in the buffer to 'size'
*/
NTSTATUS tdr_push_expand(struct tdr_push *tdr, uint32_t size)
{
	if (tdr->alloc_size >= size) {
		return NT_STATUS_OK;
	}

	tdr->alloc_size += TDR_BASE_MARSHALL_SIZE;
	if (size > tdr->alloc_size) {
		tdr->length = size;
	}
	tdr->data = talloc_realloc(tdr, tdr->data, uint8_t, tdr->alloc_size);
	return NT_STATUS_NO_MEMORY;
}


NTSTATUS tdr_pull_uint8(struct tdr_pull *tdr, uint8_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 1);
	SCVAL(tdr->data, tdr->offset, *v);
	tdr->offset += 1;
	return NT_STATUS_OK;
}

NTSTATUS tdr_push_uint8(struct tdr_push *tdr, const uint8_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 1);
	SCVAL(tdr->data, tdr->offset, *v);
	tdr->offset += 1;
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_uint8(struct tdr_print *tdr, const char *name, uint8_t *v)
{
	tdr->print(tdr, "%-25s: 0x%02x (%u)", name, *v, *v);
	return NT_STATUS_OK;
}

NTSTATUS tdr_pull_uint16(struct tdr_pull *tdr, uint16_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 2);
	*v = TDR_SVAL(tdr, tdr->offset);
	tdr->offset += 2;
	return NT_STATUS_OK;
}

NTSTATUS tdr_push_uint16(struct tdr_push *tdr, const uint16_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 2);
	TDR_SSVAL(tdr, tdr->offset, *v);
	tdr->offset += 2;
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_uint16(struct tdr_print *tdr, const char *name, uint16_t *v)
{
	tdr->print(tdr, "%-25s: 0x%02x (%u)", name, *v, *v);
	return NT_STATUS_OK;
}

NTSTATUS tdr_pull_uint32(struct tdr_pull *tdr, uint32_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 4);
	*v = TDR_IVAL(tdr, tdr->offset);
	tdr->offset += 4;
	return NT_STATUS_OK;
}

NTSTATUS tdr_push_uint32(struct tdr_push *tdr, const uint32_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 4);
	TDR_SIVAL(tdr, tdr->offset, *v);
	tdr->offset += 4;
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_uint32(struct tdr_print *tdr, const char *name, uint32_t *v)
{
	tdr->print(tdr, "%-25s: 0x%02x (%u)", name, *v, *v);
	return NT_STATUS_OK;
}

NTSTATUS tdr_pull_charset(struct tdr_pull *tdr, const char **v, uint32_t length, uint32_t el_size, int chset)
{
	int ret;

	if (length == -1) {
		switch (chset) {
			case CH_DOS:
				length = ascii_len_n((const char*)tdr->data+tdr->offset, tdr->length-tdr->offset);
				break;
			case CH_UTF16:
				length = utf16_len_n(tdr->data+tdr->offset, tdr->length-tdr->offset);
				break;

			default:
				return NT_STATUS_INVALID_PARAMETER;
		}
	}

	TDR_PULL_NEED_BYTES(tdr, el_size*length);
	
	ret = convert_string_talloc(tdr, chset, CH_UNIX, tdr->data+tdr->offset, el_size*length, discard_const_p(void *, v));

	if (ret == -1) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

NTSTATUS tdr_push_charset(struct tdr_push *tdr, const char **v, uint32_t length, uint32_t el_size, int chset)
{
	ssize_t ret, required;

	required = el_size * length;
	TDR_PUSH_NEED_BYTES(tdr, required);

	ret = convert_string(CH_UNIX, chset, *v, strlen(*v), tdr->data+tdr->offset, required);

	if (ret == -1) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Make sure the remaining part of the string is filled with zeroes */
	if (ret < required) {
		memset(tdr->data+tdr->offset+ret, 0, required-ret);
	}
	
	tdr->offset += required;
						 
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_charset(struct tdr_print *tdr, const char *name, const char **v, uint32_t length, uint32_t el_size, int chset)
{
	tdr->print(tdr, "%-25s: %s", name, *v);
	return NT_STATUS_OK;
}

/*
  pull a ipv4address
*/
NTSTATUS tdr_pull_ipv4address(struct tdr_pull *tdr, const char **address)
{
	struct ipv4_addr in;
	TDR_CHECK(tdr_pull_uint32(tdr, &in.addr));
	in.addr = htonl(in.addr);
	*address = talloc_strdup(tdr, sys_inet_ntoa(in));
	NT_STATUS_HAVE_NO_MEMORY(*address);
	return NT_STATUS_OK;
}

/*
  push a ipv4address
*/
NTSTATUS tdr_push_ipv4address(struct tdr_push *tdr, const char **address)
{
	uint32_t addr = htonl(interpret_addr(*address));
	TDR_CHECK(tdr_push_uint32(tdr, &addr));
	return NT_STATUS_OK;
}

/*
  print a ipv4address
*/
NTSTATUS tdr_print_ipv4address(struct tdr_print *tdr, const char *name, 
			   const char **address)
{
	tdr->print(tdr, "%-25s: %s", name, *address);
	return NT_STATUS_OK;
}

/*
  parse a hyper
*/
NTSTATUS tdr_pull_hyper(struct tdr_pull *tdr, uint64_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 8);
	*v = TDR_IVAL(tdr, tdr->offset);
	*v |= (uint64_t)(TDR_IVAL(tdr, tdr->offset+4)) << 32;
	tdr->offset += 8;
	return NT_STATUS_OK;
}

/*
  push a hyper
*/
NTSTATUS tdr_push_hyper(struct tdr_push *tdr, uint64_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 8);
	TDR_SIVAL(tdr, tdr->offset, ((*v) & 0xFFFFFFFF));
	TDR_SIVAL(tdr, tdr->offset+4, ((*v)>>32));
	tdr->offset += 8;
	return NT_STATUS_OK;
}



/*
  push a NTTIME
*/
NTSTATUS tdr_push_NTTIME(struct tdr_push *tdr, NTTIME *t)
{
	TDR_CHECK(tdr_push_hyper(tdr, t));
	return NT_STATUS_OK;
}

/*
  pull a NTTIME
*/
NTSTATUS tdr_pull_NTTIME(struct tdr_pull *tdr, NTTIME *t)
{
	TDR_CHECK(tdr_pull_hyper(tdr, t));
	return NT_STATUS_OK;
}

/*
  push a time_t
*/
NTSTATUS tdr_push_time_t(struct tdr_push *tdr, time_t *t)
{
	return tdr_push_uint32(tdr, (uint32_t *)t);
}

/*
  pull a time_t
*/
NTSTATUS tdr_pull_time_t(struct tdr_pull *tdr, time_t *t)
{
	uint32_t tt;
	TDR_CHECK(tdr_pull_uint32(tdr, &tt));
	*t = tt;
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_time_t(struct tdr_print *tdr, const char *name, time_t *t)
{
	if (*t == (time_t)-1 || *t == 0) {
		tdr->print(tdr, "%-25s: (time_t)%d", name, (int)*t);
	} else {
		tdr->print(tdr, "%-25s: %s", name, timestring(tdr, *t));
	}

	return NT_STATUS_OK;
}

NTSTATUS tdr_print_NTTIME(struct tdr_print *tdr, const char *name, NTTIME *t)
{
	tdr->print(tdr, "%-25s: %s", name, nt_time_string(tdr, *t));

	return NT_STATUS_OK;
}

NTSTATUS tdr_print_DATA_BLOB(struct tdr_print *tdr, const char *name, DATA_BLOB *r)
{
	tdr->print(tdr, "%-25s: DATA_BLOB length=%u", name, r->length);
	if (r->length) {
		dump_data(10, r->data, r->length);
	}

	return NT_STATUS_OK;
}

#define TDR_ALIGN(tdr,n) (((tdr)->offset & ((n)-1)) == 0?0:((n)-((tdr)->offset&((n)-1))))

/*
  push a DATA_BLOB onto the wire. 
*/
NTSTATUS tdr_push_DATA_BLOB(struct tdr_push *tdr, DATA_BLOB *blob)
{
	if (tdr->flags & TDR_ALIGN2) {
		blob->length = TDR_ALIGN(tdr, 2);
	} else if (tdr->flags & TDR_ALIGN4) {
		blob->length = TDR_ALIGN(tdr, 4);
	} else if (tdr->flags & TDR_ALIGN8) {
		blob->length = TDR_ALIGN(tdr, 8);
	}

	TDR_PUSH_NEED_BYTES(tdr, blob->length);
	
	memcpy(tdr->data+tdr->offset, blob->data, blob->length);
	return NT_STATUS_OK;
}

/*
  pull a DATA_BLOB from the wire. 
*/
NTSTATUS tdr_pull_DATA_BLOB(struct tdr_pull *tdr, DATA_BLOB *blob)
{
	uint32_t length;

	if (tdr->flags & TDR_ALIGN2) {
		length = TDR_ALIGN(tdr, 2);
	} else if (tdr->flags & TDR_ALIGN4) {
		length = TDR_ALIGN(tdr, 4);
	} else if (tdr->flags & TDR_ALIGN8) {
		length = TDR_ALIGN(tdr, 8);
	} else if (tdr->flags & TDR_REMAINING) {
		length = tdr->length - tdr->offset;
	} else {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (tdr->length - tdr->offset < length) {
		length = tdr->length - tdr->offset;
	}

	TDR_PULL_NEED_BYTES(tdr, length);

	*blob = data_blob_talloc(tdr, tdr->data+tdr->offset, length);
	tdr->offset += length;
	return NT_STATUS_OK;
}
