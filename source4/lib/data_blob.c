/* 
   Unix SMB/CIFS implementation.
   Easy management of byte-length data
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Andrew Bartlett 2001
   
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

/*******************************************************************
 construct a data blob, must be freed with data_blob_free()
 you can pass NULL for p and get a blank data blob
*******************************************************************/
DATA_BLOB data_blob_named(const void *p, size_t length, const char *name)
{
	DATA_BLOB ret;

	if (length == 0) {
		ZERO_STRUCT(ret);
		return ret;
	}

	if (p) {
		ret.data = talloc_memdup(NULL, p, length);
	} else {
		ret.data = talloc_size(NULL, length);
	}
	if (ret.data == NULL) {
		ret.length = 0;
		return ret;
	}
	talloc_set_name_const(ret.data, name);
	ret.length = length;
	return ret;
}

/*******************************************************************
 construct a data blob, using supplied TALLOC_CTX
*******************************************************************/
DATA_BLOB data_blob_talloc_named(TALLOC_CTX *mem_ctx, const void *p, size_t length, const char *name)
{
	DATA_BLOB ret = data_blob_named(p, length, name);

	if (ret.data) {
		talloc_steal(mem_ctx, ret.data);
	}
	return ret;
}


/*******************************************************************
 reference a data blob, to the supplied TALLOC_CTX.  
 Returns a NULL DATA_BLOB on failure
*******************************************************************/
DATA_BLOB data_blob_talloc_reference(TALLOC_CTX *mem_ctx, DATA_BLOB *blob)
{
	DATA_BLOB ret = *blob;

	ret.data = talloc_reference(mem_ctx, blob->data);

	if (!ret.data) {
		return data_blob(NULL, 0);
	}
	return ret;
}

/*******************************************************************
 construct a zero data blob, using supplied TALLOC_CTX. 
 use this sparingly as it initialises data - better to initialise
 yourself if you want specific data in the blob
*******************************************************************/
DATA_BLOB data_blob_talloc_zero(TALLOC_CTX *mem_ctx, size_t length)
{
	DATA_BLOB blob = data_blob_talloc(mem_ctx, NULL, length);
	data_blob_clear(&blob);
	return blob;
}

/*******************************************************************
free a data blob
*******************************************************************/
void data_blob_free(DATA_BLOB *d)
{
	if (d) {
		talloc_free(d->data);
		d->data = NULL;
		d->length = 0;
	}
}

/*******************************************************************
clear a DATA_BLOB's contents
*******************************************************************/
void data_blob_clear(DATA_BLOB *d)
{
	if (d->data) {
		memset(d->data, 0, d->length);
	}
}

/*******************************************************************
free a data blob and clear its contents
*******************************************************************/
void data_blob_clear_free(DATA_BLOB *d)
{
	data_blob_clear(d);
	data_blob_free(d);
}


/*******************************************************************
check if two data blobs are equal
*******************************************************************/
BOOL data_blob_equal(const DATA_BLOB *d1, const DATA_BLOB *d2)
{
	if (d1->length != d2->length) {
		return False;
	}
	if (d1->data == d2->data) {
		return True;
	}
	if (d1->data == NULL || d2->data == NULL) {
		return False;
	}
	if (memcmp(d1->data, d2->data, d1->length) == 0) {
		return True;
	}
	return False;
}

/*******************************************************************
print the data_blob as hex string
*******************************************************************/
char *data_blob_hex_string(TALLOC_CTX *mem_ctx, DATA_BLOB *blob)
{
	int i;
	char *hex_string;

	hex_string = talloc_array_p(mem_ctx, char, (blob->length*2)+1);
	if (!hex_string) {
		return NULL;
	}

	for (i = 0; i < blob->length; i++)
		slprintf(&hex_string[i*2], 3, "%02X", blob->data[i]);

	return hex_string;
}

/*
  useful for constructing data blobs in test suites, while
  avoiding const warnings
*/
DATA_BLOB data_blob_string_const(const char *str)
{
	DATA_BLOB blob;
	blob.data = discard_const(str);
	blob.length = strlen(str);
	return blob;
}

DATA_BLOB data_blob_const(const void *p, size_t length)
{
	DATA_BLOB blob;
	blob.data = discard_const(p);
	blob.length = length;
	return blob;
}
