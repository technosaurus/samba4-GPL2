/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Luke Kenneth Casson Leighton 1996-1998
   Copyright (C) Jeremy Allison 1998
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

#ifndef _CLI_CONTEXT_H
#define _CLI_CONTEXT_H

struct cli_tree;  /* forward declare */
struct cli_request;  /* forward declare */
struct cli_session;  /* forward declare */
struct cli_transport;  /* forward declare */

/* context that will be and has been negotiated between the client and server */
struct cli_negotiate {
	/* 
	 * negotiated maximum transmit size - this is given to us by the server
	 */
	uint32_t max_xmit;

	/* maximum number of requests that can be multiplexed */
	uint16_t max_mux;

	/* the negotiatiated protocol */
	enum protocol_types protocol;

	uint8_t sec_mode;		/* security mode returned by negprot */
	uint8_t key_len;
	DATA_BLOB server_guid;      /* server_guid */
	DATA_BLOB secblob;      /* cryptkey or negTokenInit blob */
	uint32_t sesskey;
	
	struct {
		void (*sign_outgoing_message)(struct cli_request *req);
		BOOL (*check_incoming_message)(struct cli_request *req);
		void (*free_signing_context)(struct cli_transport *transport);
		void *signing_context;
		BOOL doing_signing;
	} sign_info;

	/* capabilities that the server reported */
	uint32_t capabilities;
	
	int16_t server_zone;
	time_t server_time;
	uint_t readbraw_supported:1;
	uint_t writebraw_supported:1;

	const char *server_domain;
};
	
/* this is the context for a SMB socket associated with the socket itself */
struct cli_socket {
	TALLOC_CTX *mem_ctx;  	/* life of socket pool */

	/* when the reference count reaches zero then the socket is destroyed */
	int reference_count;

	struct in_addr dest_ip;

	/* the port used */
	int port;
	
	/* the open file descriptor */
	int fd;

	/* a count of the number of packets we have received. We
	 * actually only care about zero/non-zero at this stage */
	uint_t pkt_count;

	/* the network address of the client */
	char *client_addr;
	
	/* timeout for socket operations in milliseconds. */
	int timeout;
};

/*
  this structure allows applications to control the behaviour of the
  client library
*/
struct cli_options {
	uint_t use_oplocks:1;
	uint_t use_level2_oplocks:1;
	uint_t use_spnego:1;
};

/* this is the context for the client transport layer */
struct cli_transport {
	TALLOC_CTX *mem_ctx;

	/* when the reference count reaches zero then the transport is destroyed */
	int reference_count;

	/* socket level info */
	struct cli_socket *socket;

	/* the next mid to be allocated - needed for signing and
	   request matching */
	uint16_t next_mid;
	
	/* negotiated protocol information */
	struct cli_negotiate negotiate;

	/* options to control the behaviour of the client code */
	struct cli_options options;

	/* is a readbraw pending? we need to handle that case
	   specially on receiving packets */
	uint_t readbraw_pending:1;
	
	/* an idle function - if this is defined then it will be
	   called once every period milliseconds while we are waiting
	   for a packet */
	struct {
		void (*func)(struct cli_transport *, void *);
		void *private;
		uint_t period;
	} idle;

	/* the error fields from the last message */
	struct {
		enum {ETYPE_NONE, ETYPE_DOS, ETYPE_NT, ETYPE_SOCKET, ETYPE_NBT} etype;
		union {
			struct {
				uint8_t eclass;
				uint16_t ecode;
			} dos;
			NTSTATUS nt_status;
			enum socket_error socket_error;
			uint_t nbt_error;
		} e;
	} error;

	struct {
		/* a oplock break request handler */
		BOOL (*handler)(struct cli_transport *transport, 
				uint16_t tid, uint16_t fnum, uint8_t level, void *private);
		/* private data passed to the oplock handler */
		void *private;
	} oplock;

	/* a list of async requests that are pending on this connection */
	struct cli_request *pending_requests;

	/* remember the called name - some sub-protocols require us to
	   know the server name */
	struct nmb_name called;
};

/* this is the context for the user */

/* this is the context for the session layer */
struct cli_session {	
	TALLOC_CTX *mem_ctx;  	/* life of session */

	/* when the reference count reaches zero then the session is destroyed */
	int reference_count;	
	
	/* transport layer info */
	struct cli_transport *transport;
	
	/* after a session setup the server provides us with
	   a vuid identifying the security context */
	uint16_t vuid;

	/* default pid for this session */
	uint32_t pid;

	DATA_BLOB user_session_key;

	/* the spnego context if we use extented security */
	struct gensec_security *gensec;
};

/* 
   cli_tree context: internal state for a tree connection. 
 */
struct cli_tree {
	/* life of tree tree */
	TALLOC_CTX *mem_ctx;

	/* when the reference count reaches zero then the tree is destroyed */
	int reference_count;	

	/* session layer info */
	struct cli_session *session;

	uint16_t tid;			/* tree id, aka cnum */
	char *device;
	char *fs_type;
};

/* the context for a single SMB request. This is passed to any request-context 
 * functions (similar to context.h, the server version).
 * This will allow requests to be multi-threaded. */
struct cli_request {
	/* allow a request to be part of a list of requests */
	struct cli_request *next, *prev;

	/* a talloc context for the lifetime of this request */
	TALLOC_CTX *mem_ctx;
	
	/* a request always has a transport context, nearly always has
	   a session context and usually has a tree context */
	struct cli_transport *transport;
	struct cli_session *session;
	struct cli_tree *tree;

	/* the flags2 from the SMB request, in raw form (host byte
	   order). Used to parse strings */
	uint16_t flags2;

	/* the NT status for this request. Set by packet receive code
	   or code detecting error. */
	NTSTATUS status;
	
	/* the sequence number of this packet - used for signing */
	uint_t seq_num;

	/* set if this is a one-way request, meaning we are not
	   expecting a reply from the server. */
	uint_t one_way_request:1;

	/* the mid of this packet - used to match replies */
	uint16_t mid;

	struct {
		/* the raw SMB buffer, including the 4 byte length header */
		char *buffer;
		
		/* the size of the raw buffer, including 4 byte header */
		uint_t size;

		/* how much has been allocated - on reply the buffer is over-allocated to 
		   prevent too many realloc() calls 
		*/
		uint_t allocated;

		/* the start of the SMB header - this is always buffer+4 */
		char *hdr;

		/* the command words and command word count. vwv points
		   into the raw buffer */
		char *vwv;
		uint_t wct;

		/* the data buffer and size. data points into the raw buffer */
		char *data;
		uint_t data_size;

		/* ptr is used as a moving pointer into the data area
		 * of the packet. The reason its here and not a local
		 * variable in each function is that when a realloc of
		 * a send packet is done we need to move this
		 * pointer */
		char *ptr;
	} in, out;

	/* information on what to do with a reply when it is received
	   asyncronously. If this is not setup when a reply is received then
	   the reply is discarded

	   The private pointer is private to the caller of the client
	   library (the application), not private to the library
	*/
	struct {
		void (*fn)(struct cli_request *);
		void *private;
	} async;
};

/* 
   cli_state: internal state used in libcli library for single-threaded callers, 
   i.e. a single session on a single socket. 
 */
struct cli_state {
	TALLOC_CTX *mem_ctx;  	/* life of client pool */
	struct cli_transport *transport;
	struct cli_session *session;
	struct cli_tree *tree;
	struct substitute_context substitute;
};

/* useful way of catching wct errors with file and line number */
#define CLI_CHECK_MIN_WCT(req, wcount) if ((req)->in.wct < (wcount)) { \
      DEBUG(1,("Unexpected WCT %d at %s(%d) - expected min %d\n", (req)->in.wct, __FILE__, __LINE__, wcount)); \
      req->status = NT_STATUS_INVALID_PARAMETER; \
      goto failed; \
}

#define CLI_CHECK_WCT(req, wcount) if ((req)->in.wct != (wcount)) { \
      DEBUG(1,("Unexpected WCT %d at %s(%d) - expected %d\n", (req)->in.wct, __FILE__, __LINE__, wcount)); \
      req->status = NT_STATUS_INVALID_PARAMETER; \
      goto failed; \
}

#endif /* _CLI_CONTEXT_H */
