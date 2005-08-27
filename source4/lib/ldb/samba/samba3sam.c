/* 
   ldb database library - Samba3 SAM compatibility backend

   Copyright (C) Jelmer Vernooij 2005

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "includes.h"
#include "ldb/ldb_map/ldb_map.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_private.h"

/* FIXME: 
 * sambaSID -> member 
 * sambaSIDList -> member (special!) 
 * sambaDomainName -> name 
 * sambaTrustPassword 
 * sambaUnixIdPool 
 * sambaIdmapEntry 
 * sambaAccountPolicy 
 * sambaSidEntry 
 * sambaAcctFlags -> systemFlags ?
 * sambaPasswordHistory  -> ntPwdHistory*/

/* Not necessary:
 * sambaConfig
 * sambaShare
 * sambaConfigOption 
 * sambaNextGroupRid
 * sambaNextUserRid
 * sambaAlgorithmicRidBase
 */

/* Not in Samba4: 
 * sambaKickoffTime
 * sambaPwdCanChange
 * sambaPwdMustChange
 * sambaHomePath
 * sambaHomeDrive
 * sambaLogonScript
 * sambaProfilePath
 * sambaUserWorkstations
 * sambaMungedDial
 * sambaLogonHours */

static struct ldb_val convert_sid_rid(struct ldb_map_context *map, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	printf("Converting SID TO RID *\n");

	return ldb_val_dup(ctx, val);
}

static struct ldb_val convert_rid_sid(struct ldb_map_context *map, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	printf("Converting RID TO SID *\n");

	return ldb_val_dup(ctx, val);
}

static struct ldb_val convert_unix_id2name(struct ldb_map_context *map, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	printf("Converting UNIX ID to name\n");

	return ldb_val_dup(ctx, val);
}

static struct ldb_val convert_unix_name2id(struct ldb_map_context *map, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	printf("Converting UNIX name to ID\n");

	return ldb_val_dup(ctx, val);
}

const struct ldb_map_objectclass samba3_objectclasses[] = {
	{ "group", "sambaGroupMapping" },
	{ "user", "sambaSAMAccount" },
	{ "domain", "sambaDomain" },
	{ NULL, NULL }
};

const struct ldb_map_attribute samba3_attributes[] = 
{
	/* sambaNextRid -> nextRid */
	{
		.local_name = "nextRid",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaNextRid",
	},

	/* sambaBadPasswordTime -> badPasswordtime*/
	{
		.local_name = "badPasswordTime",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaBadPasswordTime",
	},

	/* sambaLMPassword -> lmPwdHash*/
	{
		.local_name = "lmPwdHash",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaLMPassword",
	},

	/* sambaGroupType -> groupType */
	{
		.local_name = "groupType",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaGroupType",
	},

	/* sambaNTPassword -> ntPwdHash*/
	{
		.local_name = "badPwdCount",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaNTPassword",
	},

	/* sambaPrimaryGroupSID -> primaryGroupID */
	{
		.local_name = "primaryGroupID",
		.type = MAP_CONVERT,
		.u.convert.remote_name = "sambaPrimaryGroupSID",
		.u.convert.convert_local = convert_rid_sid,
		.u.convert.convert_remote = convert_sid_rid, 
	},

	/* sambaBadPasswordCount -> badPwdCount */
	{
		.local_name = "badPwdCount",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaBadPasswordCount",
	},

	/* sambaLogonTime -> lastLogon*/
	{
		.local_name = "lastLogon",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaLogonTime",
	},

	/* sambaLogoffTime -> lastLogoff*/
	{
		.local_name = "lastLogoff",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaLogoffTime",
	},

	/* gidNumber -> unixName */
	{
		.local_name = "unixName",
		.type = MAP_CONVERT,
		.u.convert.remote_name = "gidNumber",
		.u.convert.convert_local = convert_unix_name2id,
		.u.convert.convert_remote = convert_unix_id2name, 
	},

	/* uid -> unixName */
	{
		.local_name = "unixName",
		.type = MAP_CONVERT,
		.u.convert.remote_name = "uid",
		.u.convert.convert_local = convert_unix_name2id,
		.u.convert.convert_remote = convert_unix_id2name,
	},

	/* displayName -> name */
	{
		.local_name = "name",
		.type = MAP_RENAME,
		.u.rename.remote_name = "displayName",
	},

	/* cn */
	{
		.local_name = "cn",
		.type = MAP_KEEP,
	},

	/* description */
	{
		.local_name = "description",
		.type = MAP_KEEP,
	},

	/* sambaSID -> objectSid*/
	{
		.local_name = "objectSid",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaSID", 
	},

	/* sambaPwdLastSet -> pwdLastSet */
	{
		.local_name = "pwdLastSet",
		.type = MAP_RENAME,
		.u.rename.remote_name = "sambaPwdLastSet",
	},	
	
	/* cn -> cn */
	{
		.local_name = "cn",
		.type = MAP_KEEP,
	},
	{
		.local_name = NULL,
	}
};

	/* the init function */
#ifdef HAVE_DLOPEN_DISABLED
struct ldb_module *init_module(struct ldb_context *ldb, const char *options[])
#else
struct ldb_module *ldb_samba3sam_module_init(struct ldb_context *ldb, const char *options[])
#endif
{
	return ldb_map_init(ldb, samba3_attributes, samba3_objectclasses, options);
}
