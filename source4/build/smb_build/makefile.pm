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

sub new($$$)
{
	my ($myname, $config, $mkfile) = @_;
	my $self = new smb_build::env($config);
	
	bless($self, $myname);

	$self->{manpages} = [];
	$self->{sbin_progs} = [];
	$self->{bin_progs} = [];
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
	$self->_prepare_compiler_linker();

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
VPATH = $self->{config}->{srcdir}
srcdir = $self->{config}->{srcdir}
builddir = $self->{config}->{builddir}

BASEDIR = $self->{config}->{prefix}
BINDIR = $self->{config}->{bindir}
SBINDIR = $self->{config}->{sbindir}
LIBDIR = $self->{config}->{libdir}
MODULESDIR = $self->{config}->{modulesdir}
INCLUDEDIR = $self->{config}->{includedir}
CONFIGDIR = $self->{config}->{sysconfdir}
DATADIR = $self->{config}->{datadir}
SWATDIR = $self->{config}->{datadir}/swat
JSDIR = $self->{config}->{datadir}/js
SETUPDIR = $self->{config}->{datadir}/setup
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

sub _prepare_compiler_linker($)
{
	my ($self) = @_;

	my $devld_local = "";
	my $devld_install = "";

	$self->{duplicate_build} = 0;
	if ($self->{config}->{LIBRARY_OUTPUT_TYPE} eq "SHARED_LIBRARY") {
		if ($self->{developer}) {
			$self->{duplicate_build} = 1;
			$devld_local = " -Wl,-rpath,\$(builddir)/bin";
		}
		$devld_install = " -Wl,-rpath-link,\$(builddir)/bin";
	}

	$self->output(<< "__EOD__"
SHELL=$self->{config}->{SHELL}

PERL=$self->{config}->{PERL}

CPP=$self->{config}->{CPP}
CPPFLAGS=$self->{config}->{CPPFLAGS}

CC=$self->{config}->{CC}
CFLAGS=-I\$(srcdir)/include -I\$(srcdir) -I\$(srcdir)/lib -D_SAMBA_BUILD_ -DHAVE_CONFIG_H $self->{config}->{CFLAGS} \$(CPPFLAGS)
PICFLAG=$self->{config}->{PICFLAG}
HOSTCC=$self->{config}->{HOSTCC}

LOCAL_LINK_FLAGS=$devld_local
INSTALL_LINK_FLAGS=$devld_install

LD=$self->{config}->{LD} 
LDFLAGS=$self->{config}->{LDFLAGS} -L\$(builddir)/bin

STLD=$self->{config}->{AR}
STLD_FLAGS=-rc -L\$(builddir)/bin

SHLD=$self->{config}->{CC}
SHLD_FLAGS=$self->{config}->{LDSHFLAGS} -L\$(builddir)/bin
SHLIBEXT=$self->{config}->{SHLIBEXT}

XSLTPROC=$self->{config}->{XSLTPROC}

LEX=$self->{config}->{LEX}
YACC=$self->{config}->{YACC}
YAPP=$self->{config}->{YAPP}
PIDL_ARGS=$self->{config}->{PIDL_ARGS}

GCOV=$self->{config}->{GCOV}

DEFAULT_TEST_TARGET=$self->{config}->{DEFAULT_TEST_TARGET}

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

sub SharedLibrary($$)
{
	my ($self,$ctx) = @_;

	my $installdir;
	my $init_obj = "";
	
	if ($self->{duplicate_build}) {
		$installdir = $ctx->{RELEASEDIR};
	} else {
		$installdir = $ctx->{DEBUGDIR};
	}

	if ($ctx->{TYPE} eq "LIBRARY") {
		push (@{$self->{shared_libs}}, "$ctx->{DEBUGDIR}/$ctx->{LIBRARY_REALNAME}");
		push (@{$self->{installable_shared_libs}}, "$installdir/$ctx->{LIBRARY_REALNAME}");
	} elsif ($ctx->{TYPE} eq "MODULE") {
		push (@{$self->{shared_modules}}, "$ctx->{DEBUGDIR}/$ctx->{LIBRARY_REALNAME}");
		push (@{$self->{plugins}}, "$installdir/$ctx->{LIBRARY_REALNAME}");

		my $fixedname = $ctx->{NAME};

		$fixedname =~ s/^$ctx->{SUBSYSTEM}_//g;

		$self->{install_plugins} .= "\t\@echo Installing $installdir/$ctx->{LIBRARY_REALNAME} as \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$fixedname.\$(SHLIBEXT)\n";
		$self->{install_plugins} .= "\t\@mkdir -p \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/\n";
		$self->{install_plugins} .= "\t\@cp $installdir/$ctx->{LIBRARY_REALNAME} \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$fixedname.\$(SHLIBEXT)\n";
		$self->{uninstall_plugins} .= "\t\@echo Uninstalling \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$fixedname.\$(SHLIBEXT)\n";
		$self->{uninstall_plugins} .= "\t\@-rm \$(DESTDIR)\$(MODULESDIR)/$ctx->{SUBSYSTEM}/$fixedname.\$(SHLIBEXT)\n";
	}

	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "CFLAGS");
	$self->_prepare_list($ctx, "DEPEND_LIST");
	$self->_prepare_list($ctx, "LINK_LIST");
	$self->_prepare_list($ctx, "LINK_FLAGS");

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)");

	if ($ctx->{TYPE} eq "MODULE" and defined($ctx->{INIT_FUNCTION})) {
		my $init_fn = $ctx->{INIT_FUNCTION_TYPE};
		$init_fn =~ s/\(\*\)/init_module/;
		my $proto_fn = $ctx->{INIT_FUNCTION_TYPE};
		$proto_fn =~ s/\(\*\)/$ctx->{INIT_FUNCTION}/;

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
	my $soargdebug = "";
	if ($self->{config}->{SONAMEFLAG} ne "" and 
		defined($ctx->{LIBRARY_SONAME})) {
		$soarg = "$self->{config}->{SONAMEFLAG}$ctx->{LIBRARY_SONAME} ";
	}

	if ($self->{config}->{SONAMEFLAG} ne "") {
		$soargdebug = "$self->{config}->{SONAMEFLAG}$ctx->{LIBRARY_REALNAME} ";
	}

	if ($self->{duplicate_build}) {
		$self->output(<< "__EOD__"
#

$ctx->{TARGET}: \$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST) $init_obj
	\@echo Linking \$\@
	\@mkdir -p $ctx->{DEBUGDIR}
	\@\$(SHLD) \$(SHLD_FLAGS) -o \$\@ \$(LOCAL_LINK_FLAGS) \\
		\$($ctx->{TYPE}_$ctx->{NAME}_LINK_FLAGS) $soargdebug \\
		$init_obj \$($ctx->{TYPE}_$ctx->{NAME}_LINK_LIST)

__EOD__
);
	}

	$self->output(<< "__EOD__"
#

$installdir/$ctx->{LIBRARY_REALNAME}: \$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST) $init_obj
	\@echo Linking \$\@
	\@\$(SHLD) \$(SHLD_FLAGS) -o \$\@ \\
		\$($ctx->{TYPE}_$ctx->{NAME}_LINK_FLAGS) $soarg \\
		$init_obj \$($ctx->{TYPE}_$ctx->{NAME}_LINK_LIST)

__EOD__
);
}

sub MergedObj($$)
{
	my ($self,$ctx) = @_;

	return unless $ctx->{TARGET};

	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "CFLAGS");
	$self->_prepare_list($ctx, "DEPEND_LIST");

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)");
		
	$self->output("$ctx->{TARGET}: \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)\n");

	$self->output("\t\@echo \"Pre-Linking $ctx->{TYPE} $ctx->{NAME}\"\n");
	$self->output("\t@\$(LD) -r \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST) -o $ctx->{TARGET}\n");
	$self->output("\n");
}

sub ObjList($$)
{
	my ($self,$ctx) = @_;

	return unless $ctx->{TARGET};

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)");
		
	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "CFLAGS");
	$self->_prepare_list($ctx, "DEPEND_LIST");
	$self->output("$ctx->{TARGET}: ");
	$self->output("\$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)\n");
	$self->output("\t\@touch $ctx->{TARGET}\n");
}

sub StaticLibrary($$)
{
	my ($self,$ctx) = @_;

	push (@{$self->{static_libs}}, $ctx->{OUTPUT});

	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "CFLAGS");

	$self->_prepare_list($ctx, "DEPEND_LIST");
	$self->_prepare_list($ctx, "LINK_LIST");
	$self->_prepare_list($ctx, "LINK_FLAGS");

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)");
		
	$self->output(<< "__EOD__"
#
$ctx->{TARGET}: \$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST) 
	\@echo Linking \$@
	\@\$(STLD) \$(STLD_FLAGS) \$@ \\
		\$($ctx->{TYPE}_$ctx->{NAME}_LINK_LIST)

__EOD__
);
}

sub Header($$)
{
	my ($self,$ctx) = @_;

	foreach (@{$ctx->{PUBLIC_HEADERS}}) {
		push (@{$self->{headers}}, "$ctx->{BASEDIR}/$_");
	}
}

sub Binary($$)
{
	my ($self,$ctx) = @_;

	my $installdir;
	
	if ($self->{duplicate_build}) {
		$installdir = "bin/install";
	} else {
		$installdir = "bin";
	}

	push(@{$self->{all_objs}}, "\$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)");
		
	unless (defined($ctx->{INSTALLDIR})) {
	} elsif ($ctx->{INSTALLDIR} eq "SBINDIR") {
		push (@{$self->{sbin_progs}}, "$installdir/$ctx->{BINARY}");
	} elsif ($ctx->{INSTALLDIR} eq "BINDIR") {
		push (@{$self->{bin_progs}}, "$installdir/$ctx->{BINARY}");
	}

	push (@{$self->{binaries}}, "bin/$ctx->{BINARY}");

	$self->_prepare_list($ctx, "OBJ_LIST");
	$self->_prepare_list($ctx, "CFLAGS");
	$self->_prepare_list($ctx, "DEPEND_LIST");
	$self->_prepare_list($ctx, "LINK_LIST");
	$self->_prepare_list($ctx, "LINK_FLAGS");

	if ($self->{duplicate_build}) {
	$self->output(<< "__EOD__"
#
bin/$ctx->{BINARY}: \$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST) 
	\@echo Linking \$\@
	\@\$(CC) \$(LDFLAGS) -o \$\@ \$(LOCAL_LINK_FLAGS) \$(INSTALL_LINK_FLAGS) \\
		\$\($ctx->{TYPE}_$ctx->{NAME}_LINK_LIST) \\
		\$\($ctx->{TYPE}_$ctx->{NAME}_LINK_FLAGS) 

__EOD__
);
	}

$self->output(<< "__EOD__"
$installdir/$ctx->{BINARY}: \$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST) 
	\@echo Linking \$\@
	\@\$(CC) \$(LDFLAGS) -o \$\@ \$(INSTALL_LINK_FLAGS) \\
		\$\($ctx->{TYPE}_$ctx->{NAME}_LINK_LIST) \\
		\$\($ctx->{TYPE}_$ctx->{NAME}_LINK_FLAGS) 

__EOD__
);
}

sub Manpage($$)
{
	my ($self,$ctx) = @_;

	my $dir = $ctx->{BASEDIR};
	
	$dir =~ s/^\.\///g;

	push (@{$self->{manpages}}, "$dir/$ctx->{MANPAGE}");
}

sub PkgConfig($$)
{
	my ($self,$ctx) = @_;
	
	my $link_name = $ctx->{NAME};

	$link_name =~ s/^LIB//g;
	$link_name = lc($link_name);

	if (not defined($ctx->{DESCRIPTION})) {
		warn("$ctx->{NAME} has not DESCRIPTION set, not generating .pc file");
		return;
	}

	my $path = "$ctx->{BASEDIR}/$link_name.pc";

	push (@{$self->{pc_files}}, $path);

	smb_build::env::PkgConfig($self,
		$path,
		$link_name,
		"-l$link_name",
		"",
		"$ctx->{VERSION}",
		$ctx->{DESCRIPTION},
		1
	); 
}

sub ProtoHeader($$)
{
	my ($self,$ctx) = @_;

	my $dir = $ctx->{BASEDIR};

	$dir =~ s/^\.\///g;

	my $comment = "Creating ";
	if (defined($ctx->{PRIVATE_PROTO_HEADER})) {
		$comment.= "$dir/$ctx->{PRIVATE_PROTO_HEADER}";
		if (defined($ctx->{PUBLIC_PROTO_HEADER})) {
			$comment .= " and ";
		}
		push (@{$self->{proto_headers}}, "$dir/$ctx->{PRIVATE_PROTO_HEADER}");
	} else {
		$ctx->{PRIVATE_PROTO_HEADER} = $ctx->{PUBLIC_PROTO_HEADER};
	}
	
	if (defined($ctx->{PUBLIC_PROTO_HEADER})) {
		$comment.= "$dir/$ctx->{PUBLIC_PROTO_HEADER}";
		push (@{$self->{proto_headers}}, "$dir/$ctx->{PUBLIC_PROTO_HEADER}");
	} else {
		$ctx->{PUBLIC_PROTO_HEADER} = $ctx->{PRIVATE_PROTO_HEADER};
	}	

	$self->output("$dir/$ctx->{PUBLIC_PROTO_HEADER}: \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST:.o=.c)\n");
	$self->output("\t\@echo \"$comment\"\n");

	$self->output("\t\@\$(PERL) \$(srcdir)/script/mkproto.pl --private=$dir/$ctx->{PRIVATE_PROTO_HEADER} --public=$dir/$ctx->{PUBLIC_PROTO_HEADER} \$($ctx->{TYPE}_$ctx->{NAME}_OBJ_LIST)\n\n");
}

sub write($$)
{
	my ($self,$file) = @_;

	$self->output("MANPAGES = ".array2oneperline($self->{manpages})."\n");
	$self->output("BIN_PROGS = " . array2oneperline($self->{bin_progs}) . "\n");
	$self->output("SBIN_PROGS = " . array2oneperline($self->{sbin_progs}) . "\n");
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

	# nasty hack to allow running locally
	if ($self->{duplicate_build}) {
		$self->output("bin/libdynconfig.\$(SHLIBEXT): dynconfig-devel.o\n");
		$self->output("bin/libdynconfig.\$(SHLIBEXT): LIBRARY_DYNCONFIG_OBJ_LIST=dynconfig-devel.o\n");
	}

	$self->_prepare_mk_files();

	$self->output($self->{mkfile});

	if ($self->{developer}) {
		$self->output(<<__EOD__

-include \$(DEP_FILES)

__EOD__
);
	}

	open(MAKEFILE,">$file") || die ("Can't open $file\n");
	print MAKEFILE $self->{output};
	close(MAKEFILE);

	print __FILE__.": creating $file\n";
}

1;
