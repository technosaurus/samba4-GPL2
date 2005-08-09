/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "der_locl.h"

RCSID("$Id: der_copy.c,v 1.13 2005/07/12 06:27:20 lha Exp $");

int
copy_general_string (const heim_general_string *from, heim_general_string *to)
{
    *to = strdup(*from);
    if(*to == NULL)
	return ENOMEM;
    return 0;
}

int
copy_utf8string (const heim_utf8_string *from, heim_utf8_string *to)
{
    return copy_general_string(from, to);
}

int
copy_printable_string (const heim_printable_string *from, 
		       heim_printable_string *to)
{
    return copy_general_string(from, to);
}

int
copy_ia5_string (const heim_printable_string *from, 
		       heim_printable_string *to)
{
    return copy_general_string(from, to);
}

int
copy_bmp_string (const heim_bmp_string *from, heim_bmp_string *to)
{
    to->length = from->length;
    to->data   = malloc(to->length * sizeof(to->data[0]));
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length * sizeof(to->data[0]));
    return 0;
}

int
copy_universal_string (const heim_universal_string *from,
		       heim_universal_string *to)
{
    to->length = from->length;
    to->data   = malloc(to->length * sizeof(to->data[0]));
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length * sizeof(to->data[0]));
    return 0;
}

int
copy_octet_string (const heim_octet_string *from, heim_octet_string *to)
{
    to->length = from->length;
    to->data   = malloc(to->length);
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length);
    return 0;
}

int
copy_heim_integer (const heim_integer *from, heim_integer *to)
{
    to->length = from->length;
    to->data   = malloc(to->length);
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length);
    return 0;
}

int
copy_oid (const heim_oid *from, heim_oid *to)
{
    to->length     = from->length;
    to->components = malloc(to->length * sizeof(*to->components));
    if (to->length != 0 && to->components == NULL)
	return ENOMEM;
    memcpy(to->components, from->components,
	   to->length * sizeof(*to->components));
    return 0;
}

int
copy_bit_string (const heim_bit_string *from, heim_bit_string *to)
{
    size_t len;

    len = (from->length + 7) / 8;
    to->length = from->length;
    to->data   = malloc(len);
    if(len != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, len);
    return 0;
}
