#define NDR_SCALARS 1
#define NDR_BUFFERS 2

#define LIBNDR_FLAG_BIGENDIAN  (1<<0)
#define LIBNDR_FLAG_NOALIGN    (1<<1)

#define LIBNDR_FLAG_STR_ASCII    (1<<2)
#define LIBNDR_FLAG_STR_LEN4     (1<<3)
#define LIBNDR_FLAG_STR_SIZE4    (1<<4)
#define LIBNDR_FLAG_STR_NOTERM   (1<<5)
#define LIBNDR_FLAG_STR_NULLTERM (1<<6)
#define LIBNDR_FLAG_STR_SIZE2    (1<<7)
#define LIBNDR_STRING_FLAGS      (0xFC)

#define LIBNDR_FLAG_REF_ALLOC    (1<<10)
#define LIBNDR_FLAG_REMAINING    (1<<11)
#define LIBNDR_FLAG_ALIGN2       (1<<12)
#define LIBNDR_FLAG_ALIGN4       (1<<13)
#define LIBNDR_FLAG_ALIGN8       (1<<14)

#define LIBNDR_ALIGN_FLAGS (LIBNDR_FLAG_ALIGN2|LIBNDR_FLAG_ALIGN4|LIBNDR_FLAG_ALIGN8)

#define LIBNDR_PRINT_ARRAY_HEX   (1<<15)
#define LIBNDR_PRINT_SET_VALUES  (1<<16)

/* used to force a section of IDL to be little-endian */
#define LIBNDR_FLAG_LITTLE_ENDIAN (1<<17)

/* Ethereal version of struct ndr_pull */

struct e_ndr_pull {
	tvbuff_t *tvb;
	int offset;
	packet_info *pinfo;
	proto_tree *tree;
	guint8 *drep;
	struct ndr_ofs_list *ofs_list;
	int flags;
};

/* offset lists are used to allow a push/pull function to find the
   start of an encapsulating structure */
struct ndr_ofs_list {
	guint32 offset;
	struct ndr_ofs_list *next;
};

/* Create a ndr_pull structure from data stored in a tvb at a given offset. */

struct e_ndr_pull *ndr_pull_init(tvbuff_t *tvb, int offset, packet_info *pinfo,
				 proto_tree *tree, guint8 *drep);
void ndr_pull_free(struct e_ndr_pull *ndr);
void ndr_pull_ptr(struct e_ndr_pull *ndr, int hf, guint32 *ptr);
void ndr_pull_level(struct e_ndr_pull *ndr, int hf, int *ptr);
void ndr_pull_NTSTATUS(struct e_ndr_pull *ndr, int hf);
void ndr_pull_uint8(struct e_ndr_pull *ndr, int hf);
void ndr_pull_uint16(struct e_ndr_pull *ndr, int hf);
void ndr_pull_uint32(struct e_ndr_pull *ndr, int hf);
void ndr_pull_policy_handle(struct e_ndr_pull *ndr, int hf);
void ndr_pull_advance(struct e_ndr_pull *ndr, int offset);
void ndr_pull_subcontext_flags_fn(struct e_ndr_pull *ndr, size_t sub_size,
				  void (*fn)(struct e_ndr_pull *, 
					     int ndr_flags));
void ndr_pull_subcontext_header(struct e_ndr_pull *ndr, 
				size_t sub_size,
				struct e_ndr_pull *ndr2);
void ndr_pull_struct_start(struct e_ndr_pull *ndr);
void ndr_pull_struct_end(struct e_ndr_pull *ndr);
void ndr_pull_align(struct e_ndr_pull *ndr, int size);
void ndr_pull_NTTIME(struct e_ndr_pull *e_ndr, int hf);
void ndr_pull_HYPER_T(struct e_ndr_pull *e_ndr, int hf);
void ndr_pull_int64(struct e_ndr_pull *e_ndr, int hf);
void ndr_pull_uint64(struct e_ndr_pull *e_ndr, int hf);
void ndr_pull_string(struct e_ndr_pull *e_ndr, int hf);
void ndr_pull_dom_sid2(struct e_ndr_pull *e_ndr, int hf);
void ndr_pull_security_descriptor(struct e_ndr_pull *e_ndr, int hf);

void ndr_pull_lsa_SidArray(struct e_ndr_pull *ndr, int ndr_flags);
void ndr_pull_samr_LogonHours(struct e_ndr_pull *ndr, int ndr_flags);
void ndr_pull_samr_Password(struct e_ndr_pull *ndr, int ndr_flags);
