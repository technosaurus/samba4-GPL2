###########################################################
### SMB Build System					###
### - create output for Makefile			###
###							###
###  Copyright (C) Stefan (metze) Metzmacher 2004	###
###  Released under the GNU GPL				###
###########################################################

package makefile;
use strict;

sub _prepare_command_interpreters($)
{
	my $ctx = shift;
	my $output;

	$output = << '__EOD__';
SHELL=/bin/sh
PERL=@PERL@

__EOD__

	return $output;
}

sub _prepare_path_vars($)
{
	my $ctx = shift;
	my $output;

	$output = << '__EOD__';
prefix=@prefix@
exec_prefix=@exec_prefix@
VPATH=@srcdir@
srcdir=@srcdir@
builddir=@builddir@

BASEDIR= @prefix@
BINDIR = @bindir@
SBINDIR = @sbindir@
LIBDIR = @libdir@
CONFIGDIR = @configdir@
VARDIR = @localstatedir@

# The permissions to give the executables
INSTALLPERMS = 0755

# set these to where to find various files
# These can be overridden by command line switches (see smbd(8))
# or in smb.conf (see smb.conf(5))
LOGFILEBASE = @logfilebase@
CONFIGFILE = $(CONFIGDIR)/smb.conf
LMHOSTSFILE = $(CONFIGDIR)/lmhosts
NCALRPCDIR = @localstatedir@/ncalrpc

# This is where smbpasswd et al go
PRIVATEDIR = @privatedir@
SMB_PASSWD_FILE = $(PRIVATEDIR)/smbpasswd

# the directory where lock files go
LOCKDIR = @lockdir@

# the directory where pid files go
PIDDIR = @piddir@

PASSWD_FLAGS = -DSMB_PASSWD_FILE=\"$(SMB_PASSWD_FILE)\" -DPRIVATE_DIR=\"$(PRIVATEDIR)\"
PATH_FLAGS1 = -DCONFIGFILE=\"$(CONFIGFILE)\"  -DSBINDIR=\"$(SBINDIR)\"
PATH_FLAGS2 = $(PATH_FLAGS1) -DBINDIR=\"$(BINDIR)\" 
PATH_FLAGS3 = $(PATH_FLAGS2) -DLMHOSTSFILE=\"$(LMHOSTSFILE)\" 
PATH_FLAGS4 = $(PATH_FLAGS3) -DLOCKDIR=\"$(LOCKDIR)\" -DPIDDIR=\"$(PIDDIR)\"
PATH_FLAGS5 = $(PATH_FLAGS4) -DLIBDIR=\"$(LIBDIR)\" \
	      -DLOGFILEBASE=\"$(LOGFILEBASE)\" -DSHLIBEXT=\"@SHLIBEXT@\"
PATH_FLAGS6 = $(PATH_FLAGS5) -DCONFIGDIR=\"$(CONFIGDIR)\" -DNCALRPCDIR=\"$(NCALRPCDIR)\"
PATH_FLAGS = $(PATH_FLAGS6) $(PASSWD_FLAGS)

__EOD__

	return $output;
}

sub _prepare_compiler_linker($)
{
	my $ctx = shift;
	my $output;

	$output = << '__EOD__';
CC=@CC@
CC_FLAGS=-Iinclude -I. -I$(srcdir)/include -I$(srcdir) -D_SAMBA_BUILD_ -Ilib @CFLAGS@ @CPPFLAGS@

LD=@CC@
LD_FLAGS=@LDFLAGS@ @CFLAGS@

STLD=ar
STLD_FLAGS=-rc

SHLD=@CC@
SHLD_FLAGS=@LDSHFLAGS@ @LDFLAGS@

__EOD__

	return $output;
}

sub _prepare_default_rule($)
{
	my $ctx = shift;
	my $output;

	$output = << '__EOD__';
default: all

__EOD__

	return $output;
}

sub _prepare_SUFFIXES($)
{
	my $ctx = shift;
	my $output;

	$output = << '__EOD__';
.SUFFIXES:
.SUFFIXES: .c .o .h .h.gch .a .so

__EOD__

	return $output;
}

sub _prepare_IDL($)
{
	my $ctx = shift;
	my $output;

	$output = << '__EOD__';
idl_full: build/pidl/idl.pm
	CPP="@CPP@" PERL="$(PERL)" script/build_idl.sh FULL

idl: build/pidl/idl.pm
	@CPP="@CPP@" PERL="$(PERL)" script/build_idl.sh PARTIAL

build/pidl/idl.pm: build/pidl/idl.yp
	-yapp -s build/pidl/idl.yp

pch: proto include/includes.h.gch

pch_clean:
	-rm -f include/includes.h.gch

basics: idl proto_exists

test: all
	./script/tests/selftest.sh `pwd`/prefix-test
	
__EOD__

	return $output;
}

sub _prepare_dummy_MAKEDIR()
{
	my $ctx = shift;
	my $output;

	$output =  << '__EOD__';
bin/.dummy:
	@: >> $@ || : > $@

dynconfig.o: dynconfig.c Makefile
	@echo Compiling $*.c
	@$(CC) $(CC_FLAGS) @PICFLAG@ $(PATH_FLAGS) -c $< -o $@
@BROKEN_CC@	-mv `echo $@ | sed 's%^.*/%%g'` $@

__EOD__

	return $output;
}

###########################################################
# This function creates a standard make rule which is using $(CC)
#
# $output = _prepare_std_CC_rule($srcext,$destext,$flags,$message,$comment)
#
# $srcext -	sourcefile extension
#
# $destext -	destinationfile extension
#
# $flags -	additional compiler flags
#
# $message -	logmessage which is echoed while running this rule
#
# $comment -	just a comment what this rule should do
#
# $output -		the resulting output buffer
sub _prepare_std_CC_rule($$$$$)
{
	my $src = shift;
	my $dst = shift;
	my $flags = shift;
	my $message = shift;
	my $comment = shift;
	my $flagsstr = "";
	my $output;

	$output = << "__EOD__";
###################################
# Start $comment
.$src.$dst:
	\@echo $message \$\*.$src
	\@\$(CC) \$(CC_FLAGS) $flags -c \$< -o \$\@
\@BROKEN_CC\@	-mv `echo \$\@ | sed 's%^.*/%%g'` \$\@
#End $comment
###################################

__EOD__

	return $output;
}

sub array2oneperline($)
{
	my $array = shift;
	my $i;
	my $output = "";

	foreach my $str (@{$array}) {
		if (!defined($str)) {
			next;
		}

		$output .= " \\\n\t\t";
		$output .= $str;
	}

	return $output;
}

sub array2oneline($)
{
	my $array = shift;
	my $i;
	my $output = "";

	foreach my $str (@{$array}) {
		if (!defined($str)) {
			next;
		}

		$output .= $str;
		$output .= " ";
	}

	return $output;
}

###########################################################
# This function creates a object file list
#
# $output = _prepare_var_obj_list($var, $var_ctx)
#
# $var_ctx -		the subsystem context
#
# $var_ctx->{NAME} 	-	the <var> name
# $var_ctx->{OBJ_LIST} 	-	the list of objectfiles which sould be linked to this <var>
#
# $output -		the resulting output buffer
sub _prepare_obj_list($$)
{
	my $var = shift;
	my $ctx = shift;
	my $tmpobjlist;
	my $output;

	$tmpobjlist = array2oneperline($ctx->{OBJ_LIST});

	$output = << "__EOD__";
###################################
# Start $var $ctx->{NAME} OBJ LIST
$var\_$ctx->{NAME}_OBJS =$tmpobjlist
# End $var $ctx->{NAME} OBJ LIST
###################################

__EOD__

	return $output;
}

###########################################################
# This function creates a object file list for a subsystem
#
# $output = _prepare_subsystem_obj_list($subsystem_ctx)
#
# $subsystem_ctx -		the subsystem context
#
# $subsystem_ctx->{NAME} -	the subsystem name
# $subsystem_ctx->{OBJ_LIST} -	the list of objectfiles which sould be linked to this subsystem
#
# $output -		the resulting output buffer
sub _prepare_subsystem_obj_list($)
{
	my $ctx = shift;

	return _prepare_var_obj_list("SUBSYSTEM",$ctx);
}

###########################################################
# This function creates a object file list for a module
#
# $output = _prepare_module_obj_and_lib_list($module_ctx)
#
# $module_ctx -		the module context
#
# $module_ctx->{NAME} -		the module binary name
# $module_ctx->{OBJ_LIST} -	the list of objectfiles which sould be linked to this module
#
# $output -		the resulting output buffer
sub _prepare_module_obj_list($)
{
	my $ctx = shift;

	return _prepare_var_obj_list("MODULE",$ctx);

}

###########################################################
# This function creates a make rule for linking a library
#
# $output = _prepare_shared_library_rule($library_ctx)
#
# $library_ctx -		the library context
#
# $library_ctx->{NAME} -		the library name
#
# $library_ctx->{DEPEND_LIST} -		the list of rules on which this library depends
#
# $library_ctx->{LIBRARY_NAME} -	the shared library name
# $library_ctx->{LIBRARY_REALNAME} -	the shared library real name
# $library_ctx->{LIBRARY_SONAME} - the shared library soname
# $library_ctx->{LINK_LIST} -	the list of objectfiles and external libraries
#					which sould be linked to this shared library
# $library_ctx->{LINK_FLAGS} -	linker flags used by this shared library
#
# $output -		the resulting output buffer
sub _prepare_shared_library_rule($)
{
	my $ctx = shift;
	my $tmpdepend;
	my $tmpstlink;
	my $tmpstflag;
	my $tmpshlink;
	my $tmpshflag;
	my $tmprules;
	my $output;
	my $outname = $ctx->{OUTPUT};

	$tmpdepend = array2oneperline($ctx->{DEPEND_LIST});

	$tmpshlink = array2oneperline($ctx->{LINK_LIST});
	$tmpshflag = array2oneperline($ctx->{LINK_FLAGS});

	$output = << "__EOD__";
###################################
# Start Library $ctx->{NAME}
#
LIBRARY_$ctx->{NAME}_DEPEND_LIST =$tmpdepend
#
LIBRARY_$ctx->{NAME}_SHARED_LINK_LIST =$tmpshlink
LIBRARY_$ctx->{NAME}_SHARED_LINK_FLAGS =$tmpshflag
#

# Shared $ctx->{LIBRARY_NAME}
$ctx->{OUTPUT}: \$(LIBRARY_$ctx->{NAME}_DEPEND_LIST) bin/.dummy
	\@echo Linking \$\@
	\@\$(SHLD) \$(SHLD_FLAGS) -o \$\@ \\
		\$(LIBRARY_$ctx->{NAME}_SHARED_LINK_FLAGS) \\
		\$(LIBRARY_$ctx->{NAME}_SHARED_LINK_LIST)

__EOD__

	if (defined($ctx->{LIBRARY_SONAME})) {
	    $output .= << "__EOD__";
# Symlink $ctx->{LIBRARY_SONAME}
bin/$ctx->{LIBRARY_SONAME}: bin/$ctx->{LIBRARY_REALNAME} bin/.dummy
	\@echo Symlink \$\@
	\@ln -sf $ctx->{LIBRARY_REALNAME} \$\@
# Symlink $ctx->{LIBRARY_NAME}
bin/$ctx->{LIBRARY_NAME}: bin/$ctx->{LIBRARY_SONAME} bin/.dummy
	\@echo Symlink \$\@
	\@ln -sf $ctx->{LIBRARY_SONAME} \$\@

__EOD__
		$outname = $ctx->{LIBRARY_NAME};
	}

$output .= << "__EOD__";
library_$ctx->{NAME}: basics bin/$outname
# End Library $ctx->{NAME}
###################################

__EOD__

	return $output;
}

###########################################################
# This function creates a make rule for linking a library
#
# $output = _prepare_static_library_rule($library_ctx)
#
# $library_ctx -		the library context
#
# $library_ctx->{NAME} -		the library name
#
# $library_ctx->{DEPEND_LIST} -		the list of rules on which this library depends
#
# $library_ctx->{LIBRARY_NAME} -	the static library name
# $library_ctx->{LINK_LIST} -	the list of objectfiles	which sould be linked
#					to this static library
# $library_ctx->{LINK_FLAGS} -	linker flags used by this static library
#
# $output -		the resulting output buffer
sub _prepare_static_library_rule($)
{
	my $ctx = shift;
	my $tmpdepend;
	my $tmpstlink;
	my $tmpstflag;
	my $tmpshlink;
	my $tmpshflag;
	my $tmprules;
	my $output;

	$tmpdepend = array2oneperline($ctx->{DEPEND_LIST});

	$tmpstlink = array2oneperline($ctx->{LINK_LIST});
	$tmpstflag = array2oneperline($ctx->{LINK_FLAGS});

	$tmprules = "bin/$ctx->{LIBRARY_NAME}";

	$output = << '__EOD__';
###################################
# Start Library $ctx->{NAME}
#
LIBRARY_$ctx->{NAME}_DEPEND_LIST =$tmpdepend
#
LIBRARY_$ctx->{NAME}_STATIC_LINK_LIST =$tmpstlink
#
# Static $ctx->{LIBRARY_NAME}
$ctx->{OUTPUT}: $(LIBRARY_$ctx->{NAME}_DEPEND_LIST) bin/.dummy
	@echo Linking $@
	@$(STLD) $(STLD_FLAGS) $@ \\
		$(LIBRARY_$ctx->{NAME}_STATIC_LINK_LIST)

library_$ctx->{NAME}: basics $tmprules
# End Library $ctx->{NAME}
###################################

__EOD__

	return $output;
}



###########################################################
# This function creates a make rule for linking a binary
#
# $output = _prepare_binary_rule($binary_ctx)
#
# $binary_ctx -		the binary context
#
# $binary_ctx->{NAME} -		the binary name
# $binary_ctx->{BINARY} -	the binary binary name
#
# $binary_ctx->{DEPEND_LIST} -	the list of rules on which this binary depends
# $binary_ctx->{LINK_LIST} -	the list of objectfiles and external libraries
#				which sould be linked to this binary
# $binary_ctx->{LINK_FLAGS} -	linker flags used by this binary
#
# $output -		the resulting output buffer
sub _prepare_binary_rule($)
{
	my $ctx = shift;
	my $tmpdepend;
	my $tmplink;
	my $tmpflag;
	my $output;

	$tmpdepend = array2oneperline($ctx->{DEPEND_LIST});
	$tmplink = array2oneperline($ctx->{LINK_LIST});
	$tmpflag = array2oneperline($ctx->{LINK_FLAGS});

	$output = << "__EOD__";
###################################
# Start Binary $ctx->{BINARY}
#
BINARY_$ctx->{NAME}_DEPEND_LIST =$tmpdepend
BINARY_$ctx->{NAME}_LINK_LIST =$tmplink
BINARY_$ctx->{NAME}_LINK_FLAGS =$tmpflag
#
bin/$ctx->{BINARY}: bin/.dummy \$(BINARY_$ctx->{NAME}_DEPEND_LIST)
	\@echo Linking \$\@
	\@\$(LD) \$(LD_FLAGS) -o \$\@ \\
		\$\(BINARY_$ctx->{NAME}_LINK_FLAGS) \\
		\$\(BINARY_$ctx->{NAME}_LINK_LIST) \\
		\$\(BINARY_$ctx->{NAME}_LINK_FLAGS)
binary_$ctx->{BINARY}: basics bin/$ctx->{BINARY}
# End Binary $ctx->{BINARY}
###################################

__EOD__

	return $output;
}

sub _prepare_proto_rules()
{
	my $output = "";

	$output .= << '__EOD__';
# Making this target will just make sure that the prototype files
# exist, not necessarily that they are up to date.  Since they're
# removed by 'make clean' this will always be run when you do anything
# afterwards.
proto_exists: include/proto.h include/build_env.h

delheaders: pch_clean
	-rm -f $(builddir)/include/proto.h $(builddir)/include/build_env.h:

include/proto.h:
	@cd $(srcdir) && $(SHELL) script/mkproto.sh "$(PERL)" \
	  -h _PROTO_H_ $(builddir)/include/proto.h \
	  $(PROTO_PROTO_OBJS)

include/build_env.h:
	@echo Building include/build_env.h
	@cd $(srcdir) && $(SHELL) script/build_env.sh $(srcdir) $(builddir) $(CC) > $(builddir)/include/build_env.h

# 'make headers' or 'make proto' calls a subshell because we need to
# make sure these commands are executed in sequence even for a
# parallel make.
headers: delheaders proto_exists

proto: idl headers

proto_test:
	@[ -f $(builddir)/include/proto.h ] || $(MAKE) proto

clean: delheaders
	-rm -f *.o */*.o */*/*.o */*/*/*.o bin/*
	-rm -rf librpc/gen_*

distclean: clean
	-rm -f bin/.dummy
	-rm -f include/config.h 
	-rm -f Makefile*
	-rm -f config.status
	-rm -f config.smb_build.*
	-rm -f config.log config.cache

removebackup:
	-rm -f *.bak *~ */*.bak */*~ */*/*.bak */*/*~ */*/*/*.bak */*/*/*~

realdistclean: distclean removebackup
	-rm -f include/config.h.in
	-rm -f lib/version.h
	-rm -f configure
__EOD__

	return $output;
}

sub _prepare_make_target($)
{
	my $ctx = shift;
	my $tmpdepend;
	my $output;

	$tmpdepend = array2oneperline($ctx->{DEPEND_LIST});

	$output = << "__EOD__";
###################################
# Start Target $ctx->{TARGET}
$ctx->{TARGET}: basics $tmpdepend
# End Target $ctx->{TARGET}
###################################

__EOD__

	return $output;
}

sub _prepare_obj_lists($)
{
	my $CTX = shift;
	my $output = "";

	foreach my $key (values %{$CTX}) {
		next if not defined($key->{OBJ_LIST});
		$output .= _prepare_obj_list($key->{TYPE}, $key);
	}

	return $output;
}

sub _prepare_install_rules($)
{
	my $CTX = shift;
	my $output = "";

	$output .= << '__EOD__';

showlayout: 
	@echo "Samba will be installed into:"
	@echo "  basedir: $(BASEDIR)"
	@echo "  bindir:  $(BINDIR)"
	@echo "  sbindir: $(SBINDIR)"
	@echo "  libdir:  $(LIBDIR)"
	@echo "  vardir:  $(VARDIR)"
	@echo "  privatedir:  $(PRIVATEDIR)"
	@echo "  piddir:  $(PIDDIR)"
	@echo "  lockdir:  $(LOCKDIR)"

showflags:
	@echo "Samba will be compiled with flags:"
	@echo "  CC_FLAGS = $(CC_FLAGS)"
	@echo "  LD_FLAGS = $(LD_FLAGS)"
	@echo "  STLD_FLAGS = $(STLD_FLAGS)"
	@echo "  SHLD_FLAGS = $(SHLD_FLAGS)"

SBIN_PROGS = bin/smbd

BIN_PROGS = bin/smbclient \
		bin/net \
		bin/nmblookup \
		bin/ntlm_auth

TORTURE_PROGS = bin/smbtorture \
		bin/gentest \
		bin/locktest \
		bin/masktest \
		bin/ndrdump

LDB_PROGS = 	bin/ldbadd \
		bin/ldbdel \
		bin/ldbmodify \
		bin/ldbedit \
		bin/ldbsearch

REG_PROGS = 	bin/regpatch \
		bin/regshell \
		bin/regtree \
		bin/regpatch \
		bin/regdiff

install: showlayout installbin installtorture installldb installreg installdat 

# DESTDIR is used here to prevent packagers wasting their time
# duplicating the Makefile. Remove it and you will have the privelege
# of package each samba release for muliple versions of multiple
# distributions and operating systems, or at least supplying patches
# to all the packaging files required for this, prior to committing
# the removal of DESTDIR. Do not remove it even though you think it
# is not used

installdirs:
	@$(SHELL) $(srcdir)/script/installdirs.sh $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(SBINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(PRIVATEDIR) $(DESTDIR)$(PIDDIR) $(DESTDIR)$(LOCKDIR)

installbin: all installdirs
	@$(SHELL) $(srcdir)/script/installbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(SBINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(SBIN_PROGS)
	@$(SHELL) $(srcdir)/script/installbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(BIN_PROGS)

installtorture: all installdirs
	@$(SHELL) $(srcdir)/script/installbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(TORTURE_PROGS)

installldb: all installdirs
	@$(SHELL) $(srcdir)/script/installbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(LDB_PROGS)

installreg: all installdirs
	@$(SHELL) $(srcdir)/script/installbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(REG_PROGS)

installdat: installdirs
	@$(SHELL) $(srcdir)/script/installdat.sh $(DESTDIR)$(LIBDIR) $(srcdir)

uninstall: uninstallbin uninstalltorture uninstallldb uninstallreg

uninstallbin:
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(SBINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(SBIN_PROGS)

uninstalltorture:
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(TORTURE_PROGS)

uninstallldb:
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(LDB_PROGS)

uninstallreg:
	@$(SHELL) $(srcdir)/script/uninstallbin.sh $(INSTALLPERMS) $(DESTDIR)$(BASEDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(VARDIR) $(DESTDIR)$(REG_PROGS)

# Swig extensions

swig: scripting/swig/_tdb.so scripting/swig/_dcerpc.so

scripting/swig/tdb_wrap.c: scripting/swig/tdb.i
	swig -python scripting/swig/tdb.i

scripting/swig/_tdb.so: scripting/swig/tdb_wrap.o $(LIBRARY_swig_tdb_DEPEND_LIST)
	$(SHLD) $(SHLD_FLAGS) -o scripting/swig/_tdb.so scripting/swig/tdb_wrap.o \
		$(LIBRARY_swig_tdb_SHARED_LINK_LIST) $(LIBRARY_swig_tdb_SHARED_LINK_FLAGS)

SWIG_INCLUDES = librpc/gen_ndr/samr.i librpc/gen_ndr/lsa.i librpc/gen_ndr/spoolss.i

scripting/swig/dcerpc_wrap.c: scripting/swig/dcerpc.i scripting/swig/samba.i scripting/swig/status_codes.i $(SWIG_INCLUDES)
	swig -python scripting/swig/dcerpc.i

scripting/swig/_dcerpc.so: scripting/swig/dcerpc_wrap.o $(LIBRARY_swig_dcerpc_DEPEND_LIST)
	$(SHLD) $(SHLD_FLAGS) -o scripting/swig/_dcerpc.so scripting/swig/dcerpc_wrap.o $(LIBRARY_swig_dcerpc_SHARED_LINK_LIST) $(LIBRARY_swig_dcerpc_SHARED_LINK_FLAGS)

swig_clean:
	-rm -f scripting/swig/_tdb.so scripting/swig/tdb.pyc \
		scripting/swig/tdb.py scripting/swig/tdb_wrap.c \
		scripting/swig/tdb_wrap.o

everything: all

etags:
	etags `find $(srcdir) -name "*.[ch]"`

ctags:
	ctags `find $(srcdir) -name "*.[ch]"`

__EOD__

	return $output;
}

sub _prepare_rule_lists($)
{
	my $depend = shift;
	my $output = "";

	foreach my $key (values %{$depend}) {
		next if not defined $key->{OUTPUT_TYPE};
		($output .= _prepare_static_library_rule($key)) if $key->{OUTPUT_TYPE} eq "STATIC_LIBRARY";
		($output .= _prepare_shared_library_rule($key)) if $key->{OUTPUT_TYPE} eq "SHARED_LIBRARY";
		($output .= _prepare_binary_rule($key)) if $key->{OUTPUT_TYPE} eq "BINARY";
	}

	my $idl_ctx;
	$output .= _prepare_IDL($idl_ctx);

	$output .= _prepare_proto_rules();

	$output .= _prepare_install_rules($depend);

	return $output;
}

###########################################################
# This function prepares the output for Makefile
#
# $output = _prepare_makefile_in($SMB_BUILD_CTX)
#
# $SMB_BUILD_CTX -	the global SMB_BUILD context
#
# $output -		the resulting output buffer
sub _prepare_makefile_in($)
{
	my $CTX = shift;
	my $output;

	$output  = "########################################\n";
	$output .= "# Autogenerated by config.smb_build.pl #\n";
	$output .= "########################################\n";
	$output .= "\n";

	my $cmd_ctx;
	$output .= _prepare_command_interpreters($cmd_ctx);

	my $path_ctx;
	$output .= _prepare_path_vars($path_ctx);

	my $compiler_ctx;
	$output .= _prepare_compiler_linker($compiler_ctx);

	my $rules_ctx;
	$output .= _prepare_default_rule($rules_ctx);

	my $suffix_ctx;
	$output .= _prepare_SUFFIXES($suffix_ctx);

	$output .= _prepare_dummy_MAKEDIR();

	$output .= _prepare_std_CC_rule("c","o",'@PICFLAG@',"Compiling","Rule for std objectfiles");
	$output .= _prepare_std_CC_rule("h","h.gch",'@PICFLAG@',"Precompiling","Rule for precompiled headerfiles");

	$output .= _prepare_obj_lists($CTX);

	$output .= _prepare_rule_lists($CTX);

	my @all = ();
	
	foreach my $part (values %{$CTX}) {
		push (@all, $part->{OUTPUT}) if defined ($part->{OUTPUT_TYPE}) and $part->{OUTPUT_TYPE} eq "BINARY";	
	}
	
	$output .= _prepare_make_target({ TARGET => "all", DEPEND_LIST => \@all });

	return $output;
}

###########################################################
# This function creates Makefile.in from the SMB_BUILD 
# context
#
# create_makefile_in($SMB_BUILD_CTX)
#
# $SMB_BUILD_CTX -	the global SMB_BUILD context
#
# $output -		the resulting output buffer
sub create_makefile_in($)
{
	my $CTX = shift;

	open(MAKEFILE_IN,"> Makefile.in") || die ("Can't open Makefile.in\n");
	print MAKEFILE_IN _prepare_makefile_in($CTX);
	close(MAKEFILE_IN);

	print "config.smb_build.pl: creating Makefile.in\n";
	return;	
}

1;
