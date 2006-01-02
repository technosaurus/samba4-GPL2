all: basics bin/asn1_compile bin/compile_et binaries

include heimdal_build/config.mk
include config.mk
include dsdb/config.mk
include gtk/config.mk
include smbd/config.mk
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
include scripting/config.mk
include kdc/config.mk
include passdb/config.mk

binaries: $(BINARIES)
libraries: $(STATIC_LIBS) $(SHARED_LIBS)
modules: $(SHARED_MODULES)
headers: $(PUBLIC_HEADERS)
manpages: $(MANPAGES)
everything: all

showlayout: 
	@echo "Samba will be installed into:"
	@echo "  basedir: $(BASEDIR)"
	@echo "  bindir:  $(BINDIR)"
	@echo "  sbindir: $(SBINDIR)"
	@echo "  libdir:  $(LIBDIR)"
	@echo "  modulesdir:  $(MODULESDIR)"
	@echo "  includedir:  $(INCLUDEDIR)"
	@echo "  vardir:  $(VARDIR)"
	@echo "  privatedir:  $(PRIVATEDIR)"
	@echo "  piddir:   $(PIDDIR)"
	@echo "  lockdir:  $(LOCKDIR)"
	@echo "  logfilebase:  $(LOGFILEBASE)"
	@echo "  swatdir:  $(SWATDIR)"
	@echo "  mandir:   $(MANDIR)"

showflags:
	@echo "Samba will be compiled with flags:"
	@echo "  CFLAGS = $(CFLAGS)"
	@echo "  LDFLAGS = $(LDFLAGS)"
	@echo "  STLD_FLAGS = $(STLD_FLAGS)"
	@echo "  SHLD_FLAGS = $(SHLD_FLAGS)"
	@echo "  LIBS = $(LIBS)"

# The permissions to give the executables
INSTALLPERMS = 0755

# set these to where to find various files
# These can be overridden by command line switches (see smbd(8))
# or in smb.conf (see smb.conf(5))
CONFIGFILE = $(CONFIGDIR)/smb.conf
PKGCONFIGDIR = $(LIBDIR)/pkgconfig
LMHOSTSFILE = $(CONFIGDIR)/lmhosts

PATH_FLAGS = -DCONFIGFILE=\"$(CONFIGFILE)\"  -DSBINDIR=\"$(SBINDIR)\" \
	 -DBINDIR=\"$(BINDIR)\" -DLMHOSTSFILE=\"$(LMHOSTSFILE)\" \
	 -DLOCKDIR=\"$(LOCKDIR)\" -DPIDDIR=\"$(PIDDIR)\" -DLIBDIR=\"$(LIBDIR)\" \
	 -DLOGFILEBASE=\"$(LOGFILEBASE)\" -DSHLIBEXT=\"$(SHLIBEXT)\" \
	 -DCONFIGDIR=\"$(CONFIGDIR)\" -DNCALRPCDIR=\"$(NCALRPCDIR)\" \
	 -DSWATDIR=\"$(SWATDIR)\" -DPRIVATE_DIR=\"$(PRIVATEDIR)\" \
	 -DMODULESDIR=\"$(MODULESDIR)\"

install: showlayout installbin installdat installswat installmisc installlib \
	installheader installpc

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
		$(DESTDIR)$(LIBDIR) \
		$(DESTDIR)$(VARDIR) \
		$(DESTDIR)$(PRIVATEDIR) \
		$(DESTDIR)$(PIDDIR) \
		$(DESTDIR)$(LOCKDIR) \
		$(DESTDIR)$(LOGFILEBASE) \
		$(DESTDIR)$(PRIVATEDIR)/tls \
		$(DESTDIR)$(INCLUDEDIR) \
		$(DESTDIR)$(PKGCONFIGDIR)

installbin: $(SBIN_PROGS) $(BIN_PROGS) installdirs
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

installlib: libraries installdirs
	@$(SHELL) $(srcdir)/script/installlib.sh $(DESTDIR)$(LIBDIR) $(SHARED_LIBS) 
	@$(SHELL) $(srcdir)/script/installlib.sh $(DESTDIR)$(LIBDIR) $(STATIC_LIBS)

installheader: headers installdirs
	@$(SHELL) $(srcdir)/script/installheader.sh $(DESTDIR)$(INCLUDEDIR) $(PUBLIC_HEADERS)

installdat: installdirs
	@$(SHELL) $(srcdir)/script/installdat.sh $(DESTDIR)$(LIBDIR) $(srcdir)

installswat: installdirs
	@$(SHELL) $(srcdir)/script/installswat.sh $(DESTDIR)$(SWATDIR) $(srcdir) $(DESTDIR)$(LIBDIR)

installman: installdirs
	@$(SHELL) $(srcdir)/script/installman.sh $(DESTDIR)$(MANDIR) $(MANPAGES)

installmisc: installdirs
	@$(SHELL) $(srcdir)/script/installmisc.sh $(srcdir) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(BINDIR)

installpc: installdirs
	@$(SHELL) $(srcdir)/script/installpc.sh $(srcdir) $(DESTDIR)$(PKGCONFIGDIR) $(PC_FILES)

uninstall: uninstallbin uninstallman uninstallmisc uninstalllib uninstallheader

uninstallmisc:
	#FIXME

uninstallbin:
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(SBINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(SBIN_PROGS)
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(BIN_PROGS)

uninstalllib:
	@$(SHELL) $(srcdir)/script/uninstalllib.sh $(DESTDIR)$(LIBDIR) $(SHARED_LIBS)
	@$(SHELL) $(srcdir)/script/uninstalllib.sh $(DESTDIR)$(LIBDIR) $(STATIC_LIBS) 

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

installpidl: pidl/Makefile
	cd pidl && $(MAKE) install

idl_full: pidl/lib/Parse/Pidl/IDL.pm
	@CPP="$(CPP)" PERL="$(PERL)" script/build_idl.sh FULL $(PIDL_ARGS)

idl: pidl/lib/Parse/Pidl/IDL.pm
	@CPP="$(CPP)" PERL="$(PERL)" script/build_idl.sh PARTIAL $(PIDL_ARGS)

pidl/lib/Parse/Pidl/IDL.pm: pidl/idl.yp
	-$(YAPP) -s -m 'Parse::Pidl::IDL' -o pidl/lib/Parse/Pidl/IDL.pm pidl/idl.yp 

smb_interfaces: pidl/smb_interfaces.pm
	$(PERL) -Ipidl script/build_smb_interfaces.pl \
		include/smb_interfaces.h

pidl/smb_interfaces.pm: pidl/smb_interfaces.yp
	-$(YAPP) -s -m 'smb_interfaces' -o pidl/smb_interfaces.pm pidl/smb_interfaces.yp 

include/config.h:
	@echo "include/config.h not present"
	@echo "You need to rerun ./autogen.sh and ./configure"
	@/bin/false

include/proto.h: $(PROTO_OBJS:.o=.c)
	@echo "Creating include/proto.h"
	@$(PERL) $(srcdir)/script/mkproto.pl --public-define=_PROTO_H_ \
		--public=include/proto.h --private=include/proto.h \
		$(PROTO_OBJS)

proto: include/proto.h

pch: clean_pch include/config.h \
	include/proto.h \
	idl \
	include/includes.h.gch

libcli/nbt/libnbt.h: libcli/nbt/nbt_proto.h
include/includes.h: lib/basic.h libcli/nbt/libnbt.h librpc/ndr/libndr_proto.h librpc/rpc/dcerpc_proto.h auth/credentials/credentials_proto.h

clean_pch: 
	-rm -f include/includes.h.gch

basics: include/config.h \
	include/proto.h \
	$(PROTO_HEADERS) \
	idl \
	heimdal_basics

clean: heimdal_clean
	@echo Removing headers
	@-rm -f include/proto.h
	@echo Removing objects
	@-find . -name '*.o' -exec rm -f '{}' \;
	@echo Removing hostcc objects
	@-find . -name '*.ho' -exec rm -f '{}' \;
	@echo Removing binaries
	@-rm -f $(BIN_PROGS) $(SBIN_PROGS)
	@echo Removing libraries
	@-rm -f bin/*.$(SHLIBEXT).*
	@echo Removing dummy targets
	@-rm -f bin/.*_*
	@echo Removing generated files
	@-rm -rf librpc/gen_* 
	@-rm -f lib/registry/regf.h lib/registry/tdr_regf*
	@echo Removing proto headers
	@-rm -f $(PROTO_HEADERS)

distclean: clean
	-rm -f bin/.dummy 
	-rm -f include/config.h include/smb_build.h
	-rm -f Makefile 
	-rm -f config.status
	-rm -f config.log config.cache
	-rm -f samba4-deps.dot
	-rm -f config.pm config.mk

removebackup:
	-rm -f *.bak *~ */*.bak */*~ */*/*.bak */*/*~ */*/*/*.bak */*/*/*~

realdistclean: distclean removebackup
	-rm -f include/config.h.in
	-rm -f include/version.h
	-rm -f configure
	-rm -f $(MANPAGES)

test: $(DEFAULT_TEST_TARGET)

test-swrap: all
	./script/tests/selftest.sh ${selftest_prefix}/st all SOCKET_WRAPPER

test-noswrap: all
	./script/tests/selftest.sh ${selftest_prefix}/st all

quicktest: all
	./script/tests/selftest.sh ${selftest_prefix}/st quick SOCKET_WRAPPER

valgrindtest: all
	SMBD_VALGRIND="xterm -n smbd -e valgrind -q --db-attach=yes --num-callers=30" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/st/valgrind.log" \
	./script/tests/selftest.sh ${selftest_prefix}/st quick SOCKET_WRAPPER

gdbtest: all
	SMBD_VALGRIND="xterm -n smbd -e gdb --args " \
	./script/tests/selftest.sh ${selftest_prefix}/st quick SOCKET_WRAPPER

bin/.dummy:
	@: >> $@ || : > $@

###############################################################################
# File types
###############################################################################

.c.d:
	@echo "Generating dependencies for $<"
	@$(CC) -MM -MG -MT $(<:.c=.o) -MF $@ $(CFLAGS) $<

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
