/* header auto-generated by pidl */

#define DCERPC_WKSSVC_UUID "6bffd098-a112-3610-9833-46c3f87e345a"
#define DCERPC_WKSSVC_VERSION 1.0
#define DCERPC_WKSSVC_NAME "wkssvc"

#define DCERPC_WKS_QUERYINFO 0
#define DCERPC_WKS_SETINFO 1
#define DCERPC_WKS_NETRWKSTAUSERENUM 2
#define DCERPC_WKS_NETRWKSTAUSERGETINFO 3
#define DCERPC_WKS_NETRWKSTAUSERSETINFO 4
#define DCERPC_WKS_TRANSPORTENUM 5
#define DCERPC_WKS_NETRWKSTATRANSPORTADD 6
#define DCERPC_WKS_NETRWKSTATRANSPORTDEL 7
#define DCERPC_WKS_NETRUSEADD 8
#define DCERPC_WKS_NETRUSEGETINFO 9
#define DCERPC_WKS_NETRUSEDEL 10
#define DCERPC_WKS_NETRUSEENUM 11
#define DCERPC_WKS_NETRMESSAGEBUFFERSEND 12
#define DCERPC_WKS_NETRWORKSTATIONSTATISTICSGET 13
#define DCERPC_WKS_NETRLOGONDOMAINNAMEADD 14
#define DCERPC_WKS_NETRLOGONDOMAINNAMEDEL 15
#define DCERPC_WKS_NETRJOINDOMAIN 16
#define DCERPC_WKS_NETRUNJOINDOMAIN 17
#define DCERPC_WKS_NETRRENAMEMACHINEINDOMAIN 18
#define DCERPC_WKS_NETRVALIDATENAME 19
#define DCERPC_WKS_NETRGETJOININFORMATION 20
#define DCERPC_WKS_NETRGETJOINABLEOUS 21
#define DCERPC_WKS_NETRJOINDOMAIN2 22
#define DCERPC_WKS_NETRUNJOINDOMAIN2 23
#define DCERPC_WKS_NETRRENAMEMACHINEINDOMAIN2 24
#define DCERPC_WKS_NETRVALIDATENAME2 25
#define DCERPC_WKS_NETRGETJOINABLEOUS2 26
#define DCERPC_WKS_NETRADDALTERNATECOMPUTERNAME 27
#define DCERPC_WKS_NETRREMOVEALTERNATECOMPUTERNAME 28
#define DCERPC_WKS_NETRSETPRIMARYCOMPUTERNAME 29
#define DCERPC_WKS_NETRENUMERATECOMPUTERNAMES 30


struct wks_Info100 {
	uint32 platform_id;
	const char *server;
	const char *domain;
	uint32 ver_major;
	uint32 ver_minor;
};

struct wks_Info101 {
	uint32 platform_id;
	const char *server;
	const char *domain;
	uint32 ver_major;
	uint32 ver_minor;
	const char *lan_root;
};

struct wks_Info102 {
	uint32 platform_id;
	const char *server;
	const char *domain;
	uint32 ver_major;
	uint32 ver_minor;
	const char *lan_root;
	uint32 logged_on_users;
};

union wks_Info {
/* [case(100)] */ struct wks_Info100 *info100;
/* [case(101)] */ struct wks_Info101 *info101;
/* [case(102)] */ struct wks_Info102 *info102;
};

struct wks_QueryInfo {
	struct {
		const char *server_name;
		uint32 level;
	} in;

	struct {
		union wks_Info info;
		WERROR result;
	} out;

};

struct wks_SetInfo {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRWKSTAUSERENUM {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRWKSTAUSERGETINFO {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRWKSTAUSERSETINFO {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct wks_TransportInfo0 {
	uint32 quality_of_service;
	uint32 vc_count;
	const char *name;
	const char *address;
	uint32 wan_link;
};

struct wks_TransportInfoArray {
	uint32 count;
	struct wks_TransportInfo0 *transports;
};

union wks_TransportUnion {
/* [case(0)] */ struct wks_TransportInfoArray *array;
};

struct wks_TransportInfo {
	uint32 level;
	union wks_TransportUnion u;
};

struct wks_TransportEnum {
	struct {
		const char *server_name;
		struct wks_TransportInfo *info;
		uint32 max_buffer;
		uint32 *resume_handle;
	} in;

	struct {
		struct wks_TransportInfo *info;
		uint32 unknown;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct WKS_NETRWKSTATRANSPORTADD {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRWKSTATRANSPORTDEL {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRUSEADD {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRUSEGETINFO {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRUSEDEL {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRUSEENUM {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRMESSAGEBUFFERSEND {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRWORKSTATIONSTATISTICSGET {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRLOGONDOMAINNAMEADD {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRLOGONDOMAINNAMEDEL {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRJOINDOMAIN {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRUNJOINDOMAIN {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRRENAMEMACHINEINDOMAIN {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRVALIDATENAME {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRGETJOININFORMATION {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRGETJOINABLEOUS {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRJOINDOMAIN2 {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRUNJOINDOMAIN2 {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRRENAMEMACHINEINDOMAIN2 {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRVALIDATENAME2 {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRGETJOINABLEOUS2 {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRADDALTERNATECOMPUTERNAME {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRREMOVEALTERNATECOMPUTERNAME {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRSETPRIMARYCOMPUTERNAME {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

struct WKS_NETRENUMERATECOMPUTERNAMES {
	struct {
	} in;

	struct {
		WERROR result;
	} out;

};

