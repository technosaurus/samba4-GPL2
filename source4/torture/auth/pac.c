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
#include "librpc/gen_ndr/ndr_samr.h"

static BOOL torture_pac_self_check(void) 
{
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx = talloc_named(NULL, 0, "PAC self check");
	DATA_BLOB tmp_blob;
	struct PAC_DATA *pac_data;
	struct PAC_LOGON_INFO *logon_info;
	union netr_Validation validation;

	/* Generate a nice, arbitary keyblock */
	uint8_t server_bytes[16];
	uint8_t krbtgt_bytes[16];
	krb5_keyblock server_keyblock;
	krb5_keyblock krbtgt_keyblock;
	
	krb5_error_code ret;

	struct smb_krb5_context *smb_krb5_context;

	struct auth_serversupplied_info *server_info;
	struct auth_serversupplied_info *server_info_out;

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
		printf("Server Keyblock encoding failed: %s\n", 
		       smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						  ret, mem_ctx));
		
		talloc_free(mem_ctx);
		return False;
	}

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 krbtgt_bytes, sizeof(krbtgt_bytes),
				 &krbtgt_keyblock);
	if (ret) {
		printf("KRBTGT Keyblock encoding failed: %s\n", 
		       smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						  ret, mem_ctx));
	
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
	ret = kerberos_create_pac(mem_ctx, server_info, 
				  smb_krb5_context->krb5_context,  
				  &krbtgt_keyblock,
				  &server_keyblock,
				  time(NULL),
				  &tmp_blob);
	
	if (ret) {
		printf("PAC encoding failed: %s\n", 
		       smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						  ret, mem_ctx));

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		talloc_free(mem_ctx);
		return False;
	}

	dump_data(10,tmp_blob.data,tmp_blob.length);

	/* Now check that we can read it back */
	nt_status = kerberos_decode_pac(mem_ctx, &pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					&krbtgt_keyblock,
					&server_keyblock);

	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		DEBUG(1, ("PAC decoding failed: %s\n", 
			  nt_errstr(nt_status)));

		talloc_free(mem_ctx);
		return False;
	}

	/* Now check that we can read it back */
	nt_status = kerberos_pac_logon_info(mem_ctx, &logon_info,
					    tmp_blob,
					    smb_krb5_context->krb5_context,
					    &krbtgt_keyblock,
					    &server_keyblock);
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		printf("PAC decoding (for logon info) failed: %s\n", 
		       nt_errstr(nt_status));
		
		talloc_free(mem_ctx);
		return False;
	}
	
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &krbtgt_keyblock);
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &server_keyblock);

	validation.sam3 = &logon_info->info3;
	nt_status = make_server_info_netlogon_validation(mem_ctx,
							 "",
							 3, &validation,
							 &server_info_out); 
	if (!NT_STATUS_IS_OK(nt_status)) {
		printf("PAC decoding (make server info) failed: %s\n", 
		       nt_errstr(nt_status));
		
		talloc_free(mem_ctx);
		return False;
	}
	
	if (!dom_sid_equal(server_info->account_sid, 
			   server_info_out->account_sid)) {
		printf("PAC Decode resulted in *different* domain SID: %s != %s\n",
		       dom_sid_string(mem_ctx, server_info->account_sid), 
		       dom_sid_string(mem_ctx, server_info_out->account_sid));
		talloc_free(mem_ctx);
		return False;
	}
	
	talloc_free(mem_ctx);
	return True;
}


/* This is the PAC generated on my test network, by my test Win2k3 server.
   -- abartlet 2005-07-04
 */

static const uint8_t saved_pac[] = {
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
	DATA_BLOB tmp_blob, validate_blob;
	struct PAC_DATA *pac_data;
	struct PAC_LOGON_INFO *logon_info;
	union netr_Validation validation;
	const char *pac_file, *pac_kdc_key, *pac_member_key;

	struct auth_serversupplied_info *server_info_out;

	krb5_keyblock server_keyblock;
	krb5_keyblock krbtgt_keyblock;
	struct samr_Password *krbtgt_bytes, *krbsrv_bytes;
	
	krb5_error_code ret;

	struct smb_krb5_context *smb_krb5_context;

	ret = smb_krb5_init_context(mem_ctx, &smb_krb5_context);

	if (ret) {
		talloc_free(mem_ctx);
		return False;
	}

	pac_kdc_key = lp_parm_string(-1,"torture","pac_kdc_key");
	if (pac_kdc_key == NULL) {
		pac_kdc_key = "B286757148AF7FD252C53603A150B7E7";
	}

	pac_member_key = lp_parm_string(-1,"torture","pac_member_key");
	if (pac_member_key == NULL) {
		pac_member_key = "D217FAEAE5E6B5F95CCC94077AB8A5FC";
	}

	printf("Using pac_kdc_key '%s'\n", pac_kdc_key);
	printf("Using pac_member_key '%s'\n", pac_member_key);

	/* The krbtgt key in use when the above PAC was generated.
	 * This is an arcfour-hmac-md5 key, extracted with our 'net
	 * samdump' tool. */
	krbtgt_bytes = smbpasswd_gethexpwd(mem_ctx, pac_kdc_key);
	if (!krbtgt_bytes) {
		DEBUG(0, ("Could not interpret krbtgt key"));
		talloc_free(mem_ctx);
		return False;
	}

	krbsrv_bytes = smbpasswd_gethexpwd(mem_ctx, pac_member_key);
	if (!krbsrv_bytes) {
		DEBUG(0, ("Could not interpret krbsrv key"));
		talloc_free(mem_ctx);
		return False;
	}

	ret = krb5_keyblock_init(smb_krb5_context->krb5_context,
				 ENCTYPE_ARCFOUR_HMAC,
				 krbsrv_bytes->hash, sizeof(krbsrv_bytes->hash),
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
				 krbtgt_bytes->hash, sizeof(krbtgt_bytes->hash),
				 &krbtgt_keyblock);
	if (ret) {
		DEBUG(1, ("Server Keyblock encoding failed: %s\n", 
			  smb_get_krb5_error_message(smb_krb5_context->krb5_context, 
						     ret, mem_ctx)));

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		talloc_free(mem_ctx);
		return False;
	}

	pac_file = lp_parm_string(-1,"torture","pac_file");
	if (pac_file) {
		tmp_blob.data = file_load(pac_file, &tmp_blob.length, mem_ctx);
		printf("Loaded pac of size %d from %s\n", tmp_blob.length, pac_file);
	} else {
		tmp_blob = data_blob(saved_pac, sizeof(saved_pac));
		file_save("x.dat", tmp_blob.data, tmp_blob.length);
	}
	
	dump_data(10,tmp_blob.data,tmp_blob.length);

	/* Decode and verify the signaure on the PAC */
	nt_status = kerberos_decode_pac(mem_ctx, &pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					&krbtgt_keyblock,
					&server_keyblock);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1, ("PAC decoding failed: %s\n", 
			  nt_errstr(nt_status)));

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		talloc_free(mem_ctx);
		return False;
	}

	/* Parse the PAC again, for the logon info this time */
	nt_status = kerberos_pac_logon_info(mem_ctx, &logon_info,
					    tmp_blob,
					    smb_krb5_context->krb5_context,
					    &krbtgt_keyblock,
					    &server_keyblock);

	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		printf("PAC decoding (for logon info) failed: %s\n", 
			  nt_errstr(nt_status));

		talloc_free(mem_ctx);
		return False;
	}

	validation.sam3 = &logon_info->info3;
	nt_status = make_server_info_netlogon_validation(mem_ctx,
							 "",
							 3, &validation,
							 &server_info_out); 
	if (!NT_STATUS_IS_OK(nt_status)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &server_keyblock);

		printf("PAC decoding (make server info) failed: %s\n", 
		       nt_errstr(nt_status));
		
		talloc_free(mem_ctx);
		return False;
	}

	if (!pac_file &&
	    !dom_sid_equal(dom_sid_parse_talloc(mem_ctx, "S-1-5-21-3048156945-3961193616-3706469200-1005"), 
			   server_info_out->account_sid)) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);

		printf("PAC Decode resulted in *different* domain SID: %s != %s\n",
		       "S-1-5-21-3048156945-3961193616-3706469200-1005", 
		       dom_sid_string(mem_ctx, server_info_out->account_sid));
		talloc_free(mem_ctx);
		return False;
	}

	ret = kerberos_encode_pac(mem_ctx, 
				  pac_data,
				  smb_krb5_context->krb5_context,
				  &krbtgt_keyblock,
				  &server_keyblock,
				  &validate_blob);

	if (ret != 0) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);

		DEBUG(0, ("PAC push failed\n"));
		talloc_free(mem_ctx);
		return False;
	}

	dump_data(10,validate_blob.data,validate_blob.length);

	/* compare both the length and the data bytes after a
	 * pull/push cycle.  This ensures we use the exact same
	 * pointer, padding etc algorithms as win2k3.
	 */
	if (tmp_blob.length != validate_blob.length) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);

		DEBUG(0, ("PAC push failed: original buffer length[%u] != created buffer length[%u]\n",
				(unsigned)tmp_blob.length, (unsigned)validate_blob.length));
		talloc_free(mem_ctx);
		return False;
	}

	if (memcmp(tmp_blob.data, validate_blob.data, tmp_blob.length) != 0) {
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);

		DEBUG(0, ("PAC push failed: length[%u] matches, but data does not\n",
			  (unsigned)tmp_blob.length));
		talloc_free(mem_ctx);
		return False;
	}

	/* Finally...  Bugger up the signature, and check we fail the checksum */
	tmp_blob.data[tmp_blob.length - 2]++;

	nt_status = kerberos_decode_pac(mem_ctx, &pac_data,
					tmp_blob,
					smb_krb5_context->krb5_context,
					&krbtgt_keyblock,
					&server_keyblock);
	if (NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1, ("PAC decoding DID NOT fail on broken checksum\n"));

		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &krbtgt_keyblock);
		krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
					    &server_keyblock);
		talloc_free(mem_ctx);
		return False;
	}

	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &krbtgt_keyblock);
	krb5_free_keyblock_contents(smb_krb5_context->krb5_context, 
				    &server_keyblock);

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
