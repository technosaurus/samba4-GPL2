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

BOOL test_GetPrinter(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		     struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_GetPrinter r;
	uint32 buf_size = 0;
	DATA_BLOB blob;
	union spoolss_PrinterInfo info;
	uint16 levels[] = {1, 2, 3, 4, 5, 6, 7};
	int i;
	BOOL ret = True;
	
	for (i=0;i<ARRAY_SIZE(levels);i++) {
		r.in.handle = handle;
		r.in.level = levels[i];
		r.in.buffer = NULL;
		r.in.buf_size = &buf_size;
		r.out.buf_size = &buf_size;

		printf("Testing GetPrinter level %u\n", r.in.level);

		status = dcerpc_spoolss_GetPrinter(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("GetPrinter failed - %s\n", nt_errstr(status));
			ret = False;
			continue;
		}
		
		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			blob = data_blob_talloc(mem_ctx, NULL, buf_size);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			status = dcerpc_spoolss_GetPrinter(p, mem_ctx, &r);
		}
		
		if (!NT_STATUS_IS_OK(status) ||
		    !W_ERROR_IS_OK(r.out.result)) {
			printf("GetPrinter failed - %s/%s\n", 
			       nt_errstr(status), win_errstr(r.out.result));
			ret = False;
			continue;
		}
		
		status = ndr_pull_union_blob(r.out.buffer, mem_ctx, r.in.level, &info,
					     (ndr_pull_union_fn_t)ndr_pull_spoolss_PrinterInfo);
		if (!NT_STATUS_IS_OK(status)) {
			printf("PrinterInfo parse failed - %s\n", nt_errstr(status));
			ret = False;
			continue;
		}
		
		NDR_PRINT_UNION_DEBUG(spoolss_PrinterInfo, r.in.level, &info);
	}

	return ret;
}


BOOL test_ClosePrinter(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		       struct policy_handle *handle)
{
	NTSTATUS status;
	struct spoolss_ClosePrinter r;

	r.in.handle = handle;
	r.out.handle = handle;

	printf("Testing ClosePrinter\n");

	status = dcerpc_spoolss_ClosePrinter(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ClosePrinter failed - %s\n", nt_errstr(status));
		return False;
	}

	return True;
}

static BOOL test_OpenPrinter(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			     const char *name)
{
	NTSTATUS status;
	struct spoolss_OpenPrinter r;
	struct policy_handle handle;
	DATA_BLOB blob;
	BOOL ret = True;

	blob = data_blob(NULL, 0);

	r.in.server = talloc_asprintf(mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.printer = name;
	r.in.buffer = &blob;
	r.in.access_mask = SEC_RIGHTS_MAXIMUM_ALLOWED;	
	r.out.handle = &handle;

	printf("\nTesting OpenPrinter(\\\\%s\\%s)\n", r.in.server, r.in.printer);

	status = dcerpc_spoolss_OpenPrinter(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		printf("OpenPrinter failed - %s/%s\n", 
		       nt_errstr(status), win_errstr(r.out.result));
		return False;
	}


	if (!test_GetPrinter(p, mem_ctx, &handle)) {
		ret = False;
	}

	if (!test_ClosePrinter(p, mem_ctx, &handle)) {
		ret = False;
	}
	
	return ret;
}

static BOOL test_OpenPrinterEx(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			       const char *name)
{
	struct policy_handle handle;
	struct spoolss_OpenPrinterEx r;
	struct spoolss_UserLevel1 userlevel1;
	NTSTATUS status;
	BOOL ret = True;

	r.in.printername = talloc_asprintf(mem_ctx, "\\\\%s\\%s", 
					   dcerpc_server_name(p), name);
	r.in.datatype = NULL;
	r.in.devmode_ctr.size = 0;
	r.in.devmode_ctr.devmode = NULL;
	r.in.access_required = 0x02000000;
	r.in.level = 1;
	r.out.handle = &handle;

	userlevel1.size = 1234;
	userlevel1.client = "hello";
	userlevel1.user = "spottyfoot!";
	userlevel1.build = 1;
	userlevel1.major = 2;
	userlevel1.minor = 3;
	userlevel1.processor = 4;
	r.in.userlevel.level1 = &userlevel1;

	printf("Testing OpenPrinterEx(%s)\n", r.in.printername);

	status = dcerpc_spoolss_OpenPrinterEx(p, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenPrinterEx failed - %s\n", nt_errstr(status));
		return False;
	}

	if (!W_ERROR_IS_OK(r.out.result)) {
		printf("OpenPrinterEx failed - %s\n", win_errstr(r.out.result));
		return False;
	}

	if (!test_GetPrinter(p, mem_ctx, &handle)) {
		ret = False;
	}

	if (!test_ClosePrinter(p, mem_ctx, &handle)) {
		ret = False;
	}

	return ret;
}


static BOOL test_EnumPrinters(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	struct spoolss_EnumPrinters r;
	NTSTATUS status;
	uint16 levels[] = {1, 2, 3, 4, 5, 6, 7};
	int i;
	BOOL ret = True;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		uint32 buf_size = 0;
		union spoolss_PrinterInfo *info;
		int j;

		r.in.flags = 0x02;
		r.in.server = "";
		r.in.level = levels[i];
		r.in.buffer = NULL;
		r.in.buf_size = &buf_size;
		r.out.buf_size = &buf_size;

		printf("\nTesting EnumPrinters level %u\n", r.in.level);

		status = dcerpc_spoolss_EnumPrinters(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("EnumPrinters failed - %s\n", nt_errstr(status));
			ret = False;
			continue;
		}
		
		if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
			DATA_BLOB blob = data_blob_talloc(mem_ctx, NULL, buf_size);
			data_blob_clear(&blob);
			r.in.buffer = &blob;
			status = dcerpc_spoolss_EnumPrinters(p, mem_ctx, &r);
		}
		
		if (!NT_STATUS_IS_OK(status) ||
		    !W_ERROR_IS_OK(r.out.result)) {
			printf("EnumPrinters failed - %s/%s\n", 
			       nt_errstr(status), win_errstr(r.out.result));
			continue;
		}

		status = pull_spoolss_PrinterInfoArray(r.out.buffer, mem_ctx, r.in.level, r.out.count, &info);
		if (!NT_STATUS_IS_OK(status)) {
			printf("EnumPrintersArray parse failed - %s\n", nt_errstr(status));
			continue;
		}

		for (j=0;j<r.out.count;j++) {
			printf("Printer %d\n", j);
			NDR_PRINT_UNION_DEBUG(spoolss_PrinterInfo, r.in.level, &info[j]);
		}

		for (j=0;j<r.out.count;j++) {
			if (r.in.level == 1) {
				/* the names appear to be comma-separated name lists? */
				char *name = talloc_strdup(mem_ctx, info[j].info1.name);
				char *comma = strchr(name, ',');
				if (comma) *comma = 0;
				if (!test_OpenPrinter(p, mem_ctx, name)) {
					ret = False;
				}

				if (!test_OpenPrinterEx(p, mem_ctx, name)) {
					ret = False;
				}
			}
		}
	}
	
	return ret;
}

BOOL torture_rpc_spoolss(int dummy)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	BOOL ret = True;

	mem_ctx = talloc_init("torture_rpc_spoolss");

	status = torture_rpc_connection(&p, "spoolss");
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}
	
	if (!test_EnumPrinters(p, mem_ctx)) {
		ret = False;
	}

        torture_rpc_close(p);

	return ret;
}
