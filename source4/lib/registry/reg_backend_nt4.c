/*
   Samba CIFS implementation
   Registry backend for REGF files
   Copyright (C) 2005 Jelmer Vernooij, jelmer@samba.org

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
#include "registry.h"
#include "system/filesys.h"
#include "system/time.h"
#include "lib/registry/tdr_regf.h"
#include "librpc/gen_ndr/ndr_security.h"

/* TODO:
 *  - Return error codes that make more sense
 *  - Locking
 */

/*
 * Read HBIN blocks into memory
 */

struct regf_data {
	int fd;
	struct hbin_block **hbins;
	struct regf_hdr *header;
};

static struct hbin_block *hbin_by_offset (const struct regf_data *data, uint32_t offset, uint32_t *rel_offset)
{
	int i;

	for (i = 0; data->hbins[i]; i++) {
		if (offset >= data->hbins[i]->offset_from_first && 
			offset < data->hbins[i]->offset_from_first+
					 data->hbins[i]->offset_to_next) {
			if (rel_offset)
				*rel_offset = offset - data->hbins[i]->offset_from_first - 0x20;
			return data->hbins[i];
		}
	}

	return NULL;
}

/*
 * Validate a regf header
 * For now, do nothing, but we should check the checksum
 */
static uint32_t regf_hdr_checksum(const uint8_t *buffer)
{
	uint32_t checksum = 0, x;
	int i;
	
	for (i = 0; i < 0x01FB; i+= 4) {
		x = IVAL(buffer, i);
		checksum ^= x;
	}

	return checksum;
}

static DATA_BLOB hbin_get(const struct regf_data *data, uint32_t offset)
{
	DATA_BLOB ret;
	struct hbin_block *hbin;
	uint32_t rel_offset;
	ret.data = NULL;
	ret.length = 0;

	hbin = hbin_by_offset(data, offset, &rel_offset);

	if (hbin == NULL) {
		DEBUG(1, ("Can't find HBIN containing 0x%04x\n", offset));
		return ret;
	}

	ret.length = IVAL(hbin->data, rel_offset);
	if (!(ret.length & 0x80000000)) {
		DEBUG(0, ("Trying to use dirty block at 0x%04x\n", offset));
		return ret;
	}

	/* remove high bit */
	ret.length = (ret.length ^ 0xffffffff) + 1;

	ret.length -= 4; /* 4 bytes for the length... */
	ret.data = hbin->data + 
		(offset - hbin->offset_from_first - 0x20) + 4;
	
	return ret;
}

static BOOL hbin_get_tdr (struct regf_data *regf, uint32_t offset, tdr_pull_fn_t pull_fn, void *p)
{
	DATA_BLOB data;
	struct tdr_pull *pull;

	data = hbin_get(regf, offset);
	if (!data.data) {
		DEBUG(1, ("Unable to get data at 0x%04x\n", offset));
		return False;
	}
	
	pull = talloc_zero(regf, struct tdr_pull);
	pull->data = data;

	if (NT_STATUS_IS_ERR(pull_fn(pull, p))) {
		DEBUG(1, ("Error parsing record at 0x%04x using tdr\n", offset));
		talloc_free(pull);
		return False;
	}

	/* FIXME: Free pull ! */

	return True;
}

/* Allocate some new data */
static DATA_BLOB hbin_alloc (struct regf_data *data, uint32_t size, uint32_t *offset)
{
	DATA_BLOB ret;
	uint32_t rel_offset = -1; /* Relative offset ! */
	struct hbin_block *hbin = NULL;
	int i;

	if (size == 0)
		return ret;

	size += 4; /* Need to include uint32 for the length */

	/* Allocate as a multiple of 8 */
	size = (size + 7) & ~7;

	ret.data = NULL;
	ret.length = 0;

	for (i = 0; (hbin = data->hbins[i]); i++) {
		int j;
		uint32_t my_size;
		for (j = 0; j < hbin->offset_to_next-0x20; j+= my_size) {
			my_size = IVAL(hbin->data, j);
			uint32_t header = IVAL(hbin->data, j + 4);

			if (my_size == 0x0) {
				DEBUG(0, ("Invalid zero-length block! File is corrupt.\n"));
				return ret;
			}

			if (my_size % 8 != 0) {
				DEBUG(0, ("Encountered non-aligned block!\n"));
			}

			if (my_size & 0x80000000) { /* Used... */
				my_size = (my_size ^ 0xffffffff) + 1;
			} else if (my_size == size) { /* exact match */
				rel_offset = j;
				DEBUG(4, ("Found free block of exact size %d in middle of HBIN\n", size));
				break;
			} else if (my_size > size) { /* data will remain */
				rel_offset = j;
				SIVAL(hbin->data, rel_offset+size, my_size-size); 
				DEBUG(4, ("Found free block of size %d (needing %d) in middle of HBIN\n", my_size, size));
				break;
			}

			if (header == 0xffffffff &&
				hbin->offset_to_next-rel_offset >= size)  {
				rel_offset = j;

				DEBUG(4, ("Found free block of size %d at end of HBIN\n", size));
				/* Mark new free block size */
				SIVAL(hbin->data, rel_offset+size,hbin->offset_to_next - rel_offset - size - 0x20);
				SIVAL(hbin->data, rel_offset+size+0x4, 0xffffffff);
				break;
			}

			if (header == 0xffffffff)  {
				break;
			}
		}

		if (rel_offset != -1)
			break;
	}
	
	/* No space available in previous hbins, 
	 * allocate new one */
	if (data->hbins[i] == NULL) { 
		DEBUG(4, ("No space available in other HBINs for block of size %d, allocating new HBIN\n", size));
		data->hbins = talloc_realloc(data, data->hbins, struct hbin_block *, i+2);
		hbin = talloc(data->hbins, struct hbin_block);
		data->hbins[i] = hbin;
		data->hbins[i+1] = NULL;

		hbin->HBIN_ID = talloc_strdup(hbin, "hbin");
		hbin->offset_from_first = (i == 0?0:data->hbins[i-1]->offset_from_first+data->hbins[i-1]->offset_to_next);
		hbin->offset_to_next = 0x1000;
		hbin->unknown[0] = 0;
		hbin->unknown[0] = 0;
		unix_to_nt_time(&hbin->last_change, time(NULL));
		hbin->block_size = hbin->offset_to_next;
		hbin->data = talloc_zero_array(hbin, uint8_t, hbin->block_size - 0x20);

		rel_offset = 0x0;
		SIVAL(hbin->data, size, hbin->block_size - size - 0x20);
		SIVAL(hbin->data, size + 0x4, 0xffffffff);
	}

	/* Set size and mark as used */
	SIVAL(hbin->data, rel_offset, size | 0x80000000);

	ret.data = hbin->data + rel_offset + 0x4; /* Skip past length */
	ret.length = size - 0x4;
	if (offset) {
		uint32_t new_rel_offset;
		*offset = hbin->offset_from_first + rel_offset + 0x20;
		SMB_ASSERT(hbin_by_offset(data, *offset, &new_rel_offset) == hbin);
		SMB_ASSERT(new_rel_offset == rel_offset);
	}

	return ret;
}

/* Store a data blob. Return the offset at which it was stored */
static uint32_t hbin_store (struct regf_data *data, DATA_BLOB blob)
{
	uint32_t ret;
	DATA_BLOB dest = hbin_alloc(data, blob.length, &ret);

	memcpy(dest.data, blob.data, blob.length);

	return ret;
}

static uint32_t hbin_store_tdr (struct regf_data *data, tdr_push_fn_t push_fn, void *p)
{
	struct tdr_push *push = talloc_zero(data, struct tdr_push);
	uint32_t ret;
	
	if (NT_STATUS_IS_ERR(push_fn(push, p))) {
		DEBUG(0, ("Error during push\n"));
		return -1;
	}

	ret = hbin_store(data, push->data);

	talloc_free(push);

	return ret;
}


/* Free existing data */
static void hbin_free (struct regf_data *data, uint32_t offset)
{
	uint32_t size;
	uint32_t rel_offset;
	struct hbin_block *hbin;

	SMB_ASSERT (offset > 0);
	
	hbin = hbin_by_offset(data, offset, &rel_offset);

	if (hbin == NULL)
		return;
	
	/* Get original size */
	size = IVAL(hbin->data, rel_offset);

	if (!(size & 0x80000000)) {
		DEBUG(1, ("Trying to free already freed block at 0x%04x\n", offset));
		return;
	}

	/* Mark block as free */
	SIVAL(hbin->data, rel_offset, size &~ 0x80000000);
}

/* Store a data blob data was already stored, but hsa changed in size
 * Will try to save it at the current location if possible, otherwise 
 * does a free + store */
static uint32_t hbin_store_resize (struct regf_data *data, uint32_t orig_offset, DATA_BLOB blob)
{
	uint32_t rel_offset;
	struct hbin_block *hbin = hbin_by_offset(data, orig_offset, &rel_offset);
	uint32_t my_size;
	uint32_t orig_size;
	uint32_t needed_size;
	uint32_t possible_size;
	int i;

	SMB_ASSERT(orig_offset > 0);

	if (!hbin)
		return hbin_store(data, blob);

	/* Get original size */
	orig_size = IVAL(hbin->data, rel_offset);

	needed_size = blob.length + 4; /* Add uint32 containing length */
	needed_size = (needed_size + 7) & ~7; /* Align */

	/* Fits into current allocated block */
	if (orig_size >= needed_size) {
		memcpy(hbin->data + rel_offset + 0x4, blob.data, blob.length);
		return orig_offset;
	}

	possible_size = orig_size;

	/* Check if it can be combined with the next few free records */
	for (i = rel_offset; 
		 i < hbin->offset_to_next - 0x20; 
		 i += my_size) {
		uint32_t header;
		if (IVAL(hbin->data, i) & 0x80000000) /* Used */
			break;

		my_size = IVAL(hbin->data, i);
		header = IVAL(hbin->data, i + 4);
		if (header == 0xffffffff) {
			possible_size = hbin->offset_to_next - 0x20 - rel_offset;
		} else if (my_size == 0x0) {
			DEBUG(0, ("Invalid zero-length block! File is corrupt.\n"));
			break;
		} else {
			possible_size += my_size;
		}

		if (possible_size >= blob.length) {
			SIVAL(hbin->data, rel_offset, possible_size);
			memcpy(hbin->data + rel_offset + 0x4, blob.data, blob.length);
			return orig_offset;
		}

		if (header == 0xffffffff) 
			break;
	}

	hbin_free(data, orig_offset);
	return hbin_store(data, blob);
}

static uint32_t hbin_store_tdr_resize (struct regf_data *regf, tdr_push_fn_t push_fn, uint32_t orig_offset, void *p)
{
	struct tdr_push *push = talloc_zero(regf, struct tdr_push);
	uint32_t ret;
	
	if (NT_STATUS_IS_ERR(push_fn(push, p))) {
		DEBUG(0, ("Error during push\n"));
		return -1;
	}

	ret = hbin_store_resize(regf, orig_offset, push->data);

	talloc_free(push);

	return ret;
}

static WERROR regf_num_subkeys (struct registry_key *key, uint32_t *count)
{
	struct nk_block *nk = key->backend_data;

	*count = nk->num_subkeys;
	
	return WERR_OK;
}

static WERROR regf_num_values (struct registry_key *key, uint32_t *count)
{
	struct nk_block *nk = key->backend_data;

	*count = nk->num_values;

	return WERR_OK;
}

static struct registry_key *regf_get_key (TALLOC_CTX *ctx, struct regf_data *regf, uint32_t offset)
{
	struct registry_key *ret;
	struct nk_block *nk;

	ret = talloc_zero(ctx, struct registry_key);
	nk = talloc(ret, struct nk_block);
	if (!hbin_get_tdr(regf, offset, (tdr_pull_fn_t)tdr_pull_nk_block, nk)) {
		DEBUG(0, ("Unable to find HBIN data for offset %d\n", offset));
		return NULL;
	}

	if (strcmp(nk->header, "nk") != 0) {
		DEBUG(0, ("Expected nk record, got %s\n", nk->header));
		talloc_free(ret);
		return NULL;
	}

	ret->name = talloc_steal(ret, nk->key_name);
	ret->last_mod = nk->last_change;

	if (nk->clsname_offset != -1) {
		DATA_BLOB data = hbin_get(regf, nk->clsname_offset);
		ret->class_name = talloc_strndup(ret, (char*)data.data, nk->clsname_length);
	}
	ret->backend_data = nk;

	return ret;
}

static WERROR regf_get_value (TALLOC_CTX *ctx, struct registry_key *key, int idx, struct registry_value **ret)
{
	struct nk_block *nk = key->backend_data;
	struct vk_block *vk;
	struct regf_data *regf = key->hive->backend_data;
	uint32_t vk_offset;
	DATA_BLOB data;

	if (idx >= nk->num_values)
		return WERR_NO_MORE_ITEMS;

	data = hbin_get(regf, nk->values_offset);
	if (!data.data) {
		DEBUG(0, ("Unable to find value list\n"));
		return WERR_GENERAL_FAILURE;
	}

	if (data.length < nk->num_values * 4) {
		DEBUG(1, ("Value counts mismatch\n"));
	}

	vk_offset = IVAL(data.data, idx * 4);

	*ret = talloc_zero(ctx, struct registry_value);
	if (!(*ret)) 
		return WERR_NOMEM;

	vk = talloc(*ret, struct vk_block);
	if (!vk)
		return WERR_NOMEM;
	
	if (!hbin_get_tdr(regf, vk_offset, (tdr_pull_fn_t)tdr_pull_vk_block, vk)) {
		DEBUG(0, ("Unable to get VK block at %d\n", vk_offset));
		return WERR_GENERAL_FAILURE;
	}

	(*ret)->name = talloc_steal(*ret, vk->data_name);
	(*ret)->data_type = vk->data_type;
	if (vk->data_length & 0x80000000) { 
		vk->data_length &=~0x80000000;
		(*ret)->data.data = (uint8_t *)&vk->data_offset;
		(*ret)->data.length = vk->data_length;
	} else {
		(*ret)->data = hbin_get(regf, vk->data_offset);
	}

	if ((*ret)->data.length < vk->data_length) {
		DEBUG(1, ("Read data less then indicated data length!\n"));
	}
	
	return WERR_OK;
}

static WERROR regf_get_subkey (TALLOC_CTX *ctx, struct registry_key *key, int idx, struct registry_key **ret)
{
	DATA_BLOB data;
	struct nk_block *nk = key->backend_data;
	uint32_t key_off;

	if (idx >= nk->num_subkeys)
		return WERR_NO_MORE_ITEMS;

	data = hbin_get(key->hive->backend_data, nk->subkeys_offset);
	if (!data.data) {
		DEBUG(0, ("Unable to find subkey list\n"));
		return WERR_GENERAL_FAILURE;
	}

	if (!strncmp((char *)data.data, "li", 2)) {
		DEBUG(4, ("Subkeys in LI list\n"));
		SMB_ASSERT(0);
	} else if (!strncmp((char *)data.data, "lf", 2)) {
		struct lf_block lf;
		struct tdr_pull *pull = talloc_zero(ctx, struct tdr_pull);

		DEBUG(10, ("Subkeys in LF list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lf_block(pull, &lf))) {
			DEBUG(0, ("Error parsing LF list\n"));
			return WERR_GENERAL_FAILURE;
		}

		if (lf.key_count != nk->num_subkeys) {
			DEBUG(0, ("Subkey counts don't match\n"));
			return WERR_GENERAL_FAILURE;
		}

		key_off = lf.hr[idx].nk_off;
		
		talloc_free(pull);
	} else if (!strncmp((char *)data.data, "ri", 2)) {
		DEBUG(4, ("Subkeys in RI list\n"));
		SMB_ASSERT(0);
	} else if (!strncmp((char *)data.data, "lh", 2)) {
		DEBUG(4, ("Subkeys in LH list\n"));
		SMB_ASSERT(0);
	} else {
		DEBUG(0, ("Unknown type for subkey list (0x%04x): %c%c\n", nk->subkeys_offset, data.data[0], data.data[1]));
		return WERR_GENERAL_FAILURE;
	}

	*ret = regf_get_key (ctx, key->hive->backend_data, key_off);

	return WERR_OK;
}


static WERROR regf_set_sec_desc (struct registry_key *key, struct security_descriptor *sec_desc)
{
	/* FIXME */
	return WERR_NOT_SUPPORTED;
}

static WERROR regf_get_sec_desc(TALLOC_CTX *ctx, struct registry_key *key, struct security_descriptor **sd)
{
	struct nk_block *nk = key->backend_data;
	struct sk_block sk;
	struct regf_data *regf = key->hive->backend_data;
	DATA_BLOB data;

	if (!hbin_get_tdr(regf, nk->sk_offset, (tdr_pull_fn_t) tdr_pull_sk_block, &sk)) {
		DEBUG(0, ("Unable to find security descriptor\n"));
		return WERR_GENERAL_FAILURE;
	}
		
	if (strcmp(sk.header, "sk") != 0) {
		DEBUG(0, ("Expected 'sk', got '%s'\n", sk.header));
		return WERR_GENERAL_FAILURE;
	}

	*sd = talloc(ctx, struct security_descriptor);
	if (!*sd)
		return WERR_NOMEM;

	data.data = sk.sec_desc;
	data.length = sk.rec_size;
	if (NT_STATUS_IS_ERR(ndr_pull_struct_blob(&data, ctx, *sd, (ndr_pull_flags_fn_t)ndr_pull_security_descriptor))) {
		DEBUG(0, ("Error parsing security descriptor\n"));
		return WERR_GENERAL_FAILURE;
	}

	return WERR_OK;
}

static uint32_t lf_add_entry (struct regf_data *regf, uint32_t list_offset, const char *name, uint32_t key_offset)
{
	uint32_t ret;
	struct lf_block lf;

	/* Add to subkeys list */
	if (list_offset == -1) { /* Need to create subkeys list */
		lf.header = "lf";
		lf.key_count = 0;
		lf.hr = NULL;
	} else {
		if (!hbin_get_tdr(regf, list_offset, (tdr_pull_fn_t)tdr_pull_lf_block, &lf)) {
			DEBUG(0, ("Can't get subkeys list\n"));
			return -1;
		}
	}

	lf.hr = talloc_realloc(regf, lf.hr, struct hash_record, lf.key_count+1);
	lf.hr[lf.key_count].nk_off = key_offset;
	lf.hr[lf.key_count].hash = talloc_strndup(regf, name, 4);
	lf.key_count++;

	ret = hbin_store_tdr_resize(regf, (tdr_push_fn_t)tdr_push_lf_block, list_offset, &lf);
	
	return ret;
}

static WERROR regf_del_value (struct registry_key *parent, const char *name)
{
	/* FIXME */
	return WERR_NOT_SUPPORTED;
}


static WERROR regf_del_key (struct registry_key *parent, const char *name)
{
	struct nk_block *nk = parent->backend_data;

	SMB_ASSERT(nk);
	
	if (nk->subkeys_offset == -1) 
		return WERR_BADFILE;

	/* FIXME */

	return WERR_NOT_SUPPORTED;
}

static WERROR regf_add_key (TALLOC_CTX *ctx, struct registry_key *parent, const char *name, uint32_t access_mask, struct security_descriptor *sec_desc, struct registry_key **ret)
{
	struct nk_block *parent_nk = parent->backend_data, nk;
	struct regf_data *regf = parent->hive->backend_data;
	uint32_t offset;

	nk.header = "nk";
	nk.type = REG_SUB_KEY;
	unix_to_nt_time(&nk.last_change, time(NULL));
	nk.uk1 = 0;
	nk.parent_offset = 0; /* FIXME */
	nk.num_subkeys = 0;
	nk.uk2 = 0;
	nk.subkeys_offset = -1;
	nk.unknown_offset = -1;
	nk.num_values = 0;
	nk.sk_offset = 0;
	memset(nk.unk3, 0, 5);
	nk.clsname_offset = -1;
	nk.clsname_length = 0;
	nk.key_name = name;
	
	offset = hbin_store_tdr(regf, (tdr_push_fn_t) tdr_push_nk_block, &nk);

	parent_nk->subkeys_offset = lf_add_entry(regf, parent_nk->subkeys_offset, name, nk.parent_offset);

	parent_nk->num_subkeys++;

	hbin_store_tdr_resize(regf, (tdr_push_fn_t) tdr_push_nk_block, nk.parent_offset, parent_nk);

	*ret = regf_get_key(ctx, regf, offset);

	/* FIXME: Set sec desc ! */
	return WERR_OK;
}

static WERROR regf_set_value (struct registry_key *key, const char *name, uint32_t type, DATA_BLOB data)
{
	/* FIXME */

	return WERR_NOT_SUPPORTED;
}

static WERROR regf_save_hbin(struct registry_hive *hive, struct hbin_block *hbin)
{
	struct regf_data *regf = hive->backend_data;

	/* go to right offset */
	if (lseek(regf->fd, SEEK_SET, regf->header->data_offset + hbin->offset_from_first) == -1) {
		DEBUG(0, ("Error lseeking in regf file\n"));
		return WERR_GENERAL_FAILURE;
	}

	if (NT_STATUS_IS_ERR(tdr_push_to_fd(regf->fd, (tdr_push_fn_t)tdr_push_hbin_block, hbin))) {
		DEBUG(0, ("Error writing HBIN block\n"));	
		return WERR_GENERAL_FAILURE;
	}

	return WERR_OK;
}

static WERROR nt_open_hive (struct registry_hive *h, struct registry_key **key)
{
	struct regf_data *regf;
	struct regf_hdr *regf_hdr;
	struct tdr_pull *pull;
	int i;

	regf = (struct regf_data *)talloc_zero(h, struct regf_data);
	h->backend_data = regf;

	DEBUG(5, ("Attempting to load registry file\n"));

	/* Get the header */
	regf->fd = open(h->location, O_RDWR);

	if (regf->fd == -1) {
		DEBUG(0,("Could not load file: %s, %s\n", h->location,
				 strerror(errno)));
		return WERR_GENERAL_FAILURE;
	}

	pull = talloc_zero(regf, struct tdr_pull);
	if (!pull)
		return WERR_NOMEM;

	pull->data.data = (uint8_t*)fd_load(regf->fd, &pull->data.length, regf);

	if (pull->data.data == NULL) {
		DEBUG(0, ("Error reading data\n"));
		return WERR_GENERAL_FAILURE;
	}

	regf_hdr = talloc(regf, struct regf_hdr);
	if (NT_STATUS_IS_ERR(tdr_pull_regf_hdr(pull, regf_hdr))) {
		return WERR_GENERAL_FAILURE;
	}

	regf->header = regf_hdr;

	if (strcmp(regf_hdr->REGF_ID, "regf") != 0) {
		DEBUG(0, ("Unrecognized NT registry header id: %s, %s\n",
				  regf_hdr->REGF_ID, h->location));
	}

	DEBUG(1, ("Registry '%s' read. Version %d.%d.%d.%d\n", 
			  regf_hdr->description, regf_hdr->version.major,
			  regf_hdr->version.minor, regf_hdr->version.release,
			  regf_hdr->version.build));

	/*
	 * Validate the header ...
	 */
	if (regf_hdr_checksum(pull->data.data) != regf_hdr->chksum) {
		DEBUG(0, ("Registry file checksum error: %s: %d,%d\n",
				  h->location, regf_hdr->chksum, regf_hdr_checksum(pull->data.data)));
		return WERR_GENERAL_FAILURE;
	}

	pull->offset = 0x1000;

	i = 0;
	/* Read in all hbin blocks */
	regf->hbins = talloc_array(regf, struct hbin_block *, 1);
	regf->hbins[0] = NULL;

	while (pull->offset < pull->data.length) {
		struct hbin_block *hbin = talloc(regf->hbins, struct hbin_block);

		if (NT_STATUS_IS_ERR(tdr_pull_hbin_block(pull, hbin))) {
			DEBUG(0, ("[%d] Error parsing HBIN block\n", i));
			return WERR_FOOBAR;
		}

		if (strcmp(hbin->HBIN_ID, "hbin") != 0) {
			DEBUG(0, ("[%d] Expected 'hbin', got '%s'\n", i, hbin->HBIN_ID));
			return WERR_FOOBAR;
		}

		regf->hbins[i] = hbin;
		i++;
		regf->hbins = talloc_realloc(regf, regf->hbins, struct hbin_block *, i+2);
		regf->hbins[i] = NULL;
	} 

	DEBUG(1, ("%d HBIN blocks read\n", i));

	*key = regf_get_key(h, regf, 0x20);

	return WERR_OK;
}

static struct hive_operations reg_backend_nt4 = {
	.name = "nt4",
	.open_hive = nt_open_hive,
	.num_subkeys = regf_num_subkeys,
	.num_values = regf_num_values,
	.get_subkey_by_index = regf_get_subkey,
	.get_value_by_index = regf_get_value,
	.key_get_sec_desc = regf_get_sec_desc,
	.key_set_sec_desc = regf_set_sec_desc,
	.add_key = regf_add_key,
	.set_value = regf_set_value,
	.del_key = regf_del_key,
	.del_value = regf_del_value,
};

NTSTATUS registry_nt4_init(void)
{
	return registry_register(&reg_backend_nt4);
}
