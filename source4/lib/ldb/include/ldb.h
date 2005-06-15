/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Stefan Metzmacher  2004

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 *  Name: ldb
 *
 *  Component: ldb header
 *
 *  Description: defines for base ldb API
 *
 *  Author: Andrew Tridgell
 *  Author: Stefan Metzmacher
 */

#ifndef _LDB_H_
#define _LDB_H_ 1

/*
  major restrictions as compared to normal LDAP:

     - no async calls.
     - each record must have a unique key field
     - the key must be representable as a NULL terminated C string and may not 
       contain a comma or braces

  major restrictions as compared to tdb:

     - no explicit locking calls

*/

/*
  an individual lump of data in a result comes in this format. The
  pointer will usually be to a UTF-8 string if the application is
  sensible, but it can be to anything you like, including binary data
  blobs of arbitrary size.
*/
#ifndef ldb_val
struct ldb_val {
	uint8_t *data;
	size_t length;
};
#endif

/* these flags are used in ldd_message_element.flags fields. The
   LDA_FLAGS_MOD_* flags are used in ldap_modify() calls to specify
   whether attributes are being added, deleted or modified */
#define LDB_FLAG_MOD_MASK  0x3
#define LDB_FLAG_MOD_ADD     1
#define LDB_FLAG_MOD_REPLACE 2
#define LDB_FLAG_MOD_DELETE  3


/*
  well known object IDs
*/
#define LDB_OID_COMPARATOR_AND  "1.2.840.113556.1.4.803"
#define LDB_OID_COMPARATOR_OR   "1.2.840.113556.1.4.804"

/*
  results are given back as arrays of ldb_message_element
*/
struct ldb_message_element {
	unsigned int flags;
	const char *name;
	unsigned int num_values;
	struct ldb_val *values;
};


/*
  a ldb_message represents all or part of a record. It can contain an arbitrary
  number of elements. 
*/
struct ldb_message {
	char *dn;
	unsigned int num_elements;
	struct ldb_message_element *elements;
	void *private_data; /* private to the backend */
};

enum ldb_changetype {
	LDB_CHANGETYPE_NONE=0,
	LDB_CHANGETYPE_ADD,
	LDB_CHANGETYPE_DELETE,
	LDB_CHANGETYPE_MODIFY
};

/*
  a ldif record - from ldif_read
*/
struct ldb_ldif {
	enum ldb_changetype changetype;
	struct ldb_message *msg;
};

enum ldb_scope {LDB_SCOPE_DEFAULT=-1, 
		LDB_SCOPE_BASE=0, 
		LDB_SCOPE_ONELEVEL=1,
		LDB_SCOPE_SUBTREE=2};

struct ldb_context;

/*
  the fuction type for the callback used in traversing the database
*/
typedef int (*ldb_traverse_fn)(struct ldb_context *, const struct ldb_message *);


struct ldb_module;

/* debugging uses one of the following levels */
enum ldb_debug_level {LDB_DEBUG_FATAL, LDB_DEBUG_ERROR, 
		      LDB_DEBUG_WARNING, LDB_DEBUG_TRACE};

/*
  the user can optionally supply a debug function. The function
  is based on the vfprintf() style of interface, but with the addition
  of a severity level
*/
struct ldb_debug_ops {
	void (*debug)(void *context, enum ldb_debug_level level, 
		      const char *fmt, va_list ap);
	void *context;
};

#define LDB_FLG_RDONLY 1

#ifndef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a,b)
#endif


/* structues for ldb_parse_tree handling code */
enum ldb_parse_op {LDB_OP_SIMPLE=1, LDB_OP_EXTENDED=2, 
		   LDB_OP_AND='&', LDB_OP_OR='|', LDB_OP_NOT='!'};

struct ldb_parse_tree {
	enum ldb_parse_op operation;
	union {
		struct {
			char *attr;
			struct ldb_val value;
		} simple;
		struct {
			char *attr;
			int dnAttributes;
			char *rule_id;
			struct ldb_val value;
		} extended;
		struct {
			unsigned int num_elements;
			struct ldb_parse_tree **elements;
		} list;
		struct {
			struct ldb_parse_tree *child;
		} not;
	} u;
};

struct ldb_parse_tree *ldb_parse_tree(void *mem_ctx, const char *s);
char *ldb_filter_from_tree(void *mem_ctx, struct ldb_parse_tree *tree);
char *ldb_binary_encode(void *ctx, struct ldb_val val);


/* 
 connect to a database. The URL can either be one of the following forms
   ldb://path
   ldapi://path

   flags is made up of LDB_FLG_*

   the options are passed uninterpreted to the backend, and are
   backend specific
*/
struct ldb_context *ldb_connect(const char *url, unsigned int flags,
				const char *options[]);

/*
  search the database given a LDAP-like search expression

  return the number of records found, or -1 on error

  use talloc_free to free the ldb_message returned
*/
int ldb_search(struct ldb_context *ldb, 
	       const char *base,
	       enum ldb_scope scope,
	       const char *expression,
	       const char * const *attrs, struct ldb_message ***res);

/*
  like ldb_search() but takes a parse tree
*/
int ldb_search_bytree(struct ldb_context *ldb, 
		      const char *base,
		      enum ldb_scope scope,
		      struct ldb_parse_tree *tree,
		      const char * const *attrs, struct ldb_message ***res);

/*
  add a record to the database. Will fail if a record with the given class and key
  already exists
*/
int ldb_add(struct ldb_context *ldb, 
	    const struct ldb_message *message);

/*
  modify the specified attributes of a record
*/
int ldb_modify(struct ldb_context *ldb, 
	       const struct ldb_message *message);

/*
  rename a record in the database
*/
int ldb_rename(struct ldb_context *ldb, const char *olddn, const char *newdn);

/*
  delete a record from the database
*/
int ldb_delete(struct ldb_context *ldb, const char *dn);


/*
  return extended error information from the last call
*/
const char *ldb_errstring(struct ldb_context *ldb);

/*
  casefold a string (should be UTF8, but at the moment it isn't)
*/
char *ldb_casefold(void *mem_ctx, const char *s);

/*
  ldif manipulation functions
*/
int ldb_ldif_write(struct ldb_context *ldb,
		   int (*fprintf_fn)(void *, const char *, ...), 
		   void *private_data,
		   const struct ldb_ldif *ldif);
void ldb_ldif_read_free(struct ldb_context *ldb, struct ldb_ldif *);
struct ldb_ldif *ldb_ldif_read(struct ldb_context *ldb, 
			       int (*fgetc_fn)(void *), void *private_data);
struct ldb_ldif *ldb_ldif_read_file(struct ldb_context *ldb, FILE *f);
struct ldb_ldif *ldb_ldif_read_string(struct ldb_context *ldb, const char *s);
int ldb_ldif_write_file(struct ldb_context *ldb, FILE *f, const struct ldb_ldif *msg);


/* useful functions for ldb_message structure manipulation */

int ldb_dn_cmp(const char *dn1, const char *dn2);
int ldb_attr_cmp(const char *dn1, const char *dn2);

/* case-fold a DN */
char *ldb_dn_fold(struct ldb_module *module, const char *dn, int (*case_fold_attr_fn)(struct ldb_module * module, char * attr));

/* create an empty message */
struct ldb_message *ldb_msg_new(void *mem_ctx);

/* find an element within an message */
struct ldb_message_element *ldb_msg_find_element(const struct ldb_message *msg, 
						 const char *attr_name);

/* compare two ldb_val values - return 0 on match */
int ldb_val_equal_exact(const struct ldb_val *v1, const struct ldb_val *v2);

/* find a value within an ldb_message_element */
struct ldb_val *ldb_msg_find_val(const struct ldb_message_element *el, 
				 struct ldb_val *val);

/* add a new empty element to a ldb_message */
int ldb_msg_add_empty(struct ldb_context *ldb,
		      struct ldb_message *msg, const char *attr_name, int flags);

/* add a element to a ldb_message */
int ldb_msg_add(struct ldb_context *ldb, 
		struct ldb_message *msg, 
		const struct ldb_message_element *el, 
		int flags);
int ldb_msg_add_value(struct ldb_context *ldb,
		      struct ldb_message *msg, 
		      const char *attr_name,
		      const struct ldb_val *val);
int ldb_msg_add_string(struct ldb_context *ldb, struct ldb_message *msg, 
		       const char *attr_name, const char *str);
int ldb_msg_add_fmt(struct ldb_context *ldb, struct ldb_message *msg, 
		    const char *attr_name, const char *fmt, ...) PRINTF_ATTRIBUTE(4,5);

/* compare two message elements - return 0 on match */
int ldb_msg_element_compare(struct ldb_message_element *el1, 
			    struct ldb_message_element *el2);

/* find elements in a message and convert to a specific type, with
   a give default value if not found. Assumes that elements are
   single valued */
const struct ldb_val *ldb_msg_find_ldb_val(const struct ldb_message *msg, const char *attr_name);
int ldb_msg_find_int(const struct ldb_message *msg, 
		     const char *attr_name,
		     int default_value);
unsigned int ldb_msg_find_uint(const struct ldb_message *msg, 
			       const char *attr_name,
			       unsigned int default_value);
int64_t ldb_msg_find_int64(const struct ldb_message *msg, 
			   const char *attr_name,
			   int64_t default_value);
uint64_t ldb_msg_find_uint64(const struct ldb_message *msg, 
			     const char *attr_name,
			     uint64_t default_value);
double ldb_msg_find_double(const struct ldb_message *msg, 
			   const char *attr_name,
			   double default_value);
const char *ldb_msg_find_string(const struct ldb_message *msg, 
				const char *attr_name,
				const char *default_value);

void ldb_msg_sort_elements(struct ldb_message *msg);

void ldb_msg_free(struct ldb_context *ldb, struct ldb_message *msg);

struct ldb_message *ldb_msg_copy(struct ldb_context *ldb, 
				 const struct ldb_message *msg);

struct ldb_message *ldb_msg_canonicalize(struct ldb_context *ldb, 
					 const struct ldb_message *msg);


struct ldb_message *ldb_msg_diff(struct ldb_context *ldb, 
				 struct ldb_message *msg1,
				 struct ldb_message *msg2);

struct ldb_val ldb_val_dup(void *mem_ctx, const struct ldb_val *v);

/*
  this allows the user to set a debug function for error reporting
*/
int ldb_set_debug(struct ldb_context *ldb,
		  void (*debug)(void *context, enum ldb_debug_level level, 
				const char *fmt, va_list ap),
		  void *context);

/* this sets up debug to print messages on stderr */
int ldb_set_debug_stderr(struct ldb_context *ldb);

#endif
