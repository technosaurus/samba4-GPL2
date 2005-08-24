/* 
 *  Unix SMB/CIFS implementation.
 *  account policy storage
 *  Copyright (C) Jean Fran�ois Micouleau      1998-2001.
 *  Copyright (C) Andrew Bartlett              2002
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "lib/tdb/include/tdbutil.h"
#include "lib/samba3/samba3.h"
#include "system/filesys.h"

#define DATABASE_VERSION 2

/****************************************************************************
 Open the account policy tdb.
****************************************************************************/

struct samba3_policy *samba3_read_account_policy(TALLOC_CTX *ctx, const char *fn)
{
	struct samba3_policy *ret;
	const char *vstring = "INFO/version";
	uint32_t version;

	TDB_CONTEXT *tdb = tdb_open(fn, 0, TDB_DEFAULT, O_RDONLY, 0600);
	if (!tdb) {
		DEBUG(0,("Failed to open account policy database\n"));
		return NULL;
	}

	/* handle a Samba upgrade */
	if (!tdb_fetch_uint32(tdb, vstring, &version) || version != DATABASE_VERSION) {
		tdb_store_uint32(tdb, vstring, DATABASE_VERSION);
	}

	ret = talloc_zero(ctx, struct samba3_policy);

	tdb_fetch_uint32(tdb, "min password length", &ret->min_password_length);
	tdb_fetch_uint32(tdb, "password history", &ret->password_history);
	tdb_fetch_uint32(tdb, "user must logon to change pasword", &ret->user_must_logon_to_change_password);
	tdb_fetch_uint32(tdb, "maximum password age", &ret->maximum_password_age);
	tdb_fetch_uint32(tdb, "minimum password age", &ret->minimum_password_age);
	tdb_fetch_uint32(tdb, "lockout duration", &ret->lockout_duration);
	tdb_fetch_uint32(tdb, "reset count minutes", &ret->reset_count_minutes);
	tdb_fetch_uint32(tdb, "bad lockout minutes", &ret->bad_lockout_minutes);
	tdb_fetch_uint32(tdb, "disconnect time", &ret->disconnect_time);
	tdb_fetch_uint32(tdb, "refuse machine password change", &ret->refuse_machine_password_change);

	tdb_close(tdb);

	return ret;
}


/* FIXME: Read privileges as well */
