/* 
   Unix SMB/CIFS implementation.
   client trans2 operations
   Copyright (C) James Myers 2003
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

/* local macros to make the code more readable */
#define FINFO_CHECK_MIN_SIZE(size) if (blob->length < (size)) { \
      DEBUG(1,("Unexpected FILEINFO reply size %d for level %u - expected min of %d\n", \
	       blob->length, parms->generic.level, (size))); \
      return NT_STATUS_INFO_LENGTH_MISMATCH; \
}
#define FINFO_CHECK_SIZE(size) if (blob->length != (size)) { \
      DEBUG(1,("Unexpected FILEINFO reply size %d for level %u - expected %d\n", \
	       blob->length, parms->generic.level, (size))); \
      return NT_STATUS_INFO_LENGTH_MISMATCH; \
}

/****************************************************************************
 Handle qfileinfo/qpathinfo trans2 backend.
****************************************************************************/
static NTSTATUS smb_raw_info_backend(struct cli_session *session,
				     TALLOC_CTX *mem_ctx,
				     union smb_fileinfo *parms, 
				     DATA_BLOB *blob)
{	
	uint_t len, ofs;

	switch (parms->generic.level) {
	case RAW_FILEINFO_GENERIC:
	case RAW_FILEINFO_GETATTR:
	case RAW_FILEINFO_GETATTRE:
		/* not handled here */
		return NT_STATUS_INVALID_LEVEL;

	case RAW_FILEINFO_STANDARD:
		FINFO_CHECK_SIZE(22);
		parms->standard.out.create_time = raw_pull_dos_date2(session->transport,
								     blob->data +  0);
		parms->standard.out.access_time = raw_pull_dos_date2(session->transport,
								     blob->data +  4);
		parms->standard.out.write_time =  raw_pull_dos_date2(session->transport,
								     blob->data +  8);
		parms->standard.out.size =        IVAL(blob->data,             12);
		parms->standard.out.alloc_size =  IVAL(blob->data,             16);
		parms->standard.out.attrib =      SVAL(blob->data,             20);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_SIZE:
		FINFO_CHECK_SIZE(26);
		parms->ea_size.out.create_time = raw_pull_dos_date2(session->transport,
								    blob->data +  0);
		parms->ea_size.out.access_time = raw_pull_dos_date2(session->transport,
								    blob->data +  4);
		parms->ea_size.out.write_time =  raw_pull_dos_date2(session->transport,
								    blob->data +  8);
		parms->ea_size.out.size =        IVAL(blob->data,             12);
		parms->ea_size.out.alloc_size =  IVAL(blob->data,             16);
		parms->ea_size.out.attrib =      SVAL(blob->data,             20);
		parms->ea_size.out.ea_size =     IVAL(blob->data,             22);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALL_EAS:
		FINFO_CHECK_MIN_SIZE(4);
		return ea_pull_list(blob, mem_ctx, 
				    &parms->all_eas.out.num_eas,
				    &parms->all_eas.out.eas);

	case RAW_FILEINFO_IS_NAME_VALID:
		/* no data! */
		FINFO_CHECK_SIZE(0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_BASIC_INFO:
	case RAW_FILEINFO_BASIC_INFORMATION:
		/* some servers return 40 bytes and some 36. w2k3 return 40, so thats
		   what we should do, but we need to accept 36 */
		if (blob->length != 36) {
			FINFO_CHECK_SIZE(40);
		}
		parms->basic_info.out.create_time = cli_pull_nttime(blob->data, 0);
		parms->basic_info.out.access_time = cli_pull_nttime(blob->data, 8);
		parms->basic_info.out.write_time =  cli_pull_nttime(blob->data, 16);
		parms->basic_info.out.change_time = cli_pull_nttime(blob->data, 24);
		parms->basic_info.out.attrib = 	               IVAL(blob->data, 32);
		return NT_STATUS_OK;

	case RAW_FILEINFO_STANDARD_INFO:
	case RAW_FILEINFO_STANDARD_INFORMATION:
		FINFO_CHECK_SIZE(24);
		parms->standard_info.out.alloc_size =     BVAL(blob->data, 0);
		parms->standard_info.out.size =           BVAL(blob->data, 8);
		parms->standard_info.out.nlink =          IVAL(blob->data, 16);
		parms->standard_info.out.delete_pending = CVAL(blob->data, 20);
		parms->standard_info.out.directory =      CVAL(blob->data, 21);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_INFO:
	case RAW_FILEINFO_EA_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->ea_info.out.ea_size = IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_NAME_INFO:
	case RAW_FILEINFO_NAME_INFORMATION:
		FINFO_CHECK_MIN_SIZE(4);
		cli_blob_pull_string(session, mem_ctx, blob, 
				     &parms->name_info.out.fname, 0, 4, STR_UNICODE);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALL_INFO:
	case RAW_FILEINFO_ALL_INFORMATION:
		FINFO_CHECK_MIN_SIZE(72);
		parms->all_info.out.create_time =           cli_pull_nttime(blob->data, 0);
		parms->all_info.out.access_time =           cli_pull_nttime(blob->data, 8);
		parms->all_info.out.write_time =            cli_pull_nttime(blob->data, 16);
		parms->all_info.out.change_time =           cli_pull_nttime(blob->data, 24);
		parms->all_info.out.attrib =                IVAL(blob->data, 32);
		parms->all_info.out.alloc_size =            BVAL(blob->data, 40);
		parms->all_info.out.size =                  BVAL(blob->data, 48);
		parms->all_info.out.nlink =                 IVAL(blob->data, 56);
		parms->all_info.out.delete_pending =        CVAL(blob->data, 60);
		parms->all_info.out.directory =             CVAL(blob->data, 61);
		parms->all_info.out.ea_size =               IVAL(blob->data, 64);
		cli_blob_pull_string(session, mem_ctx, blob,
				     &parms->all_info.out.fname, 68, 72, STR_UNICODE);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALT_NAME_INFO:
	case RAW_FILEINFO_ALT_NAME_INFORMATION:
		FINFO_CHECK_MIN_SIZE(4);
		cli_blob_pull_string(session, mem_ctx, blob, 
				     &parms->alt_name_info.out.fname, 0, 4, STR_UNICODE);
		return NT_STATUS_OK;

	case RAW_FILEINFO_STREAM_INFO:
	case RAW_FILEINFO_STREAM_INFORMATION:
		ofs = 0;
		parms->stream_info.out.num_streams = 0;
		parms->stream_info.out.streams = NULL;

		while (blob->length - ofs >= 24) {
			uint_t n = parms->stream_info.out.num_streams;
			parms->stream_info.out.streams = 
				talloc_realloc(mem_ctx,parms->stream_info.out.streams,
					       (n+1) * sizeof(parms->stream_info.out.streams[0]));
			if (!parms->stream_info.out.streams) {
				return NT_STATUS_NO_MEMORY;
			}
			parms->stream_info.out.streams[n].size =       BVAL(blob->data, ofs +  8);
			parms->stream_info.out.streams[n].alloc_size = BVAL(blob->data, ofs + 16);
			cli_blob_pull_string(session, mem_ctx, blob, 
					     &parms->stream_info.out.streams[n].stream_name, 
					     ofs+4, ofs+24, STR_UNICODE);
			parms->stream_info.out.num_streams++;
			len = IVAL(blob->data, ofs);
			if (len > blob->length - ofs) return NT_STATUS_INFO_LENGTH_MISMATCH;
			if (len == 0) break;
			ofs += len;
		}
		return NT_STATUS_OK;

	case RAW_FILEINFO_INTERNAL_INFORMATION:
		FINFO_CHECK_SIZE(8);
		parms->internal_information.out.file_id = BVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ACCESS_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->access_information.out.access_flags = IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_POSITION_INFORMATION:
		FINFO_CHECK_SIZE(8);
		parms->position_information.out.position = BVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_MODE_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->mode_information.out.mode = IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALIGNMENT_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->alignment_information.out.alignment_requirement 
			= IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_COMPRESSION_INFO:
	case RAW_FILEINFO_COMPRESSION_INFORMATION:
		FINFO_CHECK_SIZE(16);
		parms->compression_info.out.compressed_size = BVAL(blob->data,  0);
		parms->compression_info.out.format          = SVAL(blob->data,  8);
		parms->compression_info.out.unit_shift      = CVAL(blob->data, 10);
		parms->compression_info.out.chunk_shift     = CVAL(blob->data, 11);
		parms->compression_info.out.cluster_shift   = CVAL(blob->data, 12);
		/* 3 bytes of padding */
		return NT_STATUS_OK;

	case RAW_FILEINFO_UNIX_BASIC:
		FINFO_CHECK_SIZE(100);
		parms->unix_basic_info.out.end_of_file        =            BVAL(blob->data,  0);
		parms->unix_basic_info.out.num_bytes          =            BVAL(blob->data,  8);
		parms->unix_basic_info.out.status_change_time = cli_pull_nttime(blob->data, 16);
		parms->unix_basic_info.out.access_time        = cli_pull_nttime(blob->data, 24);
		parms->unix_basic_info.out.change_time        = cli_pull_nttime(blob->data, 32);
		parms->unix_basic_info.out.uid                =            BVAL(blob->data, 40);
		parms->unix_basic_info.out.gid                =            BVAL(blob->data, 48);
		parms->unix_basic_info.out.file_type          =            IVAL(blob->data, 52);
		parms->unix_basic_info.out.dev_major          =            BVAL(blob->data, 60);
		parms->unix_basic_info.out.dev_minor          =            BVAL(blob->data, 68);
		parms->unix_basic_info.out.unique_id          =            BVAL(blob->data, 76);
		parms->unix_basic_info.out.permissions        =            BVAL(blob->data, 84);
		parms->unix_basic_info.out.nlink              =            BVAL(blob->data, 92);
		return NT_STATUS_OK;

	case RAW_FILEINFO_UNIX_LINK:
		cli_blob_pull_string(session, mem_ctx, blob, 
				     &parms->unix_link_info.out.link_dest, 0, 4, STR_UNICODE);
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_NETWORK_OPEN_INFORMATION:		
		FINFO_CHECK_SIZE(56);
		parms->network_open_information.out.create_time = cli_pull_nttime(blob->data,  0);
		parms->network_open_information.out.access_time = cli_pull_nttime(blob->data,  8);
		parms->network_open_information.out.write_time =  cli_pull_nttime(blob->data, 16);
		parms->network_open_information.out.change_time = cli_pull_nttime(blob->data, 24);
		parms->network_open_information.out.alloc_size =             BVAL(blob->data, 32);
		parms->network_open_information.out.size = 	             BVAL(blob->data, 40);
		parms->network_open_information.out.attrib = 	             IVAL(blob->data, 48);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION:
		FINFO_CHECK_SIZE(8);
		parms->attribute_tag_information.out.attrib =      IVAL(blob->data, 0);
		parms->attribute_tag_information.out.reparse_tag = IVAL(blob->data, 4);
		return NT_STATUS_OK;
	}

	return NT_STATUS_INVALID_LEVEL;
}

/****************************************************************************
 Very raw query file info - returns param/data blobs - (async send)
****************************************************************************/
static struct cli_request *smb_raw_fileinfo_blob_send(struct cli_tree *tree,
						      uint16_t fnum, uint16_t info_level)
{
	struct smb_trans2 tp;
	uint16_t setup = TRANSACT2_QFILEINFO;
	struct cli_request *req;
	TALLOC_CTX *mem_ctx = talloc_init("raw_fileinfo");
	
	tp.in.max_setup = 0;
	tp.in.flags = 0; 
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.data = data_blob(NULL, 0);
	tp.in.max_param = 2;
	tp.in.max_data = 0xFFFF;
	tp.in.setup = &setup;
	
	tp.in.params = data_blob_talloc(mem_ctx, NULL, 4);
	if (!tp.in.params.data) {
		talloc_destroy(mem_ctx);
		return NULL;
	}

	SIVAL(tp.in.params.data, 0, fnum);
	SSVAL(tp.in.params.data, 2, info_level);

	req = smb_raw_trans2_send(tree, &tp);

	talloc_destroy(mem_ctx);

	return req;
}


/****************************************************************************
 Very raw query file info - returns param/data blobs - (async recv)
****************************************************************************/
static NTSTATUS smb_raw_fileinfo_blob_recv(struct cli_request *req,
					   TALLOC_CTX *mem_ctx,
					   DATA_BLOB *blob)
{
	struct smb_trans2 tp;
	NTSTATUS status = smb_raw_trans2_recv(req, mem_ctx, &tp);
	if (NT_STATUS_IS_OK(status)) {
		*blob = tp.out.data;
	}
	return status;
}

/****************************************************************************
 Very raw query path info - returns param/data blobs (async send)
****************************************************************************/
static struct cli_request *smb_raw_pathinfo_blob_send(struct cli_tree *tree,
						      const char *fname,
						      uint16_t info_level)
{
	struct smb_trans2 tp;
	uint16_t setup = TRANSACT2_QPATHINFO;
	struct cli_request *req;
	TALLOC_CTX *mem_ctx = talloc_init("raw_pathinfo");

	tp.in.max_setup = 0;
	tp.in.flags = 0; 
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.data = data_blob(NULL, 0);
	tp.in.max_param = 2;
	tp.in.max_data = 0xFFFF;
	tp.in.setup = &setup;
	
	tp.in.params = data_blob_talloc(mem_ctx, NULL, 6);
	if (!tp.in.params.data) {
		talloc_destroy(mem_ctx);
		return NULL;
	}

	SSVAL(tp.in.params.data, 0, info_level);
	SIVAL(tp.in.params.data, 2, 0);
	cli_blob_append_string(tree->session, mem_ctx, &tp.in.params,
			       fname, STR_TERMINATE);
	
	req = smb_raw_trans2_send(tree, &tp);

	talloc_destroy(mem_ctx);

	return req;
}

/****************************************************************************
 send a SMBgetatr (async send)
****************************************************************************/
static struct cli_request *smb_raw_getattr_send(struct cli_tree *tree,
						union smb_fileinfo *parms)
{
	struct cli_request *req;
	
	req = cli_request_setup(tree, SMBgetatr, 0, 0);
	if (!req) return NULL;

	cli_req_append_ascii4(req, parms->getattr.in.fname, STR_TERMINATE);
	
	if (!cli_request_send(req)) {
		cli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 send a SMBgetatr (async recv)
****************************************************************************/
static NTSTATUS smb_raw_getattr_recv(struct cli_request *req,
				     union smb_fileinfo *parms)
{
	if (!cli_request_receive(req) ||
	    cli_request_is_error(req)) {
		return cli_request_destroy(req);
	}

	CLI_CHECK_WCT(req, 10);
	parms->getattr.out.attrib =     SVAL(req->in.vwv, VWV(0));
	parms->getattr.out.write_time = raw_pull_dos_date3(req->transport,
							   req->in.vwv + VWV(1));
	parms->getattr.out.size =       IVAL(req->in.vwv, VWV(3));

failed:
	return cli_request_destroy(req);
}


/****************************************************************************
 Handle SMBgetattrE (async send)
****************************************************************************/
static struct cli_request *smb_raw_getattrE_send(struct cli_tree *tree,
						 union smb_fileinfo *parms)
{
	struct cli_request *req;
	
	req = cli_request_setup(tree, SMBgetattrE, 1, 0);
	if (!req) return NULL;
	
	SSVAL(req->out.vwv, VWV(0), parms->getattre.in.fnum);
	if (!cli_request_send(req)) {
		cli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Handle SMBgetattrE (async send)
****************************************************************************/
static NTSTATUS smb_raw_getattrE_recv(struct cli_request *req,
				      union smb_fileinfo *parms)
{
	if (!cli_request_receive(req) ||
	    cli_request_is_error(req)) {
		return cli_request_destroy(req);
	}
	
	CLI_CHECK_WCT(req, 11);
	parms->getattre.out.create_time =   raw_pull_dos_date2(req->transport,
							       req->in.vwv + VWV(0));
	parms->getattre.out.access_time =   raw_pull_dos_date2(req->transport,
							       req->in.vwv + VWV(2));
	parms->getattre.out.write_time  =   raw_pull_dos_date2(req->transport,
							       req->in.vwv + VWV(4));
	parms->getattre.out.size =          IVAL(req->in.vwv,             VWV(6));
	parms->getattre.out.alloc_size =    IVAL(req->in.vwv,             VWV(8));
	parms->getattre.out.attrib =        SVAL(req->in.vwv,             VWV(10));

failed:
	return cli_request_destroy(req);
}


/****************************************************************************
 Query file info (async send)
****************************************************************************/
struct cli_request *smb_raw_fileinfo_send(struct cli_tree *tree,
					  union smb_fileinfo *parms)
{
	/* pass off the non-trans2 level to specialised functions */
	if (parms->generic.level == RAW_FILEINFO_GETATTRE) {
		return smb_raw_getattrE_send(tree, parms);
	}
	if (parms->generic.level >= RAW_FILEINFO_GENERIC) {
		return NULL;
	}

	return smb_raw_fileinfo_blob_send(tree, 
					  parms->generic.in.fnum,
					  parms->generic.level);
}

/****************************************************************************
 Query file info (async recv)
****************************************************************************/
NTSTATUS smb_raw_fileinfo_recv(struct cli_request *req,
			       TALLOC_CTX *mem_ctx,
			       union smb_fileinfo *parms)
{
	DATA_BLOB blob;
	NTSTATUS status;
	struct cli_session *session = req?req->session:NULL;

	if (parms->generic.level == RAW_FILEINFO_GETATTRE) {
		return smb_raw_getattrE_recv(req, parms);
	}
	if (parms->generic.level == RAW_FILEINFO_GETATTR) {
		return smb_raw_getattr_recv(req, parms);
	}

	status = smb_raw_fileinfo_blob_recv(req, mem_ctx, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return smb_raw_info_backend(session, mem_ctx, parms, &blob);
}

/****************************************************************************
 Query file info (sync interface)
****************************************************************************/
NTSTATUS smb_raw_fileinfo(struct cli_tree *tree,
			  TALLOC_CTX *mem_ctx,
			  union smb_fileinfo *parms)
{
	struct cli_request *req = smb_raw_fileinfo_send(tree, parms);
	return smb_raw_fileinfo_recv(req, mem_ctx, parms);
}

/****************************************************************************
 Query path info (async send)
****************************************************************************/
struct cli_request *smb_raw_pathinfo_send(struct cli_tree *tree,
					  union smb_fileinfo *parms)
{
	if (parms->generic.level == RAW_FILEINFO_GETATTR) {
		return smb_raw_getattr_send(tree, parms);
	}
	if (parms->generic.level >= RAW_FILEINFO_GENERIC) {
		return NULL;
	}
	
	return smb_raw_pathinfo_blob_send(tree, parms->generic.in.fname,
					  parms->generic.level);
}

/****************************************************************************
 Query path info (async recv)
****************************************************************************/
NTSTATUS smb_raw_pathinfo_recv(struct cli_request *req,
			       TALLOC_CTX *mem_ctx,
			       union smb_fileinfo *parms)
{
	/* recv is idential to fileinfo */
	return smb_raw_fileinfo_recv(req, mem_ctx, parms);
}

/****************************************************************************
 Query path info (sync interface)
****************************************************************************/
NTSTATUS smb_raw_pathinfo(struct cli_tree *tree,
			  TALLOC_CTX *mem_ctx,
			  union smb_fileinfo *parms)
{
	struct cli_request *req = smb_raw_pathinfo_send(tree, parms);
	return smb_raw_pathinfo_recv(req, mem_ctx, parms);
}
