/* 
   Unix SMB/CIFS implementation.

   SMB client tree context management functions

   Copyright (C) Andrew Tridgell 1994-2005
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
#include "libcli/raw/libcliraw.h"
#include "libcli/composite/composite.h"

#define SETUP_REQUEST_TREE(cmd, wct, buflen) do { \
	req = smbcli_request_setup(tree, cmd, wct, buflen); \
	if (!req) return NULL; \
} while (0)

/****************************************************************************
 Initialize the tree context
****************************************************************************/
struct smbcli_tree *smbcli_tree_init(struct smbcli_session *session,
				     TALLOC_CTX *parent_ctx, BOOL primary)
{
	struct smbcli_tree *tree;

	tree = talloc_zero(parent_ctx, struct smbcli_tree);
	if (!tree) {
		return NULL;
	}

	if (primary) {
		tree->session = talloc_steal(tree, session);
	} else {
		tree->session = talloc_reference(tree, session);
	}


	return tree;
}

/****************************************************************************
 Send a tconX (async send)
****************************************************************************/
struct smbcli_request *smb_tree_connect_send(struct smbcli_tree *tree, 
					     union smb_tcon *parms)
{
	struct smbcli_request *req = NULL;

	switch (parms->tcon.level) {
	case RAW_TCON_TCON:
		SETUP_REQUEST_TREE(SMBtcon, 0, 0);
		smbcli_req_append_ascii4(req, parms->tcon.in.service, STR_ASCII);
		smbcli_req_append_ascii4(req, parms->tcon.in.password,STR_ASCII);
		smbcli_req_append_ascii4(req, parms->tcon.in.dev,     STR_ASCII);
		break;

	case RAW_TCON_TCONX:
		SETUP_REQUEST_TREE(SMBtconX, 4, 0);
		SSVAL(req->out.vwv, VWV(0), 0xFF);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->tconx.in.flags);
		SSVAL(req->out.vwv, VWV(3), parms->tconx.in.password.length);
		smbcli_req_append_blob(req, &parms->tconx.in.password);
		smbcli_req_append_string(req, parms->tconx.in.path,   STR_TERMINATE | STR_UPPER);
		smbcli_req_append_string(req, parms->tconx.in.device, STR_TERMINATE | STR_ASCII);
		break;
	}

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a tconX (async recv)
****************************************************************************/
NTSTATUS smb_tree_connect_recv(struct smbcli_request *req, TALLOC_CTX *mem_ctx, 
			       union smb_tcon *parms)
{
	uint8_t *p;

	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		goto failed;
	}

	switch (parms->tcon.level) {
	case RAW_TCON_TCON:
		SMBCLI_CHECK_WCT(req, 2);
		parms->tcon.out.max_xmit = SVAL(req->in.vwv, VWV(0));
		parms->tcon.out.tid = SVAL(req->in.vwv, VWV(1));
		break;

	case RAW_TCON_TCONX:
		ZERO_STRUCT(parms->tconx.out);
		parms->tconx.out.tid = SVAL(req->in.hdr, HDR_TID);
		if (req->in.wct >= 4) {
			parms->tconx.out.options = SVAL(req->in.vwv, VWV(3));
		}

		/* output is actual service name */
		p = req->in.data;
		if (!p) break;

		p += smbcli_req_pull_string(req, mem_ctx, &parms->tconx.out.dev_type, 
					 p, -1, STR_ASCII | STR_TERMINATE);
		p += smbcli_req_pull_string(req, mem_ctx, &parms->tconx.out.fs_type, 
					 p, -1, STR_TERMINATE);
		break;
	}

failed:
	return smbcli_request_destroy(req);
}

/****************************************************************************
 Send a tconX (sync interface)
****************************************************************************/
NTSTATUS smb_tree_connect(struct smbcli_tree *tree, TALLOC_CTX *mem_ctx, 
			  union smb_tcon *parms)
{
	struct smbcli_request *req = smb_tree_connect_send(tree, parms);
	return smb_tree_connect_recv(req, mem_ctx, parms);
}


/****************************************************************************
 Send a tree disconnect.
****************************************************************************/
NTSTATUS smb_tree_disconnect(struct smbcli_tree *tree)
{
	struct smbcli_request *req;

	if (!tree) return NT_STATUS_OK;
	req = smbcli_request_setup(tree, SMBtdis, 0, 0);

	if (smbcli_request_send(req)) {
		smbcli_request_receive(req);
	}
	return smbcli_request_destroy(req);
}


/*
  a convenient function to establish a smbcli_tree from scratch
*/
NTSTATUS smbcli_tree_full_connection(TALLOC_CTX *parent_ctx,
				     struct smbcli_tree **ret_tree, 
				     const char *my_name, 
				     const char *dest_host, int port,
				     const char *service, const char *service_type,
				     const char *user, const char *domain, 
				     const char *password)
{
	struct smb_composite_connect io;
	NTSTATUS status;

	io.in.dest_host = dest_host;
	io.in.port = port;
	io.in.called_name = dest_host;
	io.in.calling_name = my_name;
	io.in.service = service;
	io.in.service_type = service_type;
	io.in.domain = domain;
	io.in.user = user;
	if (user && user[0]) {
		io.in.password = password;
	} else {
		io.in.password = NULL;
	}
	
	status = smb_composite_connect(&io, parent_ctx);
	if (NT_STATUS_IS_OK(status)) {
		*ret_tree = io.out.tree;
	}

	return status;
}
