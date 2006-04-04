dnl SMB Build Environment Path Checks
dnl -------------------------------------------------------
dnl  Copyright (C) Stefan (metze) Metzmacher 2004
dnl  Released under the GNU GPL
dnl -------------------------------------------------------
dnl

#################################################
# Directory handling stuff to support both the
# legacy SAMBA directories and FHS compliant
# ones...
AC_PREFIX_DEFAULT(/usr/local/samba)

# Defaults and --without-fhs
logfilebase="${localstatedir}"
lockdir="${localstatedir}/locks"
piddir="${localstatedir}/run"
privatedir="\${prefix}/private"
modulesdir="\${prefix}/modules"
winbindd_socket_dir="${localstatedir}/run/winbind_pipe"

AC_ARG_WITH(fhs, 
[  --with-fhs              Use FHS-compliant paths (default=no)],
    lockdir="${localstatedir}/lib/samba"
    piddir="${localstatedir}/run/samba"
    logfilebase="${localstatedir}/log/samba"
    privatedir="${localstatedir}/lib/samba/private"
    sysconfdir="${sysconfdir}/samba"
	modulesdir="${libdir}/samba"
    datadir="${datadir}/samba"
    includedir="${includedir}/samba-4.0"
    winbindd_socket_dir="${localstatedir}/run/samba/winbind_pipe"
)

#################################################
# set private directory location
AC_ARG_WITH(privatedir,
[  --with-privatedir=DIR   Where to put sam.ldb and other private files containing key material ($ac_default_prefix/private)],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-privatedir called without argument - will use default])
  ;;
  * )
    privatedir="$withval"
    ;;
  esac])

#################################################
# set where the winbindd socket should be put
AC_ARG_WITH(winbindd-socket-dir,
[  --with-winbindd-socket-dir=DIR   Where to put the winbindd socket ($ac_default_prefix/run/winbind_pipe)],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-winbind-socketdir called without argument - will use default])
  ;;
  * )
    winbindd_socket_dir="$withval"
    ;;
  esac])

#################################################
# set lock directory location
AC_ARG_WITH(lockdir,
[  --with-lockdir=DIR      Where to put lock files ($ac_default_prefix/var/locks)],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-lockdir called without argument - will use default])
  ;;
  * )
    lockdir="$withval"
    ;;
  esac])

#################################################
# set pid directory location
AC_ARG_WITH(piddir,
[  --with-piddir=DIR       Where to put pid files ($ac_default_prefix/var/locks)],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-piddir called without argument - will use default])
  ;;
  * )
    piddir="$withval"
    ;;
  esac])

#################################################
# set log directory location
AC_ARG_WITH(logfilebase,
[  --with-logfilebase=DIR  Where to put log files (\$(VARDIR))],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody does it
  #
    AC_MSG_WARN([--with-logfilebase called without argument - will use default])
  ;;
  * )
    logfilebase="$withval"
    ;;
  esac])


AC_SUBST(lockdir)
AC_SUBST(piddir)
AC_SUBST(logfilebase)
AC_SUBST(privatedir)
AC_SUBST(bindir)
AC_SUBST(sbindir)
AC_SUBST(winbindd_socket_dir)
AC_SUBST(modulesdir)

debug=no
AC_ARG_ENABLE(debug,
[  --enable-debug          Turn on compiler debugging information (default=no)],
    [if test x$enable_debug = xyes; then
        debug=yes
    fi])

developer=no
AC_SUBST(developer)
AC_ARG_ENABLE(developer,
[  --enable-developer      Turn on developer warnings and debugging (default=no)],
    [if test x$enable_developer = xyes; then
	debug=yes
        developer=yes
    fi])

dnl disable these external libs 
AC_ARG_WITH(disable-ext-lib,
[  --with-disable-ext-lib=LIB Comma-seperated list of external libraries],
[ if test $withval; then
	for i in `echo $withval | sed -e's/,/ /g'`
	do
		eval SMB_EXT_LIB_$i=NO
	done
fi ])
