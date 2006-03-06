/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1998-2002
   
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

/**
 * @file
 * @brief Capabilities functions
 **/

/*
  capabilities fns - will be needed when we enable kernel oplocks
*/

#include "includes.h"
#include "system/network.h"
#include "system/wait.h"
#include "system/filesys.h"


#if defined(HAVE_IRIX_SPECIFIC_CAPABILITIES)
/**************************************************************************
 Try and abstract process capabilities (for systems that have them).
****************************************************************************/
static BOOL set_process_capability( uint32_t cap_flag, BOOL enable )
{
	if(cap_flag == KERNEL_OPLOCK_CAPABILITY) {
		cap_t cap = cap_get_proc();

		if (cap == NULL) {
			DEBUG(0,("set_process_capability: cap_get_proc failed. Error was %s\n",
				strerror(errno)));
			return False;
		}

		if(enable)
			cap->cap_effective |= CAP_NETWORK_MGT;
		else
			cap->cap_effective &= ~CAP_NETWORK_MGT;

		if (cap_set_proc(cap) == -1) {
			DEBUG(0,("set_process_capability: cap_set_proc failed. Error was %s\n",
				strerror(errno)));
			cap_free(cap);
			return False;
		}

		cap_free(cap);

		DEBUG(10,("set_process_capability: Set KERNEL_OPLOCK_CAPABILITY.\n"));
	}
	return True;
}

/**************************************************************************
 Try and abstract inherited process capabilities (for systems that have them).
****************************************************************************/

static BOOL set_inherited_process_capability( uint32_t cap_flag, BOOL enable )
{
	if(cap_flag == KERNEL_OPLOCK_CAPABILITY) {
		cap_t cap = cap_get_proc();

		if (cap == NULL) {
			DEBUG(0,("set_inherited_process_capability: cap_get_proc failed. Error was %s\n",
				strerror(errno)));
			return False;
		}

		if(enable)
			cap->cap_inheritable |= CAP_NETWORK_MGT;
		else
			cap->cap_inheritable &= ~CAP_NETWORK_MGT;

		if (cap_set_proc(cap) == -1) {
			DEBUG(0,("set_inherited_process_capability: cap_set_proc failed. Error was %s\n", 
				strerror(errno)));
			cap_free(cap);
			return False;
		}

		cap_free(cap);

		DEBUG(10,("set_inherited_process_capability: Set KERNEL_OPLOCK_CAPABILITY.\n"));
	}
	return True;
}
#endif

/**
 Gain the oplock capability from the kernel if possible.
**/

_PUBLIC_ void oplock_set_capability(BOOL this_process, BOOL inherit)
{
#if HAVE_KERNEL_OPLOCKS_IRIX
	set_process_capability(KERNEL_OPLOCK_CAPABILITY,this_process);
	set_inherited_process_capability(KERNEL_OPLOCK_CAPABILITY,inherit);
#endif
}

