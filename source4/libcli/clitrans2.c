/* 
   Unix SMB/CIFS implementation.
   client trans2 calls
   Copyright (C) James J Myers 2003	<myersjj@samba.org>
   
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

#include "includes.h"

/****************************************************************************
send a qpathinfo call
****************************************************************************/
BOOL cli_qpathinfo(struct cli_state *cli, const char *fname, 
		   time_t *c_time, time_t *a_time, time_t *m_time, 
		   size_t *size, uint16 *mode)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("cli_qpathinfo");
	if (!mem_ctx) return False;

	parms.standard.level = RAW_FILEINFO_STANDARD;
	parms.standard.in.fname = fname;

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &parms);
	talloc_destroy(mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	if (c_time) {
		*c_time = parms.standard.out.create_time;
	}
	if (a_time) {
		*a_time = parms.standard.out.access_time;
	}
	if (m_time) {
		*m_time = parms.standard.out.write_time;
	}
	if (size) {
		*size = parms.standard.out.size;
	}
	if (mode) {
		*mode = parms.standard.out.attrib;
	}

	return True;
}

/****************************************************************************
send a qpathinfo call with the SMB_QUERY_FILE_ALL_INFO info level
****************************************************************************/
BOOL cli_qpathinfo2(struct cli_state *cli, const char *fname, 
		    time_t *c_time, time_t *a_time, time_t *m_time, 
		    time_t *w_time, size_t *size, uint16 *mode,
		    SMB_INO_T *ino)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("cli_qfilename");
	if (!mem_ctx) return False;

	parms.all_info.level = RAW_FILEINFO_ALL_INFO;
	parms.all_info.in.fname = fname;

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &parms);
	talloc_destroy(mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	if (c_time) {
		*c_time = nt_time_to_unix(&parms.all_info.out.create_time);
	}
	if (a_time) {
		*a_time = nt_time_to_unix(&parms.all_info.out.access_time);
	}
	if (m_time) {
		*m_time = nt_time_to_unix(&parms.all_info.out.change_time);
	}
	if (w_time) {
		*w_time = nt_time_to_unix(&parms.all_info.out.write_time);
	}
	if (size) {
		*size = parms.all_info.out.size;
	}
	if (mode) {
		*mode = parms.all_info.out.attrib;
	}

	return True;
}


/****************************************************************************
send a qfileinfo QUERY_FILE_NAME_INFO call
****************************************************************************/
BOOL cli_qfilename(struct cli_state *cli, int fnum, 
		   const char **name)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("cli_qfilename");
	if (!mem_ctx) return False;

	parms.name_info.level = RAW_FILEINFO_NAME_INFO;
	parms.name_info.in.fnum = fnum;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &parms);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_destroy(mem_ctx);
		*name = NULL;
		return False;
	}

	*name = strdup(parms.name_info.out.fname.s);

	talloc_destroy(mem_ctx);

	return True;
}


/****************************************************************************
send a qfileinfo call
****************************************************************************/
BOOL cli_qfileinfo(struct cli_state *cli, int fnum, 
		   uint16 *mode, size_t *size,
		   time_t *c_time, time_t *a_time, time_t *m_time, 
		   time_t *w_time, SMB_INO_T *ino)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("cli_qfileinfo");
	if (!mem_ctx) return False;

	parms.all_info.level = RAW_FILEINFO_ALL_INFO;
	parms.all_info.in.fnum = fnum;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &parms);
	talloc_destroy(mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	if (c_time) {
		*c_time = nt_time_to_unix(&parms.all_info.out.create_time);
	}
	if (a_time) {
		*a_time = nt_time_to_unix(&parms.all_info.out.access_time);
	}
	if (m_time) {
		*m_time = nt_time_to_unix(&parms.all_info.out.change_time);
	}
	if (w_time) {
		*w_time = nt_time_to_unix(&parms.all_info.out.write_time);
	}
	if (mode) {
		*mode = parms.all_info.out.attrib;
	}
	if (size) {
		*size = (size_t)parms.all_info.out.size;
	}
	if (ino) {
		*ino = 0;
	}

	return True;
}


/****************************************************************************
send a qpathinfo SMB_QUERY_FILE_ALT_NAME_INFO call
****************************************************************************/
NTSTATUS cli_qpathinfo_alt_name(struct cli_state *cli, const char *fname, 
				const char **alt_name)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	parms.alt_name_info.level = RAW_FILEINFO_ALT_NAME_INFO;
	parms.alt_name_info.in.fname = fname;

	mem_ctx = talloc_init("cli_qpathinfo_alt_name");
	if (!mem_ctx) return NT_STATUS_NO_MEMORY;

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &parms);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_destroy(mem_ctx);
		*alt_name = NULL;
		return cli_nt_error(cli);
	}

	if (!parms.alt_name_info.out.fname.s) {
		*alt_name = strdup("");
	} else {
		*alt_name = strdup(parms.alt_name_info.out.fname.s);
	}

	talloc_destroy(mem_ctx);

	return NT_STATUS_OK;
}
