/* header auto-generated by pidl */

struct dfs_Exist {
	struct {
	} in;

	struct {
		uint32 *exist_flag;
	} out;

};

struct dfs_Add {
	struct {
		const char *path;
		const char *server;
		const char *share;
		const char *comment;
		uint32 flags;
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct dfs_Remove {
	struct {
		const char *path;
		const char *server;
		const char *share;
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct dfs_SetInfo {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct dfs_Info1 {
	const char *path;
};

struct dfs_Info2 {
	const char *path;
	const char *comment;
	uint32 state;
	uint32 num_stores;
};

struct dfs_StorageInfo {
	uint32 state;
	const char *server;
	const char *share;
};

struct dfs_Info3 {
	const char *path;
	const char *comment;
	uint32 state;
	uint32 num_stores;
	struct dfs_StorageInfo *stores;
};

struct dfs_Info4 {
	const char *path;
	const char *comment;
	uint32 state;
	uint32 timeout;
	struct GUID guid;
	uint32 num_stores;
	struct dfs_StorageInfo *stores;
};

struct dfs_Info100 {
	const char *comment;
};

struct dfs_Info101 {
	uint32 state;
};

struct dfs_Info102 {
	uint32 timeout;
};

struct dfs_Info200 {
	const char *dom_root;
};

struct dfs_Info300 {
	uint32 flags;
	const char *dom_root;
};

union dfs_Info {
/* [case(1)] */ struct dfs_Info1 *info1;
/* [case(2)] */ struct dfs_Info2 *info2;
/* [case(3)] */ struct dfs_Info3 *info3;
/* [case(4)] */ struct dfs_Info4 *info4;
/* [case(100)] */ struct dfs_Info100 *info100;
/* [case(101)] */ struct dfs_Info101 *info101;
/* [case(102)] */ struct dfs_Info102 *info102;
/* [case(200)] */ struct dfs_Info200 *info200;
/* [case(300)] */ struct dfs_Info300 *info300;
};

struct dfs_GetInfo {
	struct {
		const char *path;
		const char *server;
		const char *share;
		uint32 level;
	} in;

	struct {
		union dfs_Info info;
		NTSTATUS result;
	} out;

};

struct dfs_EnumArray1 {
	uint32 count;
	struct dfs_Info1 *s;
};

struct dfs_EnumArray2 {
	uint32 count;
	struct dfs_Info2 *s;
};

struct dfs_EnumArray3 {
	uint32 count;
	struct dfs_Info3 *s;
};

struct dfs_EnumArray4 {
	uint32 count;
	struct dfs_Info4 *s;
};

struct dfs_EnumArray200 {
	uint32 count;
	struct dfs_Info200 *s;
};

struct dfs_EnumArray300 {
	uint32 count;
	struct dfs_Info300 *s;
};

union dfs_EnumInfo {
/* [case(1)] */ struct dfs_EnumArray1 *info1;
/* [case(2)] */ struct dfs_EnumArray2 *info2;
/* [case(3)] */ struct dfs_EnumArray3 *info3;
/* [case(4)] */ struct dfs_EnumArray4 *info4;
/* [case(200)] */ struct dfs_EnumArray200 *info200;
/* [case(300)] */ struct dfs_EnumArray300 *info300;
};

struct dfs_EnumStruct {
	uint32 level;
	union dfs_EnumInfo e;
};

struct dfs_Enum {
	struct {
		uint32 level;
		uint32 bufsize;
		struct dfs_EnumStruct *info;
		uint32 *unknown;
		uint32 *total;
	} in;

	struct {
		struct dfs_EnumStruct *info;
		uint32 *total;
		NTSTATUS result;
	} out;

};

#define DCERPC_DFS_EXIST 0
#define DCERPC_DFS_ADD 1
#define DCERPC_DFS_REMOVE 2
#define DCERPC_DFS_SETINFO 3
#define DCERPC_DFS_GETINFO 4
#define DCERPC_DFS_ENUM 5
