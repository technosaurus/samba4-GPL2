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

#include "krb5_locl.h"

RCSID("$Id: creds.c 15167 2005-05-18 04:21:57Z lha $");

/* keep this for compatibility with older code */
krb5_error_code KRB5_LIB_FUNCTION
krb5_free_creds_contents (krb5_context context, krb5_creds *c)
{
    return krb5_free_cred_contents (context, c);
}    

krb5_error_code KRB5_LIB_FUNCTION
krb5_free_cred_contents (krb5_context context, krb5_creds *c)
{
    krb5_free_principal (context, c->client);
    c->client = NULL;
    krb5_free_principal (context, c->server);
    c->server = NULL;
    krb5_free_keyblock_contents (context, &c->session);
    krb5_data_free (&c->ticket);
    krb5_data_free (&c->second_ticket);
    free_AuthorizationData (&c->authdata);
    krb5_free_addresses (context, &c->addresses);
    memset(c, 0, sizeof(*c));
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_copy_creds_contents (krb5_context context,
			  const krb5_creds *incred,
			  krb5_creds *c)
{
    krb5_error_code ret;

    memset(c, 0, sizeof(*c));
    ret = krb5_copy_principal (context, incred->client, &c->client);
    if (ret)
	goto fail;
    ret = krb5_copy_principal (context, incred->server, &c->server);
    if (ret)
	goto fail;
    ret = krb5_copy_keyblock_contents (context, &incred->session, &c->session);
    if (ret)
	goto fail;
    c->times = incred->times;
    ret = krb5_data_copy (&c->ticket,
			  incred->ticket.data,
			  incred->ticket.length);
    if (ret)
	goto fail;
    ret = krb5_data_copy (&c->second_ticket,
			  incred->second_ticket.data,
			  incred->second_ticket.length);
    if (ret)
	goto fail;
    ret = copy_AuthorizationData(&incred->authdata, &c->authdata);
    if (ret)
	goto fail;
    ret = krb5_copy_addresses (context,
			       &incred->addresses,
			       &c->addresses);
    if (ret)
	goto fail;
    c->flags = incred->flags;
    return 0;

fail:
    krb5_free_cred_contents (context, c);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_copy_creds (krb5_context context,
		 const krb5_creds *incred,
		 krb5_creds **outcred)
{
    krb5_creds *c;

    c = malloc (sizeof (*c));
    if (c == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    memset (c, 0, sizeof(*c));
    *outcred = c;
    return krb5_copy_creds_contents (context, incred, c);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_free_creds (krb5_context context, krb5_creds *c)
{
    krb5_free_cred_contents (context, c);
    free (c);
    return 0;
}

/* XXX these do not belong here */
static krb5_boolean
krb5_data_equal(const krb5_data *a, const krb5_data *b)
{
    if(a->length != b->length)
	return FALSE;
    return memcmp(a->data, b->data, a->length) == 0;
}

static krb5_boolean
krb5_times_equal(const krb5_times *a, const krb5_times *b)
{
    return a->starttime == b->starttime &&
	a->authtime == b->authtime &&
	a->endtime == b->endtime &&
	a->renew_till == b->renew_till;
}

/*
 * Return TRUE if `mcreds' and `creds' are equal (`whichfields'
 * determines what equal means).
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_compare_creds(krb5_context context, krb5_flags whichfields,
		   const krb5_creds * mcreds, const krb5_creds * creds)
{
    krb5_boolean match = TRUE;
    
    if (match && mcreds->server) {
	if (whichfields & (KRB5_TC_DONT_MATCH_REALM | KRB5_TC_MATCH_SRV_NAMEONLY)) 
	    match = krb5_principal_compare_any_realm (context, mcreds->server, 
						      creds->server);
	else
	    match = krb5_principal_compare (context, mcreds->server, 
					    creds->server);
    }

    if (match && mcreds->client) {
	if(whichfields & KRB5_TC_DONT_MATCH_REALM)
	    match = krb5_principal_compare_any_realm (context, mcreds->client, 
						      creds->client);
	else
	    match = krb5_principal_compare (context, mcreds->client, 
					    creds->client);
    }
	    
    if (match && (whichfields & KRB5_TC_MATCH_KEYTYPE))
	match = krb5_enctypes_compatible_keys(context,
					      mcreds->session.keytype,
					      creds->session.keytype);

    if (match && (whichfields & KRB5_TC_MATCH_FLAGS_EXACT))
	match = mcreds->flags.i == creds->flags.i;

    if (match && (whichfields & KRB5_TC_MATCH_FLAGS))
	match = (creds->flags.i & mcreds->flags.i) == mcreds->flags.i;

    if (match && (whichfields & KRB5_TC_MATCH_TIMES_EXACT))
	match = krb5_times_equal(&mcreds->times, &creds->times);
    
    if (match && (whichfields & KRB5_TC_MATCH_TIMES))
	/* compare only expiration times */
	match = (mcreds->times.renew_till <= creds->times.renew_till) &&
	    (mcreds->times.endtime <= creds->times.endtime);

    if (match && (whichfields & KRB5_TC_MATCH_AUTHDATA)) {
	unsigned int i;
	if(mcreds->authdata.len != creds->authdata.len)
	    match = FALSE;
	else
	    for(i = 0; match && i < mcreds->authdata.len; i++)
		match = (mcreds->authdata.val[i].ad_type == 
			 creds->authdata.val[i].ad_type) &&
		    krb5_data_equal(&mcreds->authdata.val[i].ad_data,
				    &creds->authdata.val[i].ad_data);
    }
    if (match && (whichfields & KRB5_TC_MATCH_2ND_TKT))
	match = krb5_data_equal(&mcreds->second_ticket, &creds->second_ticket);

    if (match && (whichfields & KRB5_TC_MATCH_IS_SKEY))
	match = ((mcreds->second_ticket.length == 0) == 
		 (creds->second_ticket.length == 0));

    return match;
}
