#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "packet-dcerpc.h"
#include "packet-dcerpc-nt.h"
#include "packet-dcerpc-eparser.h"

static int hf_string4_len = -1;
static int hf_string4_offset = -1;
static int hf_string4_len2 = -1;
static int hf_string_data = -1;

/* Create a ndr_pull structure from data stored in a tvb at a given offset. */

struct e_ndr_pull *ndr_pull_init(tvbuff_t *tvb, int offset, packet_info *pinfo,
				 proto_tree *tree, guint8 *drep)
{
	struct e_ndr_pull *ndr;

	ndr = (struct e_ndr_pull *)g_malloc(sizeof(*ndr));
	
	ndr->tvb = tvb;
	ndr->offset = offset;
	ndr->pinfo = pinfo;
	ndr->tree = tree;
	ndr->drep = drep;
	ndr->flags = NDR_SCALARS|NDR_BUFFERS;

	return ndr;
}

/* Dispose of a dynamically allocated ndr_pull structure */

void ndr_pull_free(struct e_ndr_pull *ndr)
{
	g_free(ndr);
}

void ndr_pull_ptr(struct e_ndr_pull *e_ndr, int hf, guint32 *ptr)
{
	e_ndr->offset = dissect_ndr_uint32(
		e_ndr->tvb, e_ndr->offset, e_ndr->pinfo,
		e_ndr->tree, e_ndr->drep, hf, ptr);
}

void ndr_pull_level(struct e_ndr_pull *e_ndr, int hf, gint16 *data)
{
	e_ndr->offset = dissect_ndr_uint16(
		e_ndr->tvb, e_ndr->offset, e_ndr->pinfo,
		e_ndr->tree, e_ndr->drep, hf, data);
}

void ndr_pull_NTSTATUS(struct e_ndr_pull *e_ndr, int hf)
{
	e_ndr->offset = dissect_ntstatus(
		e_ndr->tvb, e_ndr->offset, e_ndr->pinfo,
		e_ndr->tree, e_ndr->drep, hf, NULL);
}

void ndr_pull_uint8(struct e_ndr_pull *e_ndr, int hf, guint8 *data)
{
	e_ndr->offset = dissect_ndr_uint8(
		e_ndr->tvb, e_ndr->offset, e_ndr->pinfo,
		e_ndr->tree, e_ndr->drep, hf, data);
}

void ndr_pull_uint16(struct e_ndr_pull *e_ndr, int hf, guint16 *data)
{
	e_ndr->offset = dissect_ndr_uint16(
		e_ndr->tvb, e_ndr->offset, e_ndr->pinfo,
		e_ndr->tree, e_ndr->drep, hf, data);
}

void ndr_pull_uint32(struct e_ndr_pull *e_ndr, int hf, guint32 *data)
{
	e_ndr->offset = dissect_ndr_uint32(
		e_ndr->tvb, e_ndr->offset, e_ndr->pinfo,
		e_ndr->tree, e_ndr->drep, hf, data);
}

void ndr_pull_int64(struct e_ndr_pull *e_ndr, int hf, gint64 *data)
{
}

void ndr_pull_uint64(struct e_ndr_pull *e_ndr, int hf, guint64 *data)
{
}

void ndr_pull_string(struct e_ndr_pull *ndr, int ndr_flags)
{
	guint32 len1, ofs, len2;
	char *data;

	if (!(ndr_flags & NDR_SCALARS)) {
		return;
	}
	
	switch (ndr->flags & LIBNDR_STRING_FLAGS) {
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4:
	case LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4|LIBNDR_FLAG_STR_NOTERM:

		ndr_pull_uint32(ndr, hf_string4_len, &len1);
		ndr_pull_uint32(ndr, hf_string4_offset, &ofs);
		ndr_pull_uint32(ndr, hf_string4_len2, &len2);

		if (len2 > 65535)
			return;

		data = g_malloc(len2*2);

		proto_tree_add_bytes(ndr->tree, hf_string_data, ndr->tvb,
				     ndr->offset, len2 * 2, data);

		g_free(data);

		ndr->offset += len2 * 2;

#if 0

		ndr_pull_uint32(ndr, &len1));
		ndr_pull_uint32(ndr, &ofs);
		ndr_pull_uint32(ndr, &len2);
		if (len2 > len1) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, 
					      "Bad string lengths len1=%u ofs=%u len2=%u\n", 
					      len1, ofs, len2);
		}
		if (len2 == 0) {
			*s = talloc_strdup(ndr->mem_ctx, "");
			break;
		}
		NDR_PULL_NEED_BYTES(ndr, len2*2);
		ret = convert_string_talloc(ndr->mem_ctx, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len2*2,
					    (const void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr_pull_advance(ndr, len2*2);

		/* this is a way of detecting if a string is sent with the wrong
		   termination */
		if (ndr->flags & LIBNDR_FLAG_STR_NOTERM) {
			if (strlen(as) < len2) {
				DEBUG(6,("short string '%s'\n", as));
			}
		} else {
			if (strlen(as) == len2) {
				DEBUG(6,("long string '%s'\n", as));
			}
		}
		*s = as;

#endif

		break;

	case LIBNDR_FLAG_STR_SIZE4:

#if 0

		ndr_pull_uint32(ndr, &len1);
		NDR_PULL_NEED_BYTES(ndr, len1*2);
		if (len1 == 0) {
			*s = talloc_strdup(ndr->mem_ctx, "");
			break;
		}
		ret = convert_string_talloc(ndr->mem_ctx, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len1*2,
					    (const void **)&as);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr_pull_advance(ndr, len1*2);
		*s = as;

#endif

		break;

	case LIBNDR_FLAG_STR_NULLTERM:

#if 0

		len1 = strnlen_w(ndr->data+ndr->offset, 
				 (ndr->data_size - ndr->offset)/2);
		if (len1*2+2 <= ndr->data_size - ndr->offset) {
			len1++;
		}
		ret = convert_string_talloc(ndr->mem_ctx, chset, CH_UNIX, 
					    ndr->data+ndr->offset, 
					    len1*2,
					    (const void **)s);
		if (ret == -1) {
			return ndr_pull_error(ndr, NDR_ERR_CHARCNV, 
					      "Bad character conversion");
		}
		ndr_pull_advance(ndr, len1*2);

#endif

		break;

	case LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4:
	case LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_LEN4|LIBNDR_FLAG_STR_SIZE4|LIBNDR_FLAG_STR_NOTERM:

#if 0

		ndr_pull_uint32(ndr, &len1);
		ndr_pull_uint32(ndr, &ofs);
		ndr_pull_uint32(ndr, &len2);
		if (len2 > len1) {
			return ndr_pull_error(ndr, NDR_ERR_STRING, 
					      "Bad ascii string lengths len1=%u ofs=%u len2=%u\n", 
					      len1, ofs, len2);
		}
		NDR_ALLOC_N(ndr, as, (len2+1));
		ndr_pull_bytes(ndr, as, len2);
		as[len2] = 0;
		(*s) = as;

#endif

		break;

	case LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_LEN4:

#if 0
		ndr_pull_uint32(ndr, &ofs);
		ndr_pull_uint32(ndr, &len2);
		NDR_ALLOC_N(ndr, as, (len2+1));
		ndr_pull_bytes(ndr, as, len2);
		as[len2] = 0;
		(*s) = as;

#endif

		break;

	case LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_SIZE2:

#if 0

		ndr_pull_uint16(ndr, &len3);
		NDR_ALLOC_N(ndr, as, (len3+1));
		ndr_pull_bytes(ndr, as, len3);
		as[len3] = 0;
		(*s) = as;

#endif

		break;

	case LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM:

#if 0

		len1 = strnlen(ndr->data+ndr->offset, (ndr->data_size - ndr->offset));
		if (len1+1 <= ndr->data_size - ndr->offset) {
			len1++;
		}
		NDR_ALLOC_N(ndr, as, (len1+1));
		ndr_pull_bytes(ndr, as, len1);
		as[len1] = 0;
		(*s) = as;

#endif

		break;

	default:

#if 0

		return ndr_pull_error(ndr, NDR_ERR_STRING, "Bad string flags 0x%x\n",
				      ndr->flags & LIBNDR_STRING_FLAGS);

#endif

	}	
}

void ndr_pull_NTTIME(struct e_ndr_pull *e_ndr, int hf, gNTTIME *data)
{
}

void ndr_pull_HYPER_T(struct e_ndr_pull *e_ndr, int hf, gHYPER_T *data)
{
}

void ndr_pull_dom_sid2(struct e_ndr_pull *e_ndr, int flags)
{
	guint32 num_auths;
	if (!(flags & NDR_SCALARS)) {
		return;
	}
	ndr_pull_uint32(e_ndr, hf_string4_len, &num_auths);

	ndr_pull_dom_sid(e_ndr, flags);
}

#if 0

void ndr_pull_security_descriptor(struct e_ndr_pull *e_ndr, int hf)
{
}

void ndr_pull_policy_handle(struct e_ndr_pull *e_ndr, int hf)
{
	e_ndr->offset = dissect_nt_policy_hnd(
		e_ndr->tvb, e_ndr->offset, e_ndr->pinfo, e_ndr->tree, 
		e_ndr->drep, hf, NULL, NULL, 0, 0);
}

#endif

void ndr_pull_advance(struct e_ndr_pull *e_ndr, int offset)
{
	e_ndr->offset += offset;
}

void ndr_pull_align(struct e_ndr_pull *ndr, int size)
{
	if (!(ndr->flags & LIBNDR_FLAG_NOALIGN)) {
		ndr->offset = (ndr->offset + (size-1)) & ~(size-1);
	}
}

void ndr_pull_subcontext_flags_fn(struct e_ndr_pull *ndr, size_t sub_size,
				  void (*fn)(struct e_ndr_pull *, 
					     int ndr_flags))
{
	struct e_ndr_pull ndr2;

	ndr_pull_subcontext_header(ndr, sub_size, &ndr2);
	fn(&ndr2, NDR_SCALARS|NDR_BUFFERS);
	if (sub_size) {
//		ndr_pull_advance(ndr, ndr2.data_size);
	} else {
		ndr_pull_advance(ndr, ndr2.offset);
	}
}

/*
  mark the start of a structure
*/
void ndr_pull_struct_start(struct e_ndr_pull *ndr)
{
	struct ndr_ofs_list *ofs;
	ofs = g_malloc(sizeof(*ofs));
	ofs->offset = ndr->offset;
	ofs->next = ndr->ofs_list;
	ndr->ofs_list = ofs;
}

/*
  mark the end of a structure
*/
void ndr_pull_struct_end(struct e_ndr_pull *ndr)
{
	ndr->ofs_list = ndr->ofs_list->next;
}

void ndr_pull_subcontext_header(struct e_ndr_pull *ndr, 
				size_t sub_size,
				struct e_ndr_pull *ndr2)
{
	ndr2->tvb = ndr->tvb;
	ndr2->offset = ndr->offset;
	ndr2->pinfo = ndr->pinfo;
	ndr2->tree = ndr->tree;
	ndr2->drep = ndr->drep;
	ndr2->ofs_list = ndr->ofs_list;
	ndr2->flags = ndr->flags;
}

void ndr_pull_relative(struct e_ndr_pull *ndr,
		       void (*fn)(struct e_ndr_pull *, int ndr_flags))
{
}

int lsa_dissect_LSA_SECURITY_DESCRIPTOR(tvbuff_t tvb, int offset,
					packet_info *pinfo, proto_tree *tree, 
					guint8 *drep)
{
	return offset;
}

int lsa_dissect_LSA_SECURITY_DESCRIPTOR_data(tvbuff_t tvb, int offset,
					packet_info *pinfo, proto_tree *tree, 
					guint8 *drep)
{
	return offset;
}

int lsa_dissect_POLICY_DNS_DOMAIN_INFO(tvbuff_t tvb, int offset,
				       packet_info *pinfo, proto_tree *tree, 
				       guint8 *drep)
{
	return offset;
}

void ndr_pull_bytes(struct e_ndr_pull *ndr, guint32 n)
{
	ndr->offset += n;
}

void ndr_pull_array_uint8(struct e_ndr_pull *ndr, int hf, int ndr_flags, guint32 n)
{
	guint32 i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return;
	}
	for (i=0;i<n;i++) {
		ndr_pull_uint8(ndr, hf, NULL);
	}	
}

void ndr_pull_array_uint32(struct e_ndr_pull *ndr, int hf, int ndr_flags, guint32 n)
{
	guint32 i;
	if (!(ndr_flags & NDR_SCALARS)) {
		return;
	}
	for (i=0;i<n;i++) {
		ndr_pull_uint32(ndr, hf, NULL);
	}
}

void ndr_pull_array(struct e_ndr_pull *ndr, int ndr_flags, guint32 n, 
		    void (*fn)(struct e_ndr_pull *, int ndr_flags))
{
}

void proto_register_eparser(void)
{
	static hf_register_info hf[] = {
	{ &hf_string4_len, { "String4 length", "eparser.string4_length", FT_UINT32, BASE_DEC, NULL, 0x0, "String4 length", HFILL }},
	{ &hf_string4_offset, { "String4 offset", "eparser.string4_offset", FT_UINT32, BASE_DEC, NULL, 0x0, "String4 offset", HFILL }},
	{ &hf_string4_len2, { "String4 length2", "eparser.string4_length2", FT_UINT32, BASE_DEC, NULL, 0x0, "String4 length2", HFILL }},
	{ &hf_string_data, { "String data", "eparser.string_data", FT_BYTES, BASE_NONE, NULL, 0x0, "String data", HFILL }},
	};

	int proto_dcerpc;
	
	proto_dcerpc = proto_get_id_by_filter_name("dcerpc");
	proto_register_field_array(proto_dcerpc, hf, array_length(hf));
}
