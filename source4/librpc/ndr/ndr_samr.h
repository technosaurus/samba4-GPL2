/* header auto-generated by pidl */

struct samr_Connect {
	struct {
		uint16 *system_name;
		uint32 access_mask;
	} in;

	struct {
		struct policy_handle *handle;
		NTSTATUS result;
	} out;

};

struct samr_Close {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_SetSecurity {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QuerySecurity {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_Shutdown {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_Name {
	uint16 name_len;
	uint16 name_size;
	const char *name;
};

struct samr_LookupDomain {
	struct {
		struct policy_handle *handle;
		struct samr_Name *domain;
	} in;

	struct {
		struct dom_sid2 *sid;
		NTSTATUS result;
	} out;

};

struct samr_SamEntry {
	uint32 idx;
	struct samr_Name name;
};

struct samr_SamArray {
	uint32 count;
	struct samr_SamEntry *entries;
};

struct samr_EnumDomains {
	struct {
		struct policy_handle *handle;
		uint32 *resume_handle;
		uint32 buf_size;
	} in;

	struct {
		uint32 *resume_handle;
		struct samr_SamArray *sam;
		uint32 *num_entries;
		NTSTATUS result;
	} out;

};

struct samr_OpenDomain {
	struct {
		struct policy_handle *handle;
		uint32 access_mask;
		struct dom_sid2 *sid;
	} in;

	struct {
		struct policy_handle *domain_handle;
		NTSTATUS result;
	} out;

};

struct samr_DomInfo1 {
	uint16 min_length_password;
	uint16 password_history;
	uint32 flag;
	NTTIME expire;
	NTTIME min_passwordage;
};

struct samr_DomInfo2 {
	HYPER_T force_logoff_time;
	struct samr_Name unknown1;
	struct samr_Name domain;
	struct samr_Name primary;
	HYPER_T sequence_num;
	uint32 unknown2;
	uint32 role;
	uint32 unknown3;
	uint32 num_users;
	uint32 num_groups;
	uint32 num_aliases;
};

struct samr_DomInfo3 {
	HYPER_T force_logoff_time;
};

struct samr_DomInfo4 {
	struct samr_Name unknown;
};

struct samr_DomInfo5 {
	struct samr_Name domain;
};

struct samr_DomInfo6 {
	struct samr_Name primary;
};

struct samr_DomInfo7 {
	uint32 role;
};

struct samr_DomInfo8 {
	HYPER_T sequence_num;
	NTTIME last_xxx_time;
};

struct samr_DomInfo9 {
	uint32 unknown;
};

struct samr_DomInfo11 {
	HYPER_T force_logoff_time;
	struct samr_Name unknown1;
	struct samr_Name domain;
	struct samr_Name primary;
	HYPER_T sequence_num;
	uint32 unknown2;
	uint32 role;
	uint32 unknown3;
	uint32 num_users;
	uint32 num_groups;
	uint32 num_aliases;
	HYPER_T lockout_duration;
	HYPER_T lockout_window;
	uint16 lockout_threshold;
};

struct samr_DomInfo12 {
	HYPER_T lockout_duration;
	HYPER_T lockout_window;
	uint16 lockout_threshold;
};

struct samr_DomInfo13 {
	HYPER_T sequence_num;
	NTTIME last_xxx_time;
	uint32 unknown1;
	uint32 unknown2;
};

union samr_DomainInfo {
/* [case(1)] */ struct samr_DomInfo1 info1;
/* [case(2)] */ struct samr_DomInfo2 info2;
/* [case(3)] */ struct samr_DomInfo3 info3;
/* [case(4)] */ struct samr_DomInfo4 info4;
/* [case(5)] */ struct samr_DomInfo5 info5;
/* [case(6)] */ struct samr_DomInfo6 info6;
/* [case(7)] */ struct samr_DomInfo7 info7;
/* [case(8)] */ struct samr_DomInfo8 info8;
/* [case(9)] */ struct samr_DomInfo9 info9;
/* [case(11)] */ struct samr_DomInfo11 info11;
/* [case(12)] */ struct samr_DomInfo12 info12;
/* [case(13)] */ struct samr_DomInfo13 info13;
};

struct samr_QueryDomainInfo {
	struct {
		struct policy_handle *handle;
		uint16 level;
	} in;

	struct {
		union samr_DomainInfo *info;
		NTSTATUS result;
	} out;

};

struct samr_SET_DOMAIN_INFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_CREATE_DOM_GROUP {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_EnumDomainGroups {
	struct {
		struct policy_handle *handle;
		uint32 *resume_handle;
		uint32 max_size;
	} in;

	struct {
		uint32 *resume_handle;
		struct samr_SamArray *sam;
		uint32 num_entries;
		NTSTATUS result;
	} out;

};

struct samr_CREATE_USER_IN_DOMAIN {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_EnumDomainUsers {
	struct {
		struct policy_handle *handle;
		uint32 *resume_handle;
		uint32 acct_flags;
		uint32 max_size;
	} in;

	struct {
		uint32 *resume_handle;
		struct samr_SamArray *sam;
		uint32 num_entries;
		NTSTATUS result;
	} out;

};

struct samr_CREATE_DOM_ALIAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_EnumDomainAliases {
	struct {
		struct policy_handle *handle;
		uint32 *resume_handle;
		uint32 max_size;
	} in;

	struct {
		uint32 *resume_handle;
		struct samr_SamArray *sam;
		uint32 num_entries;
		NTSTATUS result;
	} out;

};

struct samr_GET_ALIAS_MEMBERSHIP {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_LOOKUP_NAMES {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_LOOKUP_RIDS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_OpenGroup {
	struct {
		struct policy_handle *handle;
		uint32 access_mask;
		uint32 rid;
	} in;

	struct {
		struct policy_handle *acct_handle;
		NTSTATUS result;
	} out;

};

struct samr_GroupInfoAll {
	struct samr_Name name;
	uint32 unknown;
	uint32 members;
	struct samr_Name description;
};

struct samr_GroupInfoName {
	struct samr_Name Name;
};

struct samr_GroupInfoX {
	uint32 unknown;
};

struct samr_GroupInfoDesciption {
	struct samr_Name description;
};

union samr_GroupInfo {
/* [case(1)] */ struct samr_GroupInfoAll all;
/* [case(2)] */ struct samr_GroupInfoName name;
/* [case(3)] */ struct samr_GroupInfoX unknown;
/* [case(4)] */ struct samr_GroupInfoDesciption description;
};

struct samr_QueryGroupInfo {
	struct {
		struct policy_handle *handle;
		uint16 level;
	} in;

	struct {
		union samr_GroupInfo *info;
		NTSTATUS result;
	} out;

};

struct samr_SET_GROUPINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_ADD_GROUPMEM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_DELETE_DOM_GROUP {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_DEL_GROUPMEM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_GROUPMEM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_SET_MEMBER_ATTRIBUTES_OF_GROUP {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_OPEN_ALIAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_ALIASINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_SET_ALIASINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_DELETE_DOM_ALIAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_ADD_ALIASMEM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_DEL_ALIASMEM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_GET_MEMBERS_IN_ALIAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_OpenUser {
	struct {
		struct policy_handle *handle;
		uint32 access_mask;
		uint32 rid;
	} in;

	struct {
		struct policy_handle *acct_handle;
		NTSTATUS result;
	} out;

};

struct samr_DELETE_DOM_USER {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_UserInfo1 {
	struct samr_Name username;
	struct samr_Name full_name;
	uint32 primary_group_rid;
	struct samr_Name description;
	struct samr_Name comment;
};

struct samr_UserInfo2 {
	struct samr_Name comment;
	uint32 unknown1;
	uint32 unknown2;
	uint16 country_code;
	uint16 code_page;
};

struct samr_UserInfo3 {
	struct samr_Name username;
	struct samr_Name full_name;
	uint32 Rid;
	uint32 primary_group_rid;
	struct samr_Name home_directory;
	struct samr_Name home_drive;
	struct samr_Name logon_script;
	struct samr_Name profile;
	struct samr_Name workstations;
	NTTIME last_logon;
	NTTIME last_logoff;
	NTTIME last_pwd_change;
	NTTIME allow_pwd_change;
	NTTIME force_pwd_change;
	uint32 units_per_week;
	uint8 *logon_hours;
	uint16 bad_pwd_count;
	uint16 num_logons;
	uint32 acct_flags;
};

struct samr_UserInfo4 {
	uint32 units_per_week;
	uint8 *logon_hours;
};

struct samr_UserInfo5 {
	struct samr_Name username;
	struct samr_Name full_name;
	uint32 rid;
	uint32 primary_group_rid;
	struct samr_Name home_directory;
	struct samr_Name home_drive;
	struct samr_Name logon_script;
	struct samr_Name profile;
	struct samr_Name descriptiom;
	struct samr_Name workstations;
	NTTIME last_logon;
	NTTIME last_logoff;
	uint32 units_per_week;
	uint8 *logon_hours;
	uint32 unknown;
	NTTIME last_pwd_change;
	NTTIME acct_expiry;
	uint32 acct_flags;
};

struct samr_UserInfo6 {
	struct samr_Name username;
	struct samr_Name full_name;
};

struct samr_UserInfo7 {
	struct samr_Name username;
};

struct samr_UserInfo8 {
	struct samr_Name full_name;
};

struct samr_UserInfo9 {
	uint32 PrimaryGroupRid;
};

struct samr_UserInfo10 {
	struct samr_Name home_dir;
	struct samr_Name home_drive;
};

struct samr_UserInfo11 {
	struct samr_Name logon_script;
};

struct samr_UserInfo12 {
	struct samr_Name profile;
};

struct samr_UserInfo13 {
	struct samr_Name descriptiom;
};

struct samr_UserInfo14 {
	struct samr_Name workstations;
};

struct samr_UserInfo16 {
	uint32 acct_flags;
};

struct samr_UserInfo17 {
	NTTIME acct_expiry;
};

struct samr_UserInfo20 {
	struct samr_Name callback;
};

struct samr_UserInfo21 {
	NTTIME last_logon;
	NTTIME last_logoff;
	NTTIME last_pwd_change;
	NTTIME acct_expiry;
	NTTIME allow_pwd_change;
	NTTIME force_pwd_change;
	struct samr_Name username;
	struct samr_Name full_name;
	struct samr_Name home_dir;
	struct samr_Name home_drive;
	struct samr_Name logon_script;
	struct samr_Name profile;
	struct samr_Name description;
	struct samr_Name workstations;
	struct samr_Name comment;
	struct samr_Name callback;
	struct samr_Name unknown1;
	struct samr_Name unknown2;
	struct samr_Name unknown3;
	uint32 buf_count;
	uint8 *buffer;
	uint32 rid;
	uint32 primary_group_rid;
	uint32 acct_flags;
	uint32 fields_present;
	uint32 units_per_week;
	uint8 *logon_hours;
	uint16 bad_pwd_count;
	uint16 num_logons;
	uint16 country_code;
	uint16 code_page;
	uint8 nt_pwd_set;
	uint8 lm_pwd_set;
	uint8 expired_flag;
	uint8 unknown4;
};

union samr_UserInfo {
/* [case(1)] */ struct samr_UserInfo1 info1;
/* [case(2)] */ struct samr_UserInfo2 info2;
/* [case(3)] */ struct samr_UserInfo3 info3;
/* [case(4)] */ struct samr_UserInfo4 info4;
/* [case(5)] */ struct samr_UserInfo5 info5;
/* [case(6)] */ struct samr_UserInfo6 info6;
/* [case(7)] */ struct samr_UserInfo7 info7;
/* [case(8)] */ struct samr_UserInfo8 info8;
/* [case(9)] */ struct samr_UserInfo9 info9;
/* [case(10)] */ struct samr_UserInfo10 info10;
/* [case(11)] */ struct samr_UserInfo11 info11;
/* [case(12)] */ struct samr_UserInfo12 info12;
/* [case(13)] */ struct samr_UserInfo13 info13;
/* [case(14)] */ struct samr_UserInfo14 info14;
/* [case(16)] */ struct samr_UserInfo16 info16;
/* [case(17)] */ struct samr_UserInfo17 info17;
/* [case(20)] */ struct samr_UserInfo20 info20;
/* [case(21)] */ struct samr_UserInfo21 info21;
};

struct samr_QueryUserInfo {
	struct {
		struct policy_handle *handle;
		uint16 level;
	} in;

	struct {
		union samr_UserInfo *info;
		NTSTATUS result;
	} out;

};

struct samr_SET_USERINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_CHANGE_PASSWORD_USER {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_GET_GROUPS_FOR_USER {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_DISPINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_GET_DISPLAY_ENUMERATION_INDEX {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_TEST_PRIVATE_FUNCTIONS_DOMAIN {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_TEST_PRIVATE_FUNCTIONS_USER {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_GET_USRDOM_PWINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_REMOVE_MEMBER_FROM_FOREIGN_DOMAIN {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_INFORMATION_DOMAIN2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_INFORMATION_USER2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_DISPINFO2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_GET_DISPLAY_ENUMERATION_INDEX2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_CREATE_USER2_IN_DOMAIN {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_DISPINFO3 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_ADD_MULTIPLE_MEMBERS_TO_ALIAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_REMOVE_MULTIPLE_MEMBERS_FROM_ALIAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_OEM_CHANGE_PASSWORD_USER2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_UNICODE_CHANGE_PASSWORD_USER2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_GET_DOM_PWINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_CONNECT2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_SET_USERINFO2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_SET_BOOT_KEY_INFORMATION {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_GET_BOOT_KEY_INFORMATION {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_CONNECT3 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_Connect4 {
	struct {
		const char *system_name;
		uint32 unknown;
		uint32 access_mask;
	} in;

	struct {
		struct policy_handle *handle;
		NTSTATUS result;
	} out;

};

struct samr_UNICODE_CHANGE_PASSWORD_USER3 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_CONNECT5 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_RID_TO_SID {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_SET_DSRM_PASSWORD {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_VALIDATE_PASSWORD {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

#define DCERPC_SAMR_CONNECT 0
#define DCERPC_SAMR_CLOSE 1
#define DCERPC_SAMR_SETSECURITY 2
#define DCERPC_SAMR_QUERYSECURITY 3
#define DCERPC_SAMR_SHUTDOWN 4
#define DCERPC_SAMR_LOOKUPDOMAIN 5
#define DCERPC_SAMR_ENUMDOMAINS 6
#define DCERPC_SAMR_OPENDOMAIN 7
#define DCERPC_SAMR_QUERYDOMAININFO 8
#define DCERPC_SAMR_SET_DOMAIN_INFO 9
#define DCERPC_SAMR_CREATE_DOM_GROUP 10
#define DCERPC_SAMR_ENUMDOMAINGROUPS 11
#define DCERPC_SAMR_CREATE_USER_IN_DOMAIN 12
#define DCERPC_SAMR_ENUMDOMAINUSERS 13
#define DCERPC_SAMR_CREATE_DOM_ALIAS 14
#define DCERPC_SAMR_ENUMDOMAINALIASES 15
#define DCERPC_SAMR_GET_ALIAS_MEMBERSHIP 16
#define DCERPC_SAMR_LOOKUP_NAMES 17
#define DCERPC_SAMR_LOOKUP_RIDS 18
#define DCERPC_SAMR_OPENGROUP 19
#define DCERPC_SAMR_QUERYGROUPINFO 20
#define DCERPC_SAMR_SET_GROUPINFO 21
#define DCERPC_SAMR_ADD_GROUPMEM 22
#define DCERPC_SAMR_DELETE_DOM_GROUP 23
#define DCERPC_SAMR_DEL_GROUPMEM 24
#define DCERPC_SAMR_QUERY_GROUPMEM 25
#define DCERPC_SAMR_SET_MEMBER_ATTRIBUTES_OF_GROUP 26
#define DCERPC_SAMR_OPEN_ALIAS 27
#define DCERPC_SAMR_QUERY_ALIASINFO 28
#define DCERPC_SAMR_SET_ALIASINFO 29
#define DCERPC_SAMR_DELETE_DOM_ALIAS 30
#define DCERPC_SAMR_ADD_ALIASMEM 31
#define DCERPC_SAMR_DEL_ALIASMEM 32
#define DCERPC_SAMR_GET_MEMBERS_IN_ALIAS 33
#define DCERPC_SAMR_OPENUSER 34
#define DCERPC_SAMR_DELETE_DOM_USER 35
#define DCERPC_SAMR_QUERYUSERINFO 36
#define DCERPC_SAMR_SET_USERINFO 37
#define DCERPC_SAMR_CHANGE_PASSWORD_USER 38
#define DCERPC_SAMR_GET_GROUPS_FOR_USER 39
#define DCERPC_SAMR_QUERY_DISPINFO 40
#define DCERPC_SAMR_GET_DISPLAY_ENUMERATION_INDEX 41
#define DCERPC_SAMR_TEST_PRIVATE_FUNCTIONS_DOMAIN 42
#define DCERPC_SAMR_TEST_PRIVATE_FUNCTIONS_USER 43
#define DCERPC_SAMR_GET_USRDOM_PWINFO 44
#define DCERPC_SAMR_REMOVE_MEMBER_FROM_FOREIGN_DOMAIN 45
#define DCERPC_SAMR_QUERY_INFORMATION_DOMAIN2 46
#define DCERPC_SAMR_QUERY_INFORMATION_USER2 47
#define DCERPC_SAMR_QUERY_DISPINFO2 48
#define DCERPC_SAMR_GET_DISPLAY_ENUMERATION_INDEX2 49
#define DCERPC_SAMR_CREATE_USER2_IN_DOMAIN 50
#define DCERPC_SAMR_QUERY_DISPINFO3 51
#define DCERPC_SAMR_ADD_MULTIPLE_MEMBERS_TO_ALIAS 52
#define DCERPC_SAMR_REMOVE_MULTIPLE_MEMBERS_FROM_ALIAS 53
#define DCERPC_SAMR_OEM_CHANGE_PASSWORD_USER2 54
#define DCERPC_SAMR_UNICODE_CHANGE_PASSWORD_USER2 55
#define DCERPC_SAMR_GET_DOM_PWINFO 56
#define DCERPC_SAMR_CONNECT2 57
#define DCERPC_SAMR_SET_USERINFO2 58
#define DCERPC_SAMR_SET_BOOT_KEY_INFORMATION 59
#define DCERPC_SAMR_GET_BOOT_KEY_INFORMATION 60
#define DCERPC_SAMR_CONNECT3 61
#define DCERPC_SAMR_CONNECT4 62
#define DCERPC_SAMR_UNICODE_CHANGE_PASSWORD_USER3 63
#define DCERPC_SAMR_CONNECT5 64
#define DCERPC_SAMR_RID_TO_SID 65
#define DCERPC_SAMR_SET_DSRM_PASSWORD 66
#define DCERPC_SAMR_VALIDATE_PASSWORD 67
