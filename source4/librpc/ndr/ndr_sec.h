/* 
   Unix SMB/CIFS implementation.

   definitions for marshalling/unmarshalling security descriptors
   and related structures

   Copyright (C) Andrew Tridgell 2003
   
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


/* use the same structure for dom_sid2 as dom_sid */
#define dom_sid2 dom_sid

/* query security descriptor */
struct smb_query_secdesc {
	struct {
		uint16 fnum;
		uint32 secinfo_flags;
	} in;
	struct {
		struct security_descriptor *sd;
	} out;
};

/* set security descriptor */
struct smb_set_secdesc {
	struct {
		uint16 fnum;
		uint32 secinfo_flags;
		struct security_descriptor *sd;
	} in;
};
