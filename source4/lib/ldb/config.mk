################################################
# Start MODULE ldb_asq
[MODULE::ldb_asq]
PRIVATE_DEPENDENCIES = LIBTALLOC
CFLAGS = -Ilib/ldb/include
INIT_FUNCTION = ldb_asq_init
SUBSYSTEM = LIBLDB
OBJ_FILES = \
		modules/asq.o
# End MODULE ldb_asq
################################################

################################################
# Start MODULE ldb_server_sort
[MODULE::ldb_server_sort]
PRIVATE_DEPENDENCIES = LIBTALLOC
CFLAGS = -Ilib/ldb/include
INIT_FUNCTION = ldb_sort_init
SUBSYSTEM = LIBLDB
OBJ_FILES = \
		modules/sort.o
# End MODULE ldb_sort
################################################

################################################
# Start MODULE ldb_paged_results
[MODULE::ldb_paged_results]
INIT_FUNCTION = ldb_paged_results_init
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC
SUBSYSTEM = LIBLDB
OBJ_FILES = \
		modules/paged_results.o
# End MODULE ldb_paged_results
################################################

################################################
# Start MODULE ldb_paged_results
[MODULE::ldb_paged_searches]
INIT_FUNCTION = ldb_paged_searches_init
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC
SUBSYSTEM = LIBLDB
OBJ_FILES = \
		modules/paged_searches.o
# End MODULE ldb_paged_results
################################################

################################################
# Start MODULE ldb_operational
[MODULE::ldb_operational]
SUBSYSTEM = LIBLDB
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC
INIT_FUNCTION = ldb_operational_init
OBJ_FILES = \
		modules/operational.o
# End MODULE ldb_operational
################################################

################################################
# Start MODULE ldb_objectclass
[MODULE::ldb_objectclass]
INIT_FUNCTION = ldb_objectclass_init
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC
SUBSYSTEM = LIBLDB
OBJ_FILES = \
		modules/objectclass.o
# End MODULE ldb_objectclass
################################################

################################################
# Start MODULE ldb_rdn_name
[MODULE::ldb_rdn_name]
SUBSYSTEM = LIBLDB
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC
INIT_FUNCTION = ldb_rdn_name_init
OBJ_FILES = \
		modules/rdn_name.o
# End MODULE ldb_rdn_name
################################################

################################################
# Start MODULE ldb_ildap
[MODULE::ldb_ildap]
SUBSYSTEM = LIBLDB
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC LIBCLI_LDAP
INIT_FUNCTION = ldb_ildap_init
ALIASES = ldapi ldaps ldap
OBJ_FILES = \
		ldb_ildap/ldb_ildap.o
# End MODULE ldb_ildap
################################################

################################################
# Start MODULE ldb_map
[MODULE::ldb_map]
PRIVATE_DEPENDENCIES = LIBTALLOC
CFLAGS = -Ilib/ldb/include -Ilib/ldb/ldb_map
SUBSYSTEM = LIBLDB
OBJ_FILES = \
		ldb_map/ldb_map_inbound.o \
		ldb_map/ldb_map_outbound.o \
		ldb_map/ldb_map.o
# End MODULE ldb_map
################################################

################################################
# Start MODULE ldb_skel
[MODULE::ldb_skel]
SUBSYSTEM = LIBLDB
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC
INIT_FUNCTION = ldb_skel_init
OBJ_FILES = modules/skel.o
# End MODULE ldb_skel
################################################

################################################
# Start MODULE ldb_sqlite3
[MODULE::ldb_sqlite3]
SUBSYSTEM = LIBLDB
CFLAGS = -Ilib/ldb/include
PRIVATE_DEPENDENCIES = LIBTALLOC SQLITE3 LIBTALLOC
INIT_FUNCTION = ldb_sqlite3_init
OBJ_FILES = \
		ldb_sqlite3/ldb_sqlite3.o
# End MODULE ldb_sqlite3
################################################

################################################
# Start MODULE ldb_tdb
[MODULE::ldb_tdb]
SUBSYSTEM = LIBLDB
CFLAGS = -Ilib/ldb/include -Ilib/ldb/ldb_tdb
INIT_FUNCTION = ldb_tdb_init
OBJ_FILES = \
		ldb_tdb/ldb_tdb.o \
		ldb_tdb/ldb_search.o \
		ldb_tdb/ldb_pack.o \
		ldb_tdb/ldb_index.o \
		ldb_tdb/ldb_cache.o \
		ldb_tdb/ldb_tdb_wrap.o
PRIVATE_DEPENDENCIES = \
		LIBTDB LIBTALLOC
# End MODULE ldb_tdb
################################################

# NOTE: this rule is broken for some systems when $builddir != $srcdir because
# it hardcodes the use of $<. See smb_build/makefile.pm.
./lib/ldb/common/ldb_modules.o: lib/ldb/common/ldb_modules.c Makefile
	@echo Compiling $<
	@$(CC) `$(PERL) $(srcdir)/script/cflags.pl $@` $(CFLAGS) $(PICFLAG) \
	-DLDBMODULESDIR=\"$(MODULESDIR)/ldb\" -DSHLIBEXT=\"$(SHLIBEXT)\" \
	-c $< -o $@

################################################
# Start SUBSYSTEM ldb
[LIBRARY::LIBLDB]
VERSION = 0.0.1
SO_VERSION = 0
CFLAGS = -Ilib/ldb/include
DESCRIPTION = LDAP-like embedded database library
INIT_FUNCTION_TYPE = int (*) (void)
OBJ_FILES = \
		common/ldb.o \
		common/ldb_ldif.o \
		common/ldb_parse.o \
		common/ldb_msg.o \
		common/ldb_utf8.o \
		common/ldb_debug.o \
		common/ldb_modules.o \
		common/ldb_match.o \
		common/ldb_attributes.o \
		common/attrib_handlers.o \
		common/ldb_dn.o \
		common/ldb_controls.o \
		common/qsort.o
PUBLIC_DEPENDENCIES = \
		LIBTALLOC
PRIVATE_DEPENDENCIES = \
		DYNCONFIG \
		SOCKET_WRAPPER
MANPAGE = man/ldb.3
PUBLIC_HEADERS = include/ldb.h include/ldb_errors.h
#
# End SUBSYSTEM ldb
################################################

################################################
# Start SUBSYSTEM LIBLDB_CMDLINE
[SUBSYSTEM::LIBLDB_CMDLINE]
CFLAGS = -Ilib/ldb
OBJ_FILES= \
		tools/cmdline.o
PUBLIC_DEPENDENCIES = LIBLDB LIBPOPT
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL POPT_SAMBA POPT_CREDENTIALS gensec
# End SUBSYSTEM LIBLDB_CMDLINE
################################################

################################################
# Start BINARY ldbadd
[BINARY::ldbadd]
INSTALLDIR = BINDIR
OBJ_FILES = \
		tools/ldbadd.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE LIBCLI_RESOLVE
MANPAGE = man/ldbadd.1
# End BINARY ldbadd
################################################

################################################
# Start BINARY ldbdel
[BINARY::ldbdel]
INSTALLDIR = BINDIR
OBJ_FILES= \
		tools/ldbdel.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
MANPAGE = man/ldbdel.1
# End BINARY ldbdel
################################################

################################################
# Start BINARY ldbmodify
[BINARY::ldbmodify]
INSTALLDIR = BINDIR
OBJ_FILES= \
		tools/ldbmodify.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
MANPAGE = man/ldbmodify.1
# End BINARY ldbmodify
################################################

################################################
# Start BINARY ldbsearch
[BINARY::ldbsearch]
INSTALLDIR = BINDIR
OBJ_FILES= \
		tools/ldbsearch.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE 
MANPAGE = man/ldbsearch.1
# End BINARY ldbsearch
################################################

################################################
# Start BINARY ldbedit
[BINARY::ldbedit]
INSTALLDIR = BINDIR
OBJ_FILES= \
		tools/ldbedit.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
MANPAGE = man/ldbedit.1
# End BINARY ldbedit
################################################

################################################
# Start BINARY ldbrename
[BINARY::ldbrename]
INSTALLDIR = BINDIR
OBJ_FILES= \
		tools/ldbrename.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
MANPAGE = man/ldbrename.1
# End BINARY ldbrename
################################################

################################################
# Start BINARY ldbtest
[BINARY::ldbtest]
OBJ_FILES= \
		tools/ldbtest.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY ldbtest
################################################

################################################
# Start BINARY oLschema2ldif
[BINARY::oLschema2ldif]
INSTALLDIR = BINDIR
MANPAGE = man/oLschema2ldif.1
OBJ_FILES= \
		tools/convert.o \
		tools/oLschema2ldif.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY oLschema2ldif
################################################

################################################
# Start BINARY  ad2oLschema
[BINARY::ad2oLschema]
INSTALLDIR = BINDIR
MANPAGE = man/ad2oLschema.1
OBJ_FILES= \
		tools/convert.o \
		tools/ad2oLschema.o
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY ad2oLschema
################################################

#######################
# Start LIBRARY swig_ldb
[LIBRARY::swig_ldb]
PUBLIC_DEPENDENCIES = LIBLDB DYNCONFIG
LIBRARY_REALNAME = swig/_ldb.$(SHLIBEXT)
OBJ_FILES = swig/ldb_wrap.o
# End LIBRARY swig_ldb
#######################

include samba/config.mk
