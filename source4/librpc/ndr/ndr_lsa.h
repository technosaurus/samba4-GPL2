/* header auto-generated by pidl */

struct lsa_Close {
	struct {
		struct policy_handle *handle;
	} in;

	struct {
		struct policy_handle *handle;
		NTSTATUS result;
	} out;

};

struct lsa_Delete {
	struct {
		struct policy_handle *handle;
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_Name {
	uint16 name_len;
	uint16 name_size;
	const char *name;
};

struct lsa_PrivEntry {
	struct lsa_Name name;
	uint32 luid_low;
	uint32 luid_high;
};

struct lsa_PrivArray {
	uint32 count;
	struct lsa_PrivEntry *privs;
};

struct lsa_EnumPrivs {
	struct {
		struct policy_handle *handle;
		uint32 *resume_handle;
		uint32 max_count;
	} in;

	struct {
		uint32 *resume_handle;
		struct lsa_PrivArray *privs;
		NTSTATUS result;
	} out;

};

struct lsa_QuerySecObj {
	struct {
		struct policy_handle *handle;
		uint32 sec_info;
	} in;

	struct {
		struct sec_desc_buf *sd;
		NTSTATUS result;
	} out;

};

struct lsa_SetSecObj {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_ChangePassword {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_QosInfo {
	uint32 len;
	uint16 impersonation_level;
	uint8 context_mode;
	uint8 effective_only;
};

struct lsa_ObjectAttribute {
	uint32 len;
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

struct lsa_AuditLogInfo {
	uint32 percent_full;
	uint32 log_size;
	NTTIME retention_time;
	uint8 shutdown_in_progress;
	NTTIME time_to_shutdown;
	uint32 next_audit_record;
	uint32 unknown;
};

struct lsa_AuditSettings {
	uint32 count;
	uint32 *settings;
};

struct lsa_AuditEventsInfo {
	uint32 auditing_mode;
	struct lsa_AuditSettings *settings;
};

struct lsa_DomainInfo {
	struct lsa_Name name;
	struct dom_sid2 *sid;
};

struct lsa_PDAccountInfo {
	struct lsa_Name name;
};

struct lsa_ServerRole {
	uint16 role;
};

struct lsa_ReplicaSourceInfo {
	struct lsa_Name source;
	struct lsa_Name account;
};

struct lsa_DefaultQuotaInfo {
	uint32 paged_pool;
	uint32 non_paged_pool;
	uint32 min_wss;
	uint32 max_wss;
	uint32 pagefile;
	HYPER_T unknown;
};

struct lsa_ModificationInfo {
	HYPER_T modified_id;
	NTTIME db_create_time;
};

struct lsa_AuditFullSetInfo {
	uint32 shutdown_on_full;
};

struct lsa_AuditFullQueryInfo {
	uint32 shutdown_on_full;
	uint32 log_is_full;
};

struct lsa_DnsDomainInfo {
	struct lsa_Name name;
	struct lsa_Name dns_domain;
	struct lsa_Name dns_forest;
	struct GUID domain_guid;
	struct dom_sid2 *sid;
};

union lsa_PolicyInformation {
/* [case(1)] */ struct lsa_AuditLogInfo audit_log;
/* [case(2)] */ struct lsa_AuditEventsInfo audit_events;
/* [case(3)] */ struct lsa_DomainInfo domain;
/* [case(4)] */ struct lsa_PDAccountInfo pd;
/* [case(5)] */ struct lsa_DomainInfo account_domain;
/* [case(6)] */ struct lsa_ServerRole role;
/* [case(7)] */ struct lsa_ReplicaSourceInfo replica;
/* [case(8)] */ struct lsa_DefaultQuotaInfo quota;
/* [case(9)] */ struct lsa_ModificationInfo db;
/* [case(10)] */ struct lsa_AuditFullSetInfo auditfullset;
/* [case(11)] */ struct lsa_AuditFullQueryInfo auditfullquery;
/* [case(12)] */ struct lsa_DnsDomainInfo dns;
};

struct lsa_QueryInfoPolicy {
	struct {
		struct policy_handle *handle;
		uint16 level;
	} in;

	struct {
		union lsa_PolicyInformation *info;
		NTSTATUS result;
	} out;

};

struct lsa_SetInfoPolicy {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_ClearAuditLog {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_CreateAccount {
	struct {
	} in;

	struct {
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

struct lsa_EnumAccounts {
	struct {
		struct policy_handle *handle;
		uint32 *resume_handle;
		uint32 num_entries;
	} in;

	struct {
		uint32 *resume_handle;
		struct lsa_SidArray *sids;
		NTSTATUS result;
	} out;

};

struct lsa_CreateTrustDom {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_DomainInformation {
	struct lsa_Name name;
	struct dom_sid2 *sid;
};

struct lsa_DomainList {
	uint32 count;
	struct lsa_DomainInformation *domains;
};

struct lsa_EnumTrustDom {
	struct {
		struct policy_handle *handle;
		uint32 *resume_handle;
		uint32 num_entries;
	} in;

	struct {
		uint32 *resume_handle;
		struct lsa_DomainList *domains;
		NTSTATUS result;
	} out;

};

struct lsa_TranslatedSid {
	uint16 sid_type;
	uint32 rid;
	uint32 sid_index;
};

struct lsa_TransSidArray {
	uint32 count;
	struct lsa_TranslatedSid *sids;
};

struct lsa_TrustInformation {
	struct lsa_Name name;
	struct dom_sid2 *sid;
};

struct lsa_RefDomainList {
	uint32 count;
	struct lsa_TrustInformation *domains;
	uint32 max_count;
};

struct lsa_LookupNames {
	struct {
		struct policy_handle *handle;
		uint32 num_names;
		struct lsa_Name *names;
		struct lsa_TransSidArray *sids;
		uint16 level;
		uint32 *count;
	} in;

	struct {
		struct lsa_RefDomainList *domains;
		struct lsa_TransSidArray *sids;
		uint32 *count;
		NTSTATUS result;
	} out;

};

struct lsa_TranslatedName {
	uint16 sid_type;
	struct lsa_Name name;
	uint32 sid_index;
};

struct lsa_TransNameArray {
	uint32 count;
	struct lsa_TranslatedName *names;
};

struct lsa_LookupSids {
	struct {
		struct policy_handle *handle;
		struct lsa_SidArray *sids;
		struct lsa_TransNameArray *names;
		uint16 level;
		uint32 *count;
	} in;

	struct {
		struct lsa_RefDomainList *domains;
		struct lsa_TransNameArray *names;
		uint32 *count;
		NTSTATUS result;
	} out;

};

struct CREATESECRET {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_OpenAccount {
	struct {
		struct policy_handle *handle;
		struct dom_sid2 *sid;
		uint32 desired_access;
	} in;

	struct {
		struct policy_handle *acct_handle;
		NTSTATUS result;
	} out;

};

struct lsa_LUID {
	uint32 low;
	uint32 high;
};

struct lsa_LUIDAttribute {
	struct lsa_LUID luid;
	uint32 attribute;
};

struct lsa_PrivilegeSet {
	uint32 count;
	uint32 unknown;
	struct lsa_LUIDAttribute *set;
};

struct lsa_EnumPrivsAccount {
	struct {
		struct policy_handle *handle;
	} in;

	struct {
		struct lsa_PrivilegeSet *privs;
		NTSTATUS result;
	} out;

};

struct ADDPRIVS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct REMOVEPRIVS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct GETQUOTAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct SETQUOTAS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct GETSYSTEMACCOUNT {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct SETSYSTEMACCOUNT {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct OPENTRUSTDOM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct QUERYTRUSTDOM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct SETINFOTRUSTDOM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct OPENSECRET {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct SETSECRET {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct QUERYSECRET {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct LOOKUPPRIVVALUE {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_LookupPrivName {
	struct {
		struct policy_handle *handle;
		struct lsa_LUID *luid;
	} in;

	struct {
		struct lsa_Name *name;
		NTSTATUS result;
	} out;

};

struct PRIV_GET_DISPNAME {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct DELETEOBJECT {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct ENUMACCTWITHRIGHT {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct lsa_RightAttribute {
	const char *name;
};

struct lsa_RightSet {
	uint32 count;
	struct lsa_Name *names;
};

struct lsa_EnumAccountRights {
	struct {
		struct policy_handle *handle;
		struct dom_sid2 *sid;
	} in;

	struct {
		struct lsa_RightSet *rights;
		NTSTATUS result;
	} out;

};

struct ADDACCTRIGHTS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct REMOVEACCTRIGHTS {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct QUERYTRUSTDOMINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct SETTRUSTDOMINFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct DELETETRUSTDOM {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct STOREPRIVDATA {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct RETRPRIVDATA {
	struct {
	} in;

	struct {
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

struct UNK_GET_CONNUSER {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct QUERYINFO2 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

#define DCERPC_LSA_CLOSE 0
#define DCERPC_LSA_DELETE 1
#define DCERPC_LSA_ENUMPRIVS 2
#define DCERPC_LSA_QUERYSECOBJ 3
#define DCERPC_LSA_SETSECOBJ 4
#define DCERPC_LSA_CHANGEPASSWORD 5
#define DCERPC_LSA_OPENPOLICY 6
#define DCERPC_LSA_QUERYINFOPOLICY 7
#define DCERPC_LSA_SETINFOPOLICY 8
#define DCERPC_LSA_CLEARAUDITLOG 9
#define DCERPC_LSA_CREATEACCOUNT 10
#define DCERPC_LSA_ENUMACCOUNTS 11
#define DCERPC_LSA_CREATETRUSTDOM 12
#define DCERPC_LSA_ENUMTRUSTDOM 13
#define DCERPC_LSA_LOOKUPNAMES 14
#define DCERPC_LSA_LOOKUPSIDS 15
#define DCERPC_CREATESECRET 16
#define DCERPC_LSA_OPENACCOUNT 17
#define DCERPC_LSA_ENUMPRIVSACCOUNT 18
#define DCERPC_ADDPRIVS 19
#define DCERPC_REMOVEPRIVS 20
#define DCERPC_GETQUOTAS 21
#define DCERPC_SETQUOTAS 22
#define DCERPC_GETSYSTEMACCOUNT 23
#define DCERPC_SETSYSTEMACCOUNT 24
#define DCERPC_OPENTRUSTDOM 25
#define DCERPC_QUERYTRUSTDOM 26
#define DCERPC_SETINFOTRUSTDOM 27
#define DCERPC_OPENSECRET 28
#define DCERPC_SETSECRET 29
#define DCERPC_QUERYSECRET 30
#define DCERPC_LOOKUPPRIVVALUE 31
#define DCERPC_LSA_LOOKUPPRIVNAME 32
#define DCERPC_PRIV_GET_DISPNAME 33
#define DCERPC_DELETEOBJECT 34
#define DCERPC_ENUMACCTWITHRIGHT 35
#define DCERPC_LSA_ENUMACCOUNTRIGHTS 36
#define DCERPC_ADDACCTRIGHTS 37
#define DCERPC_REMOVEACCTRIGHTS 38
#define DCERPC_QUERYTRUSTDOMINFO 39
#define DCERPC_SETTRUSTDOMINFO 40
#define DCERPC_DELETETRUSTDOM 41
#define DCERPC_STOREPRIVDATA 42
#define DCERPC_RETRPRIVDATA 43
#define DCERPC_LSA_OPENPOLICY2 44
#define DCERPC_UNK_GET_CONNUSER 45
#define DCERPC_QUERYINFO2 46
