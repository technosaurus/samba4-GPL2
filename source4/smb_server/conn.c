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
#include "system/filesys.h"
#include "dlinklist.h"
#include "smb_server/smb_server.h"
#include "smbd/service_stream.h"


/****************************************************************************
init the tcon structures
****************************************************************************/
NTSTATUS smbsrv_init_tcons(struct smbsrv_connection *smb_conn, uint32_t limit)
{
	/* 
	 * the idr_* functions take 'int' as limit,
	 * and only work with a max limit 0x00FFFFFF
	 */
	limit &= 0x00FFFFFF;

	smb_conn->tcons.idtree_tid	= idr_init(smb_conn);
	NT_STATUS_HAVE_NO_MEMORY(smb_conn->tcons.idtree_tid);
	smb_conn->tcons.idtree_limit	= limit;
	smb_conn->tcons.list		= NULL;

	return NT_STATUS_OK;
}

/****************************************************************************
find a tcon given a cnum
****************************************************************************/
struct smbsrv_tcon *smbsrv_tcon_find(struct smbsrv_connection *smb_conn, uint32_t tid)
{
	void *p;
	struct smbsrv_tcon *tcon;

	if (tid == 0) return NULL;

	if (tid > smb_conn->tcons.idtree_limit) return NULL;

	p = idr_find(smb_conn->tcons.idtree_tid, tid);
	if (!p) return NULL;

	tcon = talloc_get_type(p, struct smbsrv_tcon);

	return tcon;
}

/*
  destroy a connection structure
*/
static int smbsrv_tcon_destructor(void *ptr)
{
	struct smbsrv_tcon *tcon = ptr;

	DEBUG(3,("%s closed connection to service %s\n",
		 socket_get_peer_addr(tcon->smb_conn->connection->socket, tcon),
		 lp_servicename(tcon->service)));

	/* tell the ntvfs backend that we are disconnecting */
	if (tcon->ntvfs_ctx) {
		ntvfs_disconnect(tcon);
	}

	idr_remove(tcon->smb_conn->tcons.idtree_tid, tcon->tid);
	DLIST_REMOVE(tcon->smb_conn->tcons.list, tcon);
	return 0;
}

/*
  find first available connection slot
*/
struct smbsrv_tcon *smbsrv_tcon_new(struct smbsrv_connection *smb_conn)
{
	struct smbsrv_tcon *tcon;
	int i;

	tcon = talloc_zero(smb_conn, struct smbsrv_tcon);
	if (!tcon) return NULL;
	tcon->smb_conn = smb_conn;

	i = idr_get_new_random(smb_conn->tcons.idtree_tid, tcon, smb_conn->tcons.idtree_limit);
	if (i == -1) {
		DEBUG(1,("ERROR! Out of connection structures\n"));	       
		return NULL;
	}
	tcon->tid = i;

	DLIST_ADD(smb_conn->tcons.list, tcon);
	talloc_set_destructor(tcon, smbsrv_tcon_destructor);

	/* now fill in some statistics */
	tcon->statistics.connect_time = timeval_current();

	return tcon;
}
