# Samba Build System
# - create output for Makefile
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Copyright (C) Jelmer Vernooij 2005
#  Released under the GNU GPL

package smb_build::makefile;
use smb_build::env;
use strict;

use base 'smb_build::env';
use Cwd 'abs_path';

sub new($$$)
{
	my ($myname, $config, $mkfile) = @_;
	my $self = new smb_build::env($config);
	
	bless($self, $myname);

	$self->{manpages} = [];
	$self->{sbin_progs} = [];
	$self->{bin_progs} = [];
	$self->{torture_progs} = [];
	$self->{static_libs} = [];
	$self->{shared_libs} = [];
	$self->{installable_shared_libs} = [];
	$self->{headers} = [];
	$self->{shared_modules} = [];
	$self->{plugins} = [];
	$self->{install_plugins} = "";
	$self->{uninstall_plugins} = "";
	$self->{pc_files} = [];
	$self->{proto_headers} = [];
	$self->{output} = "";

	$self->{mkfile} = $mkfile;

	$self->output("#!gmake\n");
	$self->output("################################################\n");
	$self->output("# Autogenerated by build/smb_build/makefile.pm #\n");
	$self->output("################################################\n");
	$self->output("\n");

	$self->output("default: all\n\n");

	$self->_prepare_path_vars();
	$self->_prepare_suffix_rules();
	$self->_prepare_compiler_linker();

	if (!$self->{automatic_deps}) {
		$self->output("ALL_PREDEP = proto\n");
		$self->output(".NOTPARALLEL:\n");
	}

	return $self;
}

sub output($$)
{
	my ($self, $text) = @_;

	$self->{output} .= $text;
}

sub _prepare_path_vars($)
{
	my ($self) = @_;

	$self->output(<< "__EOD__"
prefix = $self->{config}->{prefix}
exec_prefix = $self->{config}->{exec_prefix}
selftest_prefix = $self->{config}->{selftest_prefix}

builddir = $self->{config}->{builddir}
srcdir = $self->{config}->{srcdir}
datarootdir = $self->{config}->{datarootdir}

VPATH = \$(builddir):\$(srcdir):heimdal_build:heimdal/lib/asn1:heimdal/lib/krb5:heimdal/lib/gssapi:heimdal/lib/hdb:heimdal/lib/roken:heimdal/lib/des

BASEDIR = $self->{config}->{prefix}
BINDIR = $self->{config}->{bindir}
SBINDIR = $self->{config}->{sbindir}
LIBDIR = $self->{config}->{libdir}
TORTUREDIR = $self->{config}->{libdir}/torture
MODULESDIR = $self->{config}->{modulesdir}
INCLUDEDIR = $self->{config}->{includedir}
CONFIGDIR = $self->{config}->{sysconfdir}
DATADIR = $self->{config}->{datadir}
WEBAPPSDIR = \$(DATADIR)/webapps
SERVICESDIR = \$(DATADIR)/services
JSDIR = \$(DATADIR)/js
SETUPDIR = \$(DATADIR)/setup
VARDIR = $self->{config}->{localstatedir}
LOGFILEBASE = $self->{config}->{logfilebase}
NCALRPCDIR = $self->{config}->{localstatedir}/ncalrpc
LOCKDIR = $self->{config}->{lockdir}
PIDDIR = $self->{config}->{piddir}
MANDIR = $self->{config}->{mandir}
PRIVATEDIR = $self->{config}->{privatedir}
WINBINDD_SOCKET_DIR = $self->{config}->{winbindd_socket_dir}

__EOD__
);
}

sub _prepare_suffix_rules($)
{
	my ($self) = @_;
	my $first_prereq = '$*.c';

	if ($self->{config}->{GNU_MAKE} eq 'yes') {
		$first_prereq = '$<';
	}

	$self->output(<< "__EOD__"
# Dependencies command
DEPENDS = \$(CC) -M -MG -MP -MT \$(<:.c=.o) -MT \$@ \\
    `\$(PERL) \$(srcdir)/script/cflags.pl \$@` \\
    \$(CFLAGS) $first_prereq-o \$@
# Dependencies for host objects
HDEPENDS = \$(CC) -M -MG -MP -MT \$(<:.c=.ho) -MT \$@ \\
    `\$(PERL) \$(srcdir)/script/cflags.pl \$@` \\
    \$(HOSTCC_CFLAGS) $first_prereq -o \$@
# Dependencies for precompiled headers
PCHDEPENDS = \$(CC) -M -MG -MT include/includes.h.gch -MT \$@ \\
    \$(CFLAGS) $first_prereq -o \$@

# \$< is broken in older BSD versions:
# when \$@ is foo/bar.o, \$< could be torture/foo/bar.c
# if it also exists. So better use \$* which is foo/bar
# and append .c manually to get foo/bar.c
#
# If we have GNU Make, it is safe to use \$<, which also lets
# building with \$srcdir != \$builddir work.

# Run a static analysis checker
CHECK = \$(CC_CHECKER) `\$(PERL) \$(srcdir)/script/cflags.pl \$@` \\
    \$(CFLAGS) \$(PICFLAG) -c $first_prereq -o \$@

# Run the configured compiler
COMPILE = \$(CC) `\$(PERL) \$(srcdir)/script/cflags.pl \$@` \\
    \$(CFLAGS) \$(PICFLAG) -c $first_prereq -o \$@

# Run the compiler for the build host
HCOMPILE = \$(HOSTCC) `\$(PERL) \$(srcdir)/script/cflags.pl \$@` \\
    \$(HOSTCC_CFLAGS) -c $first_prereq -o \$@

# Precompile headers
PCHCOMPILE = @\$(CC) -Ilib/replace \\
    `\$(PERL) \$(srcdir)/script/cflags.pl \$@` \\
    \$(CFLAGS) \$(PICFLAG) -c $first_prereq -o \$@

__EOD__
);
}

sub _prepare_compiler_linker($)
{
	my ($self) = @_;

	my $builddir_headers = "";
	my $libdir;
	my $extra_link_flags = "";

	if ($self->{config}->{USESHARED} eq "true") {
		$libdir = "\$(builddir)/bin/shared";
		$extra_link_flags = "-Wl,-rpath-link,\$(builddir)/bin/shared";
	} else {
		$libdir = "\$(builddir)/bin/static";
	}
	
	if (!(abs_path($self->{config}->{srcdir}) eq abs_path($self->{config}->{builddir}))) {
	    $builddir_headers= "-I\$(builddir)/include -I\$(builddir) -I\$(builddir)/lib ";
	}

	$self->output(<< "__EOD__"
SHELL=$self->{config}->{SHELL}

PERL=$self->{config}->{PERL}

CPP=$self->{config}->{CPP}
CPPFLAGS=$builddir_headers-I\$(srcdir)/include -I\$(srcdir) -I\$(srcdir)/lib -I\$(srcdir)/lib/replace -D_SAMBA_BUILD_=4 -DHAVE_CONFIG_H $self->{config}->{CPPFLAGS}

CC=$self->{config}->{CC}
CFLAGS=$self->{config}->{CFLAGS} \$(CPPFLAGS)
PICFLAG=$self->{config}->{PICFLAG}

HOSTCC=$self->{config}->{HOSTCC}
HOSTCC_CFLAGS=-D_SAMBA_HOSTCC_ $self->{config}->{CFLAGS} \$(CPPFLAGS)

INSTALL_LINK_FLAGS=$extra_link_flags

LD=$self->{config}->{LD} 
LDFLAGS=$self->{config}->{LDFLAGS} -L$libdir

HOSTLD=$self->{config}->{HOSTLD}
# It's possible that we ought to have HOSTLD_LDFLAGS as well

STLD=$self->{config}->{STLD}
STLD_FLAGS=$self->{config}->{STLD_FLAGS}

SHLD=$self->{config}->{SHLD}
SHLD_FLAGS=$self->{config}->{SHLD_FLAGS} -L\$(builddir)/bin/shared
SHLD_UNDEF_FLAGS=$self->{config}->{SHLD_UNDEF_FLAGS}
SHLIBEXT=$self->{config}->{SHLIBEXT}

XSLTPROC=$self->{config}->{XSLTPROC}

LEX=$self->{config}->{LEX}
YACC=$self->{config}->{YACC}
YAPP=$self->{config}->{YAPP}

GCOV=$self->{config}->{GCOV}

DEFAULT_TEST_OPTIONS=$self->{config}->{DEFAULT_TEST_OPTIONS}

__EOD__
);
}

sub _prepare_mk_files($)
{
	my $self = shift;
	my @tmp = ();

	foreach (@smb_build::config_mk::parsed_files) {
		s/ .*$//g;
		push (@tmp, $_);
	}

	$self->output("MK_FILES = " . array2oneperline(\@tmp) . "\n");
}

sub array2oneperline($)
{
	my $array = shift;
	my $output = "";

	foreach (@$array) {
		next unless defined($_);

		$output .= " \\\n\t\t$_";
	}

	return $output;
}

sub _prepare_list($$$)
{
	my ($self,$ctx,$var) = @_;

	my $tmplist = array2oneperline($ctx->{$var});
	return if ($tmplist eq "");

	$self->output("$ctx->{TYPE}\_$ctx->{NAME}_$var =$tmplist\n");
}

sub Integrated($$)
{
	my ($self,$ctx) = @_;

	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "FULL_OBJ_LIST");
	$self->_prepare_list($ctx, "LINK_FLAGS");
}

sub SharedLibrary($$)
{
	my ($self,$ctx) = @_;

	my $init_obj = "";
	my $has_static_lib = 0;
	
	if ($ctx->{TYPE} eq "LIBRARY") {
		push (@{$self->{shared_libs}}, "$ctx->{SHAREDDIR}/$ctx->{LIBRARY_REALNAME}") if (defined($ctx->{SO_VERSION}));
		push (@{$self->{installable_shared_libs}}, "$ctx->{SHAREDDIR}/$ctx->{LIBRARY_REALNAME}") if (defined($ctx->{SO_VERSION}));
	} elsif ($ctx->{TYPE} eq "MODULE") {
		push (@{$self->{shared_modules}}, "$ctx->{TARGET_SHARED_LIBRARY}");
		push (@{$self->{plugins}}, "$ctx->{SHAREDDIR}/$ctx->{LIBRARY_REALNAME}");

		$self->{install_plugins} .= "\t\@echo Installing $ctx->{SHAREDDIR}/$ctx->{LIBRARY_REALNAME} as \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$ctx->{LIBRARY_REALNAME}\n";
		$self->{install_plugins} .= "\t\@mkdir -p \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/\n";
		$self->{install_plugins} .= "\t\@cp $ctx->{SHAREDDIR}/$ctx->{LIBRARY_REALNAME} \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$ctx->{LIBRARY_REALNAME}\n";
		$self->{uninstall_plugins} .= "\t\@echo Uninstalling \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$ctx->{LIBRARY_REALNAME}\n";
		$self->{uninstall_plugins} .= "\t\@-rm \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$ctx->{LIBRARY_REALNAME}\n";
		if (defined($ctx->{ALIASES})) {
			foreach (@{$ctx->{ALIASES}}) {
				$self->{install_plugins} .= "\t\@rm -f \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$_.\$(SHLIBEXT)\n";
				$self->{install_plugins} .= "\t\@ln -fs $ctx->{LIBRARY_REALNAME} \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$_.\$(SHLIBEXT)\n";
				$self->{uninstall_plugins} .= "\t\@-rm \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$_.\$(SHLIBEXT)\n";
			}
		}
	}

	$has_static_lib = 1 if grep(/STATIC_LIBRARY/, @{$ctx->{OUTPUT_TYPE}});

	if (not $has_static_lib) {
		$self->output("$ctx->{TYPE}_$ctx->{NAME}_OUTPUT = $ctx->{OUTPUT}\n");
		$self->_prepare_list($ctx, "OBJ_LIST");
		$self->_prepare_list($ctx, "FULL_OBJ_LIST");
	}
	$self->_prepare_list($ctx, "DEPEND_LIST");
	$self->_prepare_list($ctx, "LINK_FLAGS");

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_FULL_OBJ_LIST)");

	my $extraflags = "";
	if ($ctx->{TYPE} eq "MODULE" and defined($ctx->{INIT_FUNCTION})) {
		my $init_fn = $ctx->{INIT_FUNCTION_TYPE};
		$init_fn =~ s/\(\*\)/init_module/;
		my $proto_fn = $ctx->{INIT_FUNCTION_TYPE};
		$proto_fn =~ s/\(\*\)/$ctx->{INIT_FUNCTION}/;
		$extraflags = "\$(SHLD_UNDEF_FLAGS)";

		$self->output(<< "__EOD__"
bin/$ctx->{NAME}_init_module.c:
	\@echo Creating \$\@
	\@echo \"#include \\\"includes.h\\\"\" > \$\@
	\@echo \"$proto_fn;\" >> \$\@
	\@echo -e \"_PUBLIC_ $init_fn \\n{\\n\\treturn $ctx->{INIT_FUNCTION}();\\n}\\n\" >> \$\@
__EOD__
);
		$init_obj = "bin/$ctx->{NAME}_init_module.o";
	}

	my $soarg = "";
	my $lns = "";
	if ($self->{config}->{SONAMEFLAG} ne "" and defined($ctx->{LIBRARY_SONAME})) {
		$soarg = "$self->{config}->{SONAMEFLAG}$ctx->{LIBRARY_SONAME} ";
		if ($ctx->{LIBRARY_REALNAME} ne $ctx->{LIBRARY_SONAME}) {
			$lns .= "\n\t\@rm -f $ctx->{SHAREDDIR}/$ctx->{LIBRARY_SONAME}";
			$lns .= "\n\t\@ln -fs $ctx->{LIBRARY_REALNAME} $ctx->{SHAREDDIR}/$ctx->{LIBRARY_SONAME}";
		}
	}

	if ($self->{config}->{SONAMEFLAG} ne "" and 
		defined($ctx->{LIBRARY_SONAME}) and 
		$ctx->{LIBRARY_REALNAME} ne $ctx->{LIBRARY_SONAME}) {
		$lns .= "\n\t\@rm -f $ctx->{SHAREDDIR}/$ctx->{LIBRARY_SONAME}";
		$lns .= "\n\t\@ln -fs $ctx->{LIBRARY_REALNAME} $ctx->{SHAREDDIR}/$ctx->{LIBRARY_SONAME}";
	}

	if (defined($ctx->{LIBRARY_SONAME})) {
		$lns .= "\n\t\@rm -f $ctx->{SHAREDDIR}/$ctx->{LIBRARY_DEBUGNAME}";
		$lns .= "\n\t\@ln -fs $ctx->{LIBRARY_REALNAME} $ctx->{SHAREDDIR}/$ctx->{LIBRARY_DEBUGNAME}";
	}

	$self->output(<< "__EOD__"
#

$ctx->{SHAREDDIR}/$ctx->{LIBRARY_REALNAME}: \$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_FULL_OBJ_LIST) $init_obj
	\@echo Linking \$\@
	\@mkdir -p $ctx->{SHAREDDIR}
	\@\$(SHLD) \$(SHLD_FLAGS) -o \$\@ \$(INSTALL_LINK_FLAGS) \\
		\$($ctx->{TYPE}_$ctx->{NAME}\_FULL_OBJ_LIST) \\
		\$($ctx->{TYPE}_$ctx->{NAME}_LINK_FLAGS) $extraflags \\
		 $soarg \\
		$init_obj $lns
__EOD__
);

	if (defined($ctx->{ALIASES})) {
		foreach (@{$ctx->{ALIASES}}) {
			$self->output("\t\@rm -f $ctx->{SHAREDDIR}/$_.\$(SHLIBEXT)\n");
			$self->output("\t\@ln -fs $ctx->{LIBRARY_REALNAME} $ctx->{SHAREDDIR}/$_.\$(SHLIBEXT)\n");
		}
	}
	$self->output("\n");
}

sub StaticLibrary($$)
{
	my ($self,$ctx) = @_;

	return unless (defined($ctx->{OBJ_FILES}));

	push (@{$self->{static_libs}}, $ctx->{TARGET_STATIC_LIBRARY}) if ($ctx->{TYPE} eq "LIBRARY");

	$self->output("$ctx->{TYPE}_$ctx->{NAME}_OUTPUT = $ctx->{OUTPUT}\n");
	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "FULL_OBJ_LIST");

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_FULL_OBJ_LIST)");

	$self->output(<< "__EOD__"
#
$ctx->{TARGET_STATIC_LIBRARY}: \$($ctx->{TYPE}_$ctx->{NAME}_FULL_OBJ_LIST)
	\@echo Linking \$@
	\@rm -f \$@
	\@mkdir -p $ctx->{STATICDIR}
	\@\$(STLD) \$(STLD_FLAGS) \$@ \$($ctx->{TYPE}_$ctx->{NAME}_FULL_OBJ_LIST)

__EOD__
);
}

sub Header($$)
{
	my ($self,$ctx) = @_;

	my $dir = $ctx->{BASEDIR};

	$dir =~ s/^\.\///g;

	foreach (@{$ctx->{PUBLIC_HEADERS}}) {
		push (@{$self->{headers}}, "$dir/$_");
	}
}

sub Binary($$)
{
	my ($self,$ctx) = @_;

	my $installdir;
	my $extradir = "";

	if (defined($ctx->{INSTALLDIR}) && $ctx->{INSTALLDIR} =~ /^TORTUREDIR/) {
		$extradir = "/torture" . substr($ctx->{INSTALLDIR}, length("TORTUREDIR"));
	}
	my $localdir = "bin$extradir";

	$installdir = "bin$extradir";

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_FULL_OBJ_LIST)");
		
	unless (defined($ctx->{INSTALLDIR})) {
	} elsif ($ctx->{INSTALLDIR} eq "SBINDIR") {
		push (@{$self->{sbin_progs}}, "$installdir/$ctx->{BINARY}");
	} elsif ($ctx->{INSTALLDIR} eq "BINDIR") {
		push (@{$self->{bin_progs}}, "$installdir/$ctx->{BINARY}");
	} elsif ($ctx->{INSTALLDIR} =~ /^TORTUREDIR/) {
		push (@{$self->{torture_progs}}, "$installdir/$ctx->{BINARY}");
	}


	push (@{$self->{binaries}}, "$localdir/$ctx->{BINARY}");

	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "FULL_OBJ_LIST");
	$self->_prepare_list($ctx, "DEPEND_LIST");
	$self->_prepare_list($ctx, "LINK_FLAGS");

$self->output(<< "__EOD__"
$installdir/$ctx->{BINARY}: \$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_FULL_OBJ_LIST)
	\@echo Linking \$\@
__EOD__
	);

	if (defined($ctx->{USE_HOSTCC}) && $ctx->{USE_HOSTCC} eq "YES") {
		$self->output(<< "__EOD__"
	\@\$(HOSTLD) \$(LDFLAGS) -o \$\@ \$(INSTALL_LINK_FLAGS) \\
		\$\($ctx->{TYPE}_$ctx->{NAME}_LINK_FLAGS)
__EOD__
		);
	} else {
		$self->output(<< "__EOD__"
	\@\$(LD) \$(LDFLAGS) -o \$\@ \$(INSTALL_LINK_FLAGS) \\
		\$\($ctx->{TYPE}_$ctx->{NAME}_LINK_FLAGS) 

__EOD__
		);
	}
}

sub Manpage($$)
{
	my ($self,$ctx) = @_;

	my $dir = $ctx->{BASEDIR};
	
	$dir =~ s/^\.\///g;

	push (@{$self->{manpages}}, "$dir/$ctx->{MANPAGE}");
}

sub PkgConfig($$$)
{
	my ($self,$ctx,$other) = @_;
	
	my $link_name = $ctx->{NAME};

	$link_name =~ s/^LIB//g;
	$link_name = lc($link_name);

	return if (not defined($ctx->{DESCRIPTION}));

	my $path = "$ctx->{BASEDIR}/$link_name.pc";

	push (@{$self->{pc_files}}, $path);

	my $pubs;
	my $privs;
	my $privlibs;

	if (defined($ctx->{PUBLIC_DEPENDENCIES})) {
		foreach (@{$ctx->{PUBLIC_DEPENDENCIES}}) {
			next if ($other->{$_}->{ENABLE} eq "NO");
			if ($other->{$_}->{TYPE} eq "LIBRARY") {
				s/^LIB//g;
				$_ = lc($_);

				$pubs .= "$_ ";
			} else {
				s/^LIB//g;
				$_ = lc($_);

				$privlibs .= "-l$_ ";
			}
		}
	}

	if (defined($ctx->{PRIVATE_DEPENDENCIES})) {
		foreach (@{$ctx->{PRIVATE_DEPENDENCIES}}) {
			next if ($other->{$_}->{ENABLE} eq "NO");
			if ($other->{$_}->{TYPE} eq "LIBRARY") {
				s/^LIB//g;
				$_ = lc($_);

				$privs .= "$_ ";
			} else {
				s/^LIB//g;
				$_ = lc($_);

				$privlibs .= "-l$_ ";
			}
		}
	}

	smb_build::env::PkgConfig($self,
		$path,
		$link_name,
		"-L\${libdir} -l$link_name",
		$privlibs,
		"",
		"$ctx->{VERSION}",
		$ctx->{DESCRIPTION},
		defined($ctx->{INIT_FUNCTIONS}),
		$pubs,
		"",
		[
			"prefix=$self->{config}->{prefix}",
			"exec_prefix=$self->{config}->{exec_prefix}",
			"libdir=$self->{config}->{libdir}",
			"includedir=$self->{config}->{includedir}"
		]
	); 
	my $abs_srcdir = abs_path($self->{config}->{srcdir});
	smb_build::env::PkgConfig($self,
		"bin/pkgconfig/$link_name-uninstalled.pc",
		$link_name,
		"-Lbin/shared -Lbin/static -l$link_name",
		$privlibs,
		join(' ', 
			"-I$abs_srcdir",
			"-I$abs_srcdir/include",
			"-I$abs_srcdir/lib",
			"-I$abs_srcdir/lib/replace"),
		"$ctx->{VERSION}",
		$ctx->{DESCRIPTION},
		defined($ctx->{INIT_FUNCTIONS}),
		$pubs,
		$privs,
		[
			"prefix=bin/",
			"includedir=$ctx->{BASEDIR}"
		]
	); 
}

sub ProtoHeader($$)
{
	my ($self,$ctx) = @_;

	my $dir = $ctx->{BASEDIR};

	$dir =~ s/^\.\///g;

	my $target = "";

	my $comment = "Creating ";
	if (defined($ctx->{PRIVATE_PROTO_HEADER})) {
		$target.= "$dir/$ctx->{PRIVATE_PROTO_HEADER}";
		$comment.= "$dir/$ctx->{PRIVATE_PROTO_HEADER}";
		if (defined($ctx->{PUBLIC_PROTO_HEADER})) {
			$comment .= " and ";
			$target.= " ";
		}
		push (@{$self->{proto_headers}}, "$dir/$ctx->{PRIVATE_PROTO_HEADER}");
	} else {
		$ctx->{PRIVATE_PROTO_HEADER} = $ctx->{PUBLIC_PROTO_HEADER};
	}
	
	if (defined($ctx->{PUBLIC_PROTO_HEADER})) {
		$comment.= "$dir/$ctx->{PUBLIC_PROTO_HEADER}";
		$target .= "$dir/$ctx->{PUBLIC_PROTO_HEADER}";
		push (@{$self->{proto_headers}}, "$dir/$ctx->{PUBLIC_PROTO_HEADER}");
	} else {
		$ctx->{PUBLIC_PROTO_HEADER} = $ctx->{PRIVATE_PROTO_HEADER};
	}	

	$self->output("$dir/$ctx->{PUBLIC_PROTO_HEADER}: $ctx->{MK_FILE} \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST:.o=.c) \$(srcdir)/script/mkproto.pl\n");
	$self->output("\t\@echo \"$comment\"\n");

	$self->output("\t\@\$(PERL) \$(srcdir)/script/mkproto.pl --srcdir=\$(srcdir) --builddir=\$(builddir) --private=$dir/$ctx->{PRIVATE_PROTO_HEADER} --public=$dir/$ctx->{PUBLIC_PROTO_HEADER} \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)\n\n");
}

sub write($$)
{
	my ($self,$file) = @_;

	$self->output("MANPAGES = ".array2oneperline($self->{manpages})."\n");
	$self->output("BIN_PROGS = " . array2oneperline($self->{bin_progs}) . "\n");
	$self->output("SBIN_PROGS = " . array2oneperline($self->{sbin_progs}) . "\n");
	$self->output("TORTURE_PROGS = " . array2oneperline($self->{torture_progs}) . "\n");
	$self->output("BINARIES = " . array2oneperline($self->{binaries}) . "\n");
	$self->output("STATIC_LIBS = " . array2oneperline($self->{static_libs}) . "\n");
	$self->output("SHARED_LIBS = " . array2oneperline($self->{shared_libs}) . "\n");
	$self->output("INSTALLABLE_SHARED_LIBS = " . array2oneperline($self->{installable_shared_libs}) . "\n");
	$self->output("PUBLIC_HEADERS = " . array2oneperline($self->{headers}) . "\n");
	$self->output("PC_FILES = " . array2oneperline($self->{pc_files}) . "\n");
	$self->output("ALL_OBJS = " . array2oneperline($self->{all_objs}) . "\n");
	$self->output("PROTO_HEADERS = " . array2oneperline($self->{proto_headers}) .  "\n");
	$self->output("SHARED_MODULES = " . array2oneperline($self->{shared_modules}) . "\n");
	$self->output("PLUGINS = " . array2oneperline($self->{plugins}) . "\n");

	$self->output("\ninstallplugins: \$(PLUGINS)\n".$self->{install_plugins}."\n");
	$self->output("\nuninstallplugins:\n".$self->{uninstall_plugins}."\n");

	$self->_prepare_mk_files();

	$self->output($self->{mkfile});

	if ($self->{automatic_deps}) {
		$self->output("
ifneq (\$(MAKECMDGOALS),clean)
ifneq (\$(MAKECMDGOALS),distclean)
ifneq (\$(MAKECMDGOALS),realdistclean)
-include \$(DEP_FILES)
endif
endif
endif
");
	} else {
		$self->output("include \$(srcdir)/static_deps.mk\n");
	}

	open(MAKEFILE,">$file") || die ("Can't open $file\n");
	print MAKEFILE $self->{output};
	close(MAKEFILE);

	print __FILE__.": creating $file\n";
}

1;
