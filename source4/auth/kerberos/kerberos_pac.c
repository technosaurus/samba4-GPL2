/* 
   Unix SMB/CIFS implementation.

   Kerberos backend for GENSEC
   
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   Copyright (C) Stefan Metzmacher 2004-2005

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
#include "system/time.h"
#include "system/network.h"
#include "auth/kerberos/kerberos.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "auth/auth.h"

#ifdef KRB5_DO_VERIFY_PAC
static NTSTATUS kerberos_pac_checksum(DATA_BLOB pac_data,
					 struct PAC_SIGNATURE_DATA *sig,
					 struct smb_krb5_context *smb_krb5_context,
					 uint32 keyusage)
{
	krb5_error_code ret;
	krb5_crypto crypto;
	Checksum cksum;
	int i;

	cksum.cksumtype		= (CKSUMTYPE)sig->type;
	cksum.checksum.length	= sizeof(sig->signature);
	cksum.checksum.data	= sig->signature;


	ret = krb5_crypto_init(smb_krb5_context->krb5_context,
				&gensec_krb5_state->keyblock,
				0,
				&crypto);
	if (ret) {
		DEBUG(0,("krb5_crypto_init() failed\n"));
		return NT_STATUS_FOOBAR;
	}
	for (i=0; i < 40; i++) {
		keyusage = i;
		ret = krb5_verify_checksum(smb_krb5_context->krb5_context,
					   crypto,
					   keyusage,
					   pac_data.data,
					   pac_data.length,
					   &cksum);
		if (!ret) {
			DEBUG(0,("PAC Verified: keyusage: %d\n", keyusage));
			break;
		}
	}
	krb5_crypto_destroy(smb_krb5_context->krb5_context, crypto);

	if (ret) {
		DEBUG(0,("NOT verifying PAC checksums yet!\n"));
		//return NT_STATUS_LOGON_FAILURE;
	} else {
		DEBUG(0,("PAC checksums verified!\n"));
	}

	return NT_STATUS_OK;
}
#endif

NTSTATUS kerberos_decode_pac(TALLOC_CTX *mem_ctx,
			     struct PAC_LOGON_INFO **logon_info_out,
			     DATA_BLOB blob,
			     struct smb_krb5_context *smb_krb5_context)
{
	NTSTATUS status;
	struct PAC_SIGNATURE_DATA srv_sig;
	struct PAC_SIGNATURE_DATA *srv_sig_ptr;
	struct PAC_SIGNATURE_DATA kdc_sig;
	struct PAC_SIGNATURE_DATA *kdc_sig_ptr;
	struct PAC_LOGON_INFO *logon_info = NULL;
	struct PAC_DATA pac_data;
#ifdef KRB5_DO_VERIFY_PAC
	DATA_BLOB tmp_blob = data_blob(NULL, 0);
#endif
	int i;

	status = ndr_pull_struct_blob(&blob, mem_ctx, &pac_data,
					(ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("can't parse the PAC\n"));
		return status;
	}
	NDR_PRINT_DEBUG(PAC_DATA, &pac_data);

	if (pac_data.num_buffers < 3) {
		/* we need logon_ingo, service_key and kdc_key */
		DEBUG(0,("less than 3 PAC buffers\n"));
		return NT_STATUS_FOOBAR;
	}

	for (i=0; i < pac_data.num_buffers; i++) {
		switch (pac_data.buffers[i].type) {
			case PAC_TYPE_LOGON_INFO:
				if (!pac_data.buffers[i].info) {
					break;
				}
				logon_info = &pac_data.buffers[i].info->logon_info;
				break;
			case PAC_TYPE_SRV_CHECKSUM:
				if (!pac_data.buffers[i].info) {
					break;
				}
				srv_sig_ptr = &pac_data.buffers[i].info->srv_cksum;
				srv_sig = pac_data.buffers[i].info->srv_cksum;
				break;
			case PAC_TYPE_KDC_CHECKSUM:
				if (!pac_data.buffers[i].info) {
					break;
				}
				kdc_sig_ptr = &pac_data.buffers[i].info->kdc_cksum;
				kdc_sig = pac_data.buffers[i].info->kdc_cksum;
				break;
			case PAC_TYPE_UNKNOWN_10:
				break;
			default:
				break;
		}
	}

	if (!logon_info) {
		DEBUG(0,("PAC no logon_info\n"));
		return NT_STATUS_FOOBAR;
	}

	if (!srv_sig_ptr) {
		DEBUG(0,("PAC no srv_key\n"));
		return NT_STATUS_FOOBAR;
	}

	if (!kdc_sig_ptr) {
		DEBUG(0,("PAC no kdc_key\n"));
		return NT_STATUS_FOOBAR;
	}
#ifdef KRB5_DO_VERIFY_PAC
	/* clear the kdc_key */
/*	memset((void *)kdc_sig_ptr , '\0', sizeof(*kdc_sig_ptr));*/

	status = ndr_push_struct_blob(&tmp_blob, mem_ctx, &pac_data,
					      (ndr_push_flags_fn_t)ndr_push_PAC_DATA);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	status = ndr_pull_struct_blob(&tmp_blob, mem_ctx, &pac_data,
					(ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("can't parse the PAC\n"));
		return status;
	}
	/*NDR_PRINT_DEBUG(PAC_DATA, &pac_data);*/

	/* verify by kdc_key */
	status = kerberos_pac_checksum(tmp_blob, &kdc_sig, smb_krb5_context, 0);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* clear the service_key */
/*	memset((void *)srv_sig_ptr , '\0', sizeof(*srv_sig_ptr));*/

	status = ndr_push_struct_blob(&tmp_blob, mem_ctx, &pac_data,
					      (ndr_push_flags_fn_t)ndr_push_PAC_DATA);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	status = ndr_pull_struct_blob(&tmp_blob, mem_ctx, &pac_data,
					(ndr_pull_flags_fn_t)ndr_pull_PAC_DATA);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("can't parse the PAC\n"));
		return status;
	}
	NDR_PRINT_DEBUG(PAC_DATA, &pac_data);

	/* verify by servie_key */
	status = kerberos_pac_checksum(tmp_blob, &srv_sig, smb_krb5_context, 0);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
#endif
	DEBUG(0,("account_name: %s [%s]\n",
		 logon_info->info3.base.account_name.string, 
		 logon_info->info3.base.full_name.string));
	*logon_info_out = logon_info;

	return status;
}

