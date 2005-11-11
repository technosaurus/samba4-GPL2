/* 
   Unix SMB/CIFS implementation.

   SMB2 client library header

   Copyright (C) Andrew Tridgell 2005
   
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

struct smb2_options {
	uint32_t timeout;
};

/*
  information returned from the negotiate response
*/
struct smb2_negotiate {
	DATA_BLOB secblob;
};

/* this is the context for the smb2 transport layer */
struct smb2_transport {
	/* socket level info */
	struct smbcli_socket *socket;

	struct smb2_options options;
	struct smb2_negotiate negotiate;

	/* next seqnum to allocate */
	uint64_t seqnum;

	/* a list of requests that are pending for receive on this
	   connection */
	struct smb2_request *pending_recv;

	/* context of the stream -> packet parser */
	struct packet_context *packet;
};


/*
  SMB2 tree context
*/
struct smb2_tree {
	struct smb2_session *session;
	uint32_t tid;
};

/*
  SMB2 session context
*/
struct smb2_session {
	struct smb2_transport *transport;
	struct gensec_security *gensec;
	uint64_t uid;
};


struct smb2_request_buffer {
	/* the raw SMB2 buffer, including the 4 byte length header */
	uint8_t *buffer;
	
	/* the size of the raw buffer, including 4 byte header */
	uint_t size;
	
	/* how much has been allocated - on reply the buffer is over-allocated to 
	   prevent too many realloc() calls 
	*/
	uint_t allocated;
	
	/* the start of the SMB2 header - this is always buffer+4 */
	uint8_t *hdr;
	
	/* the packet body */
	uint8_t *body;
	uint_t body_size;
	
	/* ptr is used as a moving pointer into the data area
	 * of the packet. The reason its here and not a local
	 * variable in each function is that when a realloc of
	 * a send packet is done we need to move this
	 * pointer */
	uint8_t *ptr;
};


/*
  a client request moves between the following 4 states.
*/
enum smb2_request_state {SMB2_REQUEST_INIT, /* we are creating the request */
			SMB2_REQUEST_RECV, /* we are waiting for a matching reply */
			SMB2_REQUEST_DONE, /* the request is finished */
			SMB2_REQUEST_ERROR}; /* a packet or transport level error has occurred */

/* the context for a single SMB2 request */
struct smb2_request {
	/* allow a request to be part of a list of requests */
	struct smb2_request *next, *prev;

	/* each request is in one of 3 possible states */
	enum smb2_request_state state;
	
	struct smb2_transport *transport;

	uint64_t seqnum;

	/* the NT status for this request. Set by packet receive code
	   or code detecting error. */
	NTSTATUS status;
	
	struct smb2_request_buffer in;
	struct smb2_request_buffer out;

	/* information on what to do with a reply when it is received
	   asyncronously. If this is not setup when a reply is received then
	   the reply is discarded

	   The private pointer is private to the caller of the client
	   library (the application), not private to the library
	*/
	struct {
		void (*fn)(struct smb2_request *);
		void *private;
	} async;
};


#define SMB2_MIN_SIZE 0x40

/* offsets into header elements */
#define SMB2_HDR_LENGTH  0x04
#define SMB2_HDR_PAD1    0x06
#define SMB2_HDR_STATUS  0x08
#define SMB2_HDR_OPCODE  0x0c
#define SMB2_HDR_PAD2    0x0e
#define SMB2_HDR_FLAGS   0x10
#define SMB2_HDR_UNKNOWN 0x14
#define SMB2_HDR_SEQNUM  0x18
#define SMB2_HDR_PID     0x20
#define SMB2_HDR_TID     0x24
#define SMB2_HDR_UID     0x28 /* 64 bit */
#define SMB2_HDR_SIG     0x30 /* guess ... */
#define SMB2_HDR_BODY    0x40

/* SMB2 opcodes */
#define SMB2_OP_NEGPROT   0x00
#define SMB2_OP_SESSSETUP 0x01
#define SMB2_OP_TCON      0x03
#define SMB2_OP_TDIS      0x04
#define SMB2_OP_CREATE    0x05
#define SMB2_OP_CLOSE     0x06
#define SMB2_OP_READ      0x08
#define SMB2_OP_WRITE     0x09
#define SMB2_OP_FIND      0x0e

#define SMB2_MAGIC 0x424D53FE /* 0xFE 'S' 'M' 'B' */

/*
  check that a buffer code matches the expected value
*/
#define SMB2_CHECK_BUFFER_CODE(req, code) do { \
	io->out.buffer_code = SVAL(req->in.body, 0); \
	if (io->out.buffer_code != (code)) { \
		DEBUG(0,("Unexpected buffer code 0x%x. Expected 0x%x\n", \
			 io->out.buffer_code, code)); \
		return NT_STATUS_INVALID_PARAMETER; \
	} \
} while (0)
