/*
 * Copyright (c) 2006 Kungliga Tekniska H�gskolan
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

#include "kdc_locl.h"
#include <hex.h>

RCSID("$Id: kx509.c,v 1.1 2006/12/28 21:03:53 lha Exp $");

/*
 *
 */

krb5_error_code
_kdc_try_kx509_request(void *ptr, size_t len, Kx509Request *req, size_t *size)
{
    if (len < 4)
	return -1;
    if (memcmp("\x00\x00\x02\x00", ptr, 4) != 0)
	return -1;
    return decode_Kx509Request(((unsigned char *)ptr) + 4, len - 4, req, size);
}

/*
 *
 */

static const char version_2_0[4] = {0 , 0, 2, 0};

static krb5_error_code
verify_req_hash(krb5_context context, 
		const Kx509Request *req,
		krb5_keyblock *key)
{
    unsigned char digest[SHA_DIGEST_LENGTH];
    HMAC_CTX ctx;
    
    if (req->pk_hash.length != sizeof(digest)) {
	krb5_set_error_string(context, "pk-hash have wrong length: %lu",
			      (unsigned long)req->pk_hash.length);
	return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    HMAC_CTX_init(&ctx);
    HMAC_Init_ex(&ctx, 
		 key->keyvalue.data, key->keyvalue.length, 
		 EVP_sha1(), NULL);
    if (sizeof(digest) != HMAC_size(&ctx))
	krb5_abortx(context, "runtime error, hmac buffer wrong size in kx509");
    HMAC_Update(&ctx, version_2_0, sizeof(version_2_0));
    HMAC_Update(&ctx, req->pk_key.data, req->pk_key.length);
    HMAC_Final(&ctx, digest, 0);
    HMAC_CTX_cleanup(&ctx);

    if (memcmp(req->pk_hash.data, digest, sizeof(digest)) != 0) {
	krb5_set_error_string(context, "pk-hash is not correct");
	return KRB5KDC_ERR_PREAUTH_FAILED;
    }
    return 0;
}

static krb5_error_code
calculate_reply_hash(krb5_context context,
		     krb5_keyblock *key,
		     Kx509Response *rep)
{
    HMAC_CTX ctx;
    
    HMAC_CTX_init(&ctx);

    HMAC_Init_ex(&ctx, 
		 key->keyvalue.data, key->keyvalue.length, 
		 EVP_sha1(), NULL);
    rep->hash->length = HMAC_size(&ctx);
    rep->hash->data = malloc(rep->hash->length);
    if (rep->hash->data == NULL) {
	HMAC_CTX_cleanup(&ctx);
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }

    HMAC_Update(&ctx, version_2_0, sizeof(version_2_0));
    if (rep->error_code) {
	int32_t t = *rep->error_code;
	do {
	    unsigned char p = (t & 0xff);
	    HMAC_Update(&ctx, &p, 1);
	    t >>= 8;
	} while (t);
    }
    if (rep->certificate)
	HMAC_Update(&ctx, rep->certificate->data, rep->certificate->length);
    if (rep->e_text)
	HMAC_Update(&ctx, *rep->e_text, strlen(*rep->e_text));

    HMAC_Final(&ctx, rep->hash->data, 0);
    HMAC_CTX_cleanup(&ctx);

    return 0;
}

/*
 * Build a certifate for `principal� that will expire at `endtime�.
 */

static krb5_error_code
build_certificate(krb5_context context, 
		  krb5_kdc_configuration *config,
		  const krb5_data *key,
		  time_t endtime,
		  krb5_principal principal,
		  krb5_data *certificate)
{
    /* XXX write code here to generate certificates */
    FILE *in, *out;
    krb5_error_code ret;
    const char *program;
    char *str, *strkey;
    char tstr[64];
    pid_t pid;

    snprintf(tstr, sizeof(tstr), "%lu", (unsigned long)endtime);

    ret = base64_encode(key->data, key->length, &strkey);
    if (ret < 0) {
	krb5_set_error_string(context, "failed to base64 encode key");
	return ENOMEM;
    }

    program = krb5_config_get_string(context,
				     NULL,
				     "kdc",
				     "kx509_cert_program",
				     NULL);
    if (program == NULL) {
	free(strkey);
	krb5_set_error_string(context, "no certificate program configured");
	return ENOENT;
    }

    ret = krb5_unparse_name(context, principal, &str);
    if (ret) {
	free(strkey);
	return ret;
    }

    pid = pipe_execv(&in, &out, NULL, program, str, tstr, NULL);
    free(str);
    if (pid <= 0) {
	free(strkey);
	krb5_set_error_string(context, 
			      "Failed to run the cert program %s",
			      program);
	return ret;
    }
    fprintf(in, "%s\n", strkey);
    fclose(in);
    free(strkey);

    {
	unsigned buf[1024 * 10];
	size_t len;

	len = fread(buf, 1, sizeof(buf), out);
	fclose(out);
	if(len == 0) {
	    krb5_set_error_string(context, 
				  "Certificate program returned no data");
	    return KRB5KDC_ERR_PREAUTH_FAILED;
	}
	ret = krb5_data_copy(certificate, buf, len);
	if (ret) {
	    krb5_set_error_string(context, "Failed To copy certificate");
	    return ret;
	}
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return 0;
}

/*
 *
 */

krb5_error_code
_kdc_do_kx509(krb5_context context, 
	      krb5_kdc_configuration *config,
	      const Kx509Request *req, krb5_data *reply,
	      const char *from, struct sockaddr *addr)
{
    krb5_error_code ret;
    krb5_ticket *ticket = NULL;
    krb5_flags ap_req_options;
    krb5_auth_context ac = NULL;
    krb5_keytab id = NULL;
    krb5_principal sprincipal = NULL, cprincipal = NULL;
    char *cname = NULL;
    Kx509Response rep;
    size_t size;
    krb5_keyblock *key = NULL;

    krb5_data_zero(reply);
    memset(&rep, 0, sizeof(rep));

    if(!config->enable_kx509) {
	kdc_log(context, config, 0, 
		"Rejected kx509 request (disabled) from %s", from);
	return KRB5KDC_ERR_POLICY;
    }

    kdc_log(context, config, 0, "Kx509 request from %s", from);

    ret = krb5_kt_resolve(context, "HDB:", &id);
    if (ret) {
	kdc_log(context, config, 0, "Can't open database for digest");
	goto out;
    }

    ret = krb5_rd_req(context, 
		      &ac,
		      &req->authenticator,
		      NULL,
		      id,
		      &ap_req_options,
		      &ticket);
    if (ret)
	goto out;

    ret = krb5_ticket_get_client(context, ticket, &cprincipal);
    if (ret)
	goto out;

    ret = krb5_unparse_name(context, cprincipal, &cname);
    if (ret)
	goto out;
    
    /* verify server principal */

    ret = krb5_sname_to_principal(context, NULL, "kca_service",
				  KRB5_NT_UNKNOWN, &sprincipal);
    if (ret)
	goto out;

    {
	krb5_principal principal = NULL;

	ret = krb5_ticket_get_server(context, ticket, &principal);
	if (ret)
	    goto out;

	ret = krb5_principal_compare(context, sprincipal, principal);
	krb5_free_principal(context, principal);
	if (ret != TRUE) {
	    ret = KRB5KDC_ERR_SERVER_NOMATCH;
	    krb5_set_error_string(context, 
				  "User %s used wrong Kx509 service principal",
				  cname);
	    goto out;
	}
    }
    
    ret = krb5_auth_con_getkey(context, ac, &key);
    if (ret || key == NULL) {
	krb5_set_error_string(context, "Kx509 can't get session key");
	goto out;
    }
    
    ret = verify_req_hash(context, req, key);
    if (ret)
	goto out;

    ALLOC(rep.certificate);
    if (rep.certificate == NULL)
	goto out;
    krb5_data_zero(rep.certificate);
    ALLOC(rep.hash);
    if (rep.hash == NULL)
	goto out;
    krb5_data_zero(rep.hash);

    ret = build_certificate(context, config, &req->pk_key, 
			    krb5_ticket_get_endtime(context, ticket),
			    cprincipal, rep.certificate);
    if (ret)
	goto out;

    ret = calculate_reply_hash(context, key, &rep);
    if (ret)
	goto out;

    /*
     * Encode reply, [ version | Kx509Response ]
     */

    {
	krb5_data data;

	ASN1_MALLOC_ENCODE(Kx509Response, data.data, data.length, &rep,
			   &size, ret);
	if (ret) {
	    krb5_set_error_string(context, "Failed to encode kx509 reply");
	    goto out;
	}
	if (size != data.length)
	    krb5_abortx(context, "ASN1 internal error");

	ret = krb5_data_alloc(reply, data.length + sizeof(version_2_0));
	if (ret) {
	    free(data.data);
	    goto out;
	}
	memcpy(reply->data, version_2_0, sizeof(version_2_0));
	memcpy(((unsigned char *)reply->data) + sizeof(version_2_0),
	       data.data, data.length);
	free(data.data);
    }

    kdc_log(context, config, 0, "Successful Kx509 request for %s", cname);

out:
    if (ac)
	krb5_auth_con_free(context, ac);
    if (ret)
	krb5_warn(context, ret, "Kx509 request from %s failed", from);
    if (ticket)
	krb5_free_ticket(context, ticket);
    if (id)
	krb5_kt_close(context, id);
    if (sprincipal)
	krb5_free_principal(context, sprincipal);
    if (cprincipal)
	krb5_free_principal(context, cprincipal);
    if (key)
	krb5_free_keyblock (context, key);
    if (cname)
	free(cname);
    free_Kx509Response(&rep);

    return 0;
}