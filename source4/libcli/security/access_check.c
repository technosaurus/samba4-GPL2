/*
   Unix SMB/CIFS implementation.

   security access checking routines

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
#include "libcli/security/security.h"


/*
  check if a sid is in the supplied token
*/
static BOOL sid_active_in_token(const struct dom_sid *sid, 
				const struct security_token *token)
{
	int i;
	for (i=0;i<token->num_sids;i++) {
		if (dom_sid_equal(sid, token->sids[i])) {
			return True;
		}
	}
	return False;
}


/*
  perform a SEC_FLAG_MAXIMUM_ALLOWED access check
*/
static uint32_t access_check_max_allowed(const struct security_descriptor *sd, 
					 const struct security_token *token)
{
	uint32_t denied = 0, granted = 0;
	unsigned i;
	
	if (sid_active_in_token(sd->owner_sid, token)) {
		granted |= SEC_STD_WRITE_DAC | SEC_STD_READ_CONTROL;
	}
	if (sec_privilege_check(token, SEC_PRIV_RESTORE)) {
		granted |= SEC_STD_DELETE;
	}

	for (i = 0;i<sd->dacl->num_aces; i++) {
		struct security_ace *ace = &sd->dacl->aces[i];

		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
			continue;
		}

		if (!sid_active_in_token(&ace->trustee, token)) {
			continue;
		}

		switch (ace->type) {
			case SEC_ACE_TYPE_ACCESS_ALLOWED:
				granted |= ace->access_mask;
				break;
			case SEC_ACE_TYPE_ACCESS_DENIED:
			case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
				denied |= ace->access_mask;
				break;
		}
	}

	return granted & ~denied;
}

/*
  the main entry point for access checking. 
*/
NTSTATUS sec_access_check(const struct security_descriptor *sd, 
			  const struct security_token *token,
			  uint32_t access_desired,
			  uint32_t *access_granted)
{
	int i;
	uint32_t bits_remaining;

	*access_granted = access_desired;
	bits_remaining = access_desired;

	/* handle the maximum allowed flag */
	if (access_desired & SEC_FLAG_MAXIMUM_ALLOWED) {
		access_desired |= access_check_max_allowed(sd, token);
		access_desired &= ~SEC_FLAG_MAXIMUM_ALLOWED;
		*access_granted = access_desired;
		bits_remaining = access_desired & ~SEC_STD_DELETE;
	}

	if (access_desired & SEC_FLAG_SYSTEM_SECURITY) {
		if (sec_privilege_check(token, SEC_PRIV_SECURITY)) {
			bits_remaining &= ~SEC_FLAG_SYSTEM_SECURITY;
		} else {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	/* dacl not present allows access */
	if (!(sd->type & SEC_DESC_DACL_PRESENT)) {
		*access_granted = access_desired;
		return NT_STATUS_OK;
	}

	/* empty dacl denies access */
	if (sd->dacl == NULL || sd->dacl->num_aces == 0) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* the owner always gets SEC_STD_WRITE_DAC & SEC_STD_READ_CONTROL */
	if ((bits_remaining & (SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL)) &&
	    sid_active_in_token(sd->owner_sid, token)) {
		bits_remaining &= ~(SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL);
	}
	if ((bits_remaining & SEC_STD_DELETE) &&
	    sec_privilege_check(token, SEC_PRIV_RESTORE)) {
		bits_remaining &= ~SEC_STD_DELETE;
	}

	/* check each ace in turn. */
	for (i=0; bits_remaining && i < sd->dacl->num_aces; i++) {
		struct security_ace *ace = &sd->dacl->aces[i];

		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
			continue;
		}

		if (!sid_active_in_token(&ace->trustee, token)) {
			continue;
		}

		switch (ace->type) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED:
			bits_remaining &= ~ace->access_mask;
			break;
		case SEC_ACE_TYPE_ACCESS_DENIED:
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
			if (bits_remaining & ace->access_mask) {
				return NT_STATUS_ACCESS_DENIED;
			}
			break;
		}
	}

	if (bits_remaining != 0) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}
