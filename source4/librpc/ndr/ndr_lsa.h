/* 
   Unix SMB/CIFS implementation.

   definitions for marshalling/unmarshalling the lsa pipe

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

/* header auto-generated by pidl */

struct lsa_QosInfo {
	uint16 impersonation_level;
	uint8 context_mode;
	uint8 effective_only;
};

struct lsa_ObjectAttribute {
	uint8 *root_dir;
	const char *object_name;
	uint32 attributes;
	struct security_descriptor *sec_desc;
	struct lsa_QosInfo *sec_qos;
};

struct lsa_OpenPolicy {
	struct {
		uint16 *system_name;
		struct lsa_ObjectAttribute *attr;
		uint32 desired_access;
	} in;

	struct {
		struct policy_handle *handle;
		NTSTATUS result;
	} out;

};

struct lsa_OpenPolicy2 {
	struct {
		const char *system_name;
		struct lsa_ObjectAttribute *attr;
		uint32 desired_access;
	} in;

	struct {
		struct policy_handle *handle;
		NTSTATUS result;
	} out;

};

struct lsa_SidPtr {
	struct dom_sid2 *sid;
};

struct lsa_SidArray {
	uint32 num_sids;
	struct lsa_SidPtr *sids;
};

struct lsa_EnumSids {
	struct {
		struct policy_handle *handle;
		uint32 resume_handle;
		uint32 num_entries;
	} in;

	struct {
		uint32 resume_handle;
		struct lsa_SidArray *sids;
		NTSTATUS result;
	} out;

};


