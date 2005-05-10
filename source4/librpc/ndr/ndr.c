/* 
   Unix SMB/CIFS implementation.

   libndr interface

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

/*
  this provides the core routines for NDR parsing functions

  see http://www.opengroup.org/onlinepubs/9629399/chap14.htm for details
  of NDR encoding rules
*/

#include "includes.h"
#include "dlinklist.h"

#define NDR_BASE_MARSHALL_SIZE 1024

/*
  work out the number of bytes needed to align on a n byte boundary
*/
size_t ndr_align_size(uint32_t offset, size_t n)
{
	if ((offset & (n-1)) == 0) return 0;
	return n - (offset & (n-1));
}

/*
  initialise a ndr parse structure from a data blob
*/
struct ndr_pull *ndr_pull_init_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx)
{
	struct ndr_pull *ndr;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;

	ndr->data = blob->data;
	ndr->data_size = blob->length;

	return ndr;
}

/*
  create an ndr sub-context based on an existing context. The new context starts
  at the current offset, with the given size limit
*/
NTSTATUS ndr_pull_subcontext(struct ndr_pull *ndr, struct ndr_pull *ndr2, uint32_t size)
{
	NDR_PULL_NEED_BYTES(ndr, size);
	*ndr2 = *ndr;
	ndr2->data += ndr2->offset;
	ndr2->offset = 0;
	ndr2->data_size = size;
	ndr2->flags = ndr->flags;
	return NT_STATUS_OK;
}


/*
  advance by 'size' bytes
*/
NTSTATUS ndr_pull_advance(struct ndr_pull *ndr, uint32_t size)
{
	ndr->offset += size;
	if (ndr->offset > ndr->data_size) {
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_pull_advance by %u failed",
				      size);
	}
	return NT_STATUS_OK;
}

/*
  set the parse offset to 'ofs'
*/
NTSTATUS ndr_pull_set_offset(struct ndr_pull *ndr, uint32_t ofs)
{
	ndr->offset = ofs;
	if (ndr->offset > ndr->data_size) {
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, 
				      "ndr_pull_set_offset %u failed",
				      ofs);
	}
	return NT_STATUS_OK;
}

/* save the offset/size of the current ndr state */
void ndr_pull_save(struct ndr_pull *ndr, struct ndr_pull_save *save)
{
	save->offset = ndr->offset;
	save->data_size = ndr->data_size;
}

/* restore the size/offset of a ndr structure */
void ndr_pull_restore(struct ndr_pull *ndr, struct ndr_pull_save *save)
{
	ndr->offset = save->offset;
	ndr->data_size = save->data_size;
}


/* create a ndr_push structure, ready for some marshalling */
struct ndr_push *ndr_push_init_ctx(TALLOC_CTX *mem_ctx)
{
	struct ndr_push *ndr;

	ndr = talloc_zero(mem_ctx, struct ndr_push);
	if (!ndr) {
		return NULL;
	}

	ndr->flags = 0;
	ndr->alloc_size = NDR_BASE_MARSHALL_SIZE;
	ndr->data = talloc_array(ndr, uint8_t, ndr->alloc_size);
	if (!ndr->data) {
		return NULL;
	}

	return ndr;
}


/* create a ndr_push structure, ready for some marshalling */
struct ndr_push *ndr_push_init(void)
{
	return ndr_push_init_ctx(NULL);
}

/* free a ndr_push structure */
void ndr_push_free(struct ndr_push *ndr)
{
	talloc_free(ndr);
}


/* return a DATA_BLOB structure for the current ndr_push marshalled data */
DATA_BLOB ndr_push_blob(struct ndr_push *ndr)
{
	DATA_BLOB blob;
	blob.data = ndr->data;
	blob.length = ndr->offset;
	return blob;
}


/*
  expand the available space in the buffer to 'size'
*/
NTSTATUS ndr_push_expand(struct ndr_push *ndr, uint32_t size)
{
	if (ndr->alloc_size >= size) {
		return NT_STATUS_OK;
	}

	ndr->alloc_size += NDR_BASE_MARSHALL_SIZE;
	if (size > ndr->alloc_size) {
		ndr->alloc_size = size;
	}
	ndr->data = talloc_realloc(ndr, ndr->data, uint8_t, ndr->alloc_size);
	if (!ndr->data) {
		return ndr_push_error(ndr, NDR_ERR_ALLOC, "Failed to push_expand to %u",
				      ndr->alloc_size);
	}

	return NT_STATUS_OK;
}

/*
  set the push offset to 'ofs'
*/
NTSTATUS ndr_push_set_offset(struct ndr_push *ndr, uint32_t ofs)
{
	NDR_CHECK(ndr_push_expand(ndr, ofs));
	ndr->offset = ofs;
	return NT_STATUS_OK;
}

/*
  push a generic array
*/
NTSTATUS ndr_push_array(struct ndr_push *ndr, int ndr_flags, void *base, 
			size_t elsize, uint32_t count, 
			NTSTATUS (*push_fn)(struct ndr_push *, int, void *))
{
	int i;
	char *p = base;
	if (!(ndr_flags & NDR_SCALARS)) goto buffers;
	for (i=0;i<count;i++) {
		NDR_CHECK(push_fn(ndr, NDR_SCALARS, p));
		p += elsize;
	}
	if (!(ndr_flags & NDR_BUFFERS)) goto done;
buffers:
	p = base;
	for (i=0;i<count;i++) {
		NDR_CHECK(push_fn(ndr, NDR_BUFFERS, p));
		p += elsize;
	}
done:
	return NT_STATUS_OK;
}

/*
  pull a constant sized array
*/
NTSTATUS ndr_pull_array(struct ndr_pull *ndr, int ndr_flags, void *base, 
			size_t elsize, uint32_t count, 
			NTSTATUS (*pull_fn)(struct ndr_pull *, int, void *))
{
	int i;
	char *p;
	p = base;
	if (!(ndr_flags & NDR_SCALARS)) goto buffers;
	for (i=0;i<count;i++) {
		NDR_CHECK(pull_fn(ndr, NDR_SCALARS, p));
		p += elsize;
	}
	if (!(ndr_flags & NDR_BUFFERS)) goto done;
buffers:
	p = base;
	for (i=0;i<count;i++) {
		NDR_CHECK(pull_fn(ndr, NDR_BUFFERS, p));
		p += elsize;
	}
done:
	return NT_STATUS_OK;
}

/*
  pull a constant size array of structures
*/
NTSTATUS ndr_pull_struct_array(struct ndr_pull *ndr, uint32_t count,
			       size_t elsize, void **info,
			       NTSTATUS (*pull_fn)(struct ndr_pull *, int, void *))
{
	int i;
	char *base;

	NDR_ALLOC_N_SIZE(ndr, *info, count, elsize);
	base = (char *)*info;

	for (i = 0; i < count; i++) {
		ndr->data += ndr->offset;
		ndr->offset = 0;
		NDR_CHECK(pull_fn(ndr, NDR_SCALARS|NDR_BUFFERS, &base[count * elsize]));
	}

	return NT_STATUS_OK;
}

/*
  print a generic array
*/
void ndr_print_array(struct ndr_print *ndr, const char *name, void *base, 
		     size_t elsize, uint32_t count, 
		     void (*print_fn)(struct ndr_print *, const char *, void *))
{
	int i;
	char *p = base;
	ndr->print(ndr, "%s: ARRAY(%d)", name, count);
	ndr->depth++;
	for (i=0;i<count;i++) {
		char *idx=NULL;
		asprintf(&idx, "[%d]", i);
		if (idx) {
			print_fn(ndr, idx, p);
			free(idx);
		}
		p += elsize;
	}
	ndr->depth--;
}



void ndr_print_debug_helper(struct ndr_print *ndr, const char *format, ...) _PRINTF_ATTRIBUTE(2,3)
{
	va_list ap;
	char *s = NULL;
	int i;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	for (i=0;i<ndr->depth;i++) {
		DEBUG(0,("    "));
	}

	DEBUG(0,("%s\n", s));
	free(s);
}

/*
  a useful helper function for printing idl structures via DEBUG()
*/
void ndr_print_debug(ndr_print_fn_t fn, const char *name, void *ptr)
{
	struct ndr_print *ndr;

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->print = ndr_print_debug_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	fn(ndr, name, ptr);
	talloc_free(ndr);
}

/*
  a useful helper function for printing idl unions via DEBUG()
*/
void ndr_print_union_debug(ndr_print_fn_t fn, const char *name, uint32_t level, void *ptr)
{
	struct ndr_print *ndr;

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->print = ndr_print_debug_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	ndr_print_set_switch_value(ndr, ptr, level);
	fn(ndr, name, ptr);
	talloc_free(ndr);
}

/*
  a useful helper function for printing idl function calls via DEBUG()
*/
void ndr_print_function_debug(ndr_print_function_t fn, const char *name, int flags, void *ptr)
{
	struct ndr_print *ndr;

	ndr = talloc_zero(NULL, struct ndr_print);
	if (!ndr) return;
	ndr->print = ndr_print_debug_helper;
	ndr->depth = 1;
	ndr->flags = 0;
	fn(ndr, name, flags, ptr);
	talloc_free(ndr);
}

void ndr_set_flags(uint32_t *pflags, uint32_t new_flags)
{
	/* the big/little endian flags are inter-dependent */
	if (new_flags & LIBNDR_FLAG_LITTLE_ENDIAN) {
		(*pflags) &= ~LIBNDR_FLAG_BIGENDIAN;
	}
	if (new_flags & LIBNDR_FLAG_BIGENDIAN) {
		(*pflags) &= ~LIBNDR_FLAG_LITTLE_ENDIAN;
	}
	(*pflags) |= new_flags;
}

static NTSTATUS ndr_map_error(enum ndr_err_code err)
{
	switch (err) {
	case NDR_ERR_BUFSIZE:
		return NT_STATUS_BUFFER_TOO_SMALL;
	case NDR_ERR_TOKEN:
		return NT_STATUS_INTERNAL_ERROR;
	case NDR_ERR_ALLOC:
		return NT_STATUS_NO_MEMORY;
	case NDR_ERR_ARRAY_SIZE:
		return NT_STATUS_ARRAY_BOUNDS_EXCEEDED;
	default:
		break;
	}

	/* we should all error codes to different status codes */
	return NT_STATUS_INVALID_PARAMETER;
}

/*
  return and possibly log an NDR error
*/
NTSTATUS ndr_pull_error(struct ndr_pull *ndr, 
			enum ndr_err_code err, const char *format, ...) _PRINTF_ATTRIBUTE(3,4)
{
	char *s=NULL;
	va_list ap;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	DEBUG(3,("ndr_pull_error(%u): %s\n", err, s));

	free(s);

	return ndr_map_error(err);
}

/*
  return and possibly log an NDR error
*/
NTSTATUS ndr_push_error(struct ndr_push *ndr, enum ndr_err_code err, const char *format, ...)  _PRINTF_ATTRIBUTE(3,4)
{
	char *s=NULL;
	va_list ap;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	DEBUG(3,("ndr_push_error(%u): %s\n", err, s));

	free(s);

	return ndr_map_error(err);
}


/*
  handle subcontext buffers, which in midl land are user-marshalled, but
  we use magic in pidl to make them easier to cope with
*/
NTSTATUS ndr_pull_subcontext_header(struct ndr_pull *ndr, 
					   size_t header_size,
					   ssize_t size_is,
					   struct ndr_pull *ndr2)
{
	ndr2->flags = ndr->flags;

	switch (header_size) {
	case 0: {
		uint32_t content_size = ndr->data_size - ndr->offset;
		if (size_is >= 0) {
			content_size = size_is;
		}
		NDR_CHECK(ndr_pull_subcontext(ndr, ndr2, content_size));
		break;
	}

	case 2: {
		uint16_t content_size;
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &content_size));
		if (size_is >= 0 && size_is != content_size) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) size_is(%d) mismatch content_size %d", 
						size_is, content_size);
		}
		NDR_CHECK(ndr_pull_subcontext(ndr, ndr2, content_size));
		break;
	}

	case 4: {
		uint32_t content_size;
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &content_size));
		if (size_is >= 0 && size_is != content_size) {
			return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) size_is(%d) mismatch content_size %d", 
						size_is, content_size);
		}
		NDR_CHECK(ndr_pull_subcontext(ndr, ndr2, content_size));
		break;
	}
	default:
		return ndr_pull_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PULL) header_size %d", 
				      header_size);
	}
	return NT_STATUS_OK;
}

/*
  push a subcontext header 
*/
NTSTATUS ndr_push_subcontext_header(struct ndr_push *ndr, 
					   size_t header_size,
					   ssize_t size_is,
					   struct ndr_push *ndr2)
{
	if (size_is >= 0) {
		ssize_t padding_len = size_is - ndr2->offset;
		if (padding_len > 0) {
			NDR_CHECK(ndr_push_zero(ndr2, padding_len));
		} else if (padding_len < 0) {
			return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext (PUSH) content_size %d is larger than size_is(%d)",
					      ndr2->offset, size_is);
		}
	}

	switch (header_size) {
	case 0: 
		break;

	case 2: 
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, ndr2->offset));
		break;

	case 4: 
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, ndr2->offset));
		break;

	default:
		return ndr_push_error(ndr, NDR_ERR_SUBCONTEXT, "Bad subcontext header size %d", 
				      header_size);
	}
	return NT_STATUS_OK;
}

/*
  mark the start of a structure
*/
NTSTATUS ndr_pull_struct_start(struct ndr_pull *ndr)
{
	return NT_STATUS_OK;
}

/*
  mark the end of a structure
*/
void ndr_pull_struct_end(struct ndr_pull *ndr)
{
}

/*
  mark the start of a structure
*/
NTSTATUS ndr_push_struct_start(struct ndr_push *ndr)
{
	return NT_STATUS_OK;
}

/*
  mark the end of a structure
*/
void ndr_push_struct_end(struct ndr_push *ndr)
{
}

/*
  store a token in the ndr context, for later retrieval
*/
static NTSTATUS ndr_token_store(TALLOC_CTX *mem_ctx, 
				struct ndr_token_list **list, 
				const void *key, 
				uint32_t value)
{
	struct ndr_token_list *tok;
	tok = talloc(mem_ctx, struct ndr_token_list);
	if (tok == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tok->key = key;
	tok->value = value;
	DLIST_ADD((*list), tok);
	return NT_STATUS_OK;
}

/*
  retrieve a token from a ndr context
*/
static NTSTATUS ndr_token_retrieve(struct ndr_token_list **list, const void *key, uint32_t *v)
{
	struct ndr_token_list *tok;
	for (tok=*list;tok;tok=tok->next) {
		if (tok->key == key) {
			DLIST_REMOVE((*list), tok);
			*v = tok->value;
			return NT_STATUS_OK;
		}
	}
	return ndr_map_error(NDR_ERR_TOKEN);
}

/*
  peek at but don't removed a token from a ndr context
*/
static uint32_t ndr_token_peek(struct ndr_token_list **list, const void *key)
{
	struct ndr_token_list *tok;
	for (tok=*list;tok;tok=tok->next) {
		if (tok->key == key) {
			return tok->value;
		}
	}
	return 0;
}

/*
  pull an array size field and add it to the array_size_list token list
*/
NTSTATUS ndr_pull_array_size(struct ndr_pull *ndr, const void *p)
{
	uint32_t size;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &size));
	return ndr_token_store(ndr, &ndr->array_size_list, p, size);
}

/*
  get the stored array size field
*/
uint32_t ndr_get_array_size(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->array_size_list, p);
}

/*
  check the stored array size field
*/
NTSTATUS ndr_check_array_size(struct ndr_pull *ndr, void *p, uint32_t size)
{
	uint32_t stored;
	NDR_CHECK(ndr_token_retrieve(&ndr->array_size_list, p, &stored));
	if (stored != size) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array size - got %u expected %u\n",
				      stored, size);
	}
	return NT_STATUS_OK;
}

/*
  pull an array length field and add it to the array_length_list token list
*/
NTSTATUS ndr_pull_array_length(struct ndr_pull *ndr, const void *p)
{
	uint32_t length, offset;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &offset));
	if (offset != 0) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "non-zero array offset %u\n", offset);
	}
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	return ndr_token_store(ndr, &ndr->array_length_list, p, length);
}

/*
  get the stored array length field
*/
uint32_t ndr_get_array_length(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->array_length_list, p);
}

/*
  check the stored array length field
*/
NTSTATUS ndr_check_array_length(struct ndr_pull *ndr, void *p, uint32_t length)
{
	uint32_t stored;
	NDR_CHECK(ndr_token_retrieve(&ndr->array_length_list, p, &stored));
	if (stored != length) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array length - got %u expected %u\n",
				      stored, length);
	}
	return NT_STATUS_OK;
}

/*
  store a switch value
 */
NTSTATUS ndr_push_set_switch_value(struct ndr_push *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

NTSTATUS ndr_pull_set_switch_value(struct ndr_pull *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

NTSTATUS ndr_print_set_switch_value(struct ndr_print *ndr, const void *p, uint32_t val)
{
	return ndr_token_store(ndr, &ndr->switch_list, p, val);
}

/*
  retrieve a switch value
 */
uint32_t ndr_push_get_switch_value(struct ndr_push *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

uint32_t ndr_pull_get_switch_value(struct ndr_pull *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

uint32_t ndr_print_get_switch_value(struct ndr_print *ndr, const void *p)
{
	return ndr_token_peek(&ndr->switch_list, p);
}

/*
  pull a relative object - stage1
  called during SCALARS processing
*/
NTSTATUS ndr_pull_relative_ptr1(struct ndr_pull *ndr, const void *p, uint32_t rel_offset)
{
	if (ndr->flags & LIBNDR_FLAG_RELATIVE_CURRENT) {
		return ndr_token_store(ndr, &ndr->relative_list, p, 
				       rel_offset + ndr->offset - 4);
	} else {
		return ndr_token_store(ndr, &ndr->relative_list, p, rel_offset);
	}
}

/*
  pull a relative object - stage2
  called during BUFFERS processing
*/
NTSTATUS ndr_pull_relative_ptr2(struct ndr_pull *ndr, const void *p)
{
	uint32_t rel_offset;
	NDR_CHECK(ndr_token_retrieve(&ndr->relative_list, p, &rel_offset));
	return ndr_pull_set_offset(ndr, rel_offset);
}

/*
  push a relative object - stage1
  this is called during SCALARS processing
*/
NTSTATUS ndr_push_relative_ptr1(struct ndr_push *ndr, const void *p)
{
	if (p == NULL) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		return NT_STATUS_OK;
	}
	NDR_CHECK(ndr_push_align(ndr, 4));
	NDR_CHECK(ndr_token_store(ndr, &ndr->relative_list, p, ndr->offset));
	return ndr_push_uint32(ndr, NDR_SCALARS, 0xFFFFFFFF);
}

/*
  push a relative object - stage2
  this is called during buffers processing
*/
NTSTATUS ndr_push_relative_ptr2(struct ndr_push *ndr, const void *p)
{
	struct ndr_push_save save;
	if (p == NULL) {
		return NT_STATUS_OK;
	}
	NDR_CHECK(ndr_push_align(ndr, 4));
	ndr_push_save(ndr, &save);
	NDR_CHECK(ndr_token_retrieve(&ndr->relative_list, p, &ndr->offset));
	if (ndr->flags & LIBNDR_FLAG_RELATIVE_CURRENT) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, save.offset - ndr->offset));
	} else {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, save.offset));
	}
	ndr_push_restore(ndr, &save);
	return NT_STATUS_OK;
}

/*
  pull a struct from a blob using NDR
*/
NTSTATUS ndr_pull_struct_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			      ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	ndr = ndr_pull_init_blob(blob, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	return fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
}

/*
  pull a struct from a blob using NDR - failing if all bytes are not consumed
*/
NTSTATUS ndr_pull_struct_blob_all(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
				  ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	NTSTATUS status;

	ndr = ndr_pull_init_blob(blob, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	status = fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) return status;
	if (ndr->offset != ndr->data_size) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}
	return status;
}

/*
  pull a union from a blob using NDR, given the union discriminator
*/
NTSTATUS ndr_pull_union_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			     uint32_t level, ndr_pull_flags_fn_t fn)
{
	struct ndr_pull *ndr;
	NTSTATUS status;

	ndr = ndr_pull_init_blob(blob, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	ndr_pull_set_switch_value(ndr, p, level);
	status = fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) return status;
	if (ndr->offset != ndr->data_size) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}
	return status;
}

/*
  push a struct to a blob using NDR
*/
NTSTATUS ndr_push_struct_blob(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p,
			      ndr_push_flags_fn_t fn)
{
	NTSTATUS status;
	struct ndr_push *ndr;
	ndr = ndr_push_init_ctx(mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	status = fn(ndr, NDR_SCALARS|NDR_BUFFERS, p);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*blob = ndr_push_blob(ndr);

	return NT_STATUS_OK;
}

/*
  generic ndr_size_*() handler for structures
*/
size_t ndr_size_struct(const void *p, int flags, ndr_push_flags_fn_t push)
{
	struct ndr_push *ndr;
	NTSTATUS status;
	size_t ret;

	/* avoid recursion */
	if (flags & LIBNDR_FLAG_NO_NDR_SIZE) return 0;

	ndr = ndr_push_init_ctx(NULL);
	if (!ndr) return 0;
	ndr->flags |= flags | LIBNDR_FLAG_NO_NDR_SIZE;
	status = push(ndr, NDR_SCALARS|NDR_BUFFERS, discard_const(p));
	if (!NT_STATUS_IS_OK(status)) {
		return 0;
	}
	ret = ndr->offset;
	talloc_free(ndr);
	return ret;
}

/*
  generic ndr_size_*() handler for unions
*/
size_t ndr_size_union(const void *p, int flags, uint32_t level, ndr_push_flags_fn_t push)
{
	struct ndr_push *ndr;
	NTSTATUS status;
	size_t ret;

	/* avoid recursion */
	if (flags & LIBNDR_FLAG_NO_NDR_SIZE) return 0;

	ndr = ndr_push_init_ctx(NULL);
	if (!ndr) return 0;
	ndr->flags |= flags | LIBNDR_FLAG_NO_NDR_SIZE;
	ndr_push_set_switch_value(ndr, p, level);
	status = push(ndr, NDR_SCALARS|NDR_BUFFERS, discard_const(p));
	if (!NT_STATUS_IS_OK(status)) {
		return 0;
	}
	ret = ndr->offset;
	talloc_free(ndr);
	return ret;
}
