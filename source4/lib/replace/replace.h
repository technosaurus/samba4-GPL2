/* 
   Unix SMB/CIFS implementation.

   macros to go along with the lib/replace/ portability layer code

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2006

     ** NOTE! The following LGPL license applies to the replace
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

#ifndef _replace_h
#define _replace_h

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include "lib/replace/win32/replace.h"
#endif

#ifdef __COMPAR_FN_T
#define QSORT_CAST (__compar_fn_t)
#endif

#ifndef QSORT_CAST
#define QSORT_CAST (int (*)(const void *, const void *))
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
#define strerror(i) sys_errlist[i]
#endif

#ifndef HAVE_ERRNO_DECL
extern int errno;
#endif

#ifndef HAVE_STRDUP
#define strdup rep_strdup
char *rep_strdup(const char *s);
#endif

#ifndef HAVE_MEMMOVE
#define memmove rep_memmove
void *rep_memmove(void *dest,const void *src,int size);
#endif

#if !defined(HAVE_MKTIME) || !defined(HAVE_TIMEGM)
#include "system/time.h"
#endif

#ifndef HAVE_MKTIME
#define mktime rep_mktime
time_t rep_mktime(struct tm *t);
#endif

#ifndef HAVE_TIMEGM
struct tm;
#define timegm rep_timegm
time_t rep_timegm(struct tm *tm);
#endif

#ifndef HAVE_STRLCPY
#define strlcpy rep_strlcpy
size_t rep_strlcpy(char *d, const char *s, size_t bufsize);
#endif

#ifndef HAVE_STRLCAT
#define strlcat rep_strlcat
size_t rep_strlcat(char *d, const char *s, size_t bufsize);
#endif

#ifndef HAVE_STRNDUP
#define strndup rep_strndup
char *rep_strndup(const char *s, size_t n);
#endif

#ifndef HAVE_STRNLEN
#define strnlen rep_strnlen
size_t rep_strnlen(const char *s, size_t n);
#endif

#ifndef HAVE_SETENV
#define setenv rep_setenv
int rep_setenv(const char *name, const char *value, int overwrite); 
#endif

#ifndef HAVE_STRCASESTR
#define strcasestr rep_strcasestr
char *rep_strcasestr(const char *haystack, const char *needle);
#endif

#ifndef HAVE_STRTOK_R
#define strtok_r rep_strtok_r 
char *rep_strtok_r(char *s, const char *delim, char **save_ptr);
#endif

#ifndef HAVE_STRTOLL
#define strtoll rep_strtoll
long long int rep_strtoll(const char *str, char **endptr, int base);
#endif

#ifndef HAVE_STRTOULL
#define strtoull rep_strtoull
unsigned long long int rep_strtoull(const char *str, char **endptr, int base);
#endif

#ifndef HAVE_FTRUNCATE
#define ftruncate rep_ftruncate
int rep_ftruncate(int,off_t);
#endif

#ifndef HAVE_INITGROUPS
#define ftruncate rep_ftruncate
int rep_initgroups(char *name, gid_t id);
#endif

#if !defined(HAVE_BZERO) && defined(HAVE_MEMSET)
#define bzero(a,b) memset((a),'\0',(b))
#endif

#ifndef HAVE_DLERROR
#define dlerror rep_dlerror
char *rep_dlerror(void);
#endif

#ifndef HAVE_DLOPEN
#define dlopen rep_dlopen
void *rep_dlopen(const char *name, int flags);
#endif

#ifndef HAVE_DLSYM
#define dlsym rep_dlsym
void *rep_dlsym(void *handle, const char *symbol);
#endif

#ifndef HAVE_DLCLOSE
#define dlclose rep_dlclose
int rep_dlclose(void *handle);
#endif


#ifndef PRINTF_ATTRIBUTE
#if __GNUC__ >= 3
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. Note that some gcc 2.x versions don't handle this
 * properly **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

#ifndef HAVE_VASPRINTF
#define vasprintf rep_vasprintf
int rep_vasprintf(char **ptr, const char *format, va_list ap);
#endif

#ifndef HAVE_SNPRINTF
#define snprintf rep_snprintf
int rep_snprintf(char *,size_t ,const char *, ...) PRINTF_ATTRIBUTE(3,4);
#endif

#ifndef HAVE_VSNPRINTF
#define vsnprintf rep_vsnprintf
int rep_vsnprintf(char *,size_t ,const char *, va_list ap);
#endif

#ifndef HAVE_ASPRINTF
#define asprintf rep_asprintf
int rep_asprintf(char **,const char *, ...) PRINTF_ATTRIBUTE(2,3);
#endif


/* we used to use these fns, but now we have good replacements
   for snprintf and vsnprintf */
#define slprintf snprintf


#ifndef HAVE_VA_COPY
#undef va_copy
#ifdef HAVE___VA_COPY
#define va_copy(dest, src) __va_copy(dest, src)
#else
#define va_copy(dest, src) (dest) = (src)
#endif
#endif

#ifndef HAVE_VOLATILE
#define volatile
#endif

#ifndef HAVE_COMPARISON_FN_T
typedef int (*comparison_fn_t)(const void *, const void *);
#endif

/* Load header file for dynamic linking stuff */
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

#ifndef HAVE_SECURE_MKSTEMP
#define mkstemp(path) rep_mkstemp(path)
int rep_mkstemp(char *temp);
#endif

#ifndef HAVE_MKDTEMP
#define mkdtemp rep_mkdtemp
char *rep_mkdtemp(char *template);
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* The extra casts work around common compiler bugs.  */
#define _TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
/* The outer cast is needed to work around a bug in Cray C 5.0.3.0.
   It is necessary at least when t == time_t.  */
#define _TYPE_MINIMUM(t) ((t) (_TYPE_SIGNED (t) \
  			      ? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) : (t) 0))
#define _TYPE_MAXIMUM(t) ((t) (~ (t) 0 - _TYPE_MINIMUM (t)))

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

#ifndef UINT32_MAX
#define UINT32_MAX (4294967295U)
#endif

#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t)-1)
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef INT32_MAX
#define INT32_MAX _TYPE_MAXIMUM(int32_t)
#endif

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#elif !defined(HAVE_BOOL)
#define __bool_true_false_are_defined
typedef int bool;
#define false (0)
#define true (1)
#endif

#ifndef HAVE_FUNCTION_MACRO
#ifdef HAVE_func_MACRO
#define __FUNCTION__ __func__
#else
#define __FUNCTION__ ("")
#endif
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef __STRING
#define __STRING(x)    #x
#endif

#endif
