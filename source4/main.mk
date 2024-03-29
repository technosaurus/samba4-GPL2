default: all

include dynconfig.mk
include heimdal_build/config.mk
include config.mk
include dsdb/config.mk
include smbd/config.mk
include cluster/config.mk
include smbd/process_model.mk
include libnet/config.mk
include auth/config.mk
include nsswitch/config.mk
include lib/basic.mk
include param/config.mk
include smb_server/config.mk
include rpc_server/config.mk
include ldap_server/config.mk
include web_server/config.mk
include winbind/config.mk
include nbt_server/config.mk
include wrepl_server/config.mk
include cldap_server/config.mk
include utils/net/config.mk
include utils/config.mk
include ntvfs/config.mk
include ntptr/config.mk
include torture/config.mk
include librpc/config.mk
include client/config.mk
include libcli/config.mk
include scripting/ejs/config.mk
include scripting/swig/config.mk
include kdc/config.mk

DEFAULT_HEADERS = $(srcdir)/include/core.h \
		  $(srcdir)/lib/util/dlinklist.h \
		  $(srcdir)/version.h

binaries: $(BINARIES)
libraries: $(STATIC_LIBS) $(SHARED_LIBS)
modules: $(SHARED_MODULES)
headers: $(PUBLIC_HEADERS) $(DEFAULT_HEADERS)
manpages: $(MANPAGES)
all: showflags $(ALL_PREDEP) bin/asn1_compile bin/compile_et binaries modules
everything: all libraries headers

showlayout: 
	@echo 'Samba will be installed into:'
	@echo '  basedir:     $(BASEDIR)'
	@echo '  bindir:      $(BINDIR)'
	@echo '  sbindir:     $(SBINDIR)'
	@echo '  libdir:      $(LIBDIR)'
	@echo '  modulesdir:  $(MODULESDIR)'
	@echo '  includedir:  $(INCLUDEDIR)'
	@echo '  vardir:      $(VARDIR)'
	@echo '  privatedir:  $(PRIVATEDIR)'
	@echo '  piddir:      $(PIDDIR)'
	@echo '  lockdir:     $(LOCKDIR)'
	@echo '  logfilebase: $(LOGFILEBASE)'
	@echo '  setupdir:    $(SETUPDIR)'
	@echo '  jsdir:       $(JSDIR)'
	@echo '  webappsdir:  $(WEBAPPSDIR)'
	@echo '  servicesdir: $(SERVICESDIR)'
	@echo '  mandir:      $(MANDIR)'
	@echo '  torturedir:  $(TORTUREDIR)'
	@echo '  datadir:     $(DATADIR)'
	@echo '  winbindd_socket_dir:  $(WINBINDD_SOCKET_DIR)'

showflags:
	@echo 'Samba will be compiled with flags:'
	@echo '  CPP        = $(CPP)'
	@echo '  CPPFLAGS   = $(CPPFLAGS)'
	@echo '  CC         = $(CC)'
	@echo '  CFLAGS     = $(CFLAGS)'
	@echo '  PICFLAG    = $(PICFLAG)'
	@echo '  LD         = $(LD)'
	@echo '  LDFLAGS    = $(LDFLAGS)'
	@echo '  STLD       = $(STLD)'
	@echo '  STLD_FLAGS = $(STLD_FLAGS)'
	@echo '  SHLD       = $(SHLD)'
	@echo '  SHLD_FLAGS = $(SHLD_FLAGS)'
	@echo '  SHLIBEXT   = $(SHLIBEXT)'
	@echo '  srcdir     = $(srcdir)'
	@echo '  builddir   = $(builddir)'
	@echo '  pwd        = '`/bin/pwd`

# The permissions to give the executables
INSTALLPERMS = 0755

install: showlayout everything installbin installdat installwebapps installmisc installlib \
	installheader installpc installplugins

# DESTDIR is used here to prevent packagers wasting their time
# duplicating the Makefile. Remove it and you will have the privilege
# of packaging each samba release for multiple versions of multiple
# distributions and operating systems, or at least supplying patches
# to all the packaging files required for this, prior to committing
# the removal of DESTDIR. Do not remove it even though you think it
# is not used.

installdirs:
	@$(SHELL) $(srcdir)/script/installdirs.sh \
		$(DESTDIR)$(BASEDIR) \
		$(DESTDIR)$(BINDIR) \
		$(DESTDIR)$(SBINDIR) \
		$(DESTDIR)$(TORTUREDIR) \
		$(DESTDIR)$(LIBDIR) \
		$(DESTDIR)$(MODULESDIR) \
		$(DESTDIR)$(MANDIR) \
		$(DESTDIR)$(VARDIR) \
		$(DESTDIR)$(PRIVATEDIR) \
		$(DESTDIR)$(DATADIR) \
		$(DESTDIR)$(PIDDIR) \
		$(DESTDIR)$(LOCKDIR) \
		$(DESTDIR)$(LOGFILEBASE) \
		$(DESTDIR)$(PRIVATEDIR)/tls \
		$(DESTDIR)$(INCLUDEDIR) \
		$(DESTDIR)$(PKGCONFIGDIR) \
		$(DESTDIR)$(CONFIGDIR) \

installbin: $(SBIN_PROGS) $(BIN_PROGS) $(TORTURE_PROGS) installdirs
	@$(SHELL) $(srcdir)/script/installbin.sh \
		$(INSTALLPERMS) \
		$(DESTDIR)$(BASEDIR) \
		$(DESTDIR)$(SBINDIR) \
		$(DESTDIR)$(LIBDIR) \
		$(DESTDIR)$(VARDIR) \
		$(SBIN_PROGS)
	@$(SHELL) $(srcdir)/script/installbin.sh \
		$(INSTALLPERMS) \
		$(DESTDIR)$(BASEDIR) \
		$(DESTDIR)$(BINDIR) \
		$(DESTDIR)$(LIBDIR) \
		$(DESTDIR)$(VARDIR) \
		$(BIN_PROGS)
	@$(SHELL) $(srcdir)/script/installtorture.sh \
		$(INSTALLPERMS) \
		$(DESTDIR)$(TORTUREDIR) \
		$(TORTURE_PROGS)

installlib: $(INSTALLABLE_SHARED_LIBS) $(STATIC_LIBS) installdirs
	@$(SHELL) $(srcdir)/script/installlib.sh $(DESTDIR)$(LIBDIR) "$(SHLIBEXT)" $(INSTALLABLE_SHARED_LIBS) 
	#@$(SHELL) $(srcdir)/script/installlib.sh $(DESTDIR)$(LIBDIR) "$(STLIBEXT)" $(STATIC_LIBS)

installheader: headers installdirs
	@srcdir=$(srcdir) builddir=$(builddir) $(PERL) $(srcdir)/script/installheader.pl $(DESTDIR)$(INCLUDEDIR) $(PUBLIC_HEADERS) $(DEFAULT_HEADERS)

installdat: installdirs
	@$(SHELL) $(srcdir)/script/installdat.sh $(DESTDIR)$(DATADIR) $(srcdir)

installwebapps: installdirs
	@$(SHELL) $(srcdir)/script/installwebapps.sh $(DESTDIR)$(WEBAPPSDIR) $(srcdir)
	@$(SHELL) $(srcdir)/script/installjsonrpc.sh $(DESTDIR)$(SERVICESDIR) $(srcdir)

installman: manpages installdirs
	@$(SHELL) $(srcdir)/script/installman.sh $(DESTDIR)$(MANDIR) $(MANPAGES)

installmisc: installdirs
	@$(SHELL) $(srcdir)/script/installmisc.sh $(srcdir) $(DESTDIR)$(JSDIR) $(DESTDIR)$(SETUPDIR) $(DESTDIR)$(BINDIR)

installpc: installdirs
	@$(SHELL) $(srcdir)/script/installpc.sh $(builddir) $(DESTDIR)$(PKGCONFIGDIR) $(PC_FILES)

uninstall: uninstallbin uninstallman uninstallmisc uninstalllib uninstallheader \
	uninstallplugins

uninstallmisc:
	#FIXME

uninstallbin:
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(SBINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(SBIN_PROGS)
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(BIN_PROGS)
	@$(SHELL) $(srcdir)/script/uninstalltorture.sh $(DESTDIR)$(TORTUREDIR) $(TORTURE_PROGS)

uninstalllib:
	@$(SHELL) $(srcdir)/script/uninstalllib.sh $(DESTDIR)$(LIBDIR) $(SHARED_LIBS)
	#@$(SHELL) $(srcdir)/script/uninstalllib.sh $(DESTDIR)$(LIBDIR) $(STATIC_LIBS) 

uninstallheader:
	@$(SHELL) $(srcdir)/script/uninstallheader.sh $(DESTDIR)$(INCLUDEDIR) $(PUBLIC_HEADERS)

uninstallman:
	@$(SHELL) $(srcdir)/script/uninstallman.sh $(DESTDIR)$(MANDIR) $(MANPAGES)

Makefile: config.status $(MK_FILES)
	./config.status

etags:
	etags `find $(srcdir) -name "*.[ch]"`

ctags:
	ctags `find $(srcdir) -name "*.[ch]"`

pidl/Makefile: pidl/Makefile.PL
	cd pidl && $(PERL) Makefile.PL 

testcov-html:: pidl-testcov

pidl-testcov: pidl/Makefile
	cd pidl && cover -test

installpidl: pidl/Makefile
	$(MAKE) -C pidl install

uninstallpidl: pidl/Makefile
	$(MAKE) -C pidl uninstall

$(IDL_HEADER_FILES) \
	$(IDL_NDR_PARSE_H_FILES) $(IDL_NDR_PARSE_C_FILES) \
	$(IDL_NDR_CLIENT_C_FILES) $(IDL_NDR_CLIENT_H_FILES) \
	$(IDL_NDR_SERVER_C_FILES) $(IDL_SWIG_FILES) \
	$(IDL_NDR_EJS_C_FILES) $(IDL_NDR_EJS_H_FILES): idl

idl_full: pidl/lib/Parse/Pidl/IDL.pm pidl/lib/Parse/Pidl/Expr.pm 
	@CPP="$(CPP)" PERL="$(PERL)" srcdir=$(srcdir) $(srcdir)/script/build_idl.sh FULL

idl: pidl/lib/Parse/Pidl/IDL.pm pidl/lib/Parse/Pidl/Expr.pm 
	@CPP="$(CPP)" PERL="$(PERL)" srcdir=$(srcdir) $(srcdir)/script/build_idl.sh PARTIAL 

pidl/lib/Parse/Pidl/IDL.pm: pidl/idl.yp
	-$(YAPP) -m 'Parse::Pidl::IDL' -o pidl/lib/Parse/Pidl/IDL.pm pidl/idl.yp ||\
		touch pidl/lib/Parse/Pidl/IDL.pm 

pidl/lib/Parse/Pidl/Expr.pm: pidl/idl.yp
	-$(YAPP) -m 'Parse::Pidl::Expr' -o pidl/lib/Parse/Pidl/Expr.pm pidl/expr.yp ||\
		touch pidl/lib/Parse/Pidl/Expr.pm 

include/config.h:
	@echo "include/config.h not present"
	@echo "You need to rerun ./autogen.sh and ./configure"
	@/bin/false

$(srcdir)/version.h: $(srcdir)/VERSION
	@$(SHELL) script/mkversion.sh VERSION $(srcdir)/version.h $(srcdir)/

regen_version:
	@$(SHELL) script/mkversion.sh VERSION $(srcdir)/version.h $(srcdir)/

clean_pch:
	@echo "Removing precompiled headers"
	@-rm -f include/includes.h.gch

pch: clean_pch include/includes.h.gch

clean:: clean_pch
	@echo Removing objects
	@-find . -name '*.o' -exec rm -f '{}' \;
	@echo Removing hostcc objects
	@-find . -name '*.ho' -exec rm -f '{}' \;
	@echo Removing binaries
	@-rm -f $(BIN_PROGS) $(SBIN_PROGS) $(BINARIES) $(TORTURE_PROGS)
	@echo Removing libraries
	@-rm -f $(STATIC_LIBRARIES) $(SHARED_LIBRARIES)
	@-rm -f bin/static/*.a bin/shared/*.$(SHLIBEXT)
	@echo Removing modules
	@-rm -f bin/modules/*/*.$(SHLIBEXT)
	@-rm -f bin/*_init_module.c
	@echo Removing dummy targets
	@-rm -f bin/.*_*
	@echo Removing generated files
	@-rm -f bin/*_init_module.c
	@-rm -rf librpc/gen_* 
	@echo Removing proto headers
	@-rm -f $(PROTO_HEADERS)

distclean: clean
	-rm -f include/config.h include/config_tmp.h include/build.h
	-rm -f Makefile 
	-rm -f config.status
	-rm -f config.log config.cache
	-rm -f config.pm config.mk
	-rm -rf ../webapps/qooxdoo-*-sdk/frontend/framework/.cache
	-rm -f $(PC_FILES)

removebackup:
	-rm -f *.bak *~ */*.bak */*~ */*/*.bak */*/*~ */*/*/*.bak */*/*/*~

realdistclean: distclean removebackup
	-rm -f include/config_tmp.h.in
	-rm -f version.h
	-rm -f configure
	-rm -f $(MANPAGES)

check:: test

SELFTEST = $(PERL) $(srcdir)/selftest/selftest.pl --prefix=${selftest_prefix} \
    --builddir=$(builddir) --srcdir=$(srcdir) \
    --expected-failures=$(srcdir)/samba4-knownfail \
    --skip=$(srcdir)/samba4-skip \
    $(TEST_OPTIONS) 

test: everything
	$(SELFTEST) $(DEFAULT_TEST_OPTIONS) --immediate $(TESTS)

testone: everything
	$(SELFTEST) $(DEFAULT_TEST_OPTIONS) --one $(TESTS)

test-swrap: everything
	$(SELFTEST) --socket-wrapper --immediate $(TESTS)

test-noswrap: everything
	$(SELFTEST) --immediate $(TESTS)

quicktest: all
	$(SELFTEST) --quick --socket-wrapper --immediate $(TESTS)

quicktestone: all
	$(SELFTEST) --quick --socket-wrapper --one $(TESTS)

testenv: everything
	$(SELFTEST) --socket-wrapper --testenv

valgrindtest: valgrindtest-quick

valgrindtest-quick: all
	SMBD_VALGRIND="xterm -n smbd -e valgrind -q --db-attach=yes --num-callers=30" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST) --quick --immediate --socket-wrapper $(TESTS)

valgrindtest-all: everything
	SMBD_VALGRIND="xterm -n smbd -e valgrind -q --db-attach=yes --num-callers=30" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST) --immediate --socket-wrapper $(TESTS)

valgrindtest-env: everything
	SMBD_VALGRIND="xterm -n smbd -e valgrind -q --db-attach=yes --num-callers=30" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST) --socket-wrapper --testenv

gdbtest: gdbtest-quick

gdbtest-quick: all
	SMBD_VALGRIND="xterm -n smbd -e $(srcdir)/script/gdb_run " \
	$(SELFTEST) --immediate --quick --socket-wrapper $(TESTS)

gdbtest-all: everything
	SMBD_VALGRIND="xterm -n smbd -e $(srcdir)/script/gdb_run " \
	$(SELFTEST) --immediate --socket-wrapper $(TESTS)

gdbtest-env: everything
	SMBD_VALGRIND="xterm -n smbd -e $(srcdir)/script/gdb_run " \
	$(SELFTEST) --socket-wrapper --testenv

wintest: all
	$(SELFTEST) win

unused_macros:
	$(srcdir)/script/find_unused_macros.pl `find . -name "*.[ch]"` | sort

###############################################################################
# File types
###############################################################################

.SUFFIXES: .x .c .et .y .l .d .o .h .h.gch .a .$(SHLIBEXT) .1 .1.xml .3 .3.xml .5 .5.xml .7 .7.xml .8 .8.xml .ho .idl .hd

.c.d:
	@echo "Generating dependencies for $<"
	@$(DEPENDS)

.c.hd:
	@echo "Generating host-compiler dependencies for $<"
	@$(HDEPENDS)

include/includes.d: include/includes.h
	@echo "Generating dependencies for $<"
	@$(PCHDEPENDS)

.c.o:
	@if test -n "$(CC_CHECKER)"; then \
		echo "Checking  $< with '$(CC_CHECKER)'"; \
		$(CHECK) ; \
	fi
	@echo "Compiling $<"
	@-mkdir -p `dirname $@`
	@$(COMPILE) && exit 0 ; \
		echo "The following command failed:" 1>&2;\
		echo "$(COMPILE)" 1>&2;\
		$(COMPILE) >/dev/null 2>&1

.c.ho:
	@echo "Compiling $< with host compiler"
	@-mkdir -p `dirname $@`
	@$(HCOMPILE) && exit 0;\
		echo "The following command failed:" 1>&2;\
		echo "$(HCOMPILE)" 1>&2;\
		$(HCOMPILE) >/dev/null 2>&1

.h.h.gch:
	@echo "Precompiling $<"
	@$(PCHCOMPILE)

.y.c:
	@echo "Building $< with $(YACC)"
	@-$(srcdir)/script/yacc_compile.sh "$(YACC)" "$<" "$@"

.l.c:
	@echo "Building $< with $(LEX)"
	@-$(srcdir)/script/lex_compile.sh "$(LEX)" "$<" "$@"

DOCBOOK_MANPAGE_URL = http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl

.1.xml.1:
	$(XSLTPROC) -o $@ $(DOCBOOK_MANPAGE_URL) $<

.3.xml.3:
	$(XSLTPROC) -o $@ $(DOCBOOK_MANPAGE_URL) $<

.5.xml.5:
	$(XSLTPROC) -o $@ $(DOCBOOK_MANPAGE_URL) $<

.7.xml.7:
	$(XSLTPROC) -o $@ $(DOCBOOK_MANPAGE_URL) $<

.8.xml.8:
	$(XSLTPROC) -o $@ $(DOCBOOK_MANPAGE_URL) $<

DEP_FILES = $(patsubst %.ho,%.hd,$(patsubst %.o,%.d,$(ALL_OBJS))) \
		   include/includes.d

dist:: idl_full manpages configure distclean 

configure: 
	./autogen.sh
