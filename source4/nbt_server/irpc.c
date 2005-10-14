/* 
   Unix SMB/CIFS implementation.

   irpc services for the NBT server

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Volker Lendecke	2005
   
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
#include "smbd/service_task.h"
#include "nbt_server/nbt_server.h"


/*
  serve out the nbt statistics
*/
static NTSTATUS nbtd_information(struct irpc_message *msg, 
				 struct nbtd_information *r)
{
	struct nbtd_server *server = talloc_get_type(msg->private, struct nbtd_server);

	switch (r->in.level) {
	case NBTD_INFO_STATISTICS:
		r->out.info.stats = &server->stats;
		break;
	}

	return NT_STATUS_OK;
}


/*
  winbind needs to be able to do a getdc request, but some windows
  servers always send the reply to port 138, regardless of the request
  port. To cope with this we use a irpc request to the NBT server
  which has port 138 open, and thus can receive the replies
*/
struct getdc_state {
	struct irpc_message *msg;
	struct nbtd_getdcname *req;
};

static void getdc_recv_ntlogon_reply(struct dgram_mailslot_handler *dgmslot, 
				     struct nbt_dgram_packet *packet, 
				     const char *src_address, int src_port)
{
	struct getdc_state *s =
		talloc_get_type(dgmslot->private, struct getdc_state);

	struct nbt_ntlogon_packet ntlogon;
	NTSTATUS status;

	status = dgram_mailslot_ntlogon_parse(dgmslot, packet, packet,
					      &ntlogon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("dgram_mailslot_ntlogon_parse failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}

	status = NT_STATUS_NO_LOGON_SERVERS;

	DEBUG(10, ("reply: command=%d\n", ntlogon.command));

	switch (ntlogon.command) {
	case NTLOGON_SAM_LOGON:
		DEBUG(0, ("Huh -- got NTLOGON_SAM_LOGON as reply\n"));
		break;
	case NTLOGON_SAM_LOGON_REPLY: {
		const char *p = ntlogon.req.reply.server;

		DEBUG(10, ("NTLOGON_SAM_LOGON_REPLY: server: %s, user: %s, "
			   "domain: %s\n", p, ntlogon.req.reply.user_name,
			   ntlogon.req.reply.domain));

		if (*p == '\\') p += 1;
		if (*p == '\\') p += 1;

		s->req->out.dcname = talloc_strdup(s->req, p);
		if (s->req->out.dcname == NULL) {
			DEBUG(0, ("talloc failed\n"));
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
		status = NT_STATUS_OK;
		break;
	}
	default:
		DEBUG(0, ("Got unknown packet: %d\n", ntlogon.command));
		break;
	}

 done:
	irpc_send_reply(s->msg, status);
}

static NTSTATUS nbtd_getdcname(struct irpc_message *msg, 
			       struct nbtd_getdcname *req)
{
	struct nbtd_server *server =
		talloc_get_type(msg->private, struct nbtd_server);

	struct getdc_state *s;
	struct nbt_ntlogon_packet p;
	struct nbt_ntlogon_sam_logon *r;
	struct nbt_dgram_socket *sock;
	struct nbt_name src, dst;
	struct nbt_peer_socket dest;
	struct dgram_mailslot_handler *handler;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;

	DEBUG(0, ("nbtd_getdcname called\n"));

	sock = server->interfaces[0].dgmsock;

	s = talloc(msg, struct getdc_state);
        NT_STATUS_HAVE_NO_MEMORY(s);

	s->msg = msg;
	s->req = req;
	
	handler = dgram_mailslot_temp(sock, NBT_MAILSLOT_GETDC,
				      getdc_recv_ntlogon_reply, s);
        NT_STATUS_HAVE_NO_MEMORY(handler);
	
	ZERO_STRUCT(p);
	p.command = NTLOGON_SAM_LOGON;
	r = &p.req.logon;
	r->request_count = 0;
	r->computer_name = req->in.my_computername;
	r->user_name = req->in.my_accountname;
	r->mailslot_name = handler->mailslot_name;
	r->acct_control = req->in.account_control;
	r->sid = *req->in.domain_sid;
	r->nt_version = 1;
	r->lmnt_token = 0xffff;
	r->lm20_token = 0xffff;

	make_nbt_name_client(&src, req->in.my_computername);
	make_nbt_name(&dst, req->in.domainname, 0x1c);

	dest.addr = req->in.ip_address;
	dest.port = 138;
	status = dgram_mailslot_ntlogon_send(sock, DGRAM_DIRECT_GROUP,
					     &dst, &dest,
					     &src, &p);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dgram_mailslot_ntlogon_send failed: %s\n",
			  nt_errstr(status)));
		return status;
	}

	msg->defer_reply = True;
	return NT_STATUS_OK;
}


/*
  register the irpc handlers for the nbt server
*/
void nbtd_register_irpc(struct nbtd_server *nbtsrv)
{
	NTSTATUS status;
	struct task_server *task = nbtsrv->task;

	status = IRPC_REGISTER(task->msg_ctx, irpc, NBTD_INFORMATION, 
			       nbtd_information, nbtsrv);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "nbtd failed to setup monitoring");
		return;
	}

	status = IRPC_REGISTER(task->msg_ctx, irpc, NBTD_GETDCNAME,
			       nbtd_getdcname, nbtsrv);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "nbtd failed to setup getdcname "
				      "handler");
		return;
	}
}
