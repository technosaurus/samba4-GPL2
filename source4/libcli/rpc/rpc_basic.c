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

#define NDR_NEED_BYTES(ndr, n) do { \
	if ((n) > ndr->data_size || ndr->offset + (n) > ndr->data_size) { \
		return NT_STATUS_BUFFER_TOO_SMALL; \
	} \
} while(0)

#define NDR_ALIGN(ndr, n) do { \
	ndr->offset = (ndr->offset + (n-1)) & ~(n-1); \
	if (ndr->offset >= ndr->data_size) { \
		return NT_STATUS_BUFFER_TOO_SMALL; \
	} \
} while(0)

/*
  parse a GUID
*/
NTSTATUS ndr_parse_guid(struct ndr_parse *ndr, GUID *guid)
{
	int i;
	NDR_NEED_BYTES(ndr, GUID_SIZE);
	for (i=0;i<GUID_SIZE;i++) {
		guid->info[i] = CVAL(ndr->data, ndr->offset + i);
	}
	ndr->offset += i;
	return NT_STATUS_OK;
}


/*
  parse a u8
*/
NTSTATUS ndr_parse_u8(struct ndr_parse *ndr, uint8 *v)
{
	NDR_NEED_BYTES(ndr, 1);
	*v = CVAL(ndr->data, ndr->offset);
	ndr->offset += 1;
	return NT_STATUS_OK;
}


/*
  parse a u16
*/
NTSTATUS ndr_parse_u16(struct ndr_parse *ndr, uint16 *v)
{
	NDR_ALIGN(ndr, 2);
	NDR_NEED_BYTES(ndr, 2);
	if (ndr->flags & LIBNDR_FLAG_BIGENDIAN) {
		*v = RSVAL(ndr->data, ndr->offset);
	} else {
		*v = SVAL(ndr->data, ndr->offset);
	}
	ndr->offset += 2;
	return NT_STATUS_OK;
}


/*
  parse a u32
*/
NTSTATUS ndr_parse_u32(struct ndr_parse *ndr, uint32 *v)
{
	NDR_ALIGN(ndr, 4);
	NDR_NEED_BYTES(ndr, 4);
	if (ndr->flags & LIBNDR_FLAG_BIGENDIAN) {
		*v = RIVAL(ndr->data, ndr->offset);
	} else {
		*v = IVAL(ndr->data, ndr->offset);
	}
	ndr->offset += 2;
	return NT_STATUS_OK;
}

