/* 
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc operations

   Copyright (C) Tim Potter 2003
   
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

static BOOL test_EnumPrinters(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	struct spoolss_EnumPrinters r;
	NTSTATUS status;

	r.in.flags = 0x02;
	r.in.server = "\\\\movingforward";
	r.in.level = 1;
	r.in.buffer = NULL;
	r.in.offered = 0;

	status = dcerpc_spoolss_EnumPrinters(p, mem_ctx, &r);

	if (NT_STATUS_IS_ERR(status)) {
		printf("OpenPrinter failed - %s\n", nt_errstr(status));
		return False;
	}

	if (NT_STATUS_V(status) == 0x0000007a) {
		struct uint8_buf buffer;

		r.in.offered = r.out.needed;
		buffer.size = r.out.needed;
		buffer.data = talloc(mem_ctx, buffer.size);
		memset(buffer.data, 0xfe, buffer.size);
		r.in.buffer = &buffer;
		status = dcerpc_spoolss_EnumPrinters(p, mem_ctx, &r);
	}

	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenPrinter failed - %s\n", nt_errstr(status));
		return False;
	}
	
	return True;
}

static BOOL test_OpenPrinterEx(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle)
{
	struct spoolss_OpenPrinterEx r;
	struct spoolss_UserLevel1 userlevel1;
	NTSTATUS status;

	r.in.printername = "\\\\movingforward\\p";
	r.in.datatype = NULL;
	r.in.devmode_ctr.size = 0;
	r.in.devmode_ctr.devmode = NULL;
	r.in.access_required = 0x02000000;
	r.in.level = 1;
	r.out.handle = handle;

	userlevel1.size = 1234;
	userlevel1.client = "hello";
	userlevel1.user = "spottyfoot!";
	userlevel1.build = 1;
	userlevel1.major = 2;
	userlevel1.minor = 3;
	userlevel1.processor = 4;
	r.in.userlevel.level1 = &userlevel1;

	status = dcerpc_spoolss_OpenPrinterEx(p, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenPrinter failed - %s\n", nt_errstr(status));
		return False;
	}

	return True;
}

static BOOL test_ClosePrinter(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			      struct policy_handle *handle)
{
	struct spoolss_ClosePrinter r;
	struct policy_handle handle2;
	NTSTATUS status;

	r.in.handle = handle;
	r.out.handle = &handle2;

	status = dcerpc_spoolss_ClosePrinter(p, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		printf("ClosePrinter failed - %s\n", nt_errstr(status));
		return False;
	}

	return True;
}

BOOL torture_rpc_spoolss(int dummy)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	BOOL ret = True;
	struct policy_handle handle;

	mem_ctx = talloc_init("torture_rpc_spoolss");

	status = torture_rpc_connection(&p, "spoolss");
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}
	
	if (!test_EnumPrinters(p, mem_ctx)) {
		ret = False;
	}

	if (!test_OpenPrinterEx(p, mem_ctx, &handle)) {
		ret = False;
	}

	if (!test_ClosePrinter(p, mem_ctx, &handle)) {
		ret = False;
	}

        torture_rpc_close(p);

	return ret;
}
