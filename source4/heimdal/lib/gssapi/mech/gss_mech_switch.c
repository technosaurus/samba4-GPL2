/*-
 * Copyright (c) 2005 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/lib/libgssapi/gss_mech_switch.c,v 1.2 2006/02/04 09:40:21 dfr Exp $
 */

#include "mech_locl.h"
#include <heim_threads.h>
RCSID("$Id: gss_mech_switch.c 20625 2007-05-08 13:55:03Z lha $");

#ifndef _PATH_GSS_MECH
#define _PATH_GSS_MECH	"/etc/gss/mech"
#endif

struct _gss_mech_switch_list _gss_mechs = { NULL } ;
gss_OID_set _gss_mech_oids;
static HEIMDAL_MUTEX _gss_mech_mutex = HEIMDAL_MUTEX_INITIALIZER;

/*
 * Convert a string containing an OID in 'dot' form
 * (e.g. 1.2.840.113554.1.2.2) to a gss_OID.
 */
static int
_gss_string_to_oid(const char* s, gss_OID oid)
{
	int			number_count, i, j;
	int			byte_count;
	const char		*p, *q;
	char			*res;

	oid->length = 0;
	oid->elements = NULL;

	/*
	 * First figure out how many numbers in the oid, then
	 * calculate the compiled oid size.
	 */
	number_count = 0;
	for (p = s; p; p = q) {
		q = strchr(p, '.');
		if (q) q = q + 1;
		number_count++;
	}
	
	/*
	 * The first two numbers are in the first byte and each
	 * subsequent number is encoded in a variable byte sequence.
	 */
	if (number_count < 2)
		return (EINVAL);

	/*
	 * We do this in two passes. The first pass, we just figure
	 * out the size. Second time around, we actually encode the
	 * number.
	 */
	res = 0;
	for (i = 0; i < 2; i++) {
		byte_count = 0;
		for (p = s, j = 0; p; p = q, j++) {
			unsigned int number = 0;

			/*
			 * Find the end of this number.
			 */
			q = strchr(p, '.');
			if (q) q = q + 1;

			/*
			 * Read the number of of the string. Don't
			 * bother with anything except base ten.
			 */
			while (*p && *p != '.') {
				number = 10 * number + (*p - '0');
				p++;
			}

			/*
			 * Encode the number. The first two numbers
			 * are packed into the first byte. Subsequent
			 * numbers are encoded in bytes seven bits at
			 * a time with the last byte having the high
			 * bit set.
			 */
			if (j == 0) {
				if (res)
					*res = number * 40;
			} else if (j == 1) {
				if (res) {
					*res += number;
					res++;
				}
				byte_count++;
			} else if (j >= 2) {
				/*
				 * The number is encoded in seven bit chunks.
				 */
				unsigned int t;
				int bytes;

				bytes = 0;
				for (t = number; t; t >>= 7)
					bytes++;
				if (bytes == 0) bytes = 1;
				while (bytes) {
					if (res) {
						int bit = 7*(bytes-1);
						
						*res = (number >> bit) & 0x7f;
						if (bytes != 1)
							*res |= 0x80;
						res++;
					}
					byte_count++;
					bytes--;
				}
			}
		}
		if (!res) {
			res = malloc(byte_count);
			if (!res)
				return (ENOMEM);
			oid->length = byte_count;
			oid->elements = res;
		}
	}

	return (0);
}

#define SYM(name)							\
do {									\
	m->gm_mech.gm_ ## name = dlsym(so, "gss_" #name);		\
	if (!m->gm_mech.gm_ ## name) {					\
		fprintf(stderr, "can't find symbol gss_" #name "\n");	\
		goto bad;						\
	}								\
} while (0)

#define OPTSYM(name)							\
do {									\
	m->gm_mech.gm_ ## name = dlsym(so, "gss_" #name);			\
} while (0)

/*
 *
 */
static int
add_builtin(gssapi_mech_interface mech)
{
    struct _gss_mech_switch *m;
    OM_uint32 minor_status;

    /* not registering any mech is ok */
    if (mech == NULL)
	return 0;

    m = malloc(sizeof(*m));
    if (m == NULL)
	return 1;
    m->gm_so = NULL;
    m->gm_mech = *mech;
    m->gm_mech_oid = mech->gm_mech_oid; /* XXX */
    gss_add_oid_set_member(&minor_status,
			   &m->gm_mech.gm_mech_oid, &_gss_mech_oids);

    SLIST_INSERT_HEAD(&_gss_mechs, m, gm_link);
    return 0;
}

/*
 * Load the mechanisms file (/etc/gss/mech).
 */
void
_gss_load_mech(void)
{
	OM_uint32	major_status, minor_status;
	FILE		*fp;
	char		buf[256];
	char		*p;
	char		*name, *oid, *lib, *kobj;
	struct _gss_mech_switch *m;
	void		*so;


	HEIMDAL_MUTEX_lock(&_gss_mech_mutex);

	if (SLIST_FIRST(&_gss_mechs)) {
		HEIMDAL_MUTEX_unlock(&_gss_mech_mutex);
		return;
	}

	major_status = gss_create_empty_oid_set(&minor_status,
	    &_gss_mech_oids);
	if (major_status) {
		HEIMDAL_MUTEX_unlock(&_gss_mech_mutex);
		return;
	}

	add_builtin(__gss_krb5_initialize());
	add_builtin(__gss_spnego_initialize());
	add_builtin(__gss_ntlm_initialize());

	fp = fopen(_PATH_GSS_MECH, "r");
	if (!fp) {
/*		perror(_PATH_GSS_MECH); */
		HEIMDAL_MUTEX_unlock(&_gss_mech_mutex);
		return;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		if (*buf == '#')
			continue;
		p = buf;
		name = strsep(&p, "\t\n ");
		if (p) while (isspace((unsigned char)*p)) p++;
		oid = strsep(&p, "\t\n ");
		if (p) while (isspace((unsigned char)*p)) p++;
		lib = strsep(&p, "\t\n ");
		if (p) while (isspace((unsigned char)*p)) p++;
		kobj = strsep(&p, "\t\n ");
		if (!name || !oid || !lib || !kobj)
			continue;

#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif

		so = dlopen(lib, RTLD_LOCAL);
		if (!so) {
/*			fprintf(stderr, "dlopen: %s\n", dlerror()); */
			continue;
		}

		m = malloc(sizeof(*m));
		if (!m)
			break;
		m->gm_so = so;
		if (_gss_string_to_oid(oid, &m->gm_mech.gm_mech_oid)) {
			free(m);
			continue;
		}
		
		major_status = gss_add_oid_set_member(&minor_status,
		    &m->gm_mech.gm_mech_oid, &_gss_mech_oids);
		if (major_status) {
			free(m->gm_mech.gm_mech_oid.elements);
			free(m);
			continue;
		}

		SYM(acquire_cred);
		SYM(release_cred);
		SYM(init_sec_context);
		SYM(accept_sec_context);
		SYM(process_context_token);
		SYM(delete_sec_context);
		SYM(context_time);
		SYM(get_mic);
		SYM(verify_mic);
		SYM(wrap);
		SYM(unwrap);
		SYM(display_status);
		SYM(indicate_mechs);
		SYM(compare_name);
		SYM(display_name);
		SYM(import_name);
		SYM(export_name);
		SYM(release_name);
		SYM(inquire_cred);
		SYM(inquire_context);
		SYM(wrap_size_limit);
		SYM(add_cred);
		SYM(inquire_cred_by_mech);
		SYM(export_sec_context);
		SYM(import_sec_context);
		SYM(inquire_names_for_mech);
		SYM(inquire_mechs_for_name);
		SYM(canonicalize_name);
		SYM(duplicate_name);
		OPTSYM(inquire_cred_by_oid);
		OPTSYM(inquire_sec_context_by_oid);
		OPTSYM(set_sec_context_option);
		OPTSYM(set_cred_option);
		OPTSYM(pseudo_random);

		SLIST_INSERT_HEAD(&_gss_mechs, m, gm_link);
		continue;

	bad:
		free(m->gm_mech.gm_mech_oid.elements);
		free(m);
		dlclose(so);
		continue;
	}
	fclose(fp);
	HEIMDAL_MUTEX_unlock(&_gss_mech_mutex);
}

gssapi_mech_interface
__gss_get_mechanism(gss_OID mech)
{
        struct _gss_mech_switch	*m;

	_gss_load_mech();
	SLIST_FOREACH(m, &_gss_mechs, gm_link) {
		if (gss_oid_equal(&m->gm_mech.gm_mech_oid, mech))
			return &m->gm_mech;
	}
	return NULL;
}
