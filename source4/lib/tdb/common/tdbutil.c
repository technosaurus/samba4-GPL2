/* 
   Unix SMB/CIFS implementation.
   tdb utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   
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
#include "system/glob.h"
#include "system/wait.h"
#include "dlinklist.h"

/* these are little tdb utility functions that are meant to make
   dealing with a tdb database a little less cumbersome in Samba */

static sig_atomic_t gotalarm;

/***************************************************************
 Signal function to tell us we timed out.
****************************************************************/

static void gotalarm_sig(void)
{
	gotalarm = 1;
}


/*******************************************************************
 THIS is a copy of the function CatchSignal found in lib/signal.c
 I need to copy it there to avoid sucking all of the samba source
 into tdb.

 Catch a signal. This should implement the following semantics:

 1) The handler remains installed after being called.
 2) The signal should be blocked during handler execution.
********************************************************************/

static void (*TdbCatchSignal(int signum,void (*handler)(int )))(int)
{
#ifdef HAVE_SIGACTION
	struct sigaction act;
	struct sigaction oldact;

	ZERO_STRUCT(act);

	act.sa_handler = handler;
#ifdef SA_RESTART
	/*
	 * We *want* SIGALRM to interrupt a system call.
	 */
	if(signum != SIGALRM)
		act.sa_flags = SA_RESTART;
#endif
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask,signum);
	sigaction(signum,&act,&oldact);
	return oldact.sa_handler;
#else /* !HAVE_SIGACTION */
	/* FIXME: need to handle sigvec and systems with broken signal() */
	return signal(signum, handler);
#endif
}



/***************************************************************
 Make a TDB_DATA and keep the const warning in one place
****************************************************************/

static TDB_DATA make_tdb_data(const char *dptr, size_t dsize)
{
	TDB_DATA ret;
	ret.dptr = discard_const_p(char, dptr);
	ret.dsize = dsize;
	return ret;
}

/****************************************************************************
 Lock a chain with timeout (in seconds).
****************************************************************************/

static int tdb_chainlock_with_timeout_internal(TDB_CONTEXT *tdb, TDB_DATA key, uint_t timeout, int rw_type)
{
	/* Allow tdb_chainlock to be interrupted by an alarm. */
	int ret;
	gotalarm = 0;
	tdb_set_lock_alarm(&gotalarm);

	if (timeout) {
		TdbCatchSignal(SIGALRM, SIGNAL_CAST gotalarm_sig);
		alarm(timeout);
	}

	if (rw_type == F_RDLCK)
		ret = tdb_chainlock_read(tdb, key);
	else
		ret = tdb_chainlock(tdb, key);

	if (timeout) {
		alarm(0);
		TdbCatchSignal(SIGALRM, SIGNAL_CAST SIG_IGN);
		if (gotalarm) {
			tdb->log_fn(tdb, 0, "tdb_chainlock_with_timeout_internal: alarm (%u) timed out for key %s in tdb %s\n",
				timeout, key.dptr, tdb->name);
			/* TODO: If we time out waiting for a lock, it might
			 * be nice to use F_GETLK to get the pid of the
			 * process currently holding the lock and print that
			 * as part of the debugging message. -- mbp */
			return -1;
		}
	}

	return ret;
}

/****************************************************************************
 Write lock a chain. Return -1 if timeout or lock failed.
****************************************************************************/

int tdb_chainlock_with_timeout(TDB_CONTEXT *tdb, TDB_DATA key, uint_t timeout)
{
	return tdb_chainlock_with_timeout_internal(tdb, key, timeout, F_WRLCK);
}

/****************************************************************************
 Lock a chain by string. Return -1 if timeout or lock failed.
****************************************************************************/

int tdb_lock_bystring(TDB_CONTEXT *tdb, const char *keyval, uint_t timeout)
{
	TDB_DATA key = make_tdb_data(keyval, strlen(keyval)+1);
	
	return tdb_chainlock_with_timeout_internal(tdb, key, timeout, F_WRLCK);
}

/****************************************************************************
 Unlock a chain by string.
****************************************************************************/

void tdb_unlock_bystring(TDB_CONTEXT *tdb, const char *keyval)
{
	TDB_DATA key = make_tdb_data(keyval, strlen(keyval)+1);

	tdb_chainunlock(tdb, key);
}

/****************************************************************************
 Read lock a chain by string. Return -1 if timeout or lock failed.
****************************************************************************/

int tdb_read_lock_bystring(TDB_CONTEXT *tdb, const char *keyval, uint_t timeout)
{
	TDB_DATA key = make_tdb_data(keyval, strlen(keyval)+1);
	
	return tdb_chainlock_with_timeout_internal(tdb, key, timeout, F_RDLCK);
}

/****************************************************************************
 Read unlock a chain by string.
****************************************************************************/

void tdb_read_unlock_bystring(TDB_CONTEXT *tdb, const char *keyval)
{
	TDB_DATA key = make_tdb_data(keyval, strlen(keyval)+1);
	
	tdb_chainunlock_read(tdb, key);
}


/****************************************************************************
 Fetch a int32_t value by a arbitrary blob key, return -1 if not found.
 Output is int32_t in native byte order.
****************************************************************************/

int32_t tdb_fetch_int32_byblob(TDB_CONTEXT *tdb, const char *keyval, size_t len)
{
	TDB_DATA key = make_tdb_data(keyval, len);
	TDB_DATA data;
	int32_t ret;

	data = tdb_fetch(tdb, key);
	if (!data.dptr || data.dsize != sizeof(int32_t)) {
		SAFE_FREE(data.dptr);
		return -1;
	}

	ret = IVAL(data.dptr,0);
	SAFE_FREE(data.dptr);
	return ret;
}

/****************************************************************************
 Fetch a int32_t value by string key, return -1 if not found.
 Output is int32_t in native byte order.
****************************************************************************/

int32_t tdb_fetch_int32(TDB_CONTEXT *tdb, const char *keystr)
{
	return tdb_fetch_int32_byblob(tdb, keystr, strlen(keystr) + 1);
}

/****************************************************************************
 Store a int32_t value by an arbitary blob key, return 0 on success, -1 on failure.
 Input is int32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/

int tdb_store_int32_byblob(TDB_CONTEXT *tdb, const char *keystr, size_t len, int32_t v)
{
	TDB_DATA key = make_tdb_data(keystr, len);
	TDB_DATA data;
	int32_t v_store;

	SIVAL(&v_store,0,v);
	data.dptr = (void *)&v_store;
	data.dsize = sizeof(int32_t);

	return tdb_store(tdb, key, data, TDB_REPLACE);
}

/****************************************************************************
 Store a int32_t value by string key, return 0 on success, -1 on failure.
 Input is int32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/

int tdb_store_int32(TDB_CONTEXT *tdb, const char *keystr, int32_t v)
{
	return tdb_store_int32_byblob(tdb, keystr, strlen(keystr) + 1, v);
}

/****************************************************************************
 Fetch a uint32_t value by a arbitrary blob key, return -1 if not found.
 Output is uint32_t in native byte order.
****************************************************************************/

BOOL tdb_fetch_uint32_byblob(TDB_CONTEXT *tdb, const char *keyval, size_t len, uint32_t *value)
{
	TDB_DATA key = make_tdb_data(keyval, len);
	TDB_DATA data;

	data = tdb_fetch(tdb, key);
	if (!data.dptr || data.dsize != sizeof(uint32_t)) {
		SAFE_FREE(data.dptr);
		return False;
	}

	*value = IVAL(data.dptr,0);
	SAFE_FREE(data.dptr);
	return True;
}

/****************************************************************************
 Fetch a uint32_t value by string key, return -1 if not found.
 Output is uint32_t in native byte order.
****************************************************************************/

BOOL tdb_fetch_uint32(TDB_CONTEXT *tdb, const char *keystr, uint32_t *value)
{
	return tdb_fetch_uint32_byblob(tdb, keystr, strlen(keystr) + 1, value);
}

/****************************************************************************
 Store a uint32_t value by an arbitary blob key, return 0 on success, -1 on failure.
 Input is uint32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/

BOOL tdb_store_uint32_byblob(TDB_CONTEXT *tdb, const char *keystr, size_t len, uint32_t value)
{
	TDB_DATA key = make_tdb_data(keystr, len);
	TDB_DATA data;
	uint32_t v_store;
	BOOL ret = True;

	SIVAL(&v_store, 0, value);
	data.dptr = (void *)&v_store;
	data.dsize = sizeof(uint32_t);

	if (tdb_store(tdb, key, data, TDB_REPLACE) == -1)
		ret = False;

	return ret;
}

/****************************************************************************
 Store a uint32_t value by string key, return 0 on success, -1 on failure.
 Input is uint32_t in native byte order. Output in tdb is in little-endian.
****************************************************************************/

BOOL tdb_store_uint32(TDB_CONTEXT *tdb, const char *keystr, uint32_t value)
{
	return tdb_store_uint32_byblob(tdb, keystr, strlen(keystr) + 1, value);
}
/****************************************************************************
 Store a buffer by a null terminated string key.  Return 0 on success, -1
 on failure.
****************************************************************************/

int tdb_store_bystring(TDB_CONTEXT *tdb, const char *keystr, TDB_DATA data, int flags)
{
	TDB_DATA key = make_tdb_data(keystr, strlen(keystr)+1);
	
	return tdb_store(tdb, key, data, flags);
}

/****************************************************************************
 Fetch a buffer using a null terminated string key.  Don't forget to call
 free() on the result dptr.
****************************************************************************/

TDB_DATA tdb_fetch_bystring(TDB_CONTEXT *tdb, const char *keystr)
{
	TDB_DATA key = make_tdb_data(keystr, strlen(keystr)+1);

	return tdb_fetch(tdb, key);
}

/****************************************************************************
 Delete an entry using a null terminated string key. 
****************************************************************************/

int tdb_delete_bystring(TDB_CONTEXT *tdb, const char *keystr)
{
	TDB_DATA key = make_tdb_data(keystr, strlen(keystr)+1);

	return tdb_delete(tdb, key);
}

/****************************************************************************
 Atomic integer change. Returns old value. To create, set initial value in *oldval. 
****************************************************************************/

int32_t tdb_change_int32_atomic(TDB_CONTEXT *tdb, const char *keystr, int32_t *oldval, int32_t change_val)
{
	int32_t val;
	int32_t ret = -1;

	if (tdb_lock_bystring(tdb, keystr,0) == -1)
		return -1;

	if ((val = tdb_fetch_int32(tdb, keystr)) == -1) {
		/* The lookup failed */
		if (tdb_error(tdb) != TDB_ERR_NOEXIST) {
			/* but not because it didn't exist */
			goto err_out;
		}
		
		/* Start with 'old' value */
		val = *oldval;

	} else {
		/* It worked, set return value (oldval) to tdb data */
		*oldval = val;
	}

	/* Increment value for storage and return next time */
	val += change_val;
		
	if (tdb_store_int32(tdb, keystr, val) == -1)
		goto err_out;

	ret = 0;

  err_out:

	tdb_unlock_bystring(tdb, keystr);
	return ret;
}

/****************************************************************************
 Atomic unsigned integer change. Returns old value. To create, set initial value in *oldval. 
****************************************************************************/

BOOL tdb_change_uint32_atomic(TDB_CONTEXT *tdb, const char *keystr, uint32_t *oldval, uint32_t change_val)
{
	uint32_t val;
	BOOL ret = False;

	if (tdb_lock_bystring(tdb, keystr,0) == -1)
		return False;

	if (!tdb_fetch_uint32(tdb, keystr, &val)) {
		/* It failed */
		if (tdb_error(tdb) != TDB_ERR_NOEXIST) { 
			/* and not because it didn't exist */
			goto err_out;
		}

		/* Start with 'old' value */
		val = *oldval;

	} else {
		/* it worked, set return value (oldval) to tdb data */
		*oldval = val;

	}

	/* get a new value to store */
	val += change_val;
		
	if (!tdb_store_uint32(tdb, keystr, val))
		goto err_out;

	ret = True;

  err_out:

	tdb_unlock_bystring(tdb, keystr);
	return ret;
}

/****************************************************************************
 Useful pair of routines for packing/unpacking data consisting of
 integers and strings.
****************************************************************************/

size_t tdb_pack(TDB_CONTEXT *tdb, char *buf, int bufsize, const char *fmt, ...)
{
	va_list ap;
	uint8_t bt;
	uint16_t w;
	uint32_t d;
	int i;
	void *p;
	int len;
	char *s;
	char c;
	char *buf0 = buf;
	const char *fmt0 = fmt;
	int bufsize0 = bufsize;

	va_start(ap, fmt);

	while (*fmt) {
		switch ((c = *fmt++)) {
		case 'b': /* unsigned 8-bit integer */
			len = 1;
			bt = (uint8_t)va_arg(ap, int);
			if (bufsize && bufsize >= len)
				SSVAL(buf, 0, bt);
			break;
		case 'w': /* unsigned 16-bit integer */
			len = 2;
			w = (uint16_t)va_arg(ap, int);
			if (bufsize && bufsize >= len)
				SSVAL(buf, 0, w);
			break;
		case 'd': /* signed 32-bit integer (standard int in most systems) */
			len = 4;
			d = va_arg(ap, uint32_t);
			if (bufsize && bufsize >= len)
				SIVAL(buf, 0, d);
			break;
		case 'p': /* pointer */
			len = 4;
			p = va_arg(ap, void *);
			d = p?1:0;
			if (bufsize && bufsize >= len)
				SIVAL(buf, 0, d);
			break;
		case 'P': /* null-terminated string */
			s = va_arg(ap,char *);
			w = strlen(s);
			len = w + 1;
			if (bufsize && bufsize >= len)
				memcpy(buf, s, len);
			break;
		case 'f': /* null-terminated string */
			s = va_arg(ap,char *);
			w = strlen(s);
			len = w + 1;
			if (bufsize && bufsize >= len)
				memcpy(buf, s, len);
			break;
		case 'B': /* fixed-length string */
			i = va_arg(ap, int);
			s = va_arg(ap, char *);
			len = 4+i;
			if (bufsize && bufsize >= len) {
				SIVAL(buf, 0, i);
				memcpy(buf+4, s, i);
			}
			break;
		default:
			tdb->log_fn(tdb, 0,"Unknown tdb_pack format %c in %s\n", 
				    c, fmt);
			len = 0;
			break;
		}

		buf += len;
		if (bufsize)
			bufsize -= len;
		if (bufsize < 0)
			bufsize = 0;
	}

	va_end(ap);

	tdb->log_fn(tdb, 18,"tdb_pack(%s, %d) -> %d\n", 
		    fmt0, bufsize0, (int)PTR_DIFF(buf, buf0));
	
	return PTR_DIFF(buf, buf0);
}

/****************************************************************************
 Useful pair of routines for packing/unpacking data consisting of
 integers and strings.
****************************************************************************/

int tdb_unpack(TDB_CONTEXT *tdb, char *buf, int bufsize, const char *fmt, ...)
{
	va_list ap;
	uint8_t *bt;
	uint16_t *w;
	uint32_t *d;
	int len;
	int *i;
	void **p;
	char *s, **b;
	char c;
	char *buf0 = buf;
	const char *fmt0 = fmt;
	int bufsize0 = bufsize;

	va_start(ap, fmt);
	
	while (*fmt) {
		switch ((c=*fmt++)) {
		case 'b':
			len = 1;
			bt = va_arg(ap, uint8_t *);
			if (bufsize < len)
				goto no_space;
			*bt = SVAL(buf, 0);
			break;
		case 'w':
			len = 2;
			w = va_arg(ap, uint16_t *);
			if (bufsize < len)
				goto no_space;
			*w = SVAL(buf, 0);
			break;
		case 'd':
			len = 4;
			d = va_arg(ap, uint32_t *);
			if (bufsize < len)
				goto no_space;
			*d = IVAL(buf, 0);
			break;
		case 'p':
			len = 4;
			p = va_arg(ap, void **);
			if (bufsize < len)
				goto no_space;
			*p = (void *)IVAL(buf, 0);
			break;
		case 'P':
			s = va_arg(ap,char *);
			len = strlen(buf) + 1;
			if (bufsize < len || len > sizeof(pstring))
				goto no_space;
			memcpy(s, buf, len);
			break;
		case 'f':
			s = va_arg(ap,char *);
			len = strlen(buf) + 1;
			if (bufsize < len || len > sizeof(fstring))
				goto no_space;
			memcpy(s, buf, len);
			break;
		case 'B':
			i = va_arg(ap, int *);
			b = va_arg(ap, char **);
			len = 4;
			if (bufsize < len)
				goto no_space;
			*i = IVAL(buf, 0);
			if (! *i) {
				*b = NULL;
				break;
			}
			len += *i;
			if (bufsize < len)
				goto no_space;
			*b = (char *)malloc(*i);
			if (! *b)
				goto no_space;
			memcpy(*b, buf+4, *i);
			break;
		default:
			tdb->log_fn(tdb, 0, "Unknown tdb_unpack format %c in %s\n", 
				 c, fmt);

			len = 0;
			break;
		}

		buf += len;
		bufsize -= len;
	}

	va_end(ap);

	tdb->log_fn(tdb, 18, "tdb_unpack(%s, %d) -> %d\n", 
		    fmt0, bufsize0, (int)PTR_DIFF(buf, buf0));

	return PTR_DIFF(buf, buf0);

 no_space:
	return -1;
}

/****************************************************************************
 Allow tdb_delete to be used as a tdb_traversal_fn.
****************************************************************************/

int tdb_traverse_delete_fn(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf,
                     void *state)
{
    return tdb_delete(the_tdb, key);
}



/**
 * Search across the whole tdb for keys that match the given pattern
 * return the result as a list of keys
 *
 * @param tdb pointer to opened tdb file context
 * @param pattern searching pattern used by fnmatch(3) functions
 *
 * @return list of keys found by looking up with given pattern
 **/
TDB_LIST_NODE *tdb_search_keys(TDB_CONTEXT *tdb, const char* pattern)
{
	TDB_DATA key, next;
	TDB_LIST_NODE *list = NULL;
	TDB_LIST_NODE *rec = NULL;
	
	for (key = tdb_firstkey(tdb); key.dptr; key = next) {
		/* duplicate key string to ensure null-termination */
		char *key_str = (char*) strndup(key.dptr, key.dsize);
#if 0
		if (!key_str) {
			tdb->log_fn(tdb, 0, "tdb_search_keys: strndup() failed!\n");
			smb_panic("strndup failed!\n");
		}
#endif
		tdb->log_fn(tdb, 18, "checking %s for match to pattern %s\n", key_str, pattern);
		
		next = tdb_nextkey(tdb, key);

		/* do the pattern checking */
		if (fnmatch(pattern, key_str, 0) == 0) {
			rec = (TDB_LIST_NODE*) malloc(sizeof(*rec));
			ZERO_STRUCTP(rec);

			rec->node_key = key;
	
			DLIST_ADD_END(list, rec, TDB_LIST_NODE *);
		
			tdb->log_fn(tdb, 18, "checking %s matched pattern %s\n", key_str, pattern);
		} else {
			free(key.dptr);
		}
		
		/* free duplicated key string */
		free(key_str);
	}
	
	return list;

}


/**
 * Free the list returned by tdb_search_keys
 *
 * @param node list of results found by tdb_search_keys
 **/
void tdb_search_list_free(TDB_LIST_NODE* node)
{
	TDB_LIST_NODE *next_node;
	
	while (node) {
		next_node = node->next;
		SAFE_FREE(node->node_key.dptr);
		SAFE_FREE(node);
		node = next_node;
	}
}
