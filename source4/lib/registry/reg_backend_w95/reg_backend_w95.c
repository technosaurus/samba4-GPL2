/*
   Samba Unix/Linux SMB client utility libeditreg.c 
   Copyright (C) 2004 Jelmer Vernooij, jelmer@samba.org

   Backend for Windows '95 registry files. Explanation of file format 
   comes from http://www.cs.mun.ca/~michael/regutils/.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "includes.h"
#include "lib/registry/common/registry.h"

/**
 * The registry starts with a header that contains pointers to 
 * the rgdb.
 *
 * After the main header follows the RGKN header (key index table).
 * The RGKN keys are listed after each other. They are put into 
 * blocks, the first having a length of 0x2000 bytes, the others 
 * being 0x1000 bytes long.
 *
 * After the RGKN header follow one or more RGDB blocks. These blocks 
 * contain keys. A key is followed by its name and its values.
 *
 * Values are followed by their name and then their data.
 *
 * Basically the idea is that the RGKN contains the associations between 
 * the keys and the RGDB contains the actual data.
 */

typedef unsigned int DWORD;
typedef unsigned short WORD;

typedef struct creg_block {
	DWORD CREG_ID;		/* CREG */
	DWORD uk1;
	DWORD rgdb_offset;
	DWORD chksum;
	WORD  num_rgdb;
	WORD  flags;
	DWORD uk2;
	DWORD uk3;
	DWORD uk4;
} CREG_HDR;

typedef struct rgkn_block {
	DWORD RGKN_ID; 		/* RGKN */
	DWORD size;
	DWORD root_offset;
	DWORD free_offset;
	DWORD flags;
	DWORD chksum;
	DWORD uk1;
	DWORD uk2;
} RGKN_HDR;

typedef struct reg_id {
	WORD id;
	WORD rgdb;
} REG_ID;

typedef struct rgkn_key {
	DWORD type;			/* 0x00000000 = normal key, 0x80000000 = free block */
	DWORD hash;			/* Contains either hash or size of free blocks that follows */
	DWORD next_free;
	DWORD parent_offset;
	DWORD first_child_offset;
	DWORD next_offset;
	REG_ID id;
} RGKN_KEY;


typedef struct rgdb_block {
	DWORD RGDB_ID;		/* RGDB */
	DWORD size;
	DWORD unused_size;
	WORD flags;
	WORD section;
	DWORD free_offset;	/* -1 if there is no free space */
	WORD max_id;
	WORD first_free_id;
	DWORD uk1;
	DWORD chksum;
} RGDB_HDR;

typedef struct rgdb_key {
	DWORD size;
	REG_ID id;
	DWORD used_size;
	WORD  name_len;
	WORD  num_values;
	DWORD uk1;
} RGDB_KEY;

typedef struct rgdb_value {
	DWORD type;
	DWORD uk1;
	WORD name_len;
	WORD data_len;
} RGDB_VALUE;

typedef struct creg_struct_s {
	int fd;
	BOOL modified;
	char *base;
	struct stat sbuf;
	CREG_HDR *creg_hdr;
	RGKN_HDR *rgkn_hdr;
	RGDB_KEY ***rgdb_keys;
} CREG;

#define RGKN_START_SIZE 0x2000
#define RGKN_INC_SIZE   0x1000

#define LOCN_RGKN(creg, o) ((RGKN_KEY *)((creg)->base + sizeof(CREG_HDR) + o))
#define LOCN_RGDB_BLOCK(creg, o) (((creg)->base + (creg)->creg_hdr->rgdb_offset + o))
#define LOCN_RGDB_KEY(creg, rgdb, id) ((RGDB_KEY *)((creg)->rgdb_keys[(rgdb)][(id)]))

static DWORD str_to_dword(const char *a) {
    int i;
    unsigned long ret = 0;
    for(i = strlen(a)-1; i >= 0; i--) {
        ret = ret * 0x100 + a[i];
    }
    return ret;
}

static DWORD calc_hash(const char *str) {
	DWORD ret = 0;
	int i;
	for(i = 0; str[i] && str[i] != '\\'; i++) {
		ret+=toupper(str[i]);
	}
	return ret;
}

static void parse_rgkn_block(CREG *creg, off_t start_off, off_t end_off) 
{
	off_t i;
	for(i = start_off; end_off - i > sizeof(RGKN_KEY); i+= sizeof(RGKN_KEY)) {
		RGKN_KEY *key = (RGKN_KEY *)LOCN_RGKN(creg, i);
		if(key->type == 0) {
			DEBUG(4,("Regular, id: %d, %d, parent: %x, firstchild: %x, next: %x hash: %lX\n", key->id.id, key->id.rgdb, key->parent_offset, key->first_child_offset, key->next_offset, key->hash));
		} else if(key->type == 0x80000000) {
			DEBUG(3,("free\n"));
			i += key->hash;
		} else {
			DEBUG(0,("Invalid key type in RGKN: %0X\n", key->type));
		}
	}
}

static void parse_rgdb_block(CREG *creg, RGDB_HDR *rgdb_hdr)
{
	DWORD used_size = rgdb_hdr->size - rgdb_hdr->unused_size;
	DWORD offset = 0;

	while(offset < used_size) {
		RGDB_KEY *key = (RGDB_KEY *)(((char *)rgdb_hdr) + sizeof(RGDB_HDR) + offset);
		
		if(!(key->id.id == 0xFFFF && key->id.rgdb == 0xFFFF))creg->rgdb_keys[key->id.rgdb][key->id.id] = key;
		offset += key->size;
	}
}

static WERROR w95_open_root (REG_HANDLE *h, REG_KEY **key)
{
	CREG *creg = h->backend_data;
	
	/* First element in rgkn should be root key */
	*key = reg_key_new_abs("\\", h, LOCN_RGKN(creg, sizeof(RGKN_HDR)));
	
	return WERR_OK;
}

static WERROR w95_get_subkey_by_index (REG_KEY *parent, int n, REG_KEY **key)
{
	CREG *creg = parent->handle->backend_data;
	RGKN_KEY *rgkn_key = parent->backend_data;
	RGKN_KEY *child;
	DWORD child_offset;
	DWORD cur = 0;
	
	/* Get id of first child */
	child_offset = rgkn_key->first_child_offset;

	while(child_offset != 0xFFFFFFFF) {
		child = LOCN_RGKN(creg, child_offset);

		/* n == cur ? return! */
		if(cur == n) {
			RGDB_KEY *rgdb_key;
			char *name;
			rgdb_key = LOCN_RGDB_KEY(creg, child->id.rgdb, child->id.id);
			if(!rgdb_key) {
				DEBUG(0, ("Can't find %d,%d in RGDB table!\n", child->id.rgdb, child->id.id));
				return WERR_FOOBAR;
			}
			name = strndup((char *)rgdb_key + sizeof(RGDB_KEY), rgdb_key->name_len);
			*key = reg_key_new_rel(name, parent, child);
			SAFE_FREE(name);
			return WERR_OK;
		}

		cur++;
		
		child_offset = child->next_offset;
	}

	return WERR_NO_MORE_ITEMS;
}

static WERROR w95_open_reg (REG_HANDLE *h, const char *location, const char *credentials)
{
	CREG *creg = talloc_p(h->mem_ctx, CREG);
	DWORD creg_id, rgkn_id;
	memset(creg, 0, sizeof(CREG));
	h->backend_data = creg;
	DWORD i, nfree = 0;
	DWORD offset, end_offset;

	if((creg->fd = open(location, O_RDONLY, 0000)) < 0) {
		return WERR_FOOBAR;
	}

    if (fstat(creg->fd, &creg->sbuf) < 0) {
		return WERR_FOOBAR;
    }

    creg->base = mmap(0, creg->sbuf.st_size, PROT_READ, MAP_SHARED, creg->fd, 0);
                                                                                                                                              
    if ((int)creg->base == 1) {
		DEBUG(0,("Could not mmap file: %s, %s\n", location, strerror(errno)));
        return WERR_FOOBAR;
    }

	creg->creg_hdr = (CREG_HDR *)creg->base;

	if ((creg_id = IVAL(&creg->creg_hdr->CREG_ID,0)) != str_to_dword("CREG")) {
		DEBUG(0, ("Unrecognized Windows 95 registry header id: 0x%0X, %s\n", 
				  creg_id, location));
		return WERR_FOOBAR;
	}

	creg->rgkn_hdr = (RGKN_HDR *)LOCN_RGKN(creg, 0);

	if ((rgkn_id = IVAL(&creg->rgkn_hdr->RGKN_ID,0)) != str_to_dword("RGKN")) {
		DEBUG(0, ("Unrecognized Windows 95 registry key index id: 0x%0X, %s\n", 
				  rgkn_id, location));
		return WERR_FOOBAR;
	}

#if 0	
	/* If'ed out because we only need to parse this stuff when allocating new 
	 * entries (which we don't do at the moment */
	/* First parse the 0x2000 long block */
	parse_rgkn_block(creg, sizeof(RGKN_HDR), 0x2000);

	/* Then parse the other 0x1000 length blocks */
	for(offset = 0x2000; offset < creg->rgkn_hdr->size; offset+=0x1000) {
		parse_rgkn_block(creg, offset, offset+0x1000);
	}
#endif

	creg->rgdb_keys = talloc_array_p(h->mem_ctx, RGDB_KEY **, creg->creg_hdr->num_rgdb);

	offset = 0;
	DEBUG(3, ("Reading %d rgdb entries\n", creg->creg_hdr->num_rgdb));
	for(i = 0; i < creg->creg_hdr->num_rgdb; i++) {
		RGDB_HDR *rgdb_hdr = (RGDB_HDR *)LOCN_RGDB_BLOCK(creg, offset);
		
		if(strncmp((char *)&(rgdb_hdr->RGDB_ID), "RGDB", 4)) {
			DEBUG(0, ("unrecognized rgdb entry: %4s, %s\n", 
					  &rgdb_hdr->RGDB_ID, location));
			return WERR_FOOBAR;
		} else {
			DEBUG(3, ("Valid rgdb entry, first free id: %d, max id: %d\n", rgdb_hdr->first_free_id, rgdb_hdr->max_id));
		}


		creg->rgdb_keys[i] = talloc_array_p(h->mem_ctx, RGDB_KEY *, rgdb_hdr->max_id+1);
		memset(creg->rgdb_keys[i], 0, sizeof(RGDB_KEY *) * (rgdb_hdr->max_id+1));

		parse_rgdb_block(creg, rgdb_hdr);

		offset+=rgdb_hdr->size;
	}
	

	return WERR_OK;
}

static WERROR w95_close_reg(REG_HANDLE *h)
{
	CREG *creg = h->backend_data;
	if (creg->base) munmap(creg->base, creg->sbuf.st_size);
	creg->base = NULL;
    close(creg->fd);
	return WERR_OK;
}


static WERROR w95_fetch_values(REG_KEY *k, int *count, REG_VAL ***values)
{
	RGKN_KEY *rgkn_key = k->backend_data;
	RGDB_VALUE *val;
	DWORD i;
	DWORD offset = 0;
	RGDB_KEY *rgdb_key = LOCN_RGDB_KEY((CREG *)k->handle->backend_data, rgkn_key->id.rgdb, rgkn_key->id.id);

	if(!rgdb_key) return WERR_FOOBAR;
	
	*count = rgdb_key->num_values;
	
	if((*count) == 0) return WERR_OK;

	(*values) = talloc_array_p(k->mem_ctx, REG_VAL *, (*count)+1);
	for(i = 0; i < rgdb_key->num_values; i++) {
		RGDB_VALUE *val = (RGDB_VALUE *)(((char *)rgdb_key) + sizeof(RGDB_KEY) + rgdb_key->name_len + offset);
		(*values)[i] = reg_val_new(k, val);
		
		/* Name */
		(*values)[i]->name = talloc_strndup(k->mem_ctx, (char *)val+sizeof(RGDB_VALUE), val->name_len);
		
		/* Value */
		(*values)[i]->data_len = val->data_len;
		(*values)[i]->data_blk = talloc_memdup((*values)[i]->mem_ctx, (char *)val+sizeof(RGDB_VALUE)+val->name_len, val->data_len);

		/* Type */
		(*values)[i]->data_type = val->type;

		offset+=sizeof(RGDB_VALUE) + val->name_len + val->data_len;
	}
	
	return WERR_OK;
}

static struct registry_ops reg_backend_w95 = {
	.name = "w95",
	.open_registry = w95_open_reg,
	.close_registry = w95_close_reg,
	.open_root_key = w95_open_root,
	.fetch_values = w95_fetch_values,
	.get_subkey_by_index = w95_get_subkey_by_index,
};

NTSTATUS reg_w95_init(void)
{
	return register_backend("registry", &reg_backend_w95);
}
