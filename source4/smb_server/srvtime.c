/* 
   Unix SMB/CIFS implementation.

   server side time handling

   Copyright (C) Andrew Tridgell 2004

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
#include "smb_server/smb_server.h"


/*******************************************************************
put a dos date into a buffer (time/date format)
This takes GMT time and puts local time for zone_offset in the buffer
********************************************************************/
void srv_push_dos_date(struct smbsrv_connection *smb_server,
		      uint8_t *buf, int offset, time_t unixdate)
{
	push_dos_date(buf, offset, unixdate, smb_server->negotiate.zone_offset);
}

/*******************************************************************
put a dos date into a buffer (date/time format)
This takes GMT time and puts local time in the buffer
********************************************************************/
void srv_push_dos_date2(struct smbsrv_connection *smb_server,
		       char *buf, int offset, time_t unixdate)
{
	push_dos_date2(buf, offset, unixdate, smb_server->negotiate.zone_offset);
}

/*******************************************************************
put a dos 32 bit "unix like" date into a buffer. This routine takes
GMT and converts it to LOCAL time in zone_offset before putting it
********************************************************************/
void srv_push_dos_date3(struct smbsrv_connection *smb_server,
		       char *buf, int offset, time_t unixdate)
{
	push_dos_date3(buf, offset, unixdate, smb_server->negotiate.zone_offset);
}

/*******************************************************************
convert a dos date
********************************************************************/
time_t srv_pull_dos_date(struct smbsrv_connection *smb_server, 
			 const uint8_t *date_ptr)
{
	return pull_dos_date(date_ptr, smb_server->negotiate.zone_offset);
}

/*******************************************************************
like srv_pull_dos_date() but the words are reversed
********************************************************************/
time_t srv_pull_dos_date2(struct smbsrv_connection *smb_server, 
			  const uint8_t *date_ptr)
{
	return pull_dos_date2(date_ptr, smb_server->negotiate.zone_offset);
}

/*******************************************************************
  create a unix GMT date from a dos date in 32 bit "unix like" format
  these arrive in server zone, with corresponding DST
  ******************************************************************/
time_t srv_pull_dos_date3(struct smbsrv_connection *smb_server,
			  const uint8_t *date_ptr)
{
	return pull_dos_date3(date_ptr, smb_server->negotiate.zone_offset);
}
