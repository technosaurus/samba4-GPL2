/* 
   Unix SMB/CIFS implementation.
   SMB client negotiate context management functions
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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

static const struct {
	enum protocol_types prot;
	const char *name;
} prots[] = {
	{PROTOCOL_CORE,"PC NETWORK PROGRAM 1.0"},
	{PROTOCOL_COREPLUS,"MICROSOFT NETWORKS 1.03"},
	{PROTOCOL_LANMAN1,"MICROSOFT NETWORKS 3.0"},
	{PROTOCOL_LANMAN1,"LANMAN1.0"},
	{PROTOCOL_LANMAN1,"Windows for Workgroups 3.1a"},
	{PROTOCOL_LANMAN2,"LM1.2X002"},
	{PROTOCOL_LANMAN2,"DOS LANMAN2.1"},
	{PROTOCOL_LANMAN2,"LANMAN2.1"},
	{PROTOCOL_LANMAN2,"Samba"},
	{PROTOCOL_NT1,"NT LANMAN 1.0"},
	{PROTOCOL_NT1,"NT LM 0.12"},
};

/****************************************************************************
 Send a negprot command.
****************************************************************************/
struct smbcli_request *smb_negprot_send(struct smbcli_transport *transport, int maxprotocol)
{
	struct smbcli_request *req;
	int i;
	uint16_t flags2 = 0;

	req = smbcli_request_setup_transport(transport, SMBnegprot, 0, 0);
	if (!req) {
		return NULL;
	}

	flags2 |= FLAGS2_32_BIT_ERROR_CODES;
	if (lp_unicode()) {
		flags2 |= FLAGS2_UNICODE_STRINGS;
	}
	flags2 |= FLAGS2_EXTENDED_ATTRIBUTES;
	flags2 |= FLAGS2_LONG_PATH_COMPONENTS;
	flags2 |= FLAGS2_IS_LONG_NAME;

	if (transport->options.use_spnego) {
		flags2 |= FLAGS2_EXTENDED_SECURITY;
	}

	SSVAL(req->out.hdr,HDR_FLG2, flags2);

	/* setup the protocol strings */
	for (i=0; i < ARRAY_SIZE(prots) && prots[i].prot <= maxprotocol; i++) {
		smbcli_req_append_bytes(req, "\2", 1);
		smbcli_req_append_string(req, prots[i].name, STR_TERMINATE | STR_ASCII);
	}

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a negprot command.
****************************************************************************/
NTSTATUS smb_raw_negotiate(struct smbcli_transport *transport) 
{
	struct smbcli_request *req;
	int protocol;

	req = smb_negprot_send(transport, lp_maxprotocol());
	if (!req) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		return smbcli_request_destroy(req);
	}

	SMBCLI_CHECK_MIN_WCT(req, 1);

	protocol = SVALS(req->in.vwv, VWV(0));

	if (protocol >= ARRAY_SIZE(prots) || protocol < 0) {
		req->status = NT_STATUS_UNSUCCESSFUL;
		return smbcli_request_destroy(req);
	}

	transport->negotiate.protocol = prots[protocol].prot;

	if (transport->negotiate.protocol >= PROTOCOL_NT1) {
		NTTIME ntt;

		/* NT protocol */
		SMBCLI_CHECK_WCT(req, 17);
		transport->negotiate.sec_mode = CVAL(req->in.vwv,VWV(1));
		transport->negotiate.max_mux  = SVAL(req->in.vwv,VWV(1)+1);
		transport->negotiate.max_xmit = IVAL(req->in.vwv,VWV(3)+1);
		transport->negotiate.sesskey  = IVAL(req->in.vwv,VWV(7)+1);
		transport->negotiate.capabilities = IVAL(req->in.vwv,VWV(9)+1);

		/* this time arrives in real GMT */
		ntt = smbcli_pull_nttime(req->in.vwv, VWV(11)+1);
		transport->negotiate.server_time = nt_time_to_unix(ntt);		
		transport->negotiate.server_zone = SVALS(req->in.vwv,VWV(15)+1) * 60;
		transport->negotiate.key_len = CVAL(req->in.vwv,VWV(16)+1);

		if (transport->negotiate.capabilities & CAP_EXTENDED_SECURITY) {
			if (req->in.data_size < 16) {
				goto failed;
			}
			transport->negotiate.server_guid = smbcli_req_pull_blob(req, transport->mem_ctx, req->in.data, 16);
			transport->negotiate.secblob = smbcli_req_pull_blob(req, transport->mem_ctx, req->in.data + 16, req->in.data_size - 16);
		} else {
			if (req->in.data_size < (transport->negotiate.key_len)) {
				goto failed;
			}
			transport->negotiate.secblob = smbcli_req_pull_blob(req, transport->mem_ctx, req->in.data, transport->negotiate.key_len);
			smbcli_req_pull_string(req, transport->mem_ctx, &transport->negotiate.server_domain,
					    req->in.data+transport->negotiate.key_len,
					    req->in.data_size-transport->negotiate.key_len, STR_UNICODE|STR_NOALIGN);
			/* here comes the server name */
		}

		if (transport->negotiate.capabilities & CAP_RAW_MODE) {
			transport->negotiate.readbraw_supported = True;
			transport->negotiate.writebraw_supported = True;
		}
	} else if (transport->negotiate.protocol >= PROTOCOL_LANMAN1) {
		SMBCLI_CHECK_WCT(req, 13);
		transport->negotiate.sec_mode = SVAL(req->in.vwv,VWV(1));
		transport->negotiate.max_xmit = SVAL(req->in.vwv,VWV(2));
		transport->negotiate.sesskey =  IVAL(req->in.vwv,VWV(6));
		transport->negotiate.server_zone = SVALS(req->in.vwv,VWV(10)) * 60;
		
		/* this time is converted to GMT by raw_pull_dos_date */
		transport->negotiate.server_time = raw_pull_dos_date(transport,
								     req->in.vwv+VWV(8));
		if ((SVAL(req->in.vwv,VWV(5)) & 0x1)) {
			transport->negotiate.readbraw_supported = 1;
		}
		if ((SVAL(req->in.vwv,VWV(5)) & 0x2)) {
			transport->negotiate.writebraw_supported = 1;
		}
		transport->negotiate.secblob = smbcli_req_pull_blob(req, transport->mem_ctx, 
								 req->in.data, req->in.data_size);
	} else {
		/* the old core protocol */
		transport->negotiate.sec_mode = 0;
		transport->negotiate.server_time = time(NULL);
		transport->negotiate.max_xmit = ~0;
		transport->negotiate.server_zone = get_time_zone(transport->negotiate.server_time);
	}

	/* a way to force ascii SMB */
	if (!lp_unicode() || getenv("SMBCLI_FORCE_ASCII")) {
		transport->negotiate.capabilities &= ~CAP_UNICODE;
	}

failed:
	return smbcli_request_destroy(req);
}
