/* 
   Unix SMB/CIFS implementation.

   provide access to data blobs

   Copyright (C) Andrew Tridgell 2005
   
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
#include "scripting/ejs/smbcalls.h"
#include "lib/appweb/ejs/ejs.h"

/*
  create a data blob object from a ejs array of integers
*/
static int ejs_blobFromArray(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct MprVar *array, *v;
	unsigned length, i;
	DATA_BLOB blob;

	if (argc != 1) {
		ejsSetErrorMsg(eid, "blobFromArray invalid arguments");
		return -1;		
	}
	array = argv[0];

	v = mprGetProperty(array, "length", NULL);
	if (v == NULL) {
		goto failed;
	}
	length = mprToInt(v);

	blob = data_blob_talloc(mprMemCtx(), NULL, length);
	if (length != 0 && blob.data == NULL) {
		goto failed;
	}

	for (i=0;i<length;i++) {
		struct MprVar *vs;
		char idx[16];
		mprItoa(i, idx, sizeof(idx));		
		vs = mprGetProperty(array, idx, NULL);
		if (vs == NULL) {
			goto failed;
		}
		blob.data[i] = mprVarToNumber(vs);
	}

	mpr_Return(eid, mprDataBlob(blob));
	return 0;

failed:
	mpr_Return(eid, mprCreateUndefinedVar());
	return 0;
}

/*
  create a ejs array of integers from a data blob
*/
static int ejs_blobToArray(MprVarHandle eid, int argc, struct MprVar **argv)
{
	DATA_BLOB *blob;
	struct MprVar array;
	int i;

	if (argc != 1) {
		ejsSetErrorMsg(eid, "blobToArray invalid arguments");
		return -1;		
	}
	blob = mprToDataBlob(argv[0]);
	if (blob == NULL) {
		goto failed;
	}

	array = mprObject("array");
	
	for (i=0;i<blob->length;i++) {
		mprAddArray(&array, i, mprCreateNumberVar(blob->data[i]));
	}
	mpr_Return(eid, array);
	return 0;

failed:
	mpr_Return(eid, mprCreateUndefinedVar());
	return 0;
}


/*
  compare two data blobs
*/
static int ejs_blobCompare(MprVarHandle eid, int argc, struct MprVar **argv)
{
	DATA_BLOB *blob1, *blob2;
	BOOL ret = False;

	if (argc != 2) {
		ejsSetErrorMsg(eid, "blobCompare invalid arguments");
		return -1;		
	}
	
	blob1 = mprToDataBlob(argv[0]);
	blob2 = mprToDataBlob(argv[1]);

	if (blob1 == blob2) {
		ret = True;
		goto done;
	}
	if (blob1 == NULL || blob2 == NULL) {
		ret = False;
		goto done;
	}

	if (blob1->length != blob2->length) {
		ret = False;
		goto done;
	}

	if (memcmp(blob1->data, blob2->data, blob1->length) != 0) {
		ret = False;
		goto done;
	}
	ret = True;

done:
	mpr_Return(eid, mprCreateBoolVar(ret));
	return 0;
}

/*
  initialise datablob ejs subsystem
*/
static int ejs_datablob_init(MprVarHandle eid, int argc, struct MprVar **argv)
{
	struct MprVar *obj = mprInitObject(eid, "datablob", argc, argv);

	mprSetCFunction(obj, "blobFromArray", ejs_blobFromArray);
	mprSetCFunction(obj, "blobToArray", ejs_blobToArray);
	mprSetCFunction(obj, "blobCompare", ejs_blobCompare);

	return 0;
}

/*
  setup C functions that be called from ejs
*/
void smb_setup_ejs_datablob(void)
{
	ejsDefineCFunction(-1, "datablob_init", ejs_datablob_init, NULL, MPR_VAR_SCRIPT_HANDLE);
}
