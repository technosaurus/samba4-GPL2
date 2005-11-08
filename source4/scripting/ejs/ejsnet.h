/* 
   Unix SMB/CIFS implementation.

   provide interfaces to libnet calls from ejs scripts

   Copyright (C) Rafal Szczesniak  2005
   
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

#include "lib/appweb/ejs/ejs.h"


void ejsnet_setup(void);
static int ejs_net_userman(MprVarHandle, int, struct MprVar**);
static int ejs_net_createuser(MprVarHandle, int, char**);
