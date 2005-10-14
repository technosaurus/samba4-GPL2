/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "smbd/service_task.h"
#include "smbd/service_stream.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_winsrepl.h"
#include "wrepl_server/wrepl_server.h"

static NTSTATUS wreplsrv_in_start_association(struct wreplsrv_in_call *call)
{
	struct wrepl_start *start	= &call->req_packet.message.start;
	struct wrepl_start *start_reply	= &call->rep_packet.message.start_reply;

	if (call->req_packet.opcode & WREPL_OPCODE_BITS) {
		/*
		 *if the assoc_ctx doesn't match ignore the packet
		 */
		if ((call->req_packet.assoc_ctx != call->wreplconn->assoc_ctx.our_ctx)
		   && (call->req_packet.assoc_ctx != 0)) {
			return ERROR_INVALID_PARAMETER;
		}
	} else {
		call->wreplconn->assoc_ctx.our_ctx = WREPLSRV_INVALID_ASSOC_CTX;
		return NT_STATUS_OK;
	}

	if (start->minor_version != 2 || start->major_version != 5) {
		/* w2k terminate the connection if the versions doesn't match */
		return NT_STATUS_UNKNOWN_REVISION;
	}

	call->wreplconn->assoc_ctx.stopped	= False;
	call->wreplconn->assoc_ctx.our_ctx	= WREPLSRV_VALID_ASSOC_CTX;
	call->wreplconn->assoc_ctx.peer_ctx	= start->assoc_ctx;

	call->rep_packet.mess_type		= WREPL_START_ASSOCIATION_REPLY;
	start_reply->assoc_ctx			= call->wreplconn->assoc_ctx.our_ctx;
	start_reply->minor_version		= 2;
	start_reply->major_version		= 5;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_in_stop_assoc_ctx(struct wreplsrv_in_call *call)
{
	struct wrepl_stop *stop_out		= &call->rep_packet.message.stop;

	call->wreplconn->assoc_ctx.stopped	= True;

	call->rep_packet.mess_type		= WREPL_STOP_ASSOCIATION;
	stop_out->reason			= 4;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_in_stop_association(struct wreplsrv_in_call *call)
{
	/*
	 * w2k only check the assoc_ctx if the opcode has the 0x00007800 bits are set
	 */
	if (call->req_packet.opcode & WREPL_OPCODE_BITS) {
		/*
		 *if the assoc_ctx doesn't match ignore the packet
		 */
		if (call->req_packet.assoc_ctx != call->wreplconn->assoc_ctx.our_ctx) {
			return ERROR_INVALID_PARAMETER;
		}
		/* when the opcode bits are set the connection should be directly terminated */
		return NT_STATUS_CONNECTION_RESET;
	}

	if (call->wreplconn->assoc_ctx.stopped) {
		/* this causes the connection to be directly terminated */
		return NT_STATUS_CONNECTION_RESET;
	}

	/* this will cause to not receive packets anymore and terminate the connection if the reply is send */
	call->wreplconn->terminate = True;
	return wreplsrv_in_stop_assoc_ctx(call);
}

static NTSTATUS wreplsrv_in_replication(struct wreplsrv_in_call *call)
{
	struct wrepl_replication *repl_in = &call->req_packet.message.replication;

	/*
	 * w2k only check the assoc_ctx if the opcode has the 0x00007800 bits are set
	 */
	if (call->req_packet.opcode & WREPL_OPCODE_BITS) {
		/*
		 *if the assoc_ctx doesn't match ignore the packet
		 */
		if (call->req_packet.assoc_ctx != call->wreplconn->assoc_ctx.our_ctx) {
			return ERROR_INVALID_PARAMETER;
		}
	}

	if (!call->wreplconn->partner) {
		return wreplsrv_in_stop_assoc_ctx(call);
	}

	switch (repl_in->command) {
		case WREPL_REPL_TABLE_QUERY:
			break;
		case WREPL_REPL_TABLE_REPLY:
			break;
		case WREPL_REPL_SEND_REQUEST:
			break;
		case WREPL_REPL_SEND_REPLY:
			break;
		case WREPL_REPL_UPDATE:
			break;
		case WREPL_REPL_5:
			break;
		case WREPL_REPL_INFORM:
			break;
		case WREPL_REPL_9:
			break;
	}

	return ERROR_INVALID_PARAMETER;
}

static NTSTATUS wreplsrv_in_invalid_assoc_ctx(struct wreplsrv_in_call *call)
{
	struct wrepl_start *start	= &call->rep_packet.message.start;

	call->rep_packet.opcode		= 0x00008583;
	call->rep_packet.assoc_ctx	= 0;
	call->rep_packet.mess_type	= WREPL_START_ASSOCIATION;

	start->assoc_ctx		= 0x0000000a;
	start->minor_version		= 0x0001;
	start->major_version		= 0x0000;

	call->rep_packet.padding	= data_blob_talloc(call, NULL, 4);
	memset(call->rep_packet.padding.data, '\0', call->rep_packet.padding.length);

	return NT_STATUS_OK;
}

NTSTATUS wreplsrv_in_call(struct wreplsrv_in_call *call)
{
	NTSTATUS status;

	if (!(call->req_packet.opcode & WREPL_OPCODE_BITS)
	    && (call->wreplconn->assoc_ctx.our_ctx == WREPLSRV_INVALID_ASSOC_CTX)) {
		return wreplsrv_in_invalid_assoc_ctx(call);
	}

	switch (call->req_packet.mess_type) {
		case WREPL_START_ASSOCIATION:
			status = wreplsrv_in_start_association(call);
			break;
		case WREPL_START_ASSOCIATION_REPLY:
			/* this is not valid here, so we ignore it */
			return ERROR_INVALID_PARAMETER;

		case WREPL_STOP_ASSOCIATION:
			status = wreplsrv_in_stop_association(call);
			break;

		case WREPL_REPLICATION:
			status = wreplsrv_in_replication(call);
			break;
		default:
			/* everythingelse is also not valid here, so we ignore it */
			return ERROR_INVALID_PARAMETER;
	}

	if (call->wreplconn->assoc_ctx.our_ctx == WREPLSRV_INVALID_ASSOC_CTX) {
		return wreplsrv_in_invalid_assoc_ctx(call);
	}

	if (NT_STATUS_IS_OK(status)) {
		call->rep_packet.opcode		= WREPL_OPCODE_BITS;
		call->rep_packet.assoc_ctx	= call->wreplconn->assoc_ctx.peer_ctx;
	}

	return status;
}
