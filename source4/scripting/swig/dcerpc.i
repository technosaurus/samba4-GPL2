/* Tastes like -*- C -*- */

/* 
   Unix SMB/CIFS implementation.

   Swig interface to librpc functions.

   Copyright (C) Tim Potter 2004
   
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

%module dcerpc

%{

/* This symbol is used in both includes.h and Python.h which causes an
   annoying compiler warning. */

#ifdef HAVE_FSTAT
#undef HAVE_FSTAT
#endif

#include "includes.h"

#undef strcpy

PyObject *ntstatus_exception;

/* Set up return of a dcerpc.NTSTATUS exception */

void set_ntstatus_exception(int status)
{
	PyObject *obj = Py_BuildValue("(i,s)", status, 
				nt_errstr(NT_STATUS(status)));

	PyErr_SetObject(ntstatus_exception, obj);
}

/* Conversion functions for scalar types */

uint8 uint8_from_python(PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return 0;
	}

	if (!PyInt_Check(obj) && !PyLong_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting int or long value for %s", name);
		return 0;
	}

	if (PyLong_Check(obj))
		return (uint8)PyLong_AsLong(obj);
	else
		return (uint8)PyInt_AsLong(obj);
}

PyObject *uint8_to_python(uint8 obj)
{
	return PyInt_FromLong(obj);
}

uint16 uint16_from_python(PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return 0;
	}

	if (!PyInt_Check(obj) && !PyLong_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting int or long value for %s", name);
		return 0;
	}

	if (PyLong_Check(obj))
		return (uint16)PyLong_AsLong(obj);
	else
		return (uint16)PyInt_AsLong(obj);
}

PyObject *uint16_to_python(uint16 obj)
{
	return PyInt_FromLong(obj);
}

uint32 uint32_from_python(PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return 0;
	}

	if (!PyLong_Check(obj) && !PyInt_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting int or long value for %s", name);
		return 0;
	}

	if (PyLong_Check(obj))
		return (uint32)PyLong_AsLong(obj);
	else
		return (uint32)PyInt_AsLong(obj);
}

PyObject *uint32_to_python(uint32 obj)
{
	return PyLong_FromLong(obj);
}

int64 int64_from_python(PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return 0;
	}

	if (!PyLong_Check(obj) && !PyInt_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting int or long value for %s", name);
		return 0;
	}

	if (PyLong_Check(obj))
		return (int64)PyLong_AsLongLong(obj);
	else
		return (int64)PyInt_AsLong(obj);
}

PyObject *int64_to_python(int64 obj)
{
	return PyLong_FromLongLong(obj);
}

uint64 uint64_from_python(PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return 0;
	}

	if (!PyLong_Check(obj) && !PyInt_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting int or long value for %s", name);
		return 0;
	}

	if (PyLong_Check(obj))
		return (uint64)PyLong_AsUnsignedLongLong(obj);
	else
		return (uint64)PyInt_AsLong(obj);
}

PyObject *uint64_to_python(uint64 obj)
{
	return PyLong_FromUnsignedLongLong(obj);
}

NTTIME NTTIME_from_python(PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return 0;
	}

	if (!PyLong_Check(obj) && !PyInt_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting int or long value for %s", name);
		return 0;
	}

	if (PyLong_Check(obj))
		return (NTTIME)PyLong_AsUnsignedLongLong(obj);
	else
		return (NTTIME)PyInt_AsUnsignedLongMask(obj);
}

PyObject *NTTIME_to_python(NTTIME obj)
{
	return PyLong_FromUnsignedLongLong(obj);
}

HYPER_T HYPER_T_from_python(PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return 0;
	}

	if (!PyLong_Check(obj) && !PyInt_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting int or long value for %s", name);
		return 0;
	}

	if (PyLong_Check(obj))
		return (HYPER_T)PyLong_AsUnsignedLongLong(obj);
	else
		return (HYPER_T)PyInt_AsUnsignedLongMask(obj);
}

PyObject *HYPER_T_to_python(HYPER_T obj)
{
	return PyLong_FromUnsignedLongLong(obj);
}

/* Conversion functions for types that we don't want generated automatically.
   This is mostly security realted stuff in misc.idl */

char *string_ptr_from_python(TALLOC_CTX *mem_ctx, PyObject *obj, char *name)
{
	if (obj == NULL) {
		PyErr_Format(PyExc_ValueError, "Expecting key %s", name);
		return NULL;
	}

	if (obj == Py_None)
		return NULL;

	if (!PyString_Check(obj)) {
		PyErr_Format(PyExc_TypeError, "Expecting string value for %s", name);
		return NULL;
	}

	return PyString_AsString(obj);
}

PyObject *string_ptr_to_python(TALLOC_CTX *mem_ctx, char *obj)
{
	if (obj == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	return PyString_FromString(obj);
}

#define dom_sid2_ptr_to_python dom_sid_ptr_to_python
#define dom_sid2_ptr_from_python dom_sid_ptr_from_python

%}

%include "samba.i"

/* Win32 status codes */

#define STATUS_BUFFER_OVERFLOW            0x80000005
#define STATUS_NO_MORE_FILES              0x80000006
#define NT_STATUS_NO_MORE_ENTRIES         0x8000001a

#define STATUS_MORE_ENTRIES               0x0105
#define STATUS_SOME_UNMAPPED              0x0107
#define ERROR_INVALID_PARAMETER		  0x0057
#define ERROR_INSUFFICIENT_BUFFER	  0x007a
#define STATUS_NOTIFY_ENUM_DIR            0x010c
#define ERROR_INVALID_DATATYPE		  0x070c

/* NT status codes */

#define NT_STATUS_OK 0x00000000
#define NT_STATUS_UNSUCCESSFUL 0xC0000001
#define NT_STATUS_NOT_IMPLEMENTED 0xC0000002
#define NT_STATUS_INVALID_INFO_CLASS 0xC0000003
#define NT_STATUS_INFO_LENGTH_MISMATCH 0xC0000004
#define NT_STATUS_ACCESS_VIOLATION 0xC0000005
#define NT_STATUS_IN_PAGE_ERROR 0xC0000006
#define NT_STATUS_PAGEFILE_QUOTA 0xC0000007
#define NT_STATUS_INVALID_HANDLE 0xC0000008
#define NT_STATUS_BAD_INITIAL_STACK 0xC0000009
#define NT_STATUS_BAD_INITIAL_PC 0xC000000a
#define NT_STATUS_INVALID_CID 0xC000000b
#define NT_STATUS_TIMER_NOT_CANCELED 0xC000000c
#define NT_STATUS_INVALID_PARAMETER 0xC000000d
#define NT_STATUS_NO_SUCH_DEVICE 0xC000000e
#define NT_STATUS_NO_SUCH_FILE 0xC000000f
#define NT_STATUS_INVALID_DEVICE_REQUEST 0xC0000010
#define NT_STATUS_END_OF_FILE 0xC0000011
#define NT_STATUS_WRONG_VOLUME 0xC0000012
#define NT_STATUS_NO_MEDIA_IN_DEVICE 0xC0000013
#define NT_STATUS_UNRECOGNIZED_MEDIA 0xC0000014
#define NT_STATUS_NONEXISTENT_SECTOR 0xC0000015
#define NT_STATUS_MORE_PROCESSING_REQUIRED 0xC0000016
#define NT_STATUS_NO_MEMORY 0xC0000017
#define NT_STATUS_CONFLICTING_ADDRESSES 0xC0000018
#define NT_STATUS_NOT_MAPPED_VIEW 0xC0000019
#define NT_STATUS_UNABLE_TO_FREE_VM 0xC000001a
#define NT_STATUS_UNABLE_TO_DELETE_SECTION 0xC000001b
#define NT_STATUS_INVALID_SYSTEM_SERVICE 0xC000001c
#define NT_STATUS_ILLEGAL_INSTRUCTION 0xC000001d
#define NT_STATUS_INVALID_LOCK_SEQUENCE 0xC000001e
#define NT_STATUS_INVALID_VIEW_SIZE 0xC000001f
#define NT_STATUS_INVALID_FILE_FOR_SECTION 0xC0000020
#define NT_STATUS_ALREADY_COMMITTED 0xC0000021
#define NT_STATUS_ACCESS_DENIED 0xC0000022
#define NT_STATUS_BUFFER_TOO_SMALL 0xC0000023
#define NT_STATUS_OBJECT_TYPE_MISMATCH 0xC0000024
#define NT_STATUS_NONCONTINUABLE_EXCEPTION 0xC0000025
#define NT_STATUS_INVALID_DISPOSITION 0xC0000026
#define NT_STATUS_UNWIND 0xC0000027
#define NT_STATUS_BAD_STACK 0xC0000028
#define NT_STATUS_INVALID_UNWIND_TARGET 0xC0000029
#define NT_STATUS_NOT_LOCKED 0xC000002a
#define NT_STATUS_PARITY_ERROR 0xC000002b
#define NT_STATUS_UNABLE_TO_DECOMMIT_VM 0xC000002c
#define NT_STATUS_NOT_COMMITTED 0xC000002d
#define NT_STATUS_INVALID_PORT_ATTRIBUTES 0xC000002e
#define NT_STATUS_PORT_MESSAGE_TOO_LONG 0xC000002f
#define NT_STATUS_INVALID_PARAMETER_MIX 0xC0000030
#define NT_STATUS_INVALID_QUOTA_LOWER 0xC0000031
#define NT_STATUS_DISK_CORRUPT_ERROR 0xC0000032
#define NT_STATUS_OBJECT_NAME_INVALID 0xC0000033
#define NT_STATUS_OBJECT_NAME_NOT_FOUND 0xC0000034
#define NT_STATUS_OBJECT_NAME_COLLISION 0xC0000035
#define NT_STATUS_HANDLE_NOT_WAITABLE 0xC0000036
#define NT_STATUS_PORT_DISCONNECTED 0xC0000037
#define NT_STATUS_DEVICE_ALREADY_ATTACHED 0xC0000038
#define NT_STATUS_OBJECT_PATH_INVALID 0xC0000039
#define NT_STATUS_OBJECT_PATH_NOT_FOUND 0xC000003a
#define NT_STATUS_OBJECT_PATH_SYNTAX_BAD 0xC000003b
#define NT_STATUS_DATA_OVERRUN 0xC000003c
#define NT_STATUS_DATA_LATE_ERROR 0xC000003d
#define NT_STATUS_DATA_ERROR 0xC000003e
#define NT_STATUS_CRC_ERROR 0xC000003f
#define NT_STATUS_SECTION_TOO_BIG 0xC0000040
#define NT_STATUS_PORT_CONNECTION_REFUSED 0xC0000041
#define NT_STATUS_INVALID_PORT_HANDLE 0xC0000042
#define NT_STATUS_SHARING_VIOLATION 0xC0000043
#define NT_STATUS_QUOTA_EXCEEDED 0xC0000044
#define NT_STATUS_INVALID_PAGE_PROTECTION 0xC0000045
#define NT_STATUS_MUTANT_NOT_OWNED 0xC0000046
#define NT_STATUS_SEMAPHORE_LIMIT_EXCEEDED 0xC0000047
#define NT_STATUS_PORT_ALREADY_SET 0xC0000048
#define NT_STATUS_SECTION_NOT_IMAGE 0xC0000049
#define NT_STATUS_SUSPEND_COUNT_EXCEEDED 0xC000004a
#define NT_STATUS_THREAD_IS_TERMINATING 0xC000004b
#define NT_STATUS_BAD_WORKING_SET_LIMIT 0xC000004c
#define NT_STATUS_INCOMPATIBLE_FILE_MAP 0xC000004d
#define NT_STATUS_SECTION_PROTECTION 0xC000004e
#define NT_STATUS_EAS_NOT_SUPPORTED 0xC000004f
#define NT_STATUS_EA_TOO_LARGE 0xC0000050
#define NT_STATUS_NONEXISTENT_EA_ENTRY 0xC0000051
#define NT_STATUS_NO_EAS_ON_FILE 0xC0000052
#define NT_STATUS_EA_CORRUPT_ERROR 0xC0000053
#define NT_STATUS_FILE_LOCK_CONFLICT 0xC0000054
#define NT_STATUS_LOCK_NOT_GRANTED 0xC0000055
#define NT_STATUS_DELETE_PENDING 0xC0000056
#define NT_STATUS_CTL_FILE_NOT_SUPPORTED 0xC0000057
#define NT_STATUS_UNKNOWN_REVISION 0xC0000058
#define NT_STATUS_REVISION_MISMATCH 0xC0000059
#define NT_STATUS_INVALID_OWNER 0xC000005a
#define NT_STATUS_INVALID_PRIMARY_GROUP 0xC000005b
#define NT_STATUS_NO_IMPERSONATION_TOKEN 0xC000005c
#define NT_STATUS_CANT_DISABLE_MANDATORY 0xC000005d
#define NT_STATUS_NO_LOGON_SERVERS 0xC000005e
#define NT_STATUS_NO_SUCH_LOGON_SESSION 0xC000005f
#define NT_STATUS_NO_SUCH_PRIVILEGE 0xC0000060
#define NT_STATUS_PRIVILEGE_NOT_HELD 0xC0000061
#define NT_STATUS_INVALID_ACCOUNT_NAME 0xC0000062
#define NT_STATUS_USER_EXISTS 0xC0000063
#define NT_STATUS_NO_SUCH_USER 0xC0000064
#define NT_STATUS_GROUP_EXISTS 0xC0000065
#define NT_STATUS_NO_SUCH_GROUP 0xC0000066
#define NT_STATUS_MEMBER_IN_GROUP 0xC0000067
#define NT_STATUS_MEMBER_NOT_IN_GROUP 0xC0000068
#define NT_STATUS_LAST_ADMIN 0xC0000069
#define NT_STATUS_WRONG_PASSWORD 0xC000006a
#define NT_STATUS_ILL_FORMED_PASSWORD 0xC000006b
#define NT_STATUS_PASSWORD_RESTRICTION 0xC000006c
#define NT_STATUS_LOGON_FAILURE 0xC000006d
#define NT_STATUS_ACCOUNT_RESTRICTION 0xC000006e
#define NT_STATUS_INVALID_LOGON_HOURS 0xC000006f
#define NT_STATUS_INVALID_WORKSTATION 0xC0000070
#define NT_STATUS_PASSWORD_EXPIRED 0xC0000071
#define NT_STATUS_ACCOUNT_DISABLED 0xC0000072
#define NT_STATUS_NONE_MAPPED 0xC0000073
#define NT_STATUS_TOO_MANY_LUIDS_REQUESTED 0xC0000074
#define NT_STATUS_LUIDS_EXHAUSTED 0xC0000075
#define NT_STATUS_INVALID_SUB_AUTHORITY 0xC0000076
#define NT_STATUS_INVALID_ACL 0xC0000077
#define NT_STATUS_INVALID_SID 0xC0000078
#define NT_STATUS_INVALID_SECURITY_DESCR 0xC0000079
#define NT_STATUS_PROCEDURE_NOT_FOUND 0xC000007a
#define NT_STATUS_INVALID_IMAGE_FORMAT 0xC000007b
#define NT_STATUS_NO_TOKEN 0xC000007c
#define NT_STATUS_BAD_INHERITANCE_ACL 0xC000007d
#define NT_STATUS_RANGE_NOT_LOCKED 0xC000007e
#define NT_STATUS_DISK_FULL 0xC000007f
#define NT_STATUS_SERVER_DISABLED 0xC0000080
#define NT_STATUS_SERVER_NOT_DISABLED 0xC0000081
#define NT_STATUS_TOO_MANY_GUIDS_REQUESTED 0xC0000082
#define NT_STATUS_GUIDS_EXHAUSTED 0xC0000083
#define NT_STATUS_INVALID_ID_AUTHORITY 0xC0000084
#define NT_STATUS_AGENTS_EXHAUSTED 0xC0000085
#define NT_STATUS_INVALID_VOLUME_LABEL 0xC0000086
#define NT_STATUS_SECTION_NOT_EXTENDED 0xC0000087
#define NT_STATUS_NOT_MAPPED_DATA 0xC0000088
#define NT_STATUS_RESOURCE_DATA_NOT_FOUND 0xC0000089
#define NT_STATUS_RESOURCE_TYPE_NOT_FOUND 0xC000008a
#define NT_STATUS_RESOURCE_NAME_NOT_FOUND 0xC000008b
#define NT_STATUS_ARRAY_BOUNDS_EXCEEDED 0xC000008c
#define NT_STATUS_FLOAT_DENORMAL_OPERAND 0xC000008d
#define NT_STATUS_FLOAT_DIVIDE_BY_ZERO 0xC000008e
#define NT_STATUS_FLOAT_INEXACT_RESULT 0xC000008f
#define NT_STATUS_FLOAT_INVALID_OPERATION 0xC0000090
#define NT_STATUS_FLOAT_OVERFLOW 0xC0000091
#define NT_STATUS_FLOAT_STACK_CHECK 0xC0000092
#define NT_STATUS_FLOAT_UNDERFLOW 0xC0000093
#define NT_STATUS_INTEGER_DIVIDE_BY_ZERO 0xC0000094
#define NT_STATUS_INTEGER_OVERFLOW 0xC0000095
#define NT_STATUS_PRIVILEGED_INSTRUCTION 0xC0000096
#define NT_STATUS_TOO_MANY_PAGING_FILES 0xC0000097
#define NT_STATUS_FILE_INVALID 0xC0000098
#define NT_STATUS_ALLOTTED_SPACE_EXCEEDED 0xC0000099
#define NT_STATUS_INSUFFICIENT_RESOURCES 0xC000009a
#define NT_STATUS_DFS_EXIT_PATH_FOUND 0xC000009b
#define NT_STATUS_DEVICE_DATA_ERROR 0xC000009c
#define NT_STATUS_DEVICE_NOT_CONNECTED 0xC000009d
#define NT_STATUS_DEVICE_POWER_FAILURE 0xC000009e
#define NT_STATUS_FREE_VM_NOT_AT_BASE 0xC000009f
#define NT_STATUS_MEMORY_NOT_ALLOCATED 0xC00000a0
#define NT_STATUS_WORKING_SET_QUOTA 0xC00000a1
#define NT_STATUS_MEDIA_WRITE_PROTECTED 0xC00000a2
#define NT_STATUS_DEVICE_NOT_READY 0xC00000a3
#define NT_STATUS_INVALID_GROUP_ATTRIBUTES 0xC00000a4
#define NT_STATUS_BAD_IMPERSONATION_LEVEL 0xC00000a5
#define NT_STATUS_CANT_OPEN_ANONYMOUS 0xC00000a6
#define NT_STATUS_BAD_VALIDATION_CLASS 0xC00000a7
#define NT_STATUS_BAD_TOKEN_TYPE 0xC00000a8
#define NT_STATUS_BAD_MASTER_BOOT_RECORD 0xC00000a9
#define NT_STATUS_INSTRUCTION_MISALIGNMENT 0xC00000aa
#define NT_STATUS_INSTANCE_NOT_AVAILABLE 0xC00000ab
#define NT_STATUS_PIPE_NOT_AVAILABLE 0xC00000ac
#define NT_STATUS_INVALID_PIPE_STATE 0xC00000ad
#define NT_STATUS_PIPE_BUSY 0xC00000ae
#define NT_STATUS_ILLEGAL_FUNCTION 0xC00000af
#define NT_STATUS_PIPE_DISCONNECTED 0xC00000b0
#define NT_STATUS_PIPE_CLOSING 0xC00000b1
#define NT_STATUS_PIPE_CONNECTED 0xC00000b2
#define NT_STATUS_PIPE_LISTENING 0xC00000b3
#define NT_STATUS_INVALID_READ_MODE 0xC00000b4
#define NT_STATUS_IO_TIMEOUT 0xC00000b5
#define NT_STATUS_FILE_FORCED_CLOSED 0xC00000b6
#define NT_STATUS_PROFILING_NOT_STARTED 0xC00000b7
#define NT_STATUS_PROFILING_NOT_STOPPED 0xC00000b8
#define NT_STATUS_COULD_NOT_INTERPRET 0xC00000b9
#define NT_STATUS_FILE_IS_A_DIRECTORY 0xC00000ba
#define NT_STATUS_NOT_SUPPORTED 0xC00000bb
#define NT_STATUS_REMOTE_NOT_LISTENING 0xC00000bc
#define NT_STATUS_DUPLICATE_NAME 0xC00000bd
#define NT_STATUS_BAD_NETWORK_PATH 0xC00000be
#define NT_STATUS_NETWORK_BUSY 0xC00000bf
#define NT_STATUS_DEVICE_DOES_NOT_EXIST 0xC00000c0
#define NT_STATUS_TOO_MANY_COMMANDS 0xC00000c1
#define NT_STATUS_ADAPTER_HARDWARE_ERROR 0xC00000c2
#define NT_STATUS_INVALID_NETWORK_RESPONSE 0xC00000c3
#define NT_STATUS_UNEXPECTED_NETWORK_ERROR 0xC00000c4
#define NT_STATUS_BAD_REMOTE_ADAPTER 0xC00000c5
#define NT_STATUS_PRINT_QUEUE_FULL 0xC00000c6
#define NT_STATUS_NO_SPOOL_SPACE 0xC00000c7
#define NT_STATUS_PRINT_CANCELLED 0xC00000c8
#define NT_STATUS_NETWORK_NAME_DELETED 0xC00000c9
#define NT_STATUS_NETWORK_ACCESS_DENIED 0xC00000ca
#define NT_STATUS_BAD_DEVICE_TYPE 0xC00000cb
#define NT_STATUS_BAD_NETWORK_NAME 0xC00000cc
#define NT_STATUS_TOO_MANY_NAMES 0xC00000cd
#define NT_STATUS_TOO_MANY_SESSIONS 0xC00000ce
#define NT_STATUS_SHARING_PAUSED 0xC00000cf
#define NT_STATUS_REQUEST_NOT_ACCEPTED 0xC00000d0
#define NT_STATUS_REDIRECTOR_PAUSED 0xC00000d1
#define NT_STATUS_NET_WRITE_FAULT 0xC00000d2
#define NT_STATUS_PROFILING_AT_LIMIT 0xC00000d3
#define NT_STATUS_NOT_SAME_DEVICE 0xC00000d4
#define NT_STATUS_FILE_RENAMED 0xC00000d5
#define NT_STATUS_VIRTUAL_CIRCUIT_CLOSED 0xC00000d6
#define NT_STATUS_NO_SECURITY_ON_OBJECT 0xC00000d7
#define NT_STATUS_CANT_WAIT 0xC00000d8
#define NT_STATUS_PIPE_EMPTY 0xC00000d9
#define NT_STATUS_CANT_ACCESS_DOMAIN_INFO 0xC00000da
#define NT_STATUS_CANT_TERMINATE_SELF 0xC00000db
#define NT_STATUS_INVALID_SERVER_STATE 0xC00000dc
#define NT_STATUS_INVALID_DOMAIN_STATE 0xC00000dd
#define NT_STATUS_INVALID_DOMAIN_ROLE 0xC00000de
#define NT_STATUS_NO_SUCH_DOMAIN 0xC00000df
#define NT_STATUS_DOMAIN_EXISTS 0xC00000e0
#define NT_STATUS_DOMAIN_LIMIT_EXCEEDED 0xC00000e1
#define NT_STATUS_OPLOCK_NOT_GRANTED 0xC00000e2
#define NT_STATUS_INVALID_OPLOCK_PROTOCOL 0xC00000e3
#define NT_STATUS_INTERNAL_DB_CORRUPTION 0xC00000e4
#define NT_STATUS_INTERNAL_ERROR 0xC00000e5
#define NT_STATUS_GENERIC_NOT_MAPPED 0xC00000e6
#define NT_STATUS_BAD_DESCRIPTOR_FORMAT 0xC00000e7
#define NT_STATUS_INVALID_USER_BUFFER 0xC00000e8
#define NT_STATUS_UNEXPECTED_IO_ERROR 0xC00000e9
#define NT_STATUS_UNEXPECTED_MM_CREATE_ERR 0xC00000ea
#define NT_STATUS_UNEXPECTED_MM_MAP_ERROR 0xC00000eb
#define NT_STATUS_UNEXPECTED_MM_EXTEND_ERR 0xC00000ec
#define NT_STATUS_NOT_LOGON_PROCESS 0xC00000ed
#define NT_STATUS_LOGON_SESSION_EXISTS 0xC00000ee
#define NT_STATUS_INVALID_PARAMETER_1 0xC00000ef
#define NT_STATUS_INVALID_PARAMETER_2 0xC00000f0
#define NT_STATUS_INVALID_PARAMETER_3 0xC00000f1
#define NT_STATUS_INVALID_PARAMETER_4 0xC00000f2
#define NT_STATUS_INVALID_PARAMETER_5 0xC00000f3
#define NT_STATUS_INVALID_PARAMETER_6 0xC00000f4
#define NT_STATUS_INVALID_PARAMETER_7 0xC00000f5
#define NT_STATUS_INVALID_PARAMETER_8 0xC00000f6
#define NT_STATUS_INVALID_PARAMETER_9 0xC00000f7
#define NT_STATUS_INVALID_PARAMETER_10 0xC00000f8
#define NT_STATUS_INVALID_PARAMETER_11 0xC00000f9
#define NT_STATUS_INVALID_PARAMETER_12 0xC00000fa
#define NT_STATUS_REDIRECTOR_NOT_STARTED 0xC00000fb
#define NT_STATUS_REDIRECTOR_STARTED 0xC00000fc
#define NT_STATUS_STACK_OVERFLOW 0xC00000fd
#define NT_STATUS_NO_SUCH_PACKAGE 0xC00000fe
#define NT_STATUS_BAD_FUNCTION_TABLE 0xC00000ff
#define NT_STATUS_DIRECTORY_NOT_EMPTY 0xC0000101
#define NT_STATUS_FILE_CORRUPT_ERROR 0xC0000102
#define NT_STATUS_NOT_A_DIRECTORY 0xC0000103
#define NT_STATUS_BAD_LOGON_SESSION_STATE 0xC0000104
#define NT_STATUS_LOGON_SESSION_COLLISION 0xC0000105
#define NT_STATUS_NAME_TOO_LONG 0xC0000106
#define NT_STATUS_FILES_OPEN 0xC0000107
#define NT_STATUS_CONNECTION_IN_USE 0xC0000108
#define NT_STATUS_MESSAGE_NOT_FOUND 0xC0000109
#define NT_STATUS_PROCESS_IS_TERMINATING 0xC000010a
#define NT_STATUS_INVALID_LOGON_TYPE 0xC000010b
#define NT_STATUS_NO_GUID_TRANSLATION 0xC000010c
#define NT_STATUS_CANNOT_IMPERSONATE 0xC000010d
#define NT_STATUS_IMAGE_ALREADY_LOADED 0xC000010e
#define NT_STATUS_ABIOS_NOT_PRESENT 0xC000010f
#define NT_STATUS_ABIOS_LID_NOT_EXIST 0xC0000110
#define NT_STATUS_ABIOS_LID_ALREADY_OWNED 0xC0000111
#define NT_STATUS_ABIOS_NOT_LID_OWNER 0xC0000112
#define NT_STATUS_ABIOS_INVALID_COMMAND 0xC0000113
#define NT_STATUS_ABIOS_INVALID_LID 0xC0000114
#define NT_STATUS_ABIOS_SELECTOR_NOT_AVAILABLE 0xC0000115
#define NT_STATUS_ABIOS_INVALID_SELECTOR 0xC0000116
#define NT_STATUS_NO_LDT 0xC0000117
#define NT_STATUS_INVALID_LDT_SIZE 0xC0000118
#define NT_STATUS_INVALID_LDT_OFFSET 0xC0000119
#define NT_STATUS_INVALID_LDT_DESCRIPTOR 0xC000011a
#define NT_STATUS_INVALID_IMAGE_NE_FORMAT 0xC000011b
#define NT_STATUS_RXACT_INVALID_STATE 0xC000011c
#define NT_STATUS_RXACT_COMMIT_FAILURE 0xC000011d
#define NT_STATUS_MAPPED_FILE_SIZE_ZERO 0xC000011e
#define NT_STATUS_TOO_MANY_OPENED_FILES 0xC000011f
#define NT_STATUS_CANCELLED 0xC0000120
#define NT_STATUS_CANNOT_DELETE 0xC0000121
#define NT_STATUS_INVALID_COMPUTER_NAME 0xC0000122
#define NT_STATUS_FILE_DELETED 0xC0000123
#define NT_STATUS_SPECIAL_ACCOUNT 0xC0000124
#define NT_STATUS_SPECIAL_GROUP 0xC0000125
#define NT_STATUS_SPECIAL_USER 0xC0000126
#define NT_STATUS_MEMBERS_PRIMARY_GROUP 0xC0000127
#define NT_STATUS_FILE_CLOSED 0xC0000128
#define NT_STATUS_TOO_MANY_THREADS 0xC0000129
#define NT_STATUS_THREAD_NOT_IN_PROCESS 0xC000012a
#define NT_STATUS_TOKEN_ALREADY_IN_USE 0xC000012b
#define NT_STATUS_PAGEFILE_QUOTA_EXCEEDED 0xC000012c
#define NT_STATUS_COMMITMENT_LIMIT 0xC000012d
#define NT_STATUS_INVALID_IMAGE_LE_FORMAT 0xC000012e
#define NT_STATUS_INVALID_IMAGE_NOT_MZ 0xC000012f
#define NT_STATUS_INVALID_IMAGE_PROTECT 0xC0000130
#define NT_STATUS_INVALID_IMAGE_WIN_16 0xC0000131
#define NT_STATUS_LOGON_SERVER_CONFLICT 0xC0000132
#define NT_STATUS_TIME_DIFFERENCE_AT_DC 0xC0000133
#define NT_STATUS_SYNCHRONIZATION_REQUIRED 0xC0000134
#define NT_STATUS_DLL_NOT_FOUND 0xC0000135
#define NT_STATUS_OPEN_FAILED 0xC0000136
#define NT_STATUS_IO_PRIVILEGE_FAILED 0xC0000137
#define NT_STATUS_ORDINAL_NOT_FOUND 0xC0000138
#define NT_STATUS_ENTRYPOINT_NOT_FOUND 0xC0000139
#define NT_STATUS_CONTROL_C_EXIT 0xC000013a
#define NT_STATUS_LOCAL_DISCONNECT 0xC000013b
#define NT_STATUS_REMOTE_DISCONNECT 0xC000013c
#define NT_STATUS_REMOTE_RESOURCES 0xC000013d
#define NT_STATUS_LINK_FAILED 0xC000013e
#define NT_STATUS_LINK_TIMEOUT 0xC000013f
#define NT_STATUS_INVALID_CONNECTION 0xC0000140
#define NT_STATUS_INVALID_ADDRESS 0xC0000141
#define NT_STATUS_DLL_INIT_FAILED 0xC0000142
#define NT_STATUS_MISSING_SYSTEMFILE 0xC0000143
#define NT_STATUS_UNHANDLED_EXCEPTION 0xC0000144
#define NT_STATUS_APP_INIT_FAILURE 0xC0000145
#define NT_STATUS_PAGEFILE_CREATE_FAILED 0xC0000146
#define NT_STATUS_NO_PAGEFILE 0xC0000147
#define NT_STATUS_INVALID_LEVEL 0xC0000148
#define NT_STATUS_WRONG_PASSWORD_CORE 0xC0000149
#define NT_STATUS_ILLEGAL_FLOAT_CONTEXT 0xC000014a
#define NT_STATUS_PIPE_BROKEN 0xC000014b
#define NT_STATUS_REGISTRY_CORRUPT 0xC000014c
#define NT_STATUS_REGISTRY_IO_FAILED 0xC000014d
#define NT_STATUS_NO_EVENT_PAIR 0xC000014e
#define NT_STATUS_UNRECOGNIZED_VOLUME 0xC000014f
#define NT_STATUS_SERIAL_NO_DEVICE_INITED 0xC0000150
#define NT_STATUS_NO_SUCH_ALIAS 0xC0000151
#define NT_STATUS_MEMBER_NOT_IN_ALIAS 0xC0000152
#define NT_STATUS_MEMBER_IN_ALIAS 0xC0000153
#define NT_STATUS_ALIAS_EXISTS 0xC0000154
#define NT_STATUS_LOGON_NOT_GRANTED 0xC0000155
#define NT_STATUS_TOO_MANY_SECRETS 0xC0000156
#define NT_STATUS_SECRET_TOO_LONG 0xC0000157
#define NT_STATUS_INTERNAL_DB_ERROR 0xC0000158
#define NT_STATUS_FULLSCREEN_MODE 0xC0000159
#define NT_STATUS_TOO_MANY_CONTEXT_IDS 0xC000015a
#define NT_STATUS_LOGON_TYPE_NOT_GRANTED 0xC000015b
#define NT_STATUS_NOT_REGISTRY_FILE 0xC000015c
#define NT_STATUS_NT_CROSS_ENCRYPTION_REQUIRED 0xC000015d
#define NT_STATUS_DOMAIN_CTRLR_CONFIG_ERROR 0xC000015e
#define NT_STATUS_FT_MISSING_MEMBER 0xC000015f
#define NT_STATUS_ILL_FORMED_SERVICE_ENTRY 0xC0000160
#define NT_STATUS_ILLEGAL_CHARACTER 0xC0000161
#define NT_STATUS_UNMAPPABLE_CHARACTER 0xC0000162
#define NT_STATUS_UNDEFINED_CHARACTER 0xC0000163
#define NT_STATUS_FLOPPY_VOLUME 0xC0000164
#define NT_STATUS_FLOPPY_ID_MARK_NOT_FOUND 0xC0000165
#define NT_STATUS_FLOPPY_WRONG_CYLINDER 0xC0000166
#define NT_STATUS_FLOPPY_UNKNOWN_ERROR 0xC0000167
#define NT_STATUS_FLOPPY_BAD_REGISTERS 0xC0000168
#define NT_STATUS_DISK_RECALIBRATE_FAILED 0xC0000169
#define NT_STATUS_DISK_OPERATION_FAILED 0xC000016a
#define NT_STATUS_DISK_RESET_FAILED 0xC000016b
#define NT_STATUS_SHARED_IRQ_BUSY 0xC000016c
#define NT_STATUS_FT_ORPHANING 0xC000016d
#define NT_STATUS_PARTITION_FAILURE 0xC0000172
#define NT_STATUS_INVALID_BLOCK_LENGTH 0xC0000173
#define NT_STATUS_DEVICE_NOT_PARTITIONED 0xC0000174
#define NT_STATUS_UNABLE_TO_LOCK_MEDIA 0xC0000175
#define NT_STATUS_UNABLE_TO_UNLOAD_MEDIA 0xC0000176
#define NT_STATUS_EOM_OVERFLOW 0xC0000177
#define NT_STATUS_NO_MEDIA 0xC0000178
#define NT_STATUS_NO_SUCH_MEMBER 0xC000017a
#define NT_STATUS_INVALID_MEMBER 0xC000017b
#define NT_STATUS_KEY_DELETED 0xC000017c
#define NT_STATUS_NO_LOG_SPACE 0xC000017d
#define NT_STATUS_TOO_MANY_SIDS 0xC000017e
#define NT_STATUS_LM_CROSS_ENCRYPTION_REQUIRED 0xC000017f
#define NT_STATUS_KEY_HAS_CHILDREN 0xC0000180
#define NT_STATUS_CHILD_MUST_BE_VOLATILE 0xC0000181
#define NT_STATUS_DEVICE_CONFIGURATION_ERROR 0xC0000182
#define NT_STATUS_DRIVER_INTERNAL_ERROR 0xC0000183
#define NT_STATUS_INVALID_DEVICE_STATE 0xC0000184
#define NT_STATUS_IO_DEVICE_ERROR 0xC0000185
#define NT_STATUS_DEVICE_PROTOCOL_ERROR 0xC0000186
#define NT_STATUS_BACKUP_CONTROLLER 0xC0000187
#define NT_STATUS_LOG_FILE_FULL 0xC0000188
#define NT_STATUS_TOO_LATE 0xC0000189
#define NT_STATUS_NO_TRUST_LSA_SECRET 0xC000018a
#define NT_STATUS_NO_TRUST_SAM_ACCOUNT 0xC000018b
#define NT_STATUS_TRUSTED_DOMAIN_FAILURE 0xC000018c
#define NT_STATUS_TRUSTED_RELATIONSHIP_FAILURE 0xC000018d
#define NT_STATUS_EVENTLOG_FILE_CORRUPT 0xC000018e
#define NT_STATUS_EVENTLOG_CANT_START 0xC000018f
#define NT_STATUS_TRUST_FAILURE 0xC0000190
#define NT_STATUS_MUTANT_LIMIT_EXCEEDED 0xC0000191
#define NT_STATUS_NETLOGON_NOT_STARTED 0xC0000192
#define NT_STATUS_ACCOUNT_EXPIRED 0xC0000193
#define NT_STATUS_POSSIBLE_DEADLOCK 0xC0000194
#define NT_STATUS_NETWORK_CREDENTIAL_CONFLICT 0xC0000195
#define NT_STATUS_REMOTE_SESSION_LIMIT 0xC0000196
#define NT_STATUS_EVENTLOG_FILE_CHANGED 0xC0000197
#define NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT 0xC0000198
#define NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT 0xC0000199
#define NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT 0xC000019a
#define NT_STATUS_DOMAIN_TRUST_INCONSISTENT 0xC000019b
#define NT_STATUS_FS_DRIVER_REQUIRED 0xC000019c
#define NT_STATUS_NO_USER_SESSION_KEY 0xC0000202
#define NT_STATUS_USER_SESSION_DELETED 0xC0000203
#define NT_STATUS_RESOURCE_LANG_NOT_FOUND 0xC0000204
#define NT_STATUS_INSUFF_SERVER_RESOURCES 0xC0000205
#define NT_STATUS_INVALID_BUFFER_SIZE 0xC0000206
#define NT_STATUS_INVALID_ADDRESS_COMPONENT 0xC0000207
#define NT_STATUS_INVALID_ADDRESS_WILDCARD 0xC0000208
#define NT_STATUS_TOO_MANY_ADDRESSES 0xC0000209
#define NT_STATUS_ADDRESS_ALREADY_EXISTS 0xC000020a
#define NT_STATUS_ADDRESS_CLOSED 0xC000020b
#define NT_STATUS_CONNECTION_DISCONNECTED 0xC000020c
#define NT_STATUS_CONNECTION_RESET 0xC000020d
#define NT_STATUS_TOO_MANY_NODES 0xC000020e
#define NT_STATUS_TRANSACTION_ABORTED 0xC000020f
#define NT_STATUS_TRANSACTION_TIMED_OUT 0xC0000210
#define NT_STATUS_TRANSACTION_NO_RELEASE 0xC0000211
#define NT_STATUS_TRANSACTION_NO_MATCH 0xC0000212
#define NT_STATUS_TRANSACTION_RESPONDED 0xC0000213
#define NT_STATUS_TRANSACTION_INVALID_ID 0xC0000214
#define NT_STATUS_TRANSACTION_INVALID_TYPE 0xC0000215
#define NT_STATUS_NOT_SERVER_SESSION 0xC0000216
#define NT_STATUS_NOT_CLIENT_SESSION 0xC0000217
#define NT_STATUS_CANNOT_LOAD_REGISTRY_FILE 0xC0000218
#define NT_STATUS_DEBUG_ATTACH_FAILED 0xC0000219
#define NT_STATUS_SYSTEM_PROCESS_TERMINATED 0xC000021a
#define NT_STATUS_DATA_NOT_ACCEPTED 0xC000021b
#define NT_STATUS_NO_BROWSER_SERVERS_FOUND 0xC000021c
#define NT_STATUS_VDM_HARD_ERROR 0xC000021d
#define NT_STATUS_DRIVER_CANCEL_TIMEOUT 0xC000021e
#define NT_STATUS_REPLY_MESSAGE_MISMATCH 0xC000021f
#define NT_STATUS_MAPPED_ALIGNMENT 0xC0000220
#define NT_STATUS_IMAGE_CHECKSUM_MISMATCH 0xC0000221
#define NT_STATUS_LOST_WRITEBEHIND_DATA 0xC0000222
#define NT_STATUS_CLIENT_SERVER_PARAMETERS_INVALID 0xC0000223
#define NT_STATUS_PASSWORD_MUST_CHANGE 0xC0000224
#define NT_STATUS_NOT_FOUND 0xC0000225
#define NT_STATUS_NOT_TINY_STREAM 0xC0000226
#define NT_STATUS_RECOVERY_FAILURE 0xC0000227
#define NT_STATUS_STACK_OVERFLOW_READ 0xC0000228
#define NT_STATUS_FAIL_CHECK 0xC0000229
#define NT_STATUS_DUPLICATE_OBJECTID 0xC000022a
#define NT_STATUS_OBJECTID_EXISTS 0xC000022b
#define NT_STATUS_CONVERT_TO_LARGE 0xC000022c
#define NT_STATUS_RETRY 0xC000022d
#define NT_STATUS_FOUND_OUT_OF_SCOPE 0xC000022e
#define NT_STATUS_ALLOCATE_BUCKET 0xC000022f
#define NT_STATUS_PROPSET_NOT_FOUND 0xC0000230
#define NT_STATUS_MARSHALL_OVERFLOW 0xC0000231
#define NT_STATUS_INVALID_VARIANT 0xC0000232
#define NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND 0xC0000233
#define NT_STATUS_ACCOUNT_LOCKED_OUT 0xC0000234
#define NT_STATUS_HANDLE_NOT_CLOSABLE 0xC0000235
#define NT_STATUS_CONNECTION_REFUSED 0xC0000236
#define NT_STATUS_GRACEFUL_DISCONNECT 0xC0000237
#define NT_STATUS_ADDRESS_ALREADY_ASSOCIATED 0xC0000238
#define NT_STATUS_ADDRESS_NOT_ASSOCIATED 0xC0000239
#define NT_STATUS_CONNECTION_INVALID 0xC000023a
#define NT_STATUS_CONNECTION_ACTIVE 0xC000023b
#define NT_STATUS_NETWORK_UNREACHABLE 0xC000023c
#define NT_STATUS_HOST_UNREACHABLE 0xC000023d
#define NT_STATUS_PROTOCOL_UNREACHABLE 0xC000023e
#define NT_STATUS_PORT_UNREACHABLE 0xC000023f
#define NT_STATUS_REQUEST_ABORTED 0xC0000240
#define NT_STATUS_CONNECTION_ABORTED 0xC0000241
#define NT_STATUS_BAD_COMPRESSION_BUFFER 0xC0000242
#define NT_STATUS_USER_MAPPED_FILE 0xC0000243
#define NT_STATUS_AUDIT_FAILED 0xC0000244
#define NT_STATUS_TIMER_RESOLUTION_NOT_SET 0xC0000245
#define NT_STATUS_CONNECTION_COUNT_LIMIT 0xC0000246
#define NT_STATUS_LOGIN_TIME_RESTRICTION 0xC0000247
#define NT_STATUS_LOGIN_WKSTA_RESTRICTION 0xC0000248
#define NT_STATUS_IMAGE_MP_UP_MISMATCH 0xC0000249
#define NT_STATUS_INSUFFICIENT_LOGON_INFO 0xC0000250
#define NT_STATUS_BAD_DLL_ENTRYPOINT 0xC0000251
#define NT_STATUS_BAD_SERVICE_ENTRYPOINT 0xC0000252
#define NT_STATUS_LPC_REPLY_LOST 0xC0000253
#define NT_STATUS_IP_ADDRESS_CONFLICT1 0xC0000254
#define NT_STATUS_IP_ADDRESS_CONFLICT2 0xC0000255
#define NT_STATUS_REGISTRY_QUOTA_LIMIT 0xC0000256
#define NT_STATUS_PATH_NOT_COVERED 0xC0000257
#define NT_STATUS_NO_CALLBACK_ACTIVE 0xC0000258
#define NT_STATUS_LICENSE_QUOTA_EXCEEDED 0xC0000259
#define NT_STATUS_PWD_TOO_SHORT 0xC000025a
#define NT_STATUS_PWD_TOO_RECENT 0xC000025b
#define NT_STATUS_PWD_HISTORY_CONFLICT 0xC000025c
#define NT_STATUS_PLUGPLAY_NO_DEVICE 0xC000025e
#define NT_STATUS_UNSUPPORTED_COMPRESSION 0xC000025f
#define NT_STATUS_INVALID_HW_PROFILE 0xC0000260
#define NT_STATUS_INVALID_PLUGPLAY_DEVICE_PATH 0xC0000261
#define NT_STATUS_DRIVER_ORDINAL_NOT_FOUND 0xC0000262
#define NT_STATUS_DRIVER_ENTRYPOINT_NOT_FOUND 0xC0000263
#define NT_STATUS_RESOURCE_NOT_OWNED 0xC0000264
#define NT_STATUS_TOO_MANY_LINKS 0xC0000265
#define NT_STATUS_QUOTA_LIST_INCONSISTENT 0xC0000266
#define NT_STATUS_FILE_IS_OFFLINE 0xC0000267
#define NT_STATUS_NOT_A_REPARSE_POINT 0xC0000275
#define NT_STATUS_NO_SUCH_JOB 0xC0000EDE

%pythoncode %{
	NTSTATUS = _dcerpc.NTSTATUS
%}

%init  %{
	setup_logging("python", DEBUG_STDOUT);	
	lp_load(dyn_CONFIGFILE, True, False, False);
	load_interfaces();
	ntstatus_exception = PyErr_NewException("_dcerpc.NTSTATUS", NULL, NULL);
	PyDict_SetItemString(d, "NTSTATUS", ntstatus_exception);
%}

%typemap(in, numinputs=0) struct dcerpc_pipe **OUT (struct dcerpc_pipe *temp_dcerpc_pipe) {
        $1 = &temp_dcerpc_pipe;
}

%typemap(in, numinputs=0) TALLOC_CTX * {
	$1 = talloc_init("$symname");
}

%typemap(freearg) TALLOC_CTX * {
	talloc_free($1);
}

%typemap(argout) struct dcerpc_pipe ** {
	long status = PyLong_AsLong(resultobj);

	/* Throw exception if result was not OK */

	if (status != 0) {
		set_ntstatus_exception(status);
		return NULL;
	}

	/* Set REF_ALLOC flag so we don't have to do too much extra
	   mucking around with ref variables in ndr unmarshalling. */

	(*$1)->flags |= DCERPC_NDR_REF_ALLOC;

	/* Return swig handle on dcerpc_pipe */

        resultobj = SWIG_NewPointerObj(*$1, SWIGTYPE_p_dcerpc_pipe, 0);
}

%types(struct dcerpc_pipe *);

%rename(pipe_connect) dcerpc_pipe_connect;

NTSTATUS dcerpc_pipe_connect(struct dcerpc_pipe **OUT,
                             const char *binding,
                             const char *pipe_uuid,
                             uint32 pipe_version,
                             const char *domain,
                             const char *username,
                             const char *password);

/* Run this test after each wrapped function */

%exception {
	$action
	if (NT_STATUS_IS_ERR(result)) {
		set_ntstatus_exception(NT_STATUS_V(result));
		return NULL;
	}
}

%include "librpc/gen_ndr/misc.i"
%include "librpc/gen_ndr/lsa.i"
%include "librpc/gen_ndr/samr.i"
%include "librpc/gen_ndr/winreg.i"
