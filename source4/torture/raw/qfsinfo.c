/* 
   Unix SMB/CIFS implementation.
   RAW_QFS_* individual test suite
   Copyright (C) Andrew Tridgell 2003
   
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
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "torture/util.h"


static struct {
	const char *name;
	enum smb_fsinfo_level level;
	uint32_t capability_mask;
	NTSTATUS status;
	union smb_fsinfo fsinfo;
} levels[] = {
	{"DSKATTR",               RAW_QFS_DSKATTR, },
	{"ALLOCATION",            RAW_QFS_ALLOCATION, },
	{"VOLUME",                RAW_QFS_VOLUME, },
	{"VOLUME_INFO",           RAW_QFS_VOLUME_INFO, },
	{"SIZE_INFO",             RAW_QFS_SIZE_INFO, },
	{"DEVICE_INFO",           RAW_QFS_DEVICE_INFO, },
	{"ATTRIBUTE_INFO",        RAW_QFS_ATTRIBUTE_INFO, },
	{"UNIX_INFO",             RAW_QFS_UNIX_INFO,            CAP_UNIX},
	{"VOLUME_INFORMATION",    RAW_QFS_VOLUME_INFORMATION, },
	{"SIZE_INFORMATION",      RAW_QFS_SIZE_INFORMATION, },
	{"DEVICE_INFORMATION",    RAW_QFS_DEVICE_INFORMATION, },
	{"ATTRIBUTE_INFORMATION", RAW_QFS_ATTRIBUTE_INFORMATION, },
	{"QUOTA_INFORMATION",     RAW_QFS_QUOTA_INFORMATION, },
	{"FULL_SIZE_INFORMATION", RAW_QFS_FULL_SIZE_INFORMATION, },
#if 0
	/* w2k3 seems to no longer support this */
	{"OBJECTID_INFORMATION",  RAW_QFS_OBJECTID_INFORMATION, },
#endif
	{ NULL, }
};


/*
  find a level in the levels[] table
*/
static union smb_fsinfo *find(const char *name)
{
	int i;
	for (i=0; levels[i].name; i++) {
		if (strcmp(name, levels[i].name) == 0 &&
		    NT_STATUS_IS_OK(levels[i].status)) {
			return &levels[i].fsinfo;
		}
	}
	return NULL;
}

/* local macros to make the code below more readable */
#define VAL_EQUAL(n1, v1, n2, v2) do {if (s1->n1.out.v1 != s2->n2.out.v2) { \
        printf("%s/%s [%u] != %s/%s [%u] at %s(%d)\n", \
               #n1, #v1, (uint_t)s1->n1.out.v1, \
               #n2, #v2, (uint_t)s2->n2.out.v2, \
	       __FILE__, __LINE__); \
        ret = False; \
}} while(0)

#define VAL_APPROX_EQUAL(n1, v1, n2, v2) do {if (abs((int)(s1->n1.out.v1) - (int)(s2->n2.out.v2)) > 0.1*s1->n1.out.v1) { \
        printf("%s/%s [%u] != %s/%s [%u] at %s(%d)\n", \
               #n1, #v1, (uint_t)s1->n1.out.v1, \
               #n2, #v2, (uint_t)s2->n2.out.v2, \
	       __FILE__, __LINE__); \
        ret = False; \
}} while(0)

#define STR_EQUAL(n1, v1, n2, v2) do { \
       if (strcmp_safe(s1->n1.out.v1, s2->n2.out.v2)) { \
         printf("%s/%s [%s] != %s/%s [%s] at %s(%d)\n", \
               #n1, #v1, s1->n1.out.v1, \
               #n2, #v2, s2->n2.out.v2, \
	       __FILE__, __LINE__); \
        ret = False; \
}} while(0)

#define STRUCT_EQUAL(n1, v1, n2, v2) do {if (memcmp(&s1->n1.out.v1,&s2->n2.out.v2,sizeof(s1->n1.out.v1))) { \
        printf("%s/%s != %s/%s at %s(%d)\n", \
               #n1, #v1, \
               #n2, #v2, \
	       __FILE__, __LINE__); \
        ret = False; \
}} while(0)

/* used to find hints on unknown values - and to make sure 
   we zero-fill */
#define VAL_UNKNOWN(n1, v1) do {if (s1->n1.out.v1 != 0) { \
        printf("%s/%s non-zero unknown - %u (0x%x) at %s(%d)\n", \
               #n1, #v1, \
	       (uint_t)s1->n1.out.v1, \
	       (uint_t)s1->n1.out.v1, \
	       __FILE__, __LINE__); \
        ret = False; \
}} while(0)

/* basic testing of all RAW_QFS_* calls 
   for each call we test that it succeeds, and where possible test 
   for consistency between the calls. 

   Some of the consistency tests assume that the target filesystem is
   quiescent, which is sometimes hard to achieve
*/
BOOL torture_raw_qfsinfo(struct torture_context *torture)
{
	struct smbcli_state *cli;
	int i;
	BOOL ret = True;
	int count;
	union smb_fsinfo *s1, *s2;	
	TALLOC_CTX *mem_ctx;

	if (!torture_open_connection(&cli, 0)) {
		return False;
	}

	mem_ctx = talloc_init("torture_qfsinfo");
	
	/* scan all the levels, pulling the results */
	for (i=0; levels[i].name; i++) {
		printf("Running level %s\n", levels[i].name);
		levels[i].fsinfo.generic.level = levels[i].level;
		levels[i].status = smb_raw_fsinfo(cli->tree, mem_ctx, &levels[i].fsinfo);
	}

	/* check for completely broken levels */
	for (count=i=0; levels[i].name; i++) {
		uint32_t cap = cli->transport->negotiate.capabilities;
		/* see if this server claims to support this level */
		if ((cap & levels[i].capability_mask) != levels[i].capability_mask) {
			continue;
		}
		
		if (!NT_STATUS_IS_OK(levels[i].status)) {
			printf("ERROR: level %s failed - %s\n", 
			       levels[i].name, nt_errstr(levels[i].status));
			count++;
		}
	}

	if (count != 0) {
		ret = False;
		printf("%d levels failed\n", count);
		if (count > 13) {
			printf("too many level failures - giving up\n");
			goto done;
		}
	}

	printf("check for correct aliases\n");
	s1 = find("SIZE_INFO");
	s2 = find("SIZE_INFORMATION");
	if (s1 && s2) {
		VAL_EQUAL(size_info, total_alloc_units, size_info, total_alloc_units);
		VAL_APPROX_EQUAL(size_info, avail_alloc_units, size_info, avail_alloc_units);
		VAL_EQUAL(size_info, sectors_per_unit,  size_info, sectors_per_unit);
		VAL_EQUAL(size_info, bytes_per_sector,  size_info, bytes_per_sector);
	}	

	s1 = find("DEVICE_INFO");
	s2 = find("DEVICE_INFORMATION");
	if (s1 && s2) {
		VAL_EQUAL(device_info, device_type,     device_info, device_type);
		VAL_EQUAL(device_info, characteristics, device_info, characteristics);
	}	

	s1 = find("VOLUME_INFO");
	s2 = find("VOLUME_INFORMATION");
	if (s1 && s2) {
		STRUCT_EQUAL(volume_info, create_time,    volume_info, create_time);
		VAL_EQUAL   (volume_info, serial_number,  volume_info, serial_number);
		STR_EQUAL   (volume_info, volume_name.s,    volume_info, volume_name.s);
		printf("volume_info.volume_name = '%s'\n", s1->volume_info.out.volume_name.s);
	}	

	s1 = find("ATTRIBUTE_INFO");
	s2 = find("ATTRIBUTE_INFORMATION");
	if (s1 && s2) {
		VAL_EQUAL(attribute_info, fs_attr,    
			  attribute_info, fs_attr);
		VAL_EQUAL(attribute_info, max_file_component_length, 
			  attribute_info, max_file_component_length);
		STR_EQUAL(attribute_info, fs_type.s, attribute_info, fs_type.s);
		printf("attribute_info.fs_type = '%s'\n", s1->attribute_info.out.fs_type.s);
	}	

	printf("check for consistent disk sizes\n");
	s1 = find("DSKATTR");
	s2 = find("ALLOCATION");
	if (s1 && s2) {
		double size1, size2;
		double scale = s1->dskattr.out.blocks_per_unit * s1->dskattr.out.block_size;
		size1 = 1.0 * 
			s1->dskattr.out.units_total * 
			s1->dskattr.out.blocks_per_unit * 
			s1->dskattr.out.block_size / scale;
		size2 = 1.0 *
			s2->allocation.out.sectors_per_unit *
			s2->allocation.out.total_alloc_units *
			s2->allocation.out.bytes_per_sector / scale;
		if (abs(size1 - size2) > 1) {
			printf("Inconsistent total size in DSKATTR and ALLOCATION - size1=%.0f size2=%.0f\n", 
			       size1, size2);
			ret = False;
		}
		printf("total disk = %.0f MB\n", size1*scale/1.0e6);
	}

	printf("check consistent free disk space\n");
	s1 = find("DSKATTR");
	s2 = find("ALLOCATION");
	if (s1 && s2) {
		double size1, size2;
		double scale = s1->dskattr.out.blocks_per_unit * s1->dskattr.out.block_size;
		size1 = 1.0 * 
			s1->dskattr.out.units_free * 
			s1->dskattr.out.blocks_per_unit * 
			s1->dskattr.out.block_size / scale;
		size2 = 1.0 *
			s2->allocation.out.sectors_per_unit *
			s2->allocation.out.avail_alloc_units *
			s2->allocation.out.bytes_per_sector / scale;
		if (abs(size1 - size2) > 1) {
			printf("Inconsistent avail size in DSKATTR and ALLOCATION - size1=%.0f size2=%.0f\n", 
			       size1, size2);
			ret = False;
		}
		printf("free disk = %.0f MB\n", size1*scale/1.0e6);
	}
	
	printf("volume info consistency\n");
	s1 = find("VOLUME");
	s2 = find("VOLUME_INFO");
	if (s1 && s2) {
		VAL_EQUAL(volume, serial_number,  volume_info, serial_number);
		STR_EQUAL(volume, volume_name.s,  volume_info, volume_name.s);
	}	

	/* disk size consistency - notice that 'avail_alloc_units' maps to the caller
	   available allocation units, not the total */
	s1 = find("SIZE_INFO");
	s2 = find("FULL_SIZE_INFORMATION");
	if (s1 && s2) {
		VAL_EQUAL(size_info, total_alloc_units, full_size_information, total_alloc_units);
		VAL_APPROX_EQUAL(size_info, avail_alloc_units, full_size_information, call_avail_alloc_units);
		VAL_EQUAL(size_info, sectors_per_unit,  full_size_information, sectors_per_unit);
		VAL_EQUAL(size_info, bytes_per_sector,  full_size_information, bytes_per_sector);
	}	

	printf("check for non-zero unknown fields\n");
	s1 = find("QUOTA_INFORMATION");
	if (s1) {
		VAL_UNKNOWN(quota_information, unknown[0]);
		VAL_UNKNOWN(quota_information, unknown[1]);
		VAL_UNKNOWN(quota_information, unknown[2]);
	}

	s1 = find("OBJECTID_INFORMATION");
	if (s1) {
		VAL_UNKNOWN(objectid_information, unknown[0]);
		VAL_UNKNOWN(objectid_information, unknown[1]);
		VAL_UNKNOWN(objectid_information, unknown[2]);
		VAL_UNKNOWN(objectid_information, unknown[3]);
		VAL_UNKNOWN(objectid_information, unknown[4]);
		VAL_UNKNOWN(objectid_information, unknown[5]);
	}


#define STR_CHECK(sname, stype, field, flags) do { \
	s1 = find(sname); \
	if (s1) { \
		if (s1->stype.out.field.s && wire_bad_flags(&s1->stype.out.field, flags, cli->transport)) { \
			printf("(%d) incorrect string termination in %s/%s\n", \
			       __LINE__, #stype, #field); \
			ret = False; \
		} \
	}} while (0)

	printf("check for correct termination\n");
	
	STR_CHECK("VOLUME",                volume,         volume_name, 0);
	STR_CHECK("VOLUME_INFO",           volume_info,    volume_name, STR_UNICODE);
	STR_CHECK("VOLUME_INFORMATION",    volume_info,    volume_name, STR_UNICODE);
	STR_CHECK("ATTRIBUTE_INFO",        attribute_info, fs_type, STR_UNICODE);
	STR_CHECK("ATTRIBUTE_INFORMATION", attribute_info, fs_type, STR_UNICODE);

done:
	torture_close_connection(cli);
	talloc_free(mem_ctx);
	return ret;
}
