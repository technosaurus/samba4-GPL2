/* 
   Unix SMB/CIFS implementation.
   Samba mutex/lock functions
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James J Myers 2003
   
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
#include "mutex.h"
	 
/* the registered mutex handlers */
static struct {
	const char *name;
	struct mutex_ops ops;
} mutex_handlers;

/* read/write lock routines */


/*
  register a set of mutex/rwlock handlers. 
  Should only be called once in the execution of smbd.
*/
BOOL register_mutex_handlers(const char *name, struct mutex_ops *ops)
{
	if (mutex_handlers.name != NULL) {
		/* it's already registered! */
		DEBUG(2,("mutex handler '%s' already registered - failed '%s'\n", 
			 mutex_handlers.name, name));
		return False;
	}

	mutex_handlers.name = name;
	mutex_handlers.ops = *ops;

	DEBUG(2,("mutex handler '%s' registered\n", name));
	return True;
}

