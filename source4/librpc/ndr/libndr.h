/* 
   Unix SMB/CIFS implementation.
   rpc interface definitions
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
  this provides definitions for the libcli/rpc/ MSRPC library
*/


/* offset lists are used to allow a push/pull function to find the
   start of an encapsulating structure */
struct ndr_ofs_list {
	uint32 offset;
	struct ndr_ofs_list *next;
};


/* this is the base structure passed to routines that 
   parse MSRPC formatted data 

   note that in Samba4 we use separate routines and structures for
   MSRPC marshalling and unmarshalling. Also note that these routines
   are being kept deliberately very simple, and are not tied to a
   particular transport
*/
struct ndr_pull {
	uint32 flags; /* LIBNDR_FLAG_* */
	char *data;
	uint32 data_size;
	uint32 offset;
	TALLOC_CTX *mem_ctx;

	/* this points at a list of offsets to the structures being processed.
	   The first element in the list is the current structure */
	struct ndr_ofs_list *ofs_list;
};

struct ndr_pull_save {
	uint32 data_size;
	uint32 offset;
	struct ndr_pull_save *next;
};

/* structure passed to functions that generate NDR formatted data */
struct ndr_push {
	uint32 flags; /* LIBNDR_FLAG_* */
	char *data;
	uint32 alloc_size;
	uint32 offset;
	TALLOC_CTX *mem_ctx;

	/* this is used to ensure we generate unique reference IDs */
	uint32 ptr_count;

	/* this points at a list of offsets to the structures being processed.
	   The first element in the list is the current structure */
	struct ndr_ofs_list *ofs_list;

	/* this list is used by the [relative] code to find the offsets */
	struct ndr_ofs_list *relative_list, *relative_list_end;
};

struct ndr_push_save {
	uint32 offset;
	struct ndr_push_save *next;
};


/* structure passed to functions that print IDL structures */
struct ndr_print {
	uint32 flags; /* LIBNDR_FLAG_* */
	TALLOC_CTX *mem_ctx;
	uint32 depth;
	void (*print)(struct ndr_print *, const char *, ...);
	void *private;
};

#define LIBNDR_FLAG_BIGENDIAN  (1<<0)
#define LIBNDR_FLAG_NOALIGN    (1<<1)

#define LIBNDR_FLAG_STR_ASCII    (1<<2)
#define LIBNDR_FLAG_STR_LEN4     (1<<3)
#define LIBNDR_FLAG_STR_SIZE4    (1<<4)
#define LIBNDR_FLAG_STR_NOTERM   (1<<5)
#define LIBNDR_FLAG_STR_NULLTERM (1<<6)
#define LIBNDR_FLAG_STR_SIZE2    (1<<7)
#define LIBNDR_STRING_FLAGS      (0xFC)

#define LIBNDR_FLAG_REF_ALLOC    (1<<10)
#define LIBNDR_FLAG_REMAINING    (1<<11)
#define LIBNDR_FLAG_ALIGN2       (1<<12)
#define LIBNDR_FLAG_ALIGN4       (1<<13)
#define LIBNDR_FLAG_ALIGN8       (1<<14)

#define LIBNDR_ALIGN_FLAGS (LIBNDR_FLAG_ALIGN2|LIBNDR_FLAG_ALIGN4|LIBNDR_FLAG_ALIGN8)

#define LIBNDR_PRINT_ARRAY_HEX   (1<<15)


/* useful macro for debugging */
#define NDR_PRINT_DEBUG(type, p) ndr_print_debug((ndr_print_fn_t)ndr_print_ ##type, #p, p)
#define NDR_PRINT_UNION_DEBUG(type, level, p) ndr_print_union_debug((ndr_print_union_fn_t)ndr_print_ ##type, #p, level, p)
#define NDR_PRINT_FUNCTION_DEBUG(type, flags, p) ndr_print_function_debug((ndr_print_function_t)ndr_print_ ##type, #type, flags, p)
#define NDR_PRINT_BOTH_DEBUG(type, p) NDR_PRINT_FUNCTION_DEBUG(type, NDR_BOTH, p)
#define NDR_PRINT_OUT_DEBUG(type, p) NDR_PRINT_FUNCTION_DEBUG(type, NDR_OUT, p)
#define NDR_PRINT_IN_DEBUG(type, p) NDR_PRINT_FUNCTION_DEBUG(type, NDR_IN, p)


enum ndr_err_code {
	NDR_ERR_CONFORMANT_SIZE,
	NDR_ERR_ARRAY_SIZE,
	NDR_ERR_BAD_SWITCH,
	NDR_ERR_OFFSET,
	NDR_ERR_RELATIVE,
	NDR_ERR_CHARCNV,
	NDR_ERR_LENGTH,
	NDR_ERR_SUBCONTEXT,
	NDR_ERR_STRING,
	NDR_ERR_VALIDATE,
	NDR_ERR_BUFSIZE,
	NDR_ERR_ALLOC
};

/*
  flags passed to control parse flow
*/
#define NDR_SCALARS 1
#define NDR_BUFFERS 2

/*
  flags passed to ndr_print_*()
*/
#define NDR_IN 1
#define NDR_OUT 2
#define NDR_BOTH 3

#define NDR_PULL_NEED_BYTES(ndr, n) do { \
	if ((n) > ndr->data_size || ndr->offset + (n) > ndr->data_size) { \
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, "Pull bytes %u", n); \
	} \
} while(0)

#define NDR_ALIGN(ndr, n) ndr_align_size(ndr->offset, n)

#define NDR_PULL_ALIGN(ndr, n) do { \
	if (!(ndr->flags & LIBNDR_FLAG_NOALIGN)) { \
		ndr->offset = (ndr->offset + (n-1)) & ~(n-1); \
	} \
	if (ndr->offset >= ndr->data_size) { \
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, "Pull align %u", n); \
	} \
} while(0)

#define NDR_PUSH_NEED_BYTES(ndr, n) NDR_CHECK(ndr_push_expand(ndr, ndr->offset+(n)))

#define NDR_PUSH_ALIGN(ndr, n) do { \
	if (!(ndr->flags & LIBNDR_FLAG_NOALIGN)) { \
		uint32 _pad = ((ndr->offset + (n-1)) & ~(n-1)) - ndr->offset; \
		while (_pad--) NDR_CHECK(ndr_push_uint8(ndr, 0)); \
	} \
} while(0)


/* these are used to make the error checking on each element in libndr
   less tedious, hopefully making the code more readable */
#define NDR_CHECK(call) do { NTSTATUS _status; \
                             _status = call; \
                             if (!NT_STATUS_IS_OK(_status)) \
                                return _status; \
                        } while (0)


#define NDR_ALLOC_SIZE(ndr, s, size) do { \
	                       (s) = talloc(ndr->mem_ctx, size); \
                               if ((size) && !(s)) return ndr_pull_error(ndr, NDR_ERR_ALLOC, \
							       "Alloc %u failed\n", \
							       size); \
                           } while (0)

#define NDR_ALLOC(ndr, s) NDR_ALLOC_SIZE(ndr, s, sizeof(*(s)))


#define NDR_ALLOC_N_SIZE(ndr, s, n, elsize) do { \
				if ((n) == 0) { \
					(s) = NULL; \
				} else { \
					(s) = talloc(ndr->mem_ctx, (n) * elsize); \
                               		if (!(s)) return ndr_pull_error(ndr, \
									NDR_ERR_ALLOC, \
									"Alloc %u * %u failed\n", \
									n, elsize); \
				} \
                           } while (0)

#define NDR_ALLOC_N(ndr, s, n) NDR_ALLOC_N_SIZE(ndr, s, n, sizeof(*(s)))


#define NDR_PUSH_ALLOC_SIZE(ndr, s, size) do { \
	                       (s) = talloc(ndr->mem_ctx, size); \
                               if ((size) && !(s)) return ndr_push_error(ndr, NDR_ERR_ALLOC, \
							       "push alloc %u failed\n",\
							       size); \
                           } while (0)

#define NDR_PUSH_ALLOC(ndr, s) NDR_PUSH_ALLOC_SIZE(ndr, s, sizeof(*(s)))


/* these are used when generic fn pointers are needed for ndr push/pull fns */
typedef NTSTATUS (*ndr_push_fn_t)(struct ndr_push *, void *);
typedef NTSTATUS (*ndr_pull_fn_t)(struct ndr_pull *, void *);

typedef NTSTATUS (*ndr_push_flags_fn_t)(struct ndr_push *, int ndr_flags, void *);
typedef NTSTATUS (*ndr_push_const_fn_t)(struct ndr_push *, int ndr_flags, const void *);
typedef NTSTATUS (*ndr_pull_flags_fn_t)(struct ndr_pull *, int ndr_flags, void *);
typedef NTSTATUS (*ndr_push_union_fn_t)(struct ndr_push *, int ndr_flags, uint32, void *);
typedef NTSTATUS (*ndr_pull_union_fn_t)(struct ndr_pull *, int ndr_flags, uint32, void *);
typedef void (*ndr_print_fn_t)(struct ndr_print *, const char *, void *);
typedef void (*ndr_print_function_t)(struct ndr_print *, const char *, int, void *);
typedef void (*ndr_print_union_fn_t)(struct ndr_print *, const char *, uint32, void *);

#include "librpc/ndr/ndr_basic.h"
#include "librpc/ndr/ndr_sec.h"

/* now pull in the individual parsers */
#include "librpc/gen_ndr/tables.h"
