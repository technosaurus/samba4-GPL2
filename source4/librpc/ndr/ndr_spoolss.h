/* header auto-generated by pidl */

struct spoolss_DeviceMode {
	const char * devicename;
	uint16 specversion;
	uint16 driverversion;
	uint16 size;
	uint16 driverextra;
	uint32 fields;
	uint16 orientation;
	uint16 papersize;
	uint16 paperlength;
	uint16 paperwidth;
	uint16 scale;
	uint16 copies;
	uint16 defaultsource;
	uint16 printquality;
	uint16 color;
	uint16 duplex;
	uint16 yresolution;
	uint16 ttoption;
	uint16 collate;
	const char * formname;
	uint16 logpixels;
	uint32 bitsperpel;
	uint32 pelswidth;
	uint32 pelsheight;
	uint32 displayflags;
	uint32 displayfrequency;
	uint32 icmmethod;
	uint32 icmintent;
	uint32 mediatype;
	uint32 dithertype;
	uint32 reserved1;
	uint32 reserved2;
	uint32 panningwidth;
	uint32 panningheight;
};

struct spoolss_PrinterEnum1 {
	uint32 flags;
	const char * name;
	const char * description;
	const char * comment;
};

struct spoolss_PrinterEnum2 {
	const char * servername;
	const char * printername;
	const char * sharename;
	const char * portname;
	const char * drivername;
	const char * comment;
	const char * location;
	struct spoolss_DeviceMode *devmode;
	const char * sepfile;
	const char * printprocessor;
	const char * datatype;
	const char * parameters;
	struct security_descriptor *secdesc;
	uint32 attributes;
	uint32 priority;
	uint32 defaultpriority;
	uint32 starttime;
	uint32 untiltime;
	uint32 status;
	uint32 cjobs;
	uint32 averageppm;
};

union spoolss_PrinterEnum {
/* [case(1)] */ struct spoolss_PrinterEnum1 info1;
/* [case(2)] */ struct spoolss_PrinterEnum2 info2;
};

struct spoolss_EnumPrinters {
	struct {
		uint32 flags;
		const char *server;
		uint32 level;
		DATA_BLOB *buffer;
		uint32 *buf_size;
	} in;

	struct {
		DATA_BLOB *buffer;
		uint32 *buf_size;
		uint32 count;
		NTSTATUS result;
	} out;

};

struct spoolss_01 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_02 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_03 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_EnumJobs {
	struct {
		struct policy_handle *handle;
		uint32 firstjob;
		uint32 numjobs;
		uint32 level;
		struct uint8_buf *buffer;
		uint32 offered;
	} in;

	struct {
		struct uint8_buf *buffer;
		uint32 needed;
		uint32 numjobs;
		NTSTATUS result;
	} out;

};

struct spoolss_05 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_06 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_07 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_GetPrinter {
	struct {
		struct policy_handle *handle;
		uint32 level;
		struct uint8_buf *buffer;
		uint32 offered;
	} in;

	struct {
		struct uint8_buf *buffer;
		uint32 needed;
		uint32 returned;
		NTSTATUS result;
	} out;

};

struct spoolss_09 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_0a {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_0b {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_0c {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_0d {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_0e {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_0f {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_10 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_11 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_StartPagePrinter {
	struct {
		struct policy_handle *handle;
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_13 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_EndPagePrinter {
	struct {
		struct policy_handle *handle;
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_15 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_16 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_EndDocPrinter {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_18 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_19 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_1a {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_1b {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_1c {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_ClosePrinter {
	struct {
		struct policy_handle *handle;
	} in;

	struct {
		struct policy_handle *handle;
		NTSTATUS result;
	} out;

};

struct spoolss_1e {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_1f {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_20 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_21 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_22 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_23 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_24 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_25 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_26 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_27 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_28 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_29 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_2a {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_2b {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_2c {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_2d {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_2e {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_2f {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_30 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_31 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_32 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_33 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_34 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_35 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_36 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_37 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_38 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_39 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_3a {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_3b {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_3c {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_3d {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_3e {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_3f {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_40 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_41 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_42 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_43 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_44 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_Devmode {
	uint32 foo;
};

struct spoolss_DevmodeContainer {
	uint32 size;
	struct spoolss_Devmode *devmode;
};

struct spoolss_UserLevel1 {
	uint32 size;
	const char *client;
	const char *user;
	uint32 build;
	uint32 major;
	uint32 minor;
	uint32 processor;
};

union spoolss_UserLevel {
/* [case(1)] */ struct spoolss_UserLevel1 *level1;
};

struct spoolss_OpenPrinterEx {
	struct {
		const char *printername;
		const char *datatype;
		struct spoolss_DevmodeContainer devmode_ctr;
		uint32 access_required;
		uint32 level;
		union spoolss_UserLevel userlevel;
	} in;

	struct {
		struct policy_handle *handle;
		NTSTATUS result;
	} out;

};

struct spoolss_46 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_47 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_EnumPrinterData {
	struct {
		struct policy_handle *handle;
		uint32 enum_index;
		uint32 value_offered;
		uint32 data_offered;
	} in;

	struct {
		uint32 value_len;
		const char *value_name;
		uint32 value_needed;
		uint32 printerdata_type;
		struct uint8_buf printerdata;
		uint32 data_needed;
		NTSTATUS result;
	} out;

};

struct spoolss_49 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_4a {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_4b {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_4c {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_4d {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_4e {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_4f {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_50 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_51 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_52 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_53 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_54 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_55 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_56 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_57 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_58 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_59 {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_5a {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_5b {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_5c {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_5d {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_5e {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

struct spoolss_5f {
	struct {
	} in;

	struct {
		NTSTATUS result;
	} out;

};

#define DCERPC_SPOOLSS_ENUMPRINTERS 0
#define DCERPC_SPOOLSS_01 1
#define DCERPC_SPOOLSS_02 2
#define DCERPC_SPOOLSS_03 3
#define DCERPC_SPOOLSS_ENUMJOBS 4
#define DCERPC_SPOOLSS_05 5
#define DCERPC_SPOOLSS_06 6
#define DCERPC_SPOOLSS_07 7
#define DCERPC_SPOOLSS_GETPRINTER 8
#define DCERPC_SPOOLSS_09 9
#define DCERPC_SPOOLSS_0A 10
#define DCERPC_SPOOLSS_0B 11
#define DCERPC_SPOOLSS_0C 12
#define DCERPC_SPOOLSS_0D 13
#define DCERPC_SPOOLSS_0E 14
#define DCERPC_SPOOLSS_0F 15
#define DCERPC_SPOOLSS_10 16
#define DCERPC_SPOOLSS_11 17
#define DCERPC_SPOOLSS_STARTPAGEPRINTER 18
#define DCERPC_SPOOLSS_13 19
#define DCERPC_SPOOLSS_ENDPAGEPRINTER 20
#define DCERPC_SPOOLSS_15 21
#define DCERPC_SPOOLSS_16 22
#define DCERPC_SPOOLSS_ENDDOCPRINTER 23
#define DCERPC_SPOOLSS_18 24
#define DCERPC_SPOOLSS_19 25
#define DCERPC_SPOOLSS_1A 26
#define DCERPC_SPOOLSS_1B 27
#define DCERPC_SPOOLSS_1C 28
#define DCERPC_SPOOLSS_CLOSEPRINTER 29
#define DCERPC_SPOOLSS_1E 30
#define DCERPC_SPOOLSS_1F 31
#define DCERPC_SPOOLSS_20 32
#define DCERPC_SPOOLSS_21 33
#define DCERPC_SPOOLSS_22 34
#define DCERPC_SPOOLSS_23 35
#define DCERPC_SPOOLSS_24 36
#define DCERPC_SPOOLSS_25 37
#define DCERPC_SPOOLSS_26 38
#define DCERPC_SPOOLSS_27 39
#define DCERPC_SPOOLSS_28 40
#define DCERPC_SPOOLSS_29 41
#define DCERPC_SPOOLSS_2A 42
#define DCERPC_SPOOLSS_2B 43
#define DCERPC_SPOOLSS_2C 44
#define DCERPC_SPOOLSS_2D 45
#define DCERPC_SPOOLSS_2E 46
#define DCERPC_SPOOLSS_2F 47
#define DCERPC_SPOOLSS_30 48
#define DCERPC_SPOOLSS_31 49
#define DCERPC_SPOOLSS_32 50
#define DCERPC_SPOOLSS_33 51
#define DCERPC_SPOOLSS_34 52
#define DCERPC_SPOOLSS_35 53
#define DCERPC_SPOOLSS_36 54
#define DCERPC_SPOOLSS_37 55
#define DCERPC_SPOOLSS_38 56
#define DCERPC_SPOOLSS_39 57
#define DCERPC_SPOOLSS_3A 58
#define DCERPC_SPOOLSS_3B 59
#define DCERPC_SPOOLSS_3C 60
#define DCERPC_SPOOLSS_3D 61
#define DCERPC_SPOOLSS_3E 62
#define DCERPC_SPOOLSS_3F 63
#define DCERPC_SPOOLSS_40 64
#define DCERPC_SPOOLSS_41 65
#define DCERPC_SPOOLSS_42 66
#define DCERPC_SPOOLSS_43 67
#define DCERPC_SPOOLSS_44 68
#define DCERPC_SPOOLSS_OPENPRINTEREX 69
#define DCERPC_SPOOLSS_46 70
#define DCERPC_SPOOLSS_47 71
#define DCERPC_SPOOLSS_ENUMPRINTERDATA 72
#define DCERPC_SPOOLSS_49 73
#define DCERPC_SPOOLSS_4A 74
#define DCERPC_SPOOLSS_4B 75
#define DCERPC_SPOOLSS_4C 76
#define DCERPC_SPOOLSS_4D 77
#define DCERPC_SPOOLSS_4E 78
#define DCERPC_SPOOLSS_4F 79
#define DCERPC_SPOOLSS_50 80
#define DCERPC_SPOOLSS_51 81
#define DCERPC_SPOOLSS_52 82
#define DCERPC_SPOOLSS_53 83
#define DCERPC_SPOOLSS_54 84
#define DCERPC_SPOOLSS_55 85
#define DCERPC_SPOOLSS_56 86
#define DCERPC_SPOOLSS_57 87
#define DCERPC_SPOOLSS_58 88
#define DCERPC_SPOOLSS_59 89
#define DCERPC_SPOOLSS_5A 90
#define DCERPC_SPOOLSS_5B 91
#define DCERPC_SPOOLSS_5C 92
#define DCERPC_SPOOLSS_5D 93
#define DCERPC_SPOOLSS_5E 94
#define DCERPC_SPOOLSS_5F 95
