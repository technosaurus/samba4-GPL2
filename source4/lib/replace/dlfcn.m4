dnl dummies provided by dlfcn.c if not available
save_LIBS="$LIBS"
LIBS=""

libreplace_cv_dlfcn=no
AC_SEARCH_LIBS(dlopen, dl)

AC_CHECK_HEADERS(dlfcn.h)
AC_CHECK_FUNCS([dlopen dlsym dlerror dlclose],[],[libreplace_cv_dlfcn=yes])

AC_VERIFY_C_PROTOTYPE([void *dlopen(const char* filename, unsigned int flags)],
	[
	return 0;
	],[
	AC_DEFINE(DLOPEN_TAKES_UNSIGNED_FLAGS, 1, [Whether dlopen takes unsinged int flags])
	],[],[
	#include <dlfcn.h>
	])

if test x"${libreplace_cv_dlfcn}" = x"yes";then
	LIBREPLACEOBJ="${LIBREPLACEOBJ} dlfcn.o"
fi

LIBDL="$LIBS"
AC_SUBST(LIBDL)
LIBS="$save_LIBS"
