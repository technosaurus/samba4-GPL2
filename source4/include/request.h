/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 2003
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

/*
  Shared state structure between client and server, representing the basic packet.
*/

struct request_buffer {
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

