/* 
   Unix SMB/CIFS implementation.

   Validate the krb5 pac generation routines
   
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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
#include "system/kerberos.h"
#include "auth/auth.h"
#include "auth/kerberos/kerberos.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"

#ifdef HAVE_KRB5

static BOOL torture_pac_self_check(void) 
{
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "PAC self check");
	DATA_BLOB tmp_blob;
	struct PAC_LOGON_INFO *pac_info;

	/* Generate a nice, arbitary keyblock */
	uint8_t server_bytes[16];
	uint8_t krbtgt_bytes[16];
	krb5_keyblock server_keyblock;
	krb5_keyblock krbtgt_keyblock;
	
	krb5_error_code ret;

	struct smb_krb5_context *smb_krb5_context;

	struct auth_serversupplied_info *server_info;

	ret = smb_krb5_init_context(mem_ctx, &smb_krb5_context);

	if (ret) {
		talloc_free(mem_ctx);
		return False;
	}

	generate_random_buffer(server_bytes, 16);
	generate_random_buffer(krbtgt_bytes, 16);

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 server_bytes, sizeof(server_bytes),
				 &server_keyblock);
	if (ret) {
		DEBUG(1, ("Server Keyblock encoding failed: %s\n", 
			  smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						     ret, mem_ctx)));

		talloc_free(mem_ctx);
		return False;
	}

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 krbtgt_bytes, sizeof(krbtgt_bytes),
				 &krbtgt_keyblock);
	if (ret) {
		DEBUG(1, ("KRBTGT Keyblock encoding failed: %s\n", 
			  smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						     ret, mem_ctx)));

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		talloc_free(mem_ctx);
		return False;
	}

	/* We need an input, and this one requires no underlying database */
	nt_status = auth_anonymous_server_info(mem_ctx, &server_info);

	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		talloc_free(mem_ctx);
		return False;
	}
	
	/* OK, go ahead and make a PAC */
	ret = kerberos_encode_pac(mem_ctx, server_info, 
				  smb_krb5_context->krb5_context,  
				  &krbtgt_keyblock,
				  &server_keyblock,
				  &tmp_blob);
	
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &krbtgt_keyblock);

	if (ret) {
		DEBUG(1, ("PAC encoding failed: %s\n", 
			  smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						     ret, mem_ctx)));

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		talloc_free(mem_ctx);
		return False;
	}
	
	/* Now check that we can read it back */
	nt_status = kerberos_decode_pac(mem_ctx, &pac_info,
					tmp_blob,
					smb_krb5_context,
					&server_keyblock);
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &server_keyblock);
	if (ret) {
		DEBUG(1, ("PAC decoding failed: %s\n", 
			  nt_errstr(nt_status)));

		talloc_free(mem_ctx);
		return False;
	}

	talloc_free(mem_ctx);
	return True;
}


/* This is the PAC generated on my test network, by my test Win2k3 server.
   -- abartlet 2005-07-04
 */

static const char saved_pac[] = {
	0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xd8, 0x01, 0x00, 0x00, 
	0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
	0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
	0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x08, 0x00, 0xcc, 0xcc, 0xcc, 0xcc,
	0xc8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x30, 0xdf, 0xa6, 0xcb, 
	0x4f, 0x7d, 0xc5, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x7f, 0xc0, 0x3c, 0x4e, 0x59, 0x62, 0x73, 0xc5, 0x01, 0xc0, 0x3c, 0x4e, 0x59,
	0x62, 0x73, 0xc5, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x16, 0x00, 0x16, 0x00,
	0x04, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x14, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x02, 0x00, 0x65, 0x00, 0x00, 0x00, 
	0xed, 0x03, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x02, 0x00,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x16, 0x00, 0x20, 0x00, 0x02, 0x00, 0x16, 0x00, 0x18, 0x00,
	0x24, 0x00, 0x02, 0x00, 0x28, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
	0x57, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x33, 0x00, 0x46, 0x00, 0x49, 0x00, 0x4e, 0x00,
	0x41, 0x00, 0x4c, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
	0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x57, 0x00, 0x32, 0x00,
	0x30, 0x00, 0x30, 0x00, 0x33, 0x00, 0x46, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x4c, 0x00,
	0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x57, 0x00, 0x49, 0x00,
	0x4e, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x33, 0x00, 0x54, 0x00, 0x48, 0x00, 0x49, 0x00, 0x4e, 0x00,
	0x4b, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
	0x15, 0x00, 0x00, 0x00, 0x11, 0x2f, 0xaf, 0xb5, 0x90, 0x04, 0x1b, 0xec, 0x50, 0x3b, 0xec, 0xdc,
	0x01, 0x00, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x66, 0x28, 0xea, 0x37, 0x80, 0xc5, 0x01, 0x16, 0x00, 0x77, 0x00, 0x32, 0x00, 0x30, 0x00,
	0x30, 0x00, 0x33, 0x00, 0x66, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x24, 0x00,
	0x76, 0xff, 0xff, 0xff, 0x37, 0xd5, 0xb0, 0xf7, 0x24, 0xf0, 0xd6, 0xd4, 0xec, 0x09, 0x86, 0x5a,
	0xa0, 0xe8, 0xc3, 0xa9, 0x00, 0x00, 0x00, 0x00, 0x76, 0xff, 0xff, 0xff, 0xb4, 0xd8, 0xb8, 0xfe,
	0x83, 0xb3, 0x13, 0x3f, 0xfc, 0x5c, 0x41, 0xad, 0xe2, 0x64, 0x83, 0xe0, 0x00, 0x00, 0x00, 0x00
};

/* Check with a known 'well formed' PAC, from my test server */
static BOOL torture_pac_saved_check(void) 
{
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "PAC saved check");
	DATA_BLOB tmp_blob;
	struct PAC_LOGON_INFO *pac_info;
	krb5_keyblock server_keyblock;
	uint8_t server_bytes[16];
	
	krb5_error_code ret;

	struct smb_krb5_context *smb_krb5_context;

	ret = smb_krb5_init_context(mem_ctx, &smb_krb5_context);

	if (ret) {
		talloc_free(mem_ctx);
		return False;
	}

	/* The machine trust account in use when the above PAC 
	   was generated.  It used arcfour-hmac-md5, so this is easy */
	E_md4hash("iqvwmii8CuEkyY", server_bytes);

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 server_bytes, sizeof(server_bytes),
				 &server_keyblock);
	if (ret) {
		DEBUG(1, ("Server Keyblock encoding failed: %s\n", 
			  smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						     ret, mem_ctx)));

		talloc_free(mem_ctx);
		return False;
	}

	tmp_blob = data_blob_const(saved_pac, sizeof(saved_pac));

	/* Decode and verify the signaure on the PAC */
	nt_status = kerberos_decode_pac(mem_ctx, &pac_info,
					tmp_blob,
					smb_krb5_context,
					&server_keyblock);
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &server_keyblock);
	if (ret) {
		DEBUG(1, ("PAC decoding failed: %s\n", 
			  nt_errstr(nt_status)));

		talloc_free(mem_ctx);
		return False;
	}
	talloc_free(mem_ctx);
	return True;
}

BOOL torture_pac(void) 
{
	BOOL ret = True;
	ret &= torture_pac_self_check();
	ret &= torture_pac_saved_check();
	return ret;
}

#else 

BOOL torture_pac(void) 
{
	printf("Cannot do PAC test without Krb5\n");
	return False;
}

#endif

