/* 
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak 2005
   
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
#include "libnet/libnet.h"
#include "libnet/composite.h"
#include "libcli/composite/monitor.h"
#include "librpc/gen_ndr/ndr_nbt.h"


BOOL torture_lookup(void)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_Lookup lookup;
	const char address[16];

	mem_ctx = talloc_init("test_lookup");

	lookup.in.hostname = lp_netbios_name();
	lookup.in.methods  = lp_name_resolve_order();
	lookup.in.type     = NBT_NAME_CLIENT;
	lookup.out.address = (const char**)&address;

	status = libnet_Lookup(mem_ctx, &lookup);

	if (!NT_STATUS_IS_OK(status)) {
		printf("Couldn't lookup name %s: %s\n", lookup.in.hostname, nt_errstr(status));
		return False;
	}

	return True;
}


BOOL torture_lookup_host(void)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_Lookup lookup;
	const char address[16];

	mem_ctx = talloc_init("test_lookup_host");

	lookup.in.hostname = lp_netbios_name();
	lookup.in.methods  = lp_name_resolve_order();
	lookup.out.address = (const char**)&address;

	status = libnet_LookupHost(mem_ctx, &lookup);

	if (!NT_STATUS_IS_OK(status)) {
		printf("Couldn't lookup host %s: %s\n", lookup.in.hostname, nt_errstr(status));
		return False;
	}

	return True;
}


BOOL torture_lookup_pdc(void)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_Lookup lookup;
	const char address[16];

	mem_ctx = talloc_init("test_lookup_pdc");

	lookup.in.hostname = lp_workgroup();
	lookup.in.methods  = lp_name_resolve_order();
	lookup.out.address = (const char**)&address;

	status = libnet_LookupPdc(mem_ctx, &lookup);

	if (!NT_STATUS_IS_OK(status)) {
		printf("Couldn't lookup pdc %s: %s\n", lookup.in.hostname, nt_errstr(status));
		return False;
	}

	return True;
}
