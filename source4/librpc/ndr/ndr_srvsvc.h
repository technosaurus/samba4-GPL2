/* header auto-generated by pidl */

#define DCERPC_SRVSVC_UUID "4b324fc8-1670-01d3-1278-5a47bf6ee188"
#define DCERPC_SRVSVC_VERSION 3.0
#define DCERPC_SRVSVC_NAME "srvsvc"

#define DCERPC_SRVSVC_00 0
#define DCERPC_SRVSVC_01 1
#define DCERPC_SRVSVC_02 2
#define DCERPC_SRVSVC_03 3
#define DCERPC_SRVSVC_04 4
#define DCERPC_SRVSVC_05 5
#define DCERPC_SRVSVC_06 6
#define DCERPC_SRVSVC_07 7
#define DCERPC_SRVSVC_NETCONNENUM 8
#define DCERPC_SRVSVC_NETFILEENUM 9
#define DCERPC_SRVSVC_0A 10
#define DCERPC_SRVSVC_NET_FILE_CLOSE 11
#define DCERPC_SRVSVC_NETSESSENUM 12
#define DCERPC_SRVSVC_0D 13
#define DCERPC_SRVSVC_NET_SHARE_ADD 14
#define DCERPC_SRVSVC_NETSHAREENUMALL 15
#define DCERPC_SRVSVC_NET_SHARE_GET_INFO 16
#define DCERPC_SRVSVC_NET_SHARE_SET_INFO 17
#define DCERPC_SRVSVC_NET_SHARE_DEL 18
#define DCERPC_SRVSVC_NET_SHARE_DEL_STICKY 19
#define DCERPC_SRVSVC_14 20
#define DCERPC_SRVSVC_NET_SRV_GET_INFO 21
#define DCERPC_SRVSVC_NET_SRV_SET_INFO 22
#define DCERPC_SRVSVC_NETDISKENUM 23
#define DCERPC_SRVSVC_18 24
#define DCERPC_SRVSVC_19 25
#define DCERPC_SRVSVC_NETTRANSPORTENUM 26
#define DCERPC_SRVSVC_1B 27
#define DCERPC_SRVSVC_NET_REMOTE_TOD 28
#define DCERPC_SRVSVC_1D 29
#define DCERPC_SRVSVC_1E 30
#define DCERPC_SRVSVC_1F 31
#define DCERPC_SRVSVC_20 32
#define DCERPC_SRVSVC_NET_NAME_VALIDATE 33
#define DCERPC_SRVSVC_22 34
#define DCERPC_SRVSVC_23 35
#define DCERPC_SRVSVC_NETSHAREENUM 36
#define DCERPC_SRVSVC_25 37
#define DCERPC_SRVSVC_26 38
#define DCERPC_SRVSVC_NET_FILE_QUERY_SECDESC 39
#define DCERPC_SRVSVC_NET_FILE_SET_SECDESC 40


struct srvsvc_00 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_01 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_02 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_03 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_04 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_05 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_06 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_07 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NetConn0 {
	uint32 conn_id;
};

struct srvsvc_NetConnCtr0 {
	uint32 count;
	struct srvsvc_NetConn0 *array;
};

struct srvsvc_NetConn1 {
	uint32 conn_id;
	uint32 conn_type;
	uint32 num_open;
	uint32 num_users;
	uint32 conn_time;
	const char *user;
	const char *client;
};

struct srvsvc_NetConnCtr1 {
	uint32 count;
	struct srvsvc_NetConn1 *array;
};

struct srvsvc_NetConnCtrDefault {
};

union srvsvc_NetConnSubCtr {
/* [case(0)] */ struct srvsvc_NetConnCtr0 *ctr0;
/* [case(1)] */ struct srvsvc_NetConnCtr1 *ctr1;
/* [case(default)] */ struct srvsvc_NetConnCtrDefault ctrDefault;
};

struct srvsvc_NetConnCtr {
	uint32 level;
	uint32 level2;
	union srvsvc_NetConnSubCtr subctr;
};

struct srvsvc_NetConnEnum {
	struct {
		const char *server_unc;
		const char *path;
		struct srvsvc_NetConnCtr ctr;
		uint32 preferred_len;
		uint32 *resume_handle;
	} in;

	struct {
		struct srvsvc_NetConnCtr ctr;
		uint32 total;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct srvsvc_NetFile2 {
	uint32 fid;
};

struct srvsvc_NetFileCtr2 {
	uint32 count;
	struct srvsvc_NetFile2 *array;
};

struct srvsvc_NetFile3 {
	uint32 fid;
	uint32 permissions;
	uint32 num_locks;
	const char *path;
	const char *user;
};

struct srvsvc_NetFileCtr3 {
	uint32 count;
	struct srvsvc_NetFile3 *array;
};

struct srvsvc_NetFileCtrDefault {
};

union srvsvc_NetFileSubCtr {
/* [case(2)] */ struct srvsvc_NetFileCtr2 *ctr2;
/* [case(3)] */ struct srvsvc_NetFileCtr3 *ctr3;
/* [case(default)] */ struct srvsvc_NetFileCtrDefault ctrDefault;
};

struct srvsvc_NetFileCtr {
	uint32 level;
	uint32 level2;
	union srvsvc_NetFileSubCtr subctr;
};

struct srvsvc_NetFileEnum {
	struct {
		const char *server_unc;
		const char *path;
		const char *user;
		struct srvsvc_NetFileCtr ctr;
		uint32 preferred_len;
		uint32 *resume_handle;
	} in;

	struct {
		struct srvsvc_NetFileCtr ctr;
		uint32 total;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct srvsvc_0a {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_FILE_CLOSE {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NetSess0 {
	const char *client;
};

struct srvsvc_NetSessCtr0 {
	uint32 count;
	struct srvsvc_NetSess0 *array;
};

struct srvsvc_NetSess1 {
	const char *client;
	const char *user;
	uint32 num_open;
	uint32 time;
	uint32 idle_time;
	uint32 user_flags;
};

struct srvsvc_NetSessCtr1 {
	uint32 count;
	struct srvsvc_NetSess1 *array;
};

struct srvsvc_NetSess2 {
	const char *client;
	const char *user;
	uint32 num_open;
	uint32 time;
	uint32 idle_time;
	uint32 user_flags;
	const char *client_type;
};

struct srvsvc_NetSessCtr2 {
	uint32 count;
	struct srvsvc_NetSess2 *array;
};

struct srvsvc_NetSess10 {
	const char *client;
	const char *user;
	uint32 time;
	uint32 idle_time;
};

struct srvsvc_NetSessCtr10 {
	uint32 count;
	struct srvsvc_NetSess10 *array;
};

struct srvsvc_NetSess502 {
	const char *client;
	const char *user;
	uint32 num_open;
	uint32 time;
	uint32 idle_time;
	uint32 user_flags;
	const char *client_type;
	const char *transport;
};

struct srvsvc_NetSessCtr502 {
	uint32 count;
	struct srvsvc_NetSess502 *array;
};

struct srvsvc_NetSessCtrDefault {
};

union srvsvc_NetSessSubCtr {
/* [case(0)] */ struct srvsvc_NetSessCtr0 *ctr0;
/* [case(1)] */ struct srvsvc_NetSessCtr1 *ctr1;
/* [case(2)] */ struct srvsvc_NetSessCtr2 *ctr2;
/* [case(10)] */ struct srvsvc_NetSessCtr10 *ctr10;
/* [case(502)] */ struct srvsvc_NetSessCtr502 *ctr502;
/* [case(default)] */ struct srvsvc_NetSessCtrDefault ctrDefault;
};

struct srvsvc_NetSessCtr {
	uint32 level;
	uint32 level2;
	union srvsvc_NetSessSubCtr subctr;
};

struct srvsvc_NetSessEnum {
	struct {
		const char *server_unc;
		const char *client;
		const char *user;
		struct srvsvc_NetSessCtr ctr;
		uint32 preferred_len;
		uint32 *resume_handle;
	} in;

	struct {
		struct srvsvc_NetSessCtr ctr;
		uint32 total;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct srvsvc_0d {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_SHARE_ADD {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NetShare0 {
	const char *name;
};

struct srvsvc_NetShareCtr0 {
	uint32 count;
	struct srvsvc_NetShare0 *array;
};

struct srvsvc_NetShare1 {
	const char *name;
	uint32 type;
	const char *comment;
};

struct srvsvc_NetShareCtr1 {
	uint32 count;
	struct srvsvc_NetShare1 *array;
};

struct srvsvc_NetShare2 {
	const char *name;
	uint32 type;
	const char *comment;
	uint32 permissions;
	uint32 max_users;
	uint32 current_users;
	const char *path;
	uint32 *password;
};

struct srvsvc_NetShareCtr2 {
	uint32 count;
	struct srvsvc_NetShare2 *array;
};

struct srvsvc_NetShare501 {
	const char *name;
	uint32 type;
	const char *comment;
	uint32 csc_policy;
};

struct srvsvc_NetShareCtr501 {
	uint32 count;
	struct srvsvc_NetShare501 *array;
};

struct srvsvc_NetShare502 {
	const char *name;
	uint32 type;
	const char *comment;
	uint32 permissions;
	uint32 max_users;
	uint32 current_users;
	const char *path;
	uint32 *password;
	uint32 unknown;
	struct security_descriptor *sd;
};

struct srvsvc_NetShareCtr502 {
	uint32 count;
	struct srvsvc_NetShare502 *array;
};

struct srvsvc_NetShareCtrDefault {
};

union srvsvc_NetShareSubCtr {
/* [case(0)] */ struct srvsvc_NetShareCtr0 *ctr0;
/* [case(1)] */ struct srvsvc_NetShareCtr1 *ctr1;
/* [case(2)] */ struct srvsvc_NetShareCtr2 *ctr2;
/* [case(501)] */ struct srvsvc_NetShareCtr501 *ctr501;
/* [case(502)] */ struct srvsvc_NetShareCtr502 *ctr502;
/* [case(default)] */ struct srvsvc_NetShareCtrDefault ctrDefault;
};

struct srvsvc_NetShareCtr {
	uint32 level;
	uint32 level2;
	union srvsvc_NetShareSubCtr subctr;
};

struct srvsvc_NetShareEnumAll {
	struct {
		const char *server_unc;
		struct srvsvc_NetShareCtr ctr;
		uint32 preferred_len;
		uint32 *resume_handle;
	} in;

	struct {
		struct srvsvc_NetShareCtr ctr;
		uint32 total;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct srvsvc_NET_SHARE_GET_INFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_SHARE_SET_INFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_SHARE_DEL {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_SHARE_DEL_STICKY {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_14 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_SRV_GET_INFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_SRV_SET_INFO {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NetDisk0 {
	uint32 unknown;
	uint32 size;
	uint8 *disk;
};

struct srvsvc_NetDiskCtr0 {
	uint32 count;
	uint32 unknown1;
	uint32 unknown2;
	struct srvsvc_NetDisk0 *array;
};

struct srvsvc_NetDisk1 {
	uint32 dummy;
};

struct srvsvc_NetDiskCtr1 {
	uint32 count;
	struct srvsvc_NetDisk1 *array;
};

struct srvsvc_NetDisk2 {
	uint32 dummy;
};

struct srvsvc_NetDiskCtr2 {
	uint32 count;
	struct srvsvc_NetDisk2 *array;
};

struct srvsvc_NetDisk3 {
	uint32 dummy;
};

struct srvsvc_NetDiskCtr3 {
	uint32 count;
	struct srvsvc_NetDisk3 *array;
};

struct srvsvc_NetDiskCtrDefault {
};

union srvsvc_NetDiskSubCtr {
/* [case(0)] */ struct srvsvc_NetDiskCtr0 ctr0;
/* [case(1)] */ struct srvsvc_NetDiskCtr1 ctr1;
/* [case(2)] */ struct srvsvc_NetDiskCtr2 ctr2;
/* [case(3)] */ struct srvsvc_NetDiskCtr3 ctr3;
/* [case(default)] */ struct srvsvc_NetDiskCtrDefault ctrDefault;
};

struct srvsvc_NetDiskCtr {
	uint32 num1;
	struct srvsvc_NetDiskCtr0 *ctr0;
};

struct srvsvc_NetDiskEnum {
	struct {
		const char *server_unc;
		uint32 level;
		uint32 unknown1;
		uint32 unknown2;
		uint32 preferred_len;
		uint32 *resume_handle;
	} in;

	struct {
		struct srvsvc_NetDiskCtr ctr;
		uint32 total;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct srvsvc_18 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_19 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_TransportAddress {
	uint32 count;
	uint8 *addr;
};

struct srvsvc_NetTransport0 {
	uint32 vcs;
	const char *name;
	struct srvsvc_TransportAddress *addr;
	uint32 addr_len;
	const char *net_addr;
};

struct srvsvc_NetTransportCtr0 {
	uint32 count;
	struct srvsvc_NetTransport0 *array;
};

struct srvsvc_NetTransport1 {
	uint32 vcs;
	const char *name;
	struct srvsvc_TransportAddress *addr;
	uint32 addr_len;
	const char *net_addr;
	const char *domain;
};

struct srvsvc_NetTransportCtr1 {
	uint32 count;
	struct srvsvc_NetTransport1 *array;
};

struct srvsvc_NetTransport2 {
	uint32 dummy;
};

struct srvsvc_NetTransportCtr2 {
	uint32 count;
	struct srvsvc_NetTransport2 *array;
};

struct srvsvc_NetTransportCtrDefault {
};

union srvsvc_NetTransportSubCtr {
/* [case(0)] */ struct srvsvc_NetTransportCtr0 *ctr0;
/* [case(1)] */ struct srvsvc_NetTransportCtr1 *ctr1;
/* [case(2)] */ struct srvsvc_NetTransportCtr2 *ctr2;
/* [case(default)] */ struct srvsvc_NetTransportCtrDefault ctrDefault;
};

struct srvsvc_NetTransportCtr {
	uint32 level;
	uint32 level2;
	union srvsvc_NetTransportSubCtr subctr;
};

struct srvsvc_NetTransportEnum {
	struct {
		const char *server_unc;
		struct srvsvc_NetTransportCtr ctr;
		uint32 preferred_len;
		uint32 *resume_handle;
	} in;

	struct {
		struct srvsvc_NetTransportCtr ctr;
		uint32 total;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct srvsvc_1b {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_REMOTE_TOD {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_1d {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_1e {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_1f {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_20 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_NAME_VALIDATE {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_22 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_23 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NetShareEnum {
	struct {
		const char *server_unc;
		struct srvsvc_NetShareCtr ctr;
		uint32 preferred_len;
		uint32 *resume_handle;
	} in;

	struct {
		struct srvsvc_NetShareCtr ctr;
		uint32 total;
		uint32 *resume_handle;
		WERROR result;
	} out;

};

struct srvsvc_25 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_26 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_FILE_QUERY_SECDESC {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct srvsvc_NET_FILE_SET_SECDESC {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

