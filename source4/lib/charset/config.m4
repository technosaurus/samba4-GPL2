dnl SMB_CHECK_ICONV(action-if-found,action-if-not-found)
AC_DEFUN(SMB_CHECK_ICONV,[
  AC_CHECK_HEADERS(iconv.h giconv.h)

  AC_TRY_RUN([#include <stdlib.h>
#ifdef HAVE_GICONV_H
#include <giconv.h>
#endif
#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif

int main()
{
   iconv_t cd = iconv_open("ASCII","UCS-2LE");
   if (cd == 0 || cd == (iconv_t)-1) return -1;
   return 0;
} 
   ],[$1],[$2])
])

dnl SMB_CHECK_ICONV_DIR(dir,action-if-found,action-if-not-found)
AC_DEFUN(SMB_CHECK_ICONV_DIR,
[
    if test -f "$1/include/iconv.h" -o -f "$1/include/giconv.h"; then
	CPPFLAGS="-I$1/include"
	LDFLAGS="-L$1/lib"
	LIBS=-liconv

	SMB_CHECK_ICONV([$2], 
	[
            LIBS=-lgiconv
            SMB_CHECK_ICONV([$2],[$3])
	])

	CPPFLAGS=$save_CPPFLAGS
	LDFLAGS=$save_LDFLAGS
	LIBS=$save_LIBS
    else
	$2
    fi
])

ICONV_FOUND=no
LOOK_DIRS="/usr /usr/local /sw"
AC_ARG_WITH(libiconv,
[  --with-libiconv=BASEDIR Use libiconv in BASEDIR/lib and BASEDIR/include (default=auto) ],
[
  if test "$withval" = "no" ; then
    AC_MSG_ERROR(I won't take no for an answer)
  else
     if test "$withval" != "yes" ; then
	SMB_CHECK_ICONV_DIR($withval, [
		ICONV_FOUND=yes; 
		ICONV_CPPFLAGS="$CPPFLAGS"
		ICONV_LIBS="$LIBS"
		ICONV_LDFLAGS="$LDFLAGS"
		], [AC_MSG_ERROR([No iconv library found in $withval])])
     fi
  fi
])

if test x$ICONV_FOUND = xno; then
	SMB_CHECK_ICONV([ICONV_FOUND=yes])
fi

for i in $LOOK_DIRS ; do
	if test x$ICONV_FOUND = xyes; then
		break
	fi
	
	SMB_CHECK_ICONV_DIR($withval, [
		ICONV_FOUND=yes; 
		ICONV_CPPFLAGS="$CPPFLAGS"
		ICONV_LIBS="$LIBS"
		ICONV_LDFLAGS="$LDFLAGS"
		], [])
done

if test x"$ICONV_FOUND" = x"no"; then 
    AC_MSG_WARN([Sufficient support for iconv function was not found. 
    Install libiconv from http://www.gnu.org/software/libiconv/ for better charset compatibility!])
	SMB_ENABLE(ICONV,NO)
else
	AC_DEFINE(HAVE_NATIVE_ICONV,1,[Whether external iconv is available])
	SMB_ENABLE(ICONV,YES)
fi

SMB_EXT_LIB(ICONV,[${ICONV_LIBS}],[${ICONV_CFLAGS}],[${ICONV_CPPFLAGS}],[${ICONV_LDFLAGS}])
