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

struct samr_LookupDomain {
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

struct samr_SamEntry {
	uint32 rid;
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

struct samr_OPEN_DOMAIN {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_DOMAIN_INFO {
	struct {
	} in;

	struct {
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

struct samr_ENUM_DOM_GROUPS {
	struct {
	} in;

	struct {
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

struct samr_ENUM_DOM_USERS {
	struct {
	} in;

	struct {
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

struct samr_ENUM_DOM_ALIASES {
	struct {
	} in;

	struct {
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

struct samr_OPEN_GROUP {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct samr_QUERY_GROUPINFO {
	struct {
	} in;

	struct {
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

struct samr_OPEN_USER {
	struct {
	} in;

	struct {
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

struct samr_QUERY_USERINFO {
	struct {
	} in;

	struct {
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

struct samr_CONNECT4 {
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
#define DCERPC_SAMR_OPEN_DOMAIN 7
#define DCERPC_SAMR_QUERY_DOMAIN_INFO 8
#define DCERPC_SAMR_SET_DOMAIN_INFO 9
#define DCERPC_SAMR_CREATE_DOM_GROUP 10
#define DCERPC_SAMR_ENUM_DOM_GROUPS 11
#define DCERPC_SAMR_CREATE_USER_IN_DOMAIN 12
#define DCERPC_SAMR_ENUM_DOM_USERS 13
#define DCERPC_SAMR_CREATE_DOM_ALIAS 14
#define DCERPC_SAMR_ENUM_DOM_ALIASES 15
#define DCERPC_SAMR_GET_ALIAS_MEMBERSHIP 16
#define DCERPC_SAMR_LOOKUP_NAMES 17
#define DCERPC_SAMR_LOOKUP_RIDS 18
#define DCERPC_SAMR_OPEN_GROUP 19
#define DCERPC_SAMR_QUERY_GROUPINFO 20
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
#define DCERPC_SAMR_OPEN_USER 34
#define DCERPC_SAMR_DELETE_DOM_USER 35
#define DCERPC_SAMR_QUERY_USERINFO 36
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
