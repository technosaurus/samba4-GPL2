/* 
   Unix SMB/CIFS implementation.

   Winbind daemon - user related function

   Copyright (C) Tim Potter 2000
   Copyright (C) Anthony Liguori 2003
   
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

#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

/* High water mark keys */
#define HWM_GROUP  "GROUP HWM"
#define HWM_USER   "USER HWM"

/* idmap version determines auto-conversion */
#define IDMAP_VERSION 2

/* Globals */
static TDB_CONTEXT *idmap_tdb;

/* convert one record to the new format */
static int tdb_convert_fn(TDB_CONTEXT * tdb, TDB_DATA key, TDB_DATA data,
			  void *ignored)
{
	struct winbindd_domain *domain;
	char *p;
	DOM_SID sid;
	uint32 rid;
	fstring keystr;
	fstring dom_name;
	TDB_DATA key2;

	p = strchr(key.dptr, '/');
	if (!p)
		return 0;

	*p = 0;
	fstrcpy(dom_name, key.dptr);
	*p++ = '/';

	domain = find_domain_from_name(dom_name);
	if (!domain) {
		/* We must delete the old record. */
		DEBUG(0,
		      ("winbindd: tdb_convert_fn : Unable to find domain %s\n",
		       dom_name));
		DEBUG(0,
		      ("winbindd: tdb_convert_fn : deleting record %s\n",
		       key.dptr));
		tdb_delete(idmap_tdb, key);
		return 0;
	}

	rid = atoi(p);

	sid_copy(&sid, &domain->sid);
	sid_append_rid(&sid, rid);

	sid_to_string(keystr, &sid);
	key2.dptr = keystr;
	key2.dsize = strlen(keystr) + 1;

	if (tdb_store(idmap_tdb, key2, data, TDB_INSERT) != 0) {
		/* not good! */
		DEBUG(0,
		      ("winbindd: tdb_convert_fn : Unable to update record %s\n",
		       key2.dptr));
		DEBUG(0,
		      ("winbindd: tdb_convert_fn : conversion failed - idmap corrupt ?\n"));
		return -1;
	}

	if (tdb_store(idmap_tdb, data, key2, TDB_REPLACE) != 0) {
		/* not good! */
		DEBUG(0,
		      ("winbindd: tdb_convert_fn : Unable to update record %s\n",
		       data.dptr));
		DEBUG(0,
		      ("winbindd: tdb_convert_fn : conversion failed - idmap corrupt ?\n"));
		return -1;
	}

	tdb_delete(idmap_tdb, key);

	return 0;
}

/*****************************************************************************
 Convert the idmap database from an older version.
*****************************************************************************/
static BOOL tdb_idmap_convert(const char *idmap_name)
{
	int32 vers = tdb_fetch_int32(idmap_tdb, "IDMAP_VERSION");
	BOOL bigendianheader =
	    (idmap_tdb->flags & TDB_BIGENDIAN) ? True : False;

	if (vers == IDMAP_VERSION)
		return True;

	if (((vers == -1) && bigendianheader)
	    || (IREV(vers) == IDMAP_VERSION)) {
		/* Arrggghh ! Bytereversed or old big-endian - make order independent ! */
		/*
		 * high and low records were created on a
		 * big endian machine and will need byte-reversing.
		 */

		int32 wm;

		wm = tdb_fetch_int32(idmap_tdb, HWM_USER);

		if (wm != -1) {
			wm = IREV(wm);
		} else
			wm = server_state.uid_low;

		if (tdb_store_int32(idmap_tdb, HWM_USER, wm) == -1) {
			DEBUG(0,
			      ("tdb_idmap_convert: Unable to byteswap user hwm in idmap database\n"));
			return False;
		}

		wm = tdb_fetch_int32(idmap_tdb, HWM_GROUP);
		if (wm != -1) {
			wm = IREV(wm);
		} else
			wm = server_state.gid_low;

		if (tdb_store_int32(idmap_tdb, HWM_GROUP, wm) == -1) {
			DEBUG(0,
			      ("tdb_idmap_convert: Unable to byteswap group hwm in idmap database\n"));
			return False;
		}
	}

	/* the old format stored as DOMAIN/rid - now we store the SID direct */
	tdb_traverse(idmap_tdb, tdb_convert_fn, NULL);

	if (tdb_store_int32(idmap_tdb, "IDMAP_VERSION", IDMAP_VERSION) ==
	    -1) {
		DEBUG(0,
		      ("tdb_idmap_convert: Unable to byteswap group hwm in idmap database\n"));
		return False;
	}

	return True;
}

/* Allocate either a user or group id from the pool */
static BOOL tdb_allocate_id(uid_t * id, BOOL isgroup)
{
	int hwm;

	/* Get current high water mark */
	if ((hwm = tdb_fetch_int32(idmap_tdb,
				   isgroup ? HWM_GROUP : HWM_USER)) ==
	    -1) {
		return False;
	}

	/* Return next available uid in list */
	if ((isgroup && (hwm > server_state.gid_high)) ||
	    (!isgroup && (hwm > server_state.uid_high))) {
		DEBUG(0,
		      ("winbind %sid range full!\n", isgroup ? "g" : "u"));
		return False;
	}

	if (id) {
		*id = hwm;
	}

	hwm++;

	/* Store new high water mark */
	tdb_store_int32(idmap_tdb, isgroup ? HWM_GROUP : HWM_USER, hwm);

	return True;
}

/* Get a sid from an id */
static BOOL tdb_get_sid_from_id(int id, DOM_SID * sid, BOOL isgroup)
{
	TDB_DATA key, data;
	fstring keystr;
	BOOL result = False;

	slprintf(keystr, sizeof(keystr), "%s %d", isgroup ? "GID" : "UID",
		 id);

	key.dptr = keystr;
	key.dsize = strlen(keystr) + 1;

	data = tdb_fetch(idmap_tdb, key);

	if (data.dptr) {
		result = string_to_sid(sid, data.dptr);
		SAFE_FREE(data.dptr);
	}

	return result;
}

/* Get an id from a sid */
static BOOL tdb_get_id_from_sid(DOM_SID * sid, uid_t * id, BOOL isgroup)
{
	TDB_DATA data, key;
	fstring keystr;
	BOOL result = False;

	/* Check if sid is present in database */
	sid_to_string(keystr, sid);

	key.dptr = keystr;
	key.dsize = strlen(keystr) + 1;

	data = tdb_fetch(idmap_tdb, key);

	if (data.dptr) {
		fstring scanstr;
		int the_id;

		/* Parse and return existing uid */
		fstrcpy(scanstr, isgroup ? "GID" : "UID");
		fstrcat(scanstr, " %d");

		if (sscanf(data.dptr, scanstr, &the_id) == 1) {
			/* Store uid */
			if (id) {
				*id = the_id;
			}

			result = True;
		}

		SAFE_FREE(data.dptr);
	} else {

		/* Allocate a new id for this sid */
		if (id && tdb_allocate_id(id, isgroup)) {
			fstring keystr2;

			/* Store new id */
			slprintf(keystr2, sizeof(keystr2), "%s %d",
				 isgroup ? "GID" : "UID", *id);

			data.dptr = keystr2;
			data.dsize = strlen(keystr2) + 1;

			tdb_store(idmap_tdb, key, data, TDB_REPLACE);
			tdb_store(idmap_tdb, data, key, TDB_REPLACE);

			result = True;
		}
	}

	return result;
}

/*****************************************************************************
 Initialise idmap database. 
*****************************************************************************/
static BOOL tdb_idmap_init(void)
{
	/* Open tdb cache */
	if (!(idmap_tdb = tdb_open_log(lock_path("winbindd_idmap.tdb"), 0,
				       TDB_DEFAULT, O_RDWR | O_CREAT,
				       0600))) {
		DEBUG(0,
		      ("winbindd_idmap_init: Unable to open idmap database\n"));
		return False;
	}

	/* possibly convert from an earlier version */
	if (!tdb_idmap_convert(lock_path("winbindd_idmap.tdb"))) {
		DEBUG(0,
		      ("winbindd_idmap_init: Unable to open idmap database\n"));
		return False;
	}

	/* Create high water marks for group and user id */
	if (tdb_fetch_int32(idmap_tdb, HWM_USER) == -1) {
		if (tdb_store_int32
		    (idmap_tdb, HWM_USER, server_state.uid_low) == -1) {
			DEBUG(0,
			      ("winbindd_idmap_init: Unable to initialise user hwm in idmap database\n"));
			return False;
		}
	}

	if (tdb_fetch_int32(idmap_tdb, HWM_GROUP) == -1) {
		if (tdb_store_int32
		    (idmap_tdb, HWM_GROUP, server_state.gid_low) == -1) {
			DEBUG(0,
			      ("winbindd_idmap_init: Unable to initialise group hwm in idmap database\n"));
			return False;
		}
	}

	return True;
}

/* Get a sid from a uid */
static BOOL tdb_get_sid_from_uid(uid_t uid, DOM_SID * sid)
{
	return tdb_get_sid_from_id((int) uid, sid, False);
}

/* Get a sid from a gid */
static BOOL tdb_get_sid_from_gid(gid_t gid, DOM_SID * sid)
{
	return tdb_get_sid_from_id((int) gid, sid, True);
}

/* Get a uid from a sid */
static BOOL tdb_get_uid_from_sid(DOM_SID * sid, uid_t * uid)
{
	return tdb_get_id_from_sid(sid, uid, False);
}

/* Get a gid from a group sid */
static BOOL tdb_get_gid_from_sid(DOM_SID * sid, gid_t * gid)
{
	return tdb_get_id_from_sid(sid, gid, True);
}

/* Close the tdb */
static BOOL tdb_idmap_close(void)
{
	if (idmap_tdb)
		return (tdb_close(idmap_tdb) == 0);
	return True;
}


/* Dump status information to log file.  Display different stuff based on
   the debug level:

   Debug Level        Information Displayed
   =================================================================
   0                  Percentage of [ug]id range allocated
   0                  High water marks (next allocated ids)
*/

#define DUMP_INFO 0

static void tdb_idmap_status(void)
{
	int user_hwm, group_hwm;

	DEBUG(0, ("winbindd idmap status:\n"));

	/* Get current high water marks */

	if ((user_hwm = tdb_fetch_int32(idmap_tdb, HWM_USER)) == -1) {
		DEBUG(DUMP_INFO,
		      ("\tCould not get userid high water mark!\n"));
	}

	if ((group_hwm = tdb_fetch_int32(idmap_tdb, HWM_GROUP)) == -1) {
		DEBUG(DUMP_INFO,
		      ("\tCould not get groupid high water mark!\n"));
	}

	/* Display next ids to allocate */

	if (user_hwm != -1) {
		DEBUG(DUMP_INFO,
		      ("\tNext userid to allocate is %d\n", user_hwm));
	}

	if (group_hwm != -1) {
		DEBUG(DUMP_INFO,
		      ("\tNext groupid to allocate is %d\n", group_hwm));
	}

	/* Display percentage of id range already allocated. */

	if (user_hwm != -1) {
		int num_users = user_hwm - server_state.uid_low;
		int total_users =
		    server_state.uid_high - server_state.uid_low;

		DEBUG(DUMP_INFO,
		      ("\tUser id range is %d%% full (%d of %d)\n",
		       num_users * 100 / total_users, num_users,
		       total_users));
	}

	if (group_hwm != -1) {
		int num_groups = group_hwm - server_state.gid_low;
		int total_groups =
		    server_state.gid_high - server_state.gid_low;

		DEBUG(DUMP_INFO,
		      ("\tGroup id range is %d%% full (%d of %d)\n",
		       num_groups * 100 / total_groups, num_groups,
		       total_groups));
	}

	/* Display complete mapping of users and groups to rids */
}

struct idmap_methods tdb_idmap_methods = {
	tdb_idmap_init,

	tdb_get_sid_from_uid,
	tdb_get_sid_from_gid,

	tdb_get_uid_from_sid,
	tdb_get_gid_from_sid,

	tdb_idmap_close,

	tdb_idmap_status
};

BOOL winbind_idmap_reg_tdb(struct idmap_methods **meth)
{
	*meth = &tdb_idmap_methods;

	return True;
}
