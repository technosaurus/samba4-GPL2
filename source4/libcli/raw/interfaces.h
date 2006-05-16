/* 
   Unix SMB/CIFS implementation.
   SMB request interface structures
   Copyright (C) Andrew Tridgell			2003
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   
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

#ifndef __LIBCLI_RAW_INTERFACES_H__
#define __LIBCLI_RAW_INTERFACES_H__

#include "smb.h" 

/* this structure is just a wrapper for a string, the only reason we
   bother with this is that it allows us to check the length provided
   on the wire in testsuite test code to ensure that we are
   terminating names in the same way that win2003 is. The *ONLY* time
   you should ever look at the 'private_length' field in this
   structure is inside compliance test code, in all other cases just
   use the null terminated char* as the definitive definition of the
   string

   also note that this structure is only used in packets where there
   is an explicit length provided on the wire (hence the name). That
   length is placed in 'private_length'. For packets where the length
   is always determined by NULL or packet termination a normal char*
   is used in the structure definition.
 */
struct smb_wire_string {
	uint32_t private_length;
	const char *s;
};

/*
 * SMB2 uses a 16Byte handle,
 * (we can maybe use struct GUID later)
 */
struct smb2_handle {
	uint64_t data[2];
};

struct ntvfs_handle;

/*
 * a generic container for file handles or file pathes
 * for qfileinfo/setfileinfo and qpathinfo/setpathinfo
*/
union smb_handle_or_path {
	/*
	 * this is used for
	 * the qpathinfo and setpathinfo
	 * calls
	 */
	const char *path;
	/*
	 * this is used as file handle in SMB
	 */
	uint16_t fnum;

	/*
	 * this is used as file handle in SMB2
	 */
	struct smb2_handle handle;

	/*
	 * this is used as generic file handle for the NTVFS layer
	 */
	struct ntvfs_handle *ntvfs;
};

/*
 a generic container for file handles
*/
union smb_handle {
	/*
	 * this is used as file handle in SMB
	 */
	uint16_t fnum;

	/*
	 * this is used as file handle in SMB2
	 */
	struct smb2_handle handle;

	/*
	 * this is used as generic file handle for the NTVFS layer
	 */
	struct ntvfs_handle *ntvfs;
};

/*
  this header defines the structures and unions used between the SMB
  parser and the backends.
*/

/* struct used for SMBlseek call */
union smb_seek {
	struct {
		struct {
			union smb_handle file;
			uint16_t mode;
			int32_t  offset; /* signed */
		} in;
		struct {
			int32_t offset;
		} out;
	} lseek, generic;
};

/* struct used in unlink() call */
union smb_unlink {
	struct {
		struct {
			const char *pattern;
			uint16_t attrib;
		} in;
	} unlink;
};


/* struct used in chkpath() call */
union smb_chkpath {
	struct {
		struct {
			const char *path;
		} in;
	} chkpath;
};

enum smb_mkdir_level {RAW_MKDIR_GENERIC, RAW_MKDIR_MKDIR, RAW_MKDIR_T2MKDIR};

/* union used in mkdir() call */
union smb_mkdir {
	/* generic level */
	struct {
		enum smb_mkdir_level level;
	} generic;

	struct {
		enum smb_mkdir_level level;
		struct {
			const char *path;
		} in;
	} mkdir;

	struct {
		enum smb_mkdir_level level;
		struct {
			const char *path;
			uint_t num_eas;
			struct ea_struct *eas;			
		} in;
	} t2mkdir;
};

/* struct used in rmdir() call */
struct smb_rmdir {
	struct {
		const char *path;
	} in;
};

/* struct used in rename() call */
enum smb_rename_level {RAW_RENAME_RENAME, RAW_RENAME_NTRENAME};

union smb_rename {
	struct {
		enum smb_rename_level level;
	} generic;

	/* SMBrename interface */
	struct {
		enum smb_rename_level level;

		struct {
			const char *pattern1;
			const char *pattern2;
			uint16_t attrib;
		} in;
	} rename;


	/* SMBntrename interface */
	struct {
		enum smb_rename_level level;

		struct {
			uint16_t attrib;
			uint16_t flags; /* see RENAME_FLAG_* */
			uint32_t cluster_size;
			const char *old_name;
			const char *new_name;
		} in;
	} ntrename;
};

enum smb_tcon_level {RAW_TCON_TCON, RAW_TCON_TCONX};

/* union used in tree connect call */
union smb_tcon {
	/* generic interface */
	struct {
		enum smb_tcon_level level;
	} generic;

	/* SMBtcon interface */
	struct {
		enum smb_tcon_level level;

		struct {
			const char *service;
			const char *password;
			const char *dev;
		} in;
		struct {
			uint16_t max_xmit;
			uint16_t tid;
		} out;
	} tcon;

	/* SMBtconX interface */
	struct {
		enum smb_tcon_level level;

		struct {
			uint16_t flags;
			DATA_BLOB password;
			const char *path;
			const char *device;
		} in;
		struct {
			uint16_t options;
			char *dev_type;
			char *fs_type;
			uint16_t tid;
		} out;
	} tconx;
};


enum smb_sesssetup_level {RAW_SESSSETUP_OLD, RAW_SESSSETUP_NT1, RAW_SESSSETUP_SPNEGO};

/* union used in session_setup call */
union smb_sesssetup {
	/* the pre-NT1 interface */
	struct {
		enum smb_sesssetup_level level;

		struct {
			uint16_t bufsize;
			uint16_t mpx_max;
			uint16_t vc_num;
			uint32_t sesskey;
			DATA_BLOB password;
			const char *user;
			const char *domain;
			const char *os;
			const char *lanman;
		} in;
		struct {
			uint16_t action;
			uint16_t vuid;
			char *os;
			char *lanman;
			char *domain;
		} out;
	} old;

	/* the NT1 interface */
	struct {
		enum smb_sesssetup_level level;

		struct {
			uint16_t bufsize;
			uint16_t mpx_max;
			uint16_t vc_num;
			uint32_t sesskey;
			uint32_t capabilities;
			DATA_BLOB password1;
			DATA_BLOB password2;
			const char *user;
			const char *domain;
			const char *os;
			const char *lanman;
		} in;
		struct {
			uint16_t action;
			uint16_t vuid;
			char *os;
			char *lanman;
			char *domain;
		} out;
	} nt1;


	/* the SPNEGO interface */
	struct {
		enum smb_sesssetup_level level;

		struct {
			uint16_t bufsize;
			uint16_t mpx_max;
			uint16_t vc_num;
			uint32_t sesskey;
			uint32_t capabilities;
			DATA_BLOB secblob;
			const char *os;
			const char *lanman;
			const char *workgroup;
		} in;
		struct {
			uint16_t action;
			DATA_BLOB secblob;
			char *os;
			char *lanman;
			char *workgroup;
			uint16_t vuid;
		} out;
	} spnego;
};

/* Note that the specified enum values are identical to the actual info-levels used
 * on the wire.
 */
enum smb_fileinfo_level {
		     RAW_FILEINFO_GENERIC                    = 0xF000, 
		     RAW_FILEINFO_GETATTR,                   /* SMBgetatr */
		     RAW_FILEINFO_GETATTRE,                  /* SMBgetattrE */
		     RAW_FILEINFO_SEC_DESC,                  /* NT_TRANSACT_QUERY_SECURITY_DESC */
		     RAW_FILEINFO_STANDARD                   = SMB_QFILEINFO_STANDARD,
		     RAW_FILEINFO_EA_SIZE                    = SMB_QFILEINFO_EA_SIZE,
		     RAW_FILEINFO_EA_LIST                    = SMB_QFILEINFO_EA_LIST,
		     RAW_FILEINFO_ALL_EAS                    = SMB_QFILEINFO_ALL_EAS,
		     RAW_FILEINFO_IS_NAME_VALID              = SMB_QFILEINFO_IS_NAME_VALID,
		     RAW_FILEINFO_BASIC_INFO                 = SMB_QFILEINFO_BASIC_INFO, 
		     RAW_FILEINFO_STANDARD_INFO              = SMB_QFILEINFO_STANDARD_INFO,
		     RAW_FILEINFO_EA_INFO                    = SMB_QFILEINFO_EA_INFO,
		     RAW_FILEINFO_NAME_INFO                  = SMB_QFILEINFO_NAME_INFO, 
		     RAW_FILEINFO_ALL_INFO                   = SMB_QFILEINFO_ALL_INFO,
		     RAW_FILEINFO_ALT_NAME_INFO              = SMB_QFILEINFO_ALT_NAME_INFO,
		     RAW_FILEINFO_STREAM_INFO                = SMB_QFILEINFO_STREAM_INFO,
		     RAW_FILEINFO_COMPRESSION_INFO           = SMB_QFILEINFO_COMPRESSION_INFO,
		     RAW_FILEINFO_UNIX_BASIC                 = SMB_QFILEINFO_UNIX_BASIC,
		     RAW_FILEINFO_UNIX_LINK                  = SMB_QFILEINFO_UNIX_LINK,
		     RAW_FILEINFO_BASIC_INFORMATION          = SMB_QFILEINFO_BASIC_INFORMATION,
		     RAW_FILEINFO_STANDARD_INFORMATION       = SMB_QFILEINFO_STANDARD_INFORMATION,
		     RAW_FILEINFO_INTERNAL_INFORMATION       = SMB_QFILEINFO_INTERNAL_INFORMATION,
		     RAW_FILEINFO_EA_INFORMATION             = SMB_QFILEINFO_EA_INFORMATION,
		     RAW_FILEINFO_ACCESS_INFORMATION         = SMB_QFILEINFO_ACCESS_INFORMATION,
		     RAW_FILEINFO_NAME_INFORMATION           = SMB_QFILEINFO_NAME_INFORMATION,
		     RAW_FILEINFO_POSITION_INFORMATION       = SMB_QFILEINFO_POSITION_INFORMATION,
		     RAW_FILEINFO_MODE_INFORMATION           = SMB_QFILEINFO_MODE_INFORMATION,
		     RAW_FILEINFO_ALIGNMENT_INFORMATION      = SMB_QFILEINFO_ALIGNMENT_INFORMATION,
		     RAW_FILEINFO_ALL_INFORMATION            = SMB_QFILEINFO_ALL_INFORMATION,
		     RAW_FILEINFO_ALT_NAME_INFORMATION       = SMB_QFILEINFO_ALT_NAME_INFORMATION,
		     RAW_FILEINFO_STREAM_INFORMATION         = SMB_QFILEINFO_STREAM_INFORMATION,
		     RAW_FILEINFO_COMPRESSION_INFORMATION    = SMB_QFILEINFO_COMPRESSION_INFORMATION,
		     RAW_FILEINFO_NETWORK_OPEN_INFORMATION   = SMB_QFILEINFO_NETWORK_OPEN_INFORMATION,
		     RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION  = SMB_QFILEINFO_ATTRIBUTE_TAG_INFORMATION,
		     /* SMB2 specific levels */
		     RAW_FILEINFO_SMB2_ALL_EAS               = 0x0f01,
		     RAW_FILEINFO_SMB2_ALL_INFORMATION       = 0x1201
};

/* union used in qfileinfo() and qpathinfo() backend calls */
union smb_fileinfo {
	/* generic interface:
	 * matches RAW_FILEINFO_GENERIC */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint32_t attrib;
			uint32_t ea_size;
			uint_t num_eas;
			struct ea_struct {
				uint8_t flags;
				struct smb_wire_string name;
				DATA_BLOB value;
			} *eas;		
			NTTIME create_time;
			NTTIME access_time;
			NTTIME write_time;
			NTTIME change_time;
			uint64_t alloc_size;
			uint64_t size;
			uint32_t nlink;
			struct smb_wire_string fname;	
			struct smb_wire_string alt_fname;	
			uint8_t delete_pending;
			uint8_t directory;
			uint64_t compressed_size;
			uint16_t format;
			uint8_t unit_shift;
			uint8_t chunk_shift;
			uint8_t cluster_shift;
			uint64_t file_id;
			uint32_t access_flags; /* seen 0x001f01ff from w2k3 */
			uint64_t position;
			uint32_t mode;
			uint32_t alignment_requirement;
			uint32_t reparse_tag;
			uint_t num_streams;
			struct stream_struct {
				uint64_t size;
				uint64_t alloc_size;
				struct smb_wire_string stream_name;
			} *streams;
		} out;
	} generic;


	/* SMBgetatr interface:
	 * matches RAW_FILEINFO_GETATTR */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint16_t attrib;
			uint32_t size;
			time_t write_time;
		} out;
	} getattr;

	/* SMBgetattrE and  RAW_FILEINFO_STANDARD interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			time_t create_time;
			time_t access_time;
			time_t write_time;
			uint32_t size;
			uint32_t alloc_size;
			uint16_t attrib;
		} out;
	} getattre, standard;

	/* trans2 RAW_FILEINFO_EA_SIZE interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			time_t create_time;
			time_t access_time;
			time_t write_time;
			uint32_t size;
			uint32_t alloc_size;
			uint16_t attrib;
			uint32_t ea_size;
		} out;
	} ea_size;

	/* trans2 RAW_FILEINFO_EA_LIST interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint_t num_names;
			struct ea_name {
				struct smb_wire_string name;
			} *ea_names;	
		} in;	

		struct smb_ea_list {
			uint_t num_eas;
			struct ea_struct *eas;
		} out;
	} ea_list;

	/* trans2 RAW_FILEINFO_ALL_EAS and RAW_FILEINFO_FULL_EA_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
			/* SMB2 only - SMB2_CONTINUE_FLAG_* */
			uint8_t continue_flags;
		} in;
		struct smb_ea_list out;
	} all_eas;

	/* trans2 qpathinfo RAW_FILEINFO_IS_NAME_VALID interface 
	   only valid for a QPATHNAME call - no returned data */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
	} is_name_valid;

	/* RAW_FILEINFO_BASIC_INFO and RAW_FILEINFO_BASIC_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			NTTIME create_time;
			NTTIME access_time;
			NTTIME write_time;
			NTTIME change_time;
			uint32_t attrib;
		} out;
	} basic_info;
		

	/* RAW_FILEINFO_STANDARD_INFO and RAW_FILEINFO_STANDARD_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint64_t alloc_size;
			uint64_t size;
			uint32_t nlink;
			BOOL delete_pending;
			BOOL directory;
		} out;
	} standard_info;
	
	/* RAW_FILEINFO_EA_INFO and RAW_FILEINFO_EA_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint32_t ea_size;
		} out;
	} ea_info;

	/* RAW_FILEINFO_NAME_INFO and RAW_FILEINFO_NAME_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			struct smb_wire_string fname;
		} out;
	} name_info;

	/* RAW_FILEINFO_ALL_INFO and RAW_FILEINFO_ALL_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			NTTIME create_time;
			NTTIME access_time;
			NTTIME write_time;
			NTTIME change_time;
			uint32_t attrib;
			uint64_t alloc_size;
			uint64_t size;
			uint32_t nlink;
			uint8_t delete_pending;
			uint8_t directory;
			uint32_t ea_size;
			struct smb_wire_string fname;
		} out;
	} all_info;	

	/* RAW_FILEINFO_SMB2_ALL_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			NTTIME   create_time;
			NTTIME   access_time;
			NTTIME   write_time;
			NTTIME   change_time;
			uint32_t attrib;
			uint32_t unknown1;
			uint64_t alloc_size;
			uint64_t size;
			uint32_t nlink;
			uint8_t  delete_pending;
			uint8_t  directory;
			/* uint16_t _pad; */
			uint64_t file_id;
			uint32_t ea_size;
			uint32_t access_mask;
			uint64_t position;
			uint64_t mode;
			struct smb_wire_string fname;
		} out;
	} all_info2;

	/* RAW_FILEINFO_ALT_NAME_INFO and RAW_FILEINFO_ALT_NAME_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			struct smb_wire_string fname;
		} out;
	} alt_name_info;

	/* RAW_FILEINFO_STREAM_INFO and RAW_FILEINFO_STREAM_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct stream_information {
			uint_t num_streams;
			struct stream_struct *streams;
		} out;
	} stream_info;
	
	/* RAW_FILEINFO_COMPRESSION_INFO and RAW_FILEINFO_COMPRESSION_INFORMATION interfaces */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint64_t compressed_size;
			uint16_t format;
			uint8_t unit_shift;
			uint8_t chunk_shift;
			uint8_t cluster_shift;
		} out;
	} compression_info;

	/* RAW_FILEINFO_UNIX_BASIC interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint64_t end_of_file;
			uint64_t num_bytes;
			NTTIME status_change_time;
			NTTIME access_time;
			NTTIME change_time;
			uint64_t uid;
			uint64_t gid;
			uint32_t file_type;
			uint64_t dev_major;
			uint64_t dev_minor;
			uint64_t unique_id;
			uint64_t permissions;
			uint64_t nlink;
		} out;
	} unix_basic_info;

	/* RAW_FILEINFO_UNIX_LINK interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			struct smb_wire_string link_dest;
		} out;
	} unix_link_info;

	/* RAW_FILEINFO_INTERNAL_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint64_t file_id;
		} out;
	} internal_information;

	/* RAW_FILEINFO_ACCESS_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint32_t access_flags;
		} out;
	} access_information;

	/* RAW_FILEINFO_POSITION_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint64_t position;
		} out;
	} position_information;

	/* RAW_FILEINFO_MODE_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint32_t mode;
		} out;
	} mode_information;

	/* RAW_FILEINFO_ALIGNMENT_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint32_t alignment_requirement;
		} out;
	} alignment_information;

	/* RAW_FILEINFO_NETWORK_OPEN_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			NTTIME create_time;
			NTTIME access_time;
			NTTIME write_time;
			NTTIME change_time;
			uint64_t alloc_size;
			uint64_t size;
			uint32_t attrib;
		} out;
	} network_open_information;


	/* RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION interface */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
		struct {
			uint32_t attrib;
			uint32_t reparse_tag;
		} out;
	} attribute_tag_information;

	/* RAW_FILEINFO_SEC_DESC */
	struct {
		enum smb_fileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint32_t secinfo_flags;
		} in;
		struct {
			struct security_descriptor *sd;
		} out;
	} query_secdesc;
};


enum smb_setfileinfo_level {
	RAW_SFILEINFO_GENERIC		      = 0xF000, 
	RAW_SFILEINFO_SETATTR,		      /* SMBsetatr */
	RAW_SFILEINFO_SETATTRE,		      /* SMBsetattrE */
	RAW_SFILEINFO_SEC_DESC,               /* NT_TRANSACT_SET_SECURITY_DESC */
	RAW_SFILEINFO_STANDARD                = SMB_SFILEINFO_STANDARD,
	RAW_SFILEINFO_EA_SET                  = SMB_SFILEINFO_EA_SET,
	RAW_SFILEINFO_BASIC_INFO              = SMB_SFILEINFO_BASIC_INFO,
	RAW_SFILEINFO_DISPOSITION_INFO        = SMB_SFILEINFO_DISPOSITION_INFO,
	RAW_SFILEINFO_ALLOCATION_INFO         = SMB_SFILEINFO_ALLOCATION_INFO,
	RAW_SFILEINFO_END_OF_FILE_INFO        = SMB_SFILEINFO_END_OF_FILE_INFO,
	RAW_SFILEINFO_UNIX_BASIC              = SMB_SFILEINFO_UNIX_BASIC,
	RAW_SFILEINFO_UNIX_LINK               = SMB_SFILEINFO_UNIX_LINK,
	RAW_SFILEINFO_UNIX_HLINK	      = SMB_SFILEINFO_UNIX_HLINK,
	RAW_SFILEINFO_BASIC_INFORMATION       = SMB_SFILEINFO_BASIC_INFORMATION,
	RAW_SFILEINFO_RENAME_INFORMATION      = SMB_SFILEINFO_RENAME_INFORMATION,
	RAW_SFILEINFO_DISPOSITION_INFORMATION = SMB_SFILEINFO_DISPOSITION_INFORMATION,
	RAW_SFILEINFO_POSITION_INFORMATION    = SMB_SFILEINFO_POSITION_INFORMATION,
	RAW_SFILEINFO_MODE_INFORMATION        = SMB_SFILEINFO_MODE_INFORMATION,
	RAW_SFILEINFO_ALLOCATION_INFORMATION  = SMB_SFILEINFO_ALLOCATION_INFORMATION,
	RAW_SFILEINFO_END_OF_FILE_INFORMATION = SMB_SFILEINFO_END_OF_FILE_INFORMATION,
	RAW_SFILEINFO_1023                    = SMB_SFILEINFO_1023,
	RAW_SFILEINFO_1025                    = SMB_SFILEINFO_1025,
	RAW_SFILEINFO_1029                    = SMB_SFILEINFO_1029,
	RAW_SFILEINFO_1032                    = SMB_SFILEINFO_1032,
	RAW_SFILEINFO_1039                    = SMB_SFILEINFO_1039,
	RAW_SFILEINFO_1040                    = SMB_SFILEINFO_1040
};

/* union used in setfileinfo() and setpathinfo() calls */
union smb_setfileinfo {
	/* generic interface */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
		} in;
	} generic;

	/* RAW_SFILEINFO_SETATTR (SMBsetatr) interface - only via setpathinfo() */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint16_t attrib;
			time_t write_time;
		} in;
	} setattr;

	/* RAW_SFILEINFO_SETATTRE (SMBsetattrE) interface - only via setfileinfo() 
	   also RAW_SFILEINFO_STANDARD */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			time_t create_time;
			time_t access_time;
			time_t write_time;
			/* notice that size, alloc_size and attrib are not settable,
			   unlike the corresponding qfileinfo level */
		} in;
	} setattre, standard;

	/* RAW_SFILEINFO_EA_SET interface */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint_t num_eas;
			struct ea_struct *eas;			
		} in;
	} ea_set;

	/* RAW_SFILEINFO_BASIC_INFO and
	   RAW_SFILEINFO_BASIC_INFORMATION interfaces */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			NTTIME create_time;
			NTTIME access_time;
			NTTIME write_time;
			NTTIME change_time;
			uint32_t attrib;
		} in;
	} basic_info;

	/* RAW_SFILEINFO_DISPOSITION_INFO and 
	   RAW_SFILEINFO_DISPOSITION_INFORMATION interfaces */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			BOOL delete_on_close;
		} in;
	} disposition_info;

	/* RAW_SFILEINFO_ALLOCATION_INFO and 
	   RAW_SFILEINFO_ALLOCATION_INFORMATION interfaces */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			/* w2k3 rounds this up to nearest 4096 */
			uint64_t alloc_size;
		} in;
	} allocation_info;
	
	/* RAW_SFILEINFO_END_OF_FILE_INFO and 
	   RAW_SFILEINFO_END_OF_FILE_INFORMATION interfaces */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint64_t size;
		} in;
	} end_of_file_info;

	/* RAW_SFILEINFO_RENAME_INFORMATION interface */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint8_t overwrite;
			uint32_t root_fid;
			const char *new_name;
		} in;
	} rename_information;

	/* RAW_SFILEINFO_POSITION_INFORMATION interface */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint64_t position;
		} in;
	} position_information;

	/* RAW_SFILEINFO_MODE_INFORMATION interface */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			/* valid values seem to be 0, 2, 4 and 6 */
			uint32_t mode;
		} in;
	} mode_information;



	/* RAW_SFILEINFO_UNIX_BASIC interface */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint32_t mode; /* yuck - this field remains to fix compile of libcli/clifile.c */
			uint64_t end_of_file;
			uint64_t num_bytes;
			NTTIME status_change_time;
			NTTIME access_time;
			NTTIME change_time;
			uint64_t uid;
			uint64_t gid;
			uint32_t file_type;
			uint64_t dev_major;
			uint64_t dev_minor;
			uint64_t unique_id;
			uint64_t permissions;
			uint64_t nlink;
		} in;
	} unix_basic;
	
	/* RAW_SFILEINFO_UNIX_LINK, RAW_SFILEINFO_UNIX_HLINK interface */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			const char *link_dest;
		} in;
	} unix_link, unix_hlink;

	/* RAW_FILEINFO_SET_SEC_DESC */
	struct {
		enum smb_setfileinfo_level level;
		struct {
			union smb_handle_or_path file;
			uint32_t secinfo_flags;
			struct security_descriptor *sd;
		} in;
	} set_secdesc;
};


enum smb_fsinfo_level {
		   RAW_QFS_GENERIC                        = 0xF000, 
		   RAW_QFS_DSKATTR,                         /* SMBdskattr */
		   RAW_QFS_ALLOCATION                     = SMB_QFS_ALLOCATION,
		   RAW_QFS_VOLUME                         = SMB_QFS_VOLUME,
		   RAW_QFS_VOLUME_INFO                    = SMB_QFS_VOLUME_INFO,
		   RAW_QFS_SIZE_INFO                      = SMB_QFS_SIZE_INFO,
		   RAW_QFS_DEVICE_INFO                    = SMB_QFS_DEVICE_INFO,
		   RAW_QFS_ATTRIBUTE_INFO                 = SMB_QFS_ATTRIBUTE_INFO,
		   RAW_QFS_UNIX_INFO                      = SMB_QFS_UNIX_INFO,
		   RAW_QFS_VOLUME_INFORMATION		  = SMB_QFS_VOLUME_INFORMATION,
		   RAW_QFS_SIZE_INFORMATION               = SMB_QFS_SIZE_INFORMATION,
		   RAW_QFS_DEVICE_INFORMATION             = SMB_QFS_DEVICE_INFORMATION,
		   RAW_QFS_ATTRIBUTE_INFORMATION          = SMB_QFS_ATTRIBUTE_INFORMATION,
		   RAW_QFS_QUOTA_INFORMATION              = SMB_QFS_QUOTA_INFORMATION,
		   RAW_QFS_FULL_SIZE_INFORMATION          = SMB_QFS_FULL_SIZE_INFORMATION,
		   RAW_QFS_OBJECTID_INFORMATION           = SMB_QFS_OBJECTID_INFORMATION};


/* union for fsinfo() backend call. Note that there are no in
   structures, as this call only contains out parameters */
union smb_fsinfo {
	/* generic interface */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			uint32_t block_size;
			uint64_t blocks_total;
			uint64_t blocks_free;
			uint32_t fs_id;
			NTTIME create_time;
			uint32_t serial_number;
			uint32_t fs_attr;
			uint32_t max_file_component_length;
			uint32_t device_type;
			uint32_t device_characteristics;
			uint64_t quota_soft;
			uint64_t quota_hard;
			uint64_t quota_flags;
			struct GUID guid;
			char *volume_name;
			char *fs_type;
		} out;
	} generic;

	/* SMBdskattr interface */
	struct {
		enum smb_fsinfo_level level;

		struct {
			uint16_t units_total;
			uint16_t blocks_per_unit;
			uint16_t block_size;
			uint16_t units_free;
		} out;
	} dskattr;

	/* trans2 RAW_QFS_ALLOCATION interface */
	struct {
		enum smb_fsinfo_level level;

		struct {
			uint32_t fs_id;
			uint32_t sectors_per_unit;
			uint32_t total_alloc_units;
			uint32_t avail_alloc_units;
			uint16_t bytes_per_sector;
		} out;
	} allocation;

	/* TRANS2 RAW_QFS_VOLUME interface */
	struct {
		enum smb_fsinfo_level level;

		struct {
			uint32_t serial_number;
			struct smb_wire_string volume_name;
		} out;
	} volume;

	/* TRANS2 RAW_QFS_VOLUME_INFO and RAW_QFS_VOLUME_INFORMATION interfaces */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			NTTIME create_time;
			uint32_t serial_number;
			struct smb_wire_string volume_name;
		} out;
	} volume_info;

	/* trans2 RAW_QFS_SIZE_INFO and RAW_QFS_SIZE_INFORMATION interfaces */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			uint64_t total_alloc_units;
			uint64_t avail_alloc_units; /* maps to call_avail_alloc_units */
			uint32_t sectors_per_unit;
			uint32_t bytes_per_sector;
		} out;
	} size_info;

	/* TRANS2 RAW_QFS_DEVICE_INFO and RAW_QFS_DEVICE_INFORMATION interfaces */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			uint32_t device_type;
			uint32_t characteristics;
		} out;
	} device_info;


	/* TRANS2 RAW_QFS_ATTRIBUTE_INFO and RAW_QFS_ATTRIBUTE_INFORMATION interfaces */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			uint32_t fs_attr;
			uint32_t max_file_component_length;
			struct smb_wire_string fs_type;
		} out;
	} attribute_info;


	/* TRANS2 RAW_QFS_UNIX_INFO interface */
	struct {
		enum smb_fsinfo_level level;

		struct {
			uint16_t major_version;
			uint16_t minor_version;
			uint64_t capability;
		} out;
	} unix_info;

	/* trans2 RAW_QFS_QUOTA_INFORMATION interface */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			uint64_t unknown[3];
			uint64_t quota_soft;
			uint64_t quota_hard;
			uint64_t quota_flags;
		} out;
	} quota_information;	

	/* trans2 RAW_QFS_FULL_SIZE_INFORMATION interface */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			uint64_t total_alloc_units;
			uint64_t call_avail_alloc_units;
			uint64_t actual_avail_alloc_units;
			uint32_t sectors_per_unit;
			uint32_t bytes_per_sector;
		} out;
	} full_size_information;

	/* trans2 RAW_QFS_OBJECTID_INFORMATION interface */
	struct {
		enum smb_fsinfo_level level;
		struct smb2_handle handle; /* only for smb2 */

		struct {
			struct GUID  guid;
			uint64_t unknown[6];
		} out;
	} objectid_information;	
};



enum smb_open_level {
		 RAW_OPEN_OPEN, RAW_OPEN_OPENX, 
		 RAW_OPEN_MKNEW, RAW_OPEN_CREATE, 
		 RAW_OPEN_CTEMP, RAW_OPEN_SPLOPEN,
		 RAW_OPEN_NTCREATEX, RAW_OPEN_T2OPEN,
		 RAW_OPEN_NTTRANS_CREATE, 
		 RAW_OPEN_OPENX_READX};

/* the generic interface is defined to be equal to the NTCREATEX interface */
#define RAW_OPEN_GENERIC RAW_OPEN_NTCREATEX

/* union for open() backend call */
union smb_open {
/* 
 * because the *.out.file structs are not aligned to the same offset for each level
 * we provide a hepler macro that should be used to find the current smb_handle structure
 */
#define SMB_OPEN_OUT_FILE(op, file) do { \
	switch (op->generic.level) { \
	case RAW_OPEN_OPEN: \
		file = &op->openold.out.file; \
		break; \
	case RAW_OPEN_OPENX: \
		file = &op->openx.out.file; \
		break; \
	case RAW_OPEN_MKNEW: \
		file = &op->mknew.out.file; \
		break; \
	case RAW_OPEN_CREATE: \
		file = &op->create.out.file; \
		break; \
	case RAW_OPEN_CTEMP: \
		file = &op->ctemp.out.file; \
		break; \
	case RAW_OPEN_SPLOPEN: \
		file = &op->splopen.out.file; \
		break; \
	case RAW_OPEN_NTCREATEX: \
		file = &op->ntcreatex.out.file; \
		break; \
	case RAW_OPEN_T2OPEN: \
		file = &op->t2open.out.file; \
		break; \
	case RAW_OPEN_NTTRANS_CREATE: \
		file = &op->nttrans.out.file; \
		break; \
	case RAW_OPEN_OPENX_READX: \
		file = &op->openxreadx.out.file; \
		break; \
	default: \
		/* this must be a programmer error */ \
		file = NULL; \
		break; \
	} \
} while (0)
	/* SMBNTCreateX interface */
	struct {
		enum smb_open_level level;
		struct {
			uint32_t flags;
			uint32_t root_fid;
			uint32_t access_mask;
			uint64_t alloc_size;
			uint32_t file_attr;
			uint32_t share_access;
			uint32_t open_disposition;
			uint32_t create_options;
			uint32_t impersonation;
			uint8_t  security_flags;
			/* NOTE: fname can also be a pointer to a
			 uint64_t file_id if create_options has the
			 NTCREATEX_OPTIONS_OPEN_BY_FILE_ID flag set */
			const char *fname;

			/* these last 2 elements are only used in the
			   NTTRANS varient of the call */
			struct security_descriptor *sec_desc;
			struct smb_ea_list *ea_list;
		} in;
		struct {
			union smb_handle file;
			uint8_t oplock_level;
			uint32_t create_action;
			NTTIME create_time;
			NTTIME access_time;
			NTTIME write_time;
			NTTIME change_time;
			uint32_t attrib;
			uint64_t alloc_size;
			uint64_t size;
			uint16_t file_type;
			uint16_t ipc_state;
			uint8_t  is_directory;
		} out;
	} ntcreatex, nttrans, generic;

	/* TRANS2_OPEN interface */
	struct {
		enum smb_open_level level;
		struct {
			uint16_t flags;
			uint16_t open_mode;
			uint16_t search_attrs;
			uint16_t file_attrs;
			time_t write_time;
			uint16_t open_func;
			uint32_t size;
			uint32_t timeout;
			const char *fname;
			uint_t num_eas;
			struct ea_struct *eas;			
		} in;
		struct {
			union smb_handle file;
			uint16_t attrib;
			time_t write_time;
			uint32_t size;
			uint16_t access;
			uint16_t ftype;
			uint16_t devstate;
			uint16_t action;
			uint32_t file_id;
		} out;
	} t2open;

	/* SMBopen interface */
	struct {
		enum smb_open_level level;
		struct {
			uint16_t open_mode;
			uint16_t search_attrs;
			const char *fname;
		} in;
		struct {
			union smb_handle file;
			uint16_t attrib;
			time_t write_time;
			uint32_t size;
			uint16_t rmode;
		} out;
	} openold;

	/* SMBopenX interface */
	struct {
		enum smb_open_level level;
		struct {
			uint16_t flags;
			uint16_t open_mode;
			uint16_t search_attrs; /* not honoured by win2003 */
			uint16_t file_attrs;
			time_t write_time; /* not honoured by win2003 */
			uint16_t open_func;
			uint32_t size; /* note that this sets the
					initial file size, not
					just allocation size */
			uint32_t timeout; /* not honoured by win2003 */
			const char *fname;
		} in;
		struct {
			union smb_handle file;
			uint16_t attrib;
			time_t write_time;
			uint32_t size;
			uint16_t access;
			uint16_t ftype;
			uint16_t devstate;
			uint16_t action;
			uint32_t unique_fid;
			uint32_t access_mask;
			uint32_t unknown;
		} out;
	} openx;

	/* SMBmknew interface */
	struct {
		enum smb_open_level level;
		struct {
			uint16_t attrib;
			time_t write_time;
			const char *fname;
		} in;
		struct {
			union smb_handle file;
		} out;
	} mknew, create;

	/* SMBctemp interface */
	struct {
		enum smb_open_level level;
		struct {
			uint16_t attrib;
			time_t write_time;
			const char *directory;
		} in;
		struct {
			union smb_handle file;
			/* temp name, relative to directory */
			char *name; 
		} out;
	} ctemp;

	/* SMBsplopen interface */
	struct {
		enum smb_open_level level;
		struct {
			uint16_t setup_length;
			uint16_t mode;
			const char *ident;
		} in;
		struct {
			union smb_handle file;
		} out;
	} splopen;


	/* chained OpenX/ReadX interface */
	struct {
		enum smb_open_level level;
		struct {
			uint16_t flags;
			uint16_t open_mode;
			uint16_t search_attrs; /* not honoured by win2003 */
			uint16_t file_attrs;
			time_t write_time; /* not honoured by win2003 */
			uint16_t open_func;
			uint32_t size; /* note that this sets the
					initial file size, not
					just allocation size */
			uint32_t timeout; /* not honoured by win2003 */
			const char *fname;

			/* readx part */
			uint64_t offset;
			uint16_t mincnt;
			uint32_t maxcnt;
			uint16_t remaining;
		} in;
		struct {
			union smb_handle file;
			uint16_t attrib;
			time_t write_time;
			uint32_t size;
			uint16_t access;
			uint16_t ftype;
			uint16_t devstate;
			uint16_t action;
			uint32_t unique_fid;
			uint32_t access_mask;
			uint32_t unknown;
			
			/* readx part */
			uint8_t *data;
			uint16_t remaining;
			uint16_t compaction_mode;
			uint16_t nread;
		} out;
	} openxreadx;
};



enum smb_read_level {RAW_READ_READBRAW, RAW_READ_LOCKREAD, RAW_READ_READ, RAW_READ_READX};

#define RAW_READ_GENERIC RAW_READ_READX

/* union for read() backend call 

   note that .infoX.out.data will be allocated before the backend is
   called. It will be big enough to hold the maximum size asked for
*/
union smb_read {
	/* SMBreadX (and generic) interface */
	struct {
		enum smb_read_level level;
		struct {
			union smb_handle file;
			uint64_t offset;
			uint16_t mincnt;
			uint32_t maxcnt;
			uint16_t remaining;
			BOOL read_for_execute;
		} in;
		struct {
			uint8_t *data;
			uint16_t remaining;
			uint16_t compaction_mode;
			uint16_t nread;
		} out;
	} readx, generic;

	/* SMBreadbraw interface */
	struct {
		enum smb_read_level level;
		struct {
			union smb_handle file;
			uint64_t offset;
			uint16_t  maxcnt;
			uint16_t  mincnt;
			uint32_t  timeout;
		} in;
		struct {
			uint8_t *data;
			uint32_t nread;
		} out;
	} readbraw;


	/* SMBlockandread interface */
	struct {
		enum smb_read_level level;
		struct {
			union smb_handle file;
			uint16_t count;
			uint32_t offset;
			uint16_t remaining;
		} in;
		struct {
			uint8_t *data;
			uint16_t nread;
		} out;
	} lockread;

	/* SMBread interface */
	struct {
		enum smb_read_level level;
		struct {
			union smb_handle file;
			uint16_t count;
			uint32_t offset;
			uint16_t remaining;
		} in;
		struct {
			uint8_t *data;
			uint16_t nread;
		} out;
	} read;
};


enum smb_write_level {RAW_WRITE_WRITEUNLOCK, RAW_WRITE_WRITE, 
		      RAW_WRITE_WRITEX, RAW_WRITE_WRITECLOSE, 
		      RAW_WRITE_SPLWRITE};

#define RAW_WRITE_GENERIC RAW_WRITE_WRITEX

/* union for write() backend call 
*/
union smb_write {
	/* SMBwriteX interface */
	struct {
		enum smb_write_level level;
		struct {
			union smb_handle file;
			uint64_t offset;
			uint16_t wmode;
			uint16_t remaining;
			uint32_t count;
			const uint8_t *data;
		} in;
		struct {
			uint32_t nwritten;
			uint16_t remaining;
		} out;
	} writex, generic;

	/* SMBwriteunlock interface */
	struct {
		enum smb_write_level level;
		struct {
			union smb_handle file;
			uint16_t count;
			uint32_t offset;
			uint16_t remaining;
			const uint8_t *data;
		} in;
		struct {
			uint32_t nwritten;
		} out;
	} writeunlock;

	/* SMBwrite interface */
	struct {
		enum smb_write_level level;
		struct {
			union smb_handle file;
			uint16_t count;
			uint32_t offset;
			uint16_t remaining;
			const uint8_t *data;
		} in;
		struct {
			uint16_t nwritten;
		} out;
	} write;

	/* SMBwriteclose interface */
	struct {
		enum smb_write_level level;
		struct {
			union smb_handle file;
			uint16_t count;
			uint32_t offset;
			time_t mtime;
			const uint8_t *data;
		} in;
		struct {
			uint16_t nwritten;
		} out;
	} writeclose;

	/* SMBsplwrite interface */
	struct {
		enum smb_write_level level;
		struct {
			union smb_handle file;
			uint16_t count;
			const uint8_t *data;
		} in;
	} splwrite;
};


enum smb_lock_level {RAW_LOCK_LOCK, RAW_LOCK_UNLOCK, RAW_LOCK_LOCKX};

/* the generic interface is defined to be equal to the lockingX interface */
#define RAW_LOCK_GENERIC RAW_LOCK_LOCKX

/* union for lock() backend call 
*/
union smb_lock {
	/* SMBlockingX (and generic) interface */
	struct {
		enum smb_lock_level level;
		struct {
			union smb_handle file;
			uint16_t mode;
			uint32_t timeout;
			uint16_t ulock_cnt;
			uint16_t lock_cnt;
			struct smb_lock_entry {
				uint16_t pid;
				uint64_t offset;
				uint64_t count;
			} *locks; /* unlocks are first in the arrray */
		} in;
	} lockx, generic;

	/* SMBlock and SMBunlock interface */
	struct {
		enum smb_lock_level level;
		struct {
			union smb_handle file;
			uint32_t count;
			uint32_t offset;
		} in;
	} lock, unlock;
};


enum smb_close_level {RAW_CLOSE_CLOSE, RAW_CLOSE_SPLCLOSE};

#define RAW_CLOSE_GENERIC RAW_CLOSE_CLOSE

/*
  union for close() backend call
*/
union smb_close {
	/* SMBclose (and generic) interface */
	struct {
		enum smb_close_level level;
		struct {
			union smb_handle file;
			time_t write_time;
		} in;
	} close, generic;

	/* SMBsplclose interface - empty! */
	struct {
		enum smb_close_level level;
		struct {
			union smb_handle file;
		} in;
	} splclose;
};


enum smb_lpq_level {RAW_LPQ_GENERIC, RAW_LPQ_RETQ};

/*
  union for lpq() backend
*/
union smb_lpq {
	/* generic interface */
	struct {
		enum smb_lpq_level level;

	} generic;


	/* SMBsplretq interface */
	struct {
		enum smb_lpq_level level;

		struct {
			uint16_t maxcount;
			uint16_t startidx;
		} in;
		struct {
			uint16_t count;
			uint16_t restart_idx;
			struct {
				time_t time;
				uint8_t status;
				uint16_t job;
				uint32_t size;
				char *user;
			} *queue;
		} out;
	} retq;
};

enum smb_ioctl_level {RAW_IOCTL_IOCTL, RAW_IOCTL_NTIOCTL};

/*
  union for ioctl() backend
*/
union smb_ioctl {
	/* generic interface */
	struct {
		enum smb_ioctl_level level;
		struct {
			union smb_handle file;
		} in;
	} generic;

	/* struct for SMBioctl */
	struct {
		enum smb_ioctl_level level;
		struct {
			union smb_handle file;
			uint32_t request;
		} in;
		struct {
			DATA_BLOB blob;
		} out;
	} ioctl;


	/* struct for NT ioctl call */
	struct {
		enum smb_ioctl_level level;
		struct {
			union smb_handle file;
			uint32_t function;
			BOOL fsctl;
			uint8_t filter;
			DATA_BLOB blob;
		} in;
		struct {
			DATA_BLOB blob;
		} out;
	} ntioctl;
};

/* struct for SMBflush */
union smb_flush {
	struct {
		struct {
			union smb_handle file;
		} in;
	} flush, generic;
};


/* struct for SMBcopy */
struct smb_copy {
	struct {
		uint16_t tid2;
		uint16_t ofun;
		uint16_t flags;
		const char *path1;
		const char *path2;
	} in;
	struct {
		uint16_t count;
	} out;
};


/* struct for transact/transact2 call */
struct smb_trans2 {
	struct {
		uint16_t max_param;
		uint16_t max_data;
		uint8_t  max_setup;
		uint16_t flags;
		uint32_t timeout;
		uint8_t  setup_count;
		uint16_t *setup;
		const char *trans_name; /* SMBtrans only */
		DATA_BLOB params;
		DATA_BLOB data;
	} in;

	struct {
		uint8_t  setup_count;
		uint16_t *setup;
		DATA_BLOB params;
		DATA_BLOB data;
	} out;
};

/* struct for nttransact2 call */
struct smb_nttrans {
	struct {
		uint8_t  max_setup;
		uint32_t max_param;
		uint32_t max_data;
		uint32_t setup_count;
		uint16_t function;
		uint16_t *setup;
		DATA_BLOB params;
		DATA_BLOB data;
	} in;

	struct {
		uint8_t  setup_count;
		uint16_t *setup;
		DATA_BLOB params;
		DATA_BLOB data;
	} out;
};


/* struct for nttrans change notify call */
struct smb_notify {
	struct {
		union smb_handle file;
		uint32_t buffer_size;
		uint32_t completion_filter;
		BOOL recursive;
	} in;

	struct {
		uint32_t num_changes;
		struct notify_changes {
			uint32_t action;
			struct smb_wire_string name;
		} *changes;
	} out;
};

enum smb_search_level {RAW_SEARCH_GENERIC                 = 0xF000, 
		       RAW_SEARCH_SEARCH,                 /* SMBsearch */ 
		       RAW_SEARCH_FFIRST,                 /* SMBffirst */ 
		       RAW_SEARCH_FUNIQUE,                /* SMBfunique */ 
		       RAW_SEARCH_STANDARD                = SMB_FIND_STANDARD,
		       RAW_SEARCH_EA_SIZE                 = SMB_FIND_EA_SIZE,
		       RAW_SEARCH_EA_LIST                 = SMB_FIND_EA_LIST,
		       RAW_SEARCH_DIRECTORY_INFO          = SMB_FIND_DIRECTORY_INFO,
		       RAW_SEARCH_FULL_DIRECTORY_INFO     = SMB_FIND_FULL_DIRECTORY_INFO,
		       RAW_SEARCH_NAME_INFO               = SMB_FIND_NAME_INFO,
		       RAW_SEARCH_BOTH_DIRECTORY_INFO     = SMB_FIND_BOTH_DIRECTORY_INFO,
		       RAW_SEARCH_ID_FULL_DIRECTORY_INFO  = SMB_FIND_ID_FULL_DIRECTORY_INFO,
		       RAW_SEARCH_ID_BOTH_DIRECTORY_INFO  = SMB_FIND_ID_BOTH_DIRECTORY_INFO,
		       RAW_SEARCH_UNIX_INFO               = SMB_FIND_UNIX_INFO};

	
/* union for file search */
union smb_search_first {
	struct {
		enum smb_search_level level;
	} generic;
	
	/* search (old) findfirst interface. 
	   Also used for ffirst and funique. */
	struct {
		enum smb_search_level level;
	
		struct {
			uint16_t max_count;
			uint16_t search_attrib;
			const char *pattern;
		} in;
		struct {
			int16_t count;
		} out;
	} search_first;

	/* trans2 findfirst interface */
	struct {
		enum smb_search_level level;
		
		struct {
			uint16_t search_attrib;
			uint16_t max_count;
			uint16_t flags;
			uint32_t storage_type;
			const char *pattern;

			/* the ea names are only used for RAW_SEARCH_EA_LIST */
			uint_t num_names;
			struct ea_name *ea_names;
		} in;
		struct {
			uint16_t handle;
			uint16_t count;
			uint16_t end_of_search;
		} out;
	} t2ffirst;
};

/* union for file search continue */
union smb_search_next {
	struct {
		enum smb_search_level level;
	} generic;

	/* search (old) findnext interface. Also used
	   for ffirst when continuing */
	struct {
		enum smb_search_level level;
	
		struct {
			uint16_t max_count;
			uint16_t search_attrib;
			struct smb_search_id {
				uint8_t reserved;
				char name[11];
				uint8_t handle;
				uint32_t server_cookie;
				uint32_t client_cookie;
			} id;
		} in;
		struct {
			uint16_t count;
		} out;
	} search_next;
	
	/* trans2 findnext interface */
	struct {
		enum smb_search_level level;
		
		struct {
			uint16_t handle;
			uint16_t max_count;
			uint32_t resume_key;
			uint16_t flags;
			const char *last_name;

			/* the ea names are only used for RAW_SEARCH_EA_LIST */
			uint_t num_names;
			struct ea_name *ea_names;
		} in;
		struct {
			uint16_t count;
			uint16_t end_of_search;
		} out;
	} t2fnext;
};

/* union for search reply file data */
union smb_search_data {
	/* search (old) findfirst */
	struct {
		uint16_t attrib;
		time_t write_time;
		uint32_t size;
		struct smb_search_id id;
		const char *name;
	} search;
	
	/* trans2 findfirst RAW_SEARCH_STANDARD level */
	struct {
		uint32_t resume_key;
		time_t create_time;
		time_t access_time;
		time_t write_time;
		uint32_t size;
		uint32_t alloc_size;
		uint16_t attrib;
		struct smb_wire_string name;
	} standard;

	/* trans2 findfirst RAW_SEARCH_EA_SIZE level */
	struct {
		uint32_t resume_key;
		time_t create_time;
		time_t access_time;
		time_t write_time;
		uint32_t size;
		uint32_t alloc_size;
		uint16_t attrib;
		uint32_t ea_size;
		struct smb_wire_string name;
	} ea_size;

	/* trans2 findfirst RAW_SEARCH_EA_LIST level */
	struct {
		uint32_t resume_key;
		time_t create_time;
		time_t access_time;
		time_t write_time;
		uint32_t size;
		uint32_t alloc_size;
		uint16_t attrib;
		struct smb_ea_list eas;
		struct smb_wire_string name;
	} ea_list;

	/* RAW_SEARCH_DIRECTORY_INFO interface */
	struct {
		uint32_t file_index;
		NTTIME create_time;
		NTTIME access_time;
		NTTIME write_time;
		NTTIME change_time;
		uint64_t  size;
		uint64_t  alloc_size;
		uint32_t   attrib;
		struct smb_wire_string name;
	} directory_info;

	/* RAW_SEARCH_FULL_DIRECTORY_INFO interface */
	struct {
		uint32_t file_index;
		NTTIME create_time;
		NTTIME access_time;
		NTTIME write_time;
		NTTIME change_time;
		uint64_t  size;
		uint64_t  alloc_size;
		uint32_t   attrib;
		uint32_t   ea_size;
		struct smb_wire_string name;
	} full_directory_info;

	/* RAW_SEARCH_NAME_INFO interface */
	struct {
		uint32_t file_index;
		struct smb_wire_string name;
	} name_info;

	/* RAW_SEARCH_BOTH_DIRECTORY_INFO interface */
	struct {
		uint32_t file_index;
		NTTIME create_time;
		NTTIME access_time;
		NTTIME write_time;
		NTTIME change_time;
		uint64_t  size;
		uint64_t  alloc_size;
		uint32_t   attrib;
		uint32_t   ea_size;
		struct smb_wire_string short_name;
		struct smb_wire_string name;
	} both_directory_info;

	/* RAW_SEARCH_ID_FULL_DIRECTORY_INFO interface */
	struct {
		uint32_t file_index;
		NTTIME create_time;
		NTTIME access_time;
		NTTIME write_time;
		NTTIME change_time;
		uint64_t size;
		uint64_t alloc_size;
		uint32_t attrib;
		uint32_t ea_size;
		uint64_t file_id;
		struct smb_wire_string name;
	} id_full_directory_info;

	/* RAW_SEARCH_ID_BOTH_DIRECTORY_INFO interface */
	struct {
		uint32_t file_index;
		NTTIME create_time;
		NTTIME access_time;
		NTTIME write_time;
		NTTIME change_time;
		uint64_t size;
		uint64_t alloc_size;
		uint32_t  attrib;
		uint32_t  ea_size;
		uint64_t file_id;
		struct smb_wire_string short_name;
		struct smb_wire_string name;
	} id_both_directory_info;

	/* RAW_SEARCH_UNIX_INFO interface */
	struct {
		uint32_t file_index;
		uint64_t size;
		uint64_t alloc_size;
		NTTIME status_change_time;
		NTTIME access_time;
		NTTIME change_time;
		uint64_t uid;
		uint64_t gid;
		uint32_t file_type;
		uint64_t dev_major;
		uint64_t dev_minor;
		uint64_t unique_id;
		uint64_t permissions;
		uint64_t nlink;		
		const char *name;
	} unix_info;
};


enum smb_search_close_level {RAW_FINDCLOSE_GENERIC, RAW_FINDCLOSE_FCLOSE, RAW_FINDCLOSE_FINDCLOSE};

/* union for file search close */
union smb_search_close {
	struct {
		enum smb_search_close_level level;
	} generic;

	/* SMBfclose (old search) interface */
	struct {
		enum smb_search_close_level level;
	
		struct {
			/* max_count and search_attrib are not used, but are present */
			uint16_t max_count;
			uint16_t search_attrib;
			struct smb_search_id id;
		} in;
	} fclose;
	
	/* SMBfindclose interface */
	struct {
		enum smb_search_close_level level;
		
		struct {
			uint16_t handle;
		} in;
	} findclose;
};

#endif /* __LIBCLI_RAW_INTERFACES_H__ */
