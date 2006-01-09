/* 
   Unix SMB/CIFS implementation.
   Manage smbsrv_tcon structures
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Alexander Bokovoy 2002
   
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
#include "dlinklist.h"
#include "smb_server/smb_server.h"
#include "smbd/service_stream.h"
#include "ntvfs/ntvfs.h"


/****************************************************************************
init the tcon structures
****************************************************************************/
static NTSTATUS smbsrv_init_tcons(struct smbsrv_tcons_context *tcons_ctx, TALLOC_CTX *mem_ctx, uint32_t limit)
{
	/* 
	 * the idr_* functions take 'int' as limit,
	 * and only work with a max limit 0x00FFFFFF
	 */
	limit &= 0x00FFFFFF;

	tcons_ctx->idtree_tid	= idr_init(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tcons_ctx->idtree_tid);
	tcons_ctx->idtree_limit	= limit;
	tcons_ctx->list		= NULL;

	return NT_STATUS_OK;
}

NTSTATUS smbsrv_smb_init_tcons(struct smbsrv_connection *smb_conn)
{
	return smbsrv_init_tcons(&smb_conn->smb_tcons, smb_conn, UINT16_MAX);
}

NTSTATUS smbsrv_smb2_init_tcons(struct smbsrv_session *smb_sess)
{
	return smbsrv_init_tcons(&smb_sess->smb2_tcons, smb_sess, UINT32_MAX);
}

/****************************************************************************
find a tcon given a tid for SMB
****************************************************************************/
static struct smbsrv_tcon *smbsrv_tcon_find(struct smbsrv_tcons_context *tcons_ctx, uint32_t tid)
{
	void *p;
	struct smbsrv_tcon *tcon;

	if (tid == 0) return NULL;

	if (tid > tcons_ctx->idtree_limit) return NULL;

	p = idr_find(tcons_ctx->idtree_tid, tid);
	if (!p) return NULL;

	tcon = talloc_get_type(p, struct smbsrv_tcon);

	return tcon;
}

struct smbsrv_tcon *smbsrv_smb_tcon_find(struct smbsrv_connection *smb_conn, uint32_t tid)
{
	return smbsrv_tcon_find(&smb_conn->smb_tcons, tid);
}

struct smbsrv_tcon *smbsrv_smb2_tcon_find(struct smbsrv_session *smb_sess, uint32_t tid)
{
	if (!smb_sess) return NULL;
	return smbsrv_tcon_find(&smb_sess->smb2_tcons, tid);
}

/*
  destroy a connection structure
*/
static int smbsrv_tcon_destructor(void *ptr)
{
	struct smbsrv_tcon *tcon = talloc_get_type(ptr, struct smbsrv_tcon);
	struct smbsrv_tcons_context *tcons_ctx;

	struct socket_address *client_addr;
	client_addr = socket_get_peer_addr(tcon->smb_conn->connection->socket, ptr);
	DEBUG(3,("%s closed connection to service %s\n",
		 client_addr ? client_addr->addr : "(unknown)",
		 lp_servicename(tcon->service)));

	/* tell the ntvfs backend that we are disconnecting */
	if (tcon->ntvfs_ctx) {
		ntvfs_disconnect(tcon);
	}

	if (tcon->smb2.session) {
		tcons_ctx = &tcon->smb2.session->smb2_tcons;
	} else {
		tcons_ctx = &tcon->smb_conn->smb_tcons;
	}

	idr_remove(tcons_ctx->idtree_tid, tcon->tid);
	DLIST_REMOVE(tcons_ctx->list, tcon);
	return 0;
}

/*
  find first available connection slot
*/
static struct smbsrv_tcon *smbsrv_tcon_new(struct smbsrv_connection *smb_conn, struct smbsrv_session *smb_sess)
{
	TALLOC_CTX *mem_ctx;
	struct smbsrv_tcons_context *tcons_ctx;
	struct smbsrv_tcon *tcon;
	int i;

	if (smb_sess) {
		mem_ctx = smb_sess;
		tcons_ctx = &smb_sess->smb2_tcons;
	} else {
		mem_ctx = smb_conn;
		tcons_ctx = &smb_conn->smb_tcons;
	}

	tcon = talloc_zero(mem_ctx, struct smbsrv_tcon);
	if (!tcon) return NULL;
	tcon->smb_conn		= smb_conn;
	tcon->smb2.session	= smb_sess;

	i = idr_get_new_random(tcons_ctx->idtree_tid, tcon, tcons_ctx->idtree_limit);
	if (i == -1) {
		DEBUG(1,("ERROR! Out of connection structures\n"));
		return NULL;
	}
	tcon->tid = i;

	DLIST_ADD(tcons_ctx->list, tcon);
	talloc_set_destructor(tcon, smbsrv_tcon_destructor);

	/* now fill in some statistics */
	tcon->statistics.connect_time = timeval_current();

	return tcon;
}

struct smbsrv_tcon *smbsrv_smb_tcon_new(struct smbsrv_connection *smb_conn)
{
	return smbsrv_tcon_new(smb_conn, NULL);
}

struct smbsrv_tcon *smbsrv_smb2_tcon_new(struct smbsrv_session *smb_sess)
{
	return smbsrv_tcon_new(smb_sess->smb_conn, smb_sess);
}
