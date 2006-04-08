/* 
   Unix SMB/CIFS implementation.

   Swig interface to ldb.

   Copyright (C) 2005,2006 Tim Potter <tpot@samba.org>
   Copyright (C) 2006 Simo Sorce <idra@samba.org>

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

%module ldb

%{

/* Some typedefs to help swig along */

typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

/* Include headers */

#include "lib/ldb/include/ldb.h"
#include "lib/talloc/talloc.h"

%}

%include "carrays.i"
%include "exception.i"

/* 
 * Wrap struct ldb_context
 */

/* The ldb functions will crash if a NULL ldb context is passed so
   catch this before it happens. */

%typemap(check) struct ldb_context* {
	if ($1 == NULL)
		SWIG_exception(SWIG_ValueError, 
			"ldb context must be non-NULL");
}

/* 
 * Wrap TALLOC_CTX
 */

/* Use talloc_init() to create a parameter to pass to ldb_init().  Don't
   forget to free it using talloc_free() afterwards. */

TALLOC_CTX *talloc_init(char *name);
int talloc_free(TALLOC_CTX *ptr);

/*
 * Wrap struct ldb_val
 */

%typemap(in) struct ldb_val {
	if (!PyString_Check($input)) {
		PyErr_SetString(PyExc_TypeError, "string arg expected");
		return NULL;
	}
	$1.length = PyString_Size($input);
	$1.data = PyString_AsString($input);
}

%typemap(out) struct ldb_val {
	if ($1.data == NULL && $1.length == 0) {
		$result = Py_None;
	} else {
		$result = PyString_FromStringAndSize($1.data, $1.length);
	}
}

enum ldb_scope {LDB_SCOPE_DEFAULT=-1, 
		LDB_SCOPE_BASE=0, 
		LDB_SCOPE_ONELEVEL=1,
		LDB_SCOPE_SUBTREE=2};

/*
 * Wrap struct ldb_result
 */

%typemap(in, numinputs=0) struct ldb_result **OUT (struct ldb_result *temp_ldb_result) {
	$1 = &temp_ldb_result;
}

%typemap(argout) struct ldb_result ** {

	/* XXX: Check result for error and throw exception if necessary */

	resultobj = SWIG_NewPointerObj(*$1, SWIGTYPE_p_ldb_result, 0);
}	

%types(struct ldb_result *);

/*
 * Wrap struct ldb_dn
 */

%typemap(in) struct ldb_dn * {
	if ($input == Py_None) {
		$1 = NULL;
	} else if (!PyString_Check($input)) {
		PyErr_SetString(PyExc_TypeError, "string arg expected");
		return NULL;
	} else {
		$1 = ldb_dn_explode(NULL, PyString_AsString($input));
	}
}

%typemap(out) struct ldb_dn * {
	$result = PyString_FromString(ldb_dn_linearize($1, $1));
}

/*
 * Wrap struct ldb_message_element
 */

%array_functions(struct ldb_val, ldb_val_array);

struct ldb_message_element {
	unsigned int flags;
	const char *name;
	unsigned int num_values;
	struct ldb_val *values;
};

/*
 * Wrap struct ldb_message
 */

%array_functions(struct ldb_message_element, ldb_message_element_array);

struct ldb_message {
	struct ldb_dn *dn;
	unsigned int num_elements;
	struct ldb_message_element *elements;
	void *private_data; /* private to the backend */
};

%typemap(in) struct ldb_message * {
	PyObject *obj, *key, *value;
	int pos;

	$1 = ldb_msg_new(NULL);

	obj = PyObject_GetAttrString($input, "dn");
	$1->dn = ldb_dn_explode(NULL, PyString_AsString(obj));

	obj = PyObject_GetAttrString($input, "private_data");
	$1->private_data = PyString_AsString(obj);

	obj = PyObject_GetAttrString($input, "elements");

	pos = 0;
	while (PyDict_Next(obj, &pos, &key, &value)) {
		struct ldb_val v;

		v.data = PyString_AsString(value);
		v.length = PyString_Size(value);
		ldb_msg_add_value($1, PyString_AsString(key), &v);
	}
}

/*
 * Wrap struct ldb_result
 */

%array_functions(struct ldb_message *, ldb_message_ptr_array);

struct ldb_result {
	unsigned int count;
	struct ldb_message **msgs;
	char **refs;
	struct ldb_control **controls;
};

/*
 * Wrap ldb functions 
 */

%rename ldb_init init;
struct ldb_context *ldb_init(TALLOC_CTX *mem_ctx);

%rename ldb_errstring errstring;
const char *ldb_errstring(struct ldb_context *ldb);

%rename ldb_connect connect;
int ldb_connect(struct ldb_context *ldb, const char *url, unsigned int flags, const char *options[]);

%rename ldb_search search;
int ldb_search(struct ldb_context *ldb, const struct ldb_dn *base, enum ldb_scope scope, const char *expression, const char * const *attrs, struct ldb_result **OUT);

%rename ldb_delete delete;
int ldb_delete(struct ldb_context *ldb, const struct ldb_dn *dn);

%rename ldb_rename rename;
int ldb_rename(struct ldb_context *ldb, const struct ldb_dn *olddn, const struct ldb_dn *newdn);

%rename ldb_add add;
int ldb_add(struct ldb_context *ldb, const struct ldb_message *message);