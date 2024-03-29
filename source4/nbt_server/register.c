/* 
   Unix SMB/CIFS implementation.

   register our names

   Copyright (C) Andrew Tridgell	2005
   
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
#include "lib/events/events.h"
#include "lib/util/dlinklist.h"
#include "nbt_server/nbt_server.h"
#include "smbd/service_task.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "nbt_server/wins/winsserver.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "dsdb/samdb/samdb.h"


static void nbtd_start_refresh_timer(struct nbtd_iface_name *iname);

/*
  a name refresh request has completed
*/
static void refresh_completion_handler(struct nbt_name_request *req)
{
	struct nbtd_iface_name *iname = talloc_get_type(req->async.private, 
							struct nbtd_iface_name);
	NTSTATUS status;
	struct nbt_name_refresh io;
	TALLOC_CTX *tmp_ctx = talloc_new(iname);

	status = nbt_name_refresh_recv(req, tmp_ctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		DEBUG(4,("Refreshed name %s with %s on interface %s\n", 
			 nbt_name_string(tmp_ctx, &iname->name),
			 iname->iface->ip_address, iname->iface->bcast_address));
		iname->registration_time = timeval_current();
		nbtd_start_refresh_timer(iname);
		talloc_free(tmp_ctx);
		return;
	}

	iname->nb_flags |= NBT_NM_CONFLICT;
	iname->nb_flags &= ~NBT_NM_ACTIVE;

	if (NT_STATUS_IS_OK(status)) {
		DEBUG(1,("Name conflict from %s refreshing name %s with %s on interface %s - %s\n", 
			 io.out.reply_addr, nbt_name_string(tmp_ctx, &iname->name),
			 iname->iface->ip_address, iname->iface->bcast_address,
			 nt_errstr(nbt_rcode_to_ntstatus(io.out.rcode))));
	} else {
		DEBUG(1,("Error refreshing name %s with %s on interface %s - %s\n", 
			 nbt_name_string(tmp_ctx, &iname->name), 
			 iname->iface->ip_address, iname->iface->bcast_address,
			 nt_errstr(status)));
	}

	talloc_free(tmp_ctx);
}


/*
  handle name refresh timer events
*/
static void name_refresh_handler(struct event_context *ev, struct timed_event *te, 
				 struct timeval t, void *private_data)
{
	struct nbtd_iface_name *iname = talloc_get_type(private_data, struct nbtd_iface_name);
	struct nbtd_interface *iface = iname->iface;
	struct nbt_name_register io;
	struct nbt_name_request *req;
	struct nbtd_server *nbtsrv = iface->nbtsrv;

	/* setup a single name register request. Notice that we don't
	   use a name refresh request, as Windows and Samba3 do not
	   defend against broadcast name refresh packets. So for this
	   to be of any use at all, we need to refresh using name
	   registration packets */
	io.in.name            = iname->name;
	io.in.dest_addr       = iface->bcast_address;
	io.in.address         = iface->ip_address;
	io.in.nb_flags        = iname->nb_flags;
	io.in.ttl             = iname->ttl;
	io.in.register_demand = False;
	io.in.broadcast       = True;
	io.in.multi_homed     = False;
	io.in.timeout         = 3;
	io.in.retries         = 0;

	nbtsrv->stats.total_sent++;
	req = nbt_name_register_send(iface->nbtsock, &io);
	if (req == NULL) return;

	req->async.fn = refresh_completion_handler;
	req->async.private = iname;
}


/*
  start a timer to refresh this name
*/
static void nbtd_start_refresh_timer(struct nbtd_iface_name *iname)
{
	uint32_t refresh_time;
	uint32_t max_refresh_time = lp_parm_int(-1, "nbtd", "max_refresh_time", 7200);

	refresh_time = MIN(max_refresh_time, iname->ttl/2);
	
	event_add_timed(iname->iface->nbtsrv->task->event_ctx, 
			iname, 
			timeval_add(&iname->registration_time, refresh_time, 0),
			name_refresh_handler, iname);
}


/*
  a name registration has completed
*/
static void nbtd_register_handler(struct composite_context *creq)
{
	struct nbtd_iface_name *iname = talloc_get_type(creq->async.private_data, 
							struct nbtd_iface_name);
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(iname);

	status = nbt_name_register_bcast_recv(creq);
	if (NT_STATUS_IS_OK(status)) {
		/* good - nobody complained about our registration */
		iname->nb_flags |= NBT_NM_ACTIVE;
		DEBUG(3,("Registered %s with %s on interface %s\n",
			 nbt_name_string(tmp_ctx, &iname->name), 
			 iname->iface->ip_address, iname->iface->bcast_address));
		iname->registration_time = timeval_current();
		talloc_free(tmp_ctx);
		nbtd_start_refresh_timer(iname);
		return;
	}

	/* someone must have replied with an objection! */
	iname->nb_flags |= NBT_NM_CONFLICT;

	DEBUG(1,("Error registering %s with %s on interface %s - %s\n",
		 nbt_name_string(tmp_ctx, &iname->name),
		 iname->iface->ip_address, iname->iface->bcast_address,
		 nt_errstr(status)));
	talloc_free(tmp_ctx);
}


/*
  register a name on a network interface
*/
static void nbtd_register_name_iface(struct nbtd_interface *iface,
				     const char *name, enum nbt_name_type type,
				     uint16_t nb_flags)
{
	struct nbtd_iface_name *iname;
	const char *scope = lp_netbios_scope();
	struct nbt_name_register_bcast io;
	struct composite_context *creq;
	struct nbtd_server *nbtsrv = iface->nbtsrv;

	iname = talloc(iface, struct nbtd_iface_name);
	if (!iname) return;

	iname->iface     = iface;
	iname->name.name = strupper_talloc(iname, name);
	iname->name.type = type;
	if (scope && *scope) {
		iname->name.scope = strupper_talloc(iname, scope);
	} else {
		iname->name.scope = NULL;
	}
	iname->nb_flags          = nb_flags;
	iname->ttl               = lp_parm_int(-1, "nbtd", "bcast_ttl", 300000);
	iname->registration_time = timeval_zero();
	iname->wins_server       = NULL;

	DLIST_ADD_END(iface->names, iname, struct nbtd_iface_name *);

	if (nb_flags & NBT_NM_PERMANENT) {
		/* permanent names are not announced and are immediately active */
		iname->nb_flags |= NBT_NM_ACTIVE;
		iname->ttl       = 0;
		return;
	}

	/* if this is the wins interface, then we need to do a special
	   wins name registration */
	if (iface == iface->nbtsrv->wins_interface) {
		nbtd_winsclient_register(iname);
		return;
	}

	/* setup a broadcast name registration request */
	io.in.name            = iname->name;
	io.in.dest_addr       = iface->bcast_address;
	io.in.address         = iface->ip_address;
	io.in.nb_flags        = nb_flags;
	io.in.ttl             = iname->ttl;

	nbtsrv->stats.total_sent++;
	creq = nbt_name_register_bcast_send(iface->nbtsock, &io);
	if (creq == NULL) return;

	creq->async.fn = nbtd_register_handler;
	creq->async.private_data = iname;
}


/*
  register one name on all our interfaces
*/
static void nbtd_register_name(struct nbtd_server *nbtsrv, 
			       const char *name, enum nbt_name_type type,
			       uint16_t nb_flags)
{
	struct nbtd_interface *iface;
	
	/* register with all the local interfaces */
	for (iface=nbtsrv->interfaces;iface;iface=iface->next) {
		nbtd_register_name_iface(iface, name, type, nb_flags);
	}

	/* register on our general broadcast interface as a permanent name */
	if (nbtsrv->bcast_interface) {
		nbtd_register_name_iface(nbtsrv->bcast_interface, name, type, 
					 nb_flags | NBT_NM_PERMANENT);
	}

	/* register with our WINS servers */
	if (nbtsrv->wins_interface) {
		nbtd_register_name_iface(nbtsrv->wins_interface, name, type, nb_flags);
	}
}


/*
  register our names on all interfaces
*/
void nbtd_register_names(struct nbtd_server *nbtsrv)
{
	uint16_t nb_flags = NBT_NODE_M;
	const char **aliases;

	/* note that we don't initially mark the names "ACTIVE". They are 
	   marked active once registration is successful */
	nbtd_register_name(nbtsrv, lp_netbios_name(), NBT_NAME_CLIENT, nb_flags);
	nbtd_register_name(nbtsrv, lp_netbios_name(), NBT_NAME_USER,   nb_flags);
	nbtd_register_name(nbtsrv, lp_netbios_name(), NBT_NAME_SERVER, nb_flags);

	aliases = lp_netbios_aliases();
	while (aliases && aliases[0]) {
		nbtd_register_name(nbtsrv, aliases[0], NBT_NAME_CLIENT, nb_flags);
		nbtd_register_name(nbtsrv, aliases[0], NBT_NAME_SERVER, nb_flags);
		aliases++;
	}

	if (lp_server_role() == ROLE_DOMAIN_CONTROLLER)	{
		BOOL is_pdc = samdb_is_pdc(nbtsrv->sam_ctx);
		if (is_pdc) {
			nbtd_register_name(nbtsrv, lp_workgroup(),
					   NBT_NAME_PDC, nb_flags);
		}
		nbtd_register_name(nbtsrv, lp_workgroup(),
				   NBT_NAME_LOGON, nb_flags | NBT_NM_GROUP);
	}

	nb_flags |= NBT_NM_GROUP;
	nbtd_register_name(nbtsrv, lp_workgroup(),    NBT_NAME_CLIENT, nb_flags);

	nb_flags |= NBT_NM_PERMANENT;
	nbtd_register_name(nbtsrv, "__SAMBA__",       NBT_NAME_CLIENT, nb_flags);
	nbtd_register_name(nbtsrv, "__SAMBA__",       NBT_NAME_SERVER, nb_flags);
	nbtd_register_name(nbtsrv, "*",               NBT_NAME_CLIENT, nb_flags);
}
