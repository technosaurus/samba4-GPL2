/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2005
   
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

/*
  a composite function for getting user information via samr pipe
*/

#include "includes.h"
#include "libcli/composite/composite.h"
#include "libnet/composite.h"
#include "librpc/gen_ndr/security.h"
#include "libcli/security/security.h"
#include "libnet/userman.h"
#include "libnet/userinfo.h"
#include "librpc/gen_ndr/ndr_samr_c.h"


struct userinfo_state {
	struct dcerpc_pipe        *pipe;
	struct policy_handle      domain_handle;
	struct policy_handle      user_handle;
	uint16_t                  level;
	struct samr_LookupNames   lookup;
	struct samr_OpenUser      openuser;
	struct samr_QueryUserInfo queryuserinfo;
	struct samr_Close         samrclose;	
	union  samr_UserInfo      *info;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static void continue_userinfo_lookup(struct rpc_request *req);
static void continue_userinfo_openuser(struct rpc_request *req);
static void continue_userinfo_getuser(struct rpc_request *req);
static void continue_userinfo_closeuser(struct rpc_request *req);


/**
 * Stage 1 (optional): Look for a username in SAM server.
 */
static void continue_userinfo_lookup(struct rpc_request *req)
{
	struct composite_context *c;
	struct userinfo_state *s;
	struct rpc_request *openuser_req;
	struct monitor_msg msg;
	struct msg_rpc_lookup_name *msg_lookup;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct userinfo_state);

	/* receive samr_Lookup reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;
	
	/* there could be a problem with name resolving itself */
	if (!NT_STATUS_IS_OK(s->lookup.out.result)) {
		composite_error(c, s->lookup.out.result);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = rpc_lookup_name;
		msg_lookup = talloc(s, struct msg_rpc_lookup_name);
		msg_lookup->rid = s->lookup.out.rids.ids;
		msg_lookup->count = s->lookup.out.rids.count;
		msg.data = (void*)msg_lookup;
		msg.data_size = sizeof(*msg_lookup);
		
		s->monitor_fn(&msg);
	}
	

	/* have we actually got name resolved
	   - we're looking for only one at the moment */
	if (s->lookup.out.rids.count == 0) {
		composite_error(c, NT_STATUS_NO_SUCH_USER);
	}

	/* TODO: find proper status code for more than one rid found */

	/* prepare parameters for LookupNames */
	s->openuser.in.domain_handle  = &s->domain_handle;
	s->openuser.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	s->openuser.in.rid            = s->lookup.out.rids.ids[0];
	s->openuser.out.user_handle   = &s->user_handle;

	/* send request */
	openuser_req = dcerpc_samr_OpenUser_send(s->pipe, c, &s->openuser);
	if (composite_nomem(openuser_req, c)) return;

	composite_continue_rpc(c, openuser_req, continue_userinfo_openuser, c);
}


/**
 * Stage 2: Open user policy handle.
 */
static void continue_userinfo_openuser(struct rpc_request *req)
{
	struct composite_context *c;
	struct userinfo_state *s;
	struct rpc_request *queryuser_req;
	struct monitor_msg msg;
	struct msg_rpc_open_user *msg_open;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct userinfo_state);

	/* receive samr_OpenUser reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	if (!NT_STATUS_IS_OK(s->queryuserinfo.out.result)) {
		composite_error(c, s->queryuserinfo.out.result);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = rpc_open_user;
		msg_open = talloc(s, struct msg_rpc_open_user);
		msg_open->rid = s->openuser.in.rid;
		msg_open->access_mask = s->openuser.in.access_mask;
		msg.data = (void*)msg_open;
		msg.data_size = sizeof(*msg_open);
		
		s->monitor_fn(&msg);
	}
	
	/* prepare parameters for QueryUserInfo call */
	s->queryuserinfo.in.user_handle = &s->user_handle;
	s->queryuserinfo.in.level       = s->level;
	
	/* queue rpc call, set event handling and new state */
	queryuser_req = dcerpc_samr_QueryUserInfo_send(s->pipe, c, &s->queryuserinfo);
	if (composite_nomem(queryuser_req, c)) return;
	
	composite_continue_rpc(c, queryuser_req, continue_userinfo_getuser, c);
}


/**
 * Stage 3: Get requested user information.
 */
static void continue_userinfo_getuser(struct rpc_request *req)
{
	struct composite_context *c;
	struct userinfo_state *s;
	struct rpc_request *close_req;
	struct monitor_msg msg;
	struct msg_rpc_query_user *msg_query;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct userinfo_state);

	/* receive samr_QueryUserInfo reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	/* check if queryuser itself went ok */
	if (!NT_STATUS_IS_OK(s->queryuserinfo.out.result)) {
		composite_error(c, s->queryuserinfo.out.result);
		return;
	}

	s->info = talloc_steal(s, s->queryuserinfo.out.info);

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = rpc_query_user;
		msg_query = talloc(s, struct msg_rpc_query_user);
		msg_query->level = s->queryuserinfo.in.level;
		msg.data = (void*)msg_query;
		msg.data_size = sizeof(*msg_query);
		
		s->monitor_fn(&msg);
	}
	
	/* prepare arguments for Close call */
	s->samrclose.in.handle  = &s->user_handle;
	s->samrclose.out.handle = &s->user_handle;
	
	/* queue rpc call, set event handling and new state */
	close_req = dcerpc_samr_Close_send(s->pipe, c, &s->samrclose);
	if (composite_nomem(close_req, c)) return;
	
	composite_continue_rpc(c, close_req, continue_userinfo_closeuser, c);
}


/**
 * Stage 4: Close policy handle associated with opened user.
 */
static void continue_userinfo_closeuser(struct rpc_request *req)
{
	struct composite_context *c;
	struct userinfo_state *s;
	struct monitor_msg msg;
	struct msg_rpc_close_user *msg_close;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct userinfo_state);

	/* receive samr_Close reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	if (!NT_STATUS_IS_OK(s->samrclose.out.result)) {
		composite_error(c, s->samrclose.out.result);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = rpc_close_user;
		msg_close = talloc(s, struct msg_rpc_close_user);
		msg_close->rid = s->openuser.in.rid;
		msg.data = (void*)msg_close;
		msg.data_size = sizeof(*msg_close);

		s->monitor_fn(&msg);
	}

	composite_done(c);
}


/**
 * Sends asynchronous userinfo request
 *
 * @param p dce/rpc call pipe 
 * @param io arguments and results of the call
 */
struct composite_context *libnet_rpc_userinfo_send(struct dcerpc_pipe *p,
						   struct libnet_rpc_userinfo *io,
						   void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct userinfo_state *s;
	struct dom_sid *sid;
	struct rpc_request *openuser_req, *lookup_req;

	if (!p || !io) return NULL;
	
	c = composite_create(p, dcerpc_event_context(p));
	if (c == NULL) return c;
	
	s = talloc_zero(c, struct userinfo_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;

	s->level         = io->in.level;
	s->pipe          = p;
	s->domain_handle = io->in.domain_handle;
	s->monitor_fn    = monitor;

	if (io->in.sid) {
		sid = dom_sid_parse_talloc(s, io->in.sid);
		if (composite_nomem(sid, c)) return c;

		s->openuser.in.domain_handle  = &s->domain_handle;
		s->openuser.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
		s->openuser.in.rid            = sid->sub_auths[sid->num_auths - 1];
		s->openuser.out.user_handle   = &s->user_handle;
		
		/* send request */
		openuser_req = dcerpc_samr_OpenUser_send(p, c, &s->openuser);
		if (composite_nomem(openuser_req, c)) return c;

		composite_continue_rpc(c, openuser_req, continue_userinfo_openuser, c);

	} else {
		/* preparing parameters to send rpc request */
		s->lookup.in.domain_handle    = &s->domain_handle;
		s->lookup.in.num_names        = 1;
		s->lookup.in.names            = talloc_array(s, struct lsa_String, 1);
		if (composite_nomem(s->lookup.in.names, c)) return c;

		s->lookup.in.names[0].string  = talloc_strdup(s, io->in.username);
		if (composite_nomem(s->lookup.in.names[0].string, c)) return c;
		
		/* send request */
		lookup_req = dcerpc_samr_LookupNames_send(p, c, &s->lookup);
		if (composite_nomem(lookup_req, c)) return c;
		
		composite_continue_rpc(c, lookup_req, continue_userinfo_lookup, c);
	}

	return c;
}


/**
 * Waits for and receives result of asynchronous userinfo call
 * 
 * @param c composite context returned by asynchronous userinfo call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_userinfo_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				  struct libnet_rpc_userinfo *io)
{
	NTSTATUS status;
	struct userinfo_state *s;
	
	/* wait for results of sending request */
	status = composite_wait(c);
	
	if (NT_STATUS_IS_OK(status) && io) {
		s = talloc_get_type(c->private_data, struct userinfo_state);
		talloc_steal(mem_ctx, s->info);
		io->out.info = *s->info;
	}
	
	/* memory context associated to composite context is no longer needed */
	talloc_free(c);
	return status;
}


/**
 * Synchronous version of userinfo call
 *
 * @param pipe dce/rpc call pipe
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_userinfo(struct dcerpc_pipe *p,
			     TALLOC_CTX *mem_ctx,
			     struct libnet_rpc_userinfo *io)
{
	struct composite_context *c = libnet_rpc_userinfo_send(p, io, NULL);
	return libnet_rpc_userinfo_recv(c, mem_ctx, io);
}
