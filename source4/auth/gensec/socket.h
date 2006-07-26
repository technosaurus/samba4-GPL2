/* 
   Unix SMB/CIFS implementation.
 
   Generic Authentication Interface (socket wrapper)

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
   
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

NTSTATUS gensec_socket_init(struct gensec_security *gensec_security,
			    struct socket_context *current_socket,
			    struct event_context *ev,
			    void (*recv_handler)(void *, uint16_t),
			    void *recv_private,
			    struct socket_context **new_socket);