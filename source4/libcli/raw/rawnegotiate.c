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
	int prot;
	const char *name;
} prots[] = {
	{PROTOCOL_CORE,"PC NETWORK PROGRAM 1.0"},
	{PROTOCOL_COREPLUS,"MICROSOFT NETWORKS 1.03"},
	{PROTOCOL_LANMAN1,"MICROSOFT NETWORKS 3.0"},
	{PROTOCOL_LANMAN1,"LANMAN1.0"},
	{PROTOCOL_LANMAN1,"Windows for Workgroups 3.1a"},
	{PROTOCOL_LANMAN2,"LM1.2X002"},
	{PROTOCOL_LANMAN2,"DOS LANMAN2.1"},
	{PROTOCOL_LANMAN2,"Samba"},
	{PROTOCOL_NT1,"NT LANMAN 1.0"},
	{PROTOCOL_NT1,"NT LM 0.12"},
};

/****************************************************************************
 Send a negprot command.
****************************************************************************/
struct cli_request *smb_negprot_send(struct cli_transport *transport, int maxprotocol)
{
	struct cli_request *req;
	int i;

	req = cli_request_setup_transport(transport, SMBnegprot, 0, 0);
	if (!req) {
		return NULL;
	}

	/* setup the protocol strings */
	for (i=0; i < ARRAY_SIZE(prots) && prots[i].prot <= maxprotocol; i++) {
		cli_req_append_bytes(req, "\2", 1);
		cli_req_append_string(req, prots[i].name, STR_TERMINATE | STR_ASCII);
	}

	if (!cli_request_send(req)) {
		cli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a negprot command.
****************************************************************************/
NTSTATUS smb_raw_negotiate(struct cli_transport *transport) 
{
	struct cli_request *req;
	int protocol;

	req = smb_negprot_send(transport, PROTOCOL_NT1);
	if (!req) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!cli_request_receive(req) ||
	    cli_request_is_error(req)) {
		return cli_request_destroy(req);
	}

	CLI_CHECK_MIN_WCT(req, 1);

	protocol = SVALS(req->in.vwv, VWV(0));

	if (protocol >= ARRAY_SIZE(prots) || protocol < 0) {
		req->status = NT_STATUS_UNSUCCESSFUL;
		return cli_request_destroy(req);
	}

	transport->negotiate.protocol = prots[protocol].prot;

	if (transport->negotiate.protocol >= PROTOCOL_NT1) {
		NTTIME ntt;

		/* NT protocol */
		CLI_CHECK_WCT(req, 17);
		transport->negotiate.sec_mode = CVAL(req->in.vwv,VWV(1));
		transport->negotiate.max_mux  = SVAL(req->in.vwv,VWV(1)+1);
		transport->negotiate.max_xmit = IVAL(req->in.vwv,VWV(3)+1);
		transport->negotiate.sesskey  = IVAL(req->in.vwv,VWV(7)+1);
		transport->negotiate.server_zone = SVALS(req->in.vwv,VWV(15)+1) * 60;

		/* this time arrives in real GMT */
		ntt = cli_pull_nttime(req->in.vwv, VWV(11)+1);
		transport->negotiate.server_time = nt_time_to_unix(&ntt);
		transport->negotiate.capabilities = IVAL(req->in.vwv,VWV(9)+1);

		transport->negotiate.secblob = cli_req_pull_blob(req, transport->mem_ctx, req->in.data, req->in.data_size);
		if (transport->negotiate.capabilities & CAP_RAW_MODE) {
			transport->negotiate.readbraw_supported = True;
			transport->negotiate.writebraw_supported = True;
		}

		/* work out if they sent us a workgroup */
		if ((transport->negotiate.capabilities & CAP_EXTENDED_SECURITY) &&
		    req->in.data_size > 16) {
			cli_req_pull_string(req, transport->mem_ctx, &transport->negotiate.server_domain,
					    req->in.data+16,
					    req->in.data_size-16, STR_UNICODE|STR_NOALIGN);
		}
	} else if (transport->negotiate.protocol >= PROTOCOL_LANMAN1) {
		CLI_CHECK_WCT(req, 13);
		transport->negotiate.sec_mode = SVAL(req->in.vwv,VWV(1));
		transport->negotiate.max_xmit = SVAL(req->in.vwv,VWV(2));
		transport->negotiate.sesskey =  IVAL(req->in.vwv,VWV(6));
		transport->negotiate.server_zone = SVALS(req->in.vwv,VWV(10)) * 60;
		
		/* this time is converted to GMT by make_unix_date */
		transport->negotiate.server_time = make_unix_date(req->in.vwv+VWV(8));
		if ((SVAL(req->in.vwv,VWV(5)) & 0x1)) {
			transport->negotiate.readbraw_supported = 1;
		}
		if ((SVAL(req->in.vwv,VWV(5)) & 0x2)) {
			transport->negotiate.writebraw_supported = 1;
		}
		transport->negotiate.secblob = cli_req_pull_blob(req, transport->mem_ctx, 
								 req->in.data, req->in.data_size);
	} else {
		/* the old core protocol */
		transport->negotiate.sec_mode = 0;
		transport->negotiate.server_time = time(NULL);
		transport->negotiate.max_xmit = ~0;
		transport->negotiate.server_zone = TimeDiff(time(NULL));
	}

	/* a way to force ascii SMB */
	if (getenv("CLI_FORCE_ASCII")) {
		transport->negotiate.capabilities &= ~CAP_UNICODE;
	}

failed:
	return cli_request_destroy(req);
}
