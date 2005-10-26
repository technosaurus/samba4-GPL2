###########################################################
### SMB Build System					###
### - create output for Makefile			###
###							###
###  Copyright (C) Stefan (metze) Metzmacher 2004	###
###  Copyright (C) Jelmer Vernooij 2005			###
###  Released under the GNU GPL				###
###########################################################

package smb_build::makefile;
use smb_build::env;
use strict;

use base 'smb_build::env';

sub new($$$$)
{
	my ($myname, $config, $CTX, $mkfile) = @_;
	my $self = new smb_build::env($config);
	
	bless($self, $myname);

	$self->{manpages} = [];
	$self->{sbin_progs} = [];
	$self->{bin_progs} = [];
	$self->{static_libs} = [];
	$self->{shared_libs} = [];
	$self->{headers} = [];
	$self->{output} = "";

	$self->{mkfile} = $mkfile;

	$self->output("################################################\n");
	$self->output("# Autogenerated by build/smb_build/makefile.pm #\n");
	$self->output("################################################\n");
	$self->output("\n");

	$self->output("default: all\n\n");

	$self->_prepare_path_vars();
	$self->_prepare_compiler_linker();
	$self->_prepare_hostcc_rule();
	$self->_prepare_std_CC_rule("c","o",'$(PICFLAG)',"Compiling","Rule for std objectfiles");
	$self->_prepare_std_CC_rule("h","h.gch",'$(PICFLAG)',"Precompiling","Rule for precompiled headerfiles");

	$self->_prepare_mk_files();

	$self->_prepare_dummy_MAKEDIR($CTX);

	foreach my $key (values %$CTX) {
		if (defined($key->{OBJ_LIST})) {
			$self->_prepare_obj_list($key->{TYPE}, $key);
			$self->_prepare_cflags($key->{TYPE}, $key);
		}
	}
	
	foreach my $key (values %$CTX) {
		next unless defined $key->{OUTPUT_TYPE};

		$self->MergedObj($key) if $key->{OUTPUT_TYPE} eq "MERGEDOBJ";
		$self->ObjList($key) if $key->{OUTPUT_TYPE} eq "OBJLIST";
		$self->StaticLibrary($key) if $key->{OUTPUT_TYPE} eq "STATIC_LIBRARY";
		$self->SharedLibrary($key) if $key->{OUTPUT_TYPE} eq "SHARED_LIBRARY";
		$self->Binary($key) if $key->{OUTPUT_TYPE} eq "BINARY";
		$self->Manpage($key) if defined($key->{MANPAGE});
		$self->Header($key) if defined($key->{PUBLIC_HEADERS});
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
VPATH = $self->{config}->{srcdir}
srcdir = $self->{config}->{srcdir}
builddir = $self->{config}->{builddir}

BASEDIR = $self->{config}->{prefix}
BINDIR = $self->{config}->{bindir}
SBINDIR = $self->{config}->{sbindir}
datadir = $self->{config}->{datadir}
LIBDIR = $self->{config}->{libdir}
INCLUDEDIR = $self->{config}->{includedir}
CONFIGDIR = $self->{config}->{configdir}
localstatedir = $self->{config}->{localstatedir}
SWATDIR = $self->{config}->{swatdir}
VARDIR = $self->{config}->{localstatedir}
LOGFILEBASE = $self->{config}->{logfilebase}
NCALRPCDIR = $self->{config}->{localstatedir}/ncalrpc
LOCKDIR = $self->{config}->{lockdir}
PIDDIR = $self->{config}->{piddir}
MANDIR = $self->{config}->{mandir}
PRIVATEDIR = $self->{config}->{privatedir}

__EOD__
);
}

sub _prepare_compiler_linker($)
{
	my ($self) = @_;

	$self->output(<< "__EOD__"
SHELL=$self->{config}->{SHELL}

PERL=$self->{config}->{PERL}

CC=$self->{config}->{CC}
CFLAGS=-I\$(srcdir)/include -I\$(srcdir) -I\$(srcdir)/lib -D_SAMBA_BUILD_ -DHAVE_CONFIG_H $self->{config}->{CFLAGS} $self->{config}->{CPPFLAGS}
PICFLAG=$self->{config}->{PICFLAG}
HOSTCC=$self->{config}->{HOSTCC}

CPP=$self->{config}->{CPP}
CPPFLAGS=$self->{config}->{CPPFLAGS}

LD=$self->{config}->{LD}
LD_FLAGS=$self->{config}->{LDFLAGS} 

STLD=$self->{config}->{AR}
STLD_FLAGS=-rc

SHLD=$self->{config}->{CC}
SHLD_FLAGS=$self->{config}->{LDSHFLAGS}
SONAMEFLAG=$self->{config}->{SONAMEFLAG}
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

sub _prepare_dummy_MAKEDIR($$)
{
	my ($self,$ctx) = @_;

	$self->output(<< '__EOD__'
dynconfig.o: dynconfig.c Makefile
	@echo Compiling $*.c
	@$(CC) $(CFLAGS) $(PICFLAG) $(PATH_FLAGS) -c $< -o $@
__EOD__
);
	if ($self->{config}->{BROKEN_CC} eq "yes") {
		$self->output('	-mv `echo $@ | sed \'s%^.*/%%g\'` $@
');
	}
	$self->output("\n");
}

sub _prepare_std_CC_rule($$$$$$)
{
	my ($self,$src,$dst,$flags,$message,$comment) = @_;

	$self->output(<< "__EOD__"
# $comment
.$src.$dst:
	\@echo $message \$\*.$src
	\@\$(CC) `script/cflags.sh \$\@` \$(CFLAGS) $flags -c \$< -o \$\@
__EOD__
);
	if ($self->{config}->{BROKEN_CC} eq "yes") {
		$self->output('	-mv `echo $@ | sed \'s%^.*/%%g\'` $@
');
	}

	$self->output("\n");
}

sub _prepare_hostcc_rule($)
{
	my ($self) = @_;
	
	$self->output(<< "__EOD__"
.c.ho:
	\@echo Compiling \$\*.c with host compiler
	\@\$(HOSTCC) `script/cflags.sh \$\@` \$(CFLAGS) -c \$< -o \$\@
__EOD__
);
	if ($self->{config}->{BROKEN_CC} eq "yes") {
		$self->output('	-mv `echo $@ | sed \'s%^.*/%%g\' -e \'s%\.ho$$%.o%\'` $@
');
	}

	$self->output("\n");
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

sub _prepare_obj_list($$$)
{
	my ($self,$var,$ctx) = @_;

	my $tmplist = array2oneperline($ctx->{OBJ_LIST});
	return if ($tmplist eq "");

	$self->output("$var\_$ctx->{NAME}_OBJS =$tmplist\n");
}

sub _prepare_cflags($$$)
{
	my ($self, $var,$ctx) = @_;

	my $tmplist = array2oneperline($ctx->{CFLAGS});
	return if ($tmplist eq "");

	$self->output("$var\_$ctx->{NAME}_CFLAGS =$tmplist\n");
}

sub SharedLibrary($$)
{
	my ($self,$ctx) = @_;

	push (@{$self->{shared_libs}}, $ctx->{OUTPUT});

	my $tmpdepend = array2oneperline($ctx->{DEPEND_LIST});
	my $tmpshlink = array2oneperline($ctx->{LINK_LIST});
	my $tmpshflag = array2oneperline($ctx->{LINK_FLAGS});

	$self->output(<< "__EOD__"
LIBRARY_$ctx->{NAME}_DEPEND_LIST =$tmpdepend
LIBRARY_$ctx->{NAME}_SHARED_LINK_LIST =$tmpshlink
LIBRARY_$ctx->{NAME}_SHARED_LINK_FLAGS =$tmpshflag
#

$ctx->{TARGET}: \$(LIBRARY_$ctx->{NAME}_DEPEND_LIST) \$(LIBRARY_$ctx->{NAME}_OBJS) bin/.dummy
	\@echo Linking \$\@
	\@\$(SHLD) \$(SHLD_FLAGS) -o \$\@ \\
		\$(LIBRARY_$ctx->{NAME}_SHARED_LINK_FLAGS) \\
		\$(LIBRARY_$ctx->{NAME}_SHARED_LINK_LIST)

__EOD__
);
	if (defined($ctx->{LIBRARY_SONAME})) {
	    $self->output(<< "__EOD__"
# Symlink $ctx->{LIBRARY_SONAME}
bin/$ctx->{LIBRARY_SONAME}: bin/$ctx->{LIBRARY_REALNAME} bin/.dummy
	\@echo Symlink \$\@
	\@ln -sf $ctx->{LIBRARY_REALNAME} \$\@
# Symlink $ctx->{LIBRARY_NAME}
bin/$ctx->{LIBRARY_NAME}: bin/$ctx->{LIBRARY_SONAME} bin/.dummy
	\@echo Symlink \$\@
	\@ln -sf $ctx->{LIBRARY_SONAME} \$\@

__EOD__
);
	}

	$self->output("library_$ctx->{NAME}: basics bin/lib$ctx->{LIBRARY_NAME}\n");
}

sub MergedObj($$)
{
	my ($self,$ctx) = @_;

	return unless $ctx->{TARGET};

	my $tmpdepend = array2oneperline($ctx->{DEPEND_LIST});

	$self->output("$ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST = $tmpdepend\n");

	$self->output("$ctx->{TARGET}: \$($ctx->{TYPE}_$ctx->{NAME}_OBJS)\n");

	$self->output("\t\@echo \"Pre-Linking $ctx->{TYPE} $ctx->{NAME}\"\n");
	$self->output("\t@\$(LD) -r \$($ctx->{TYPE}_$ctx->{NAME}_OBJS) -o $ctx->{TARGET}\n");
	$self->output("\n");
}

sub ObjList($$)
{
	my ($self,$ctx) = @_;
	my $tmpdepend = array2oneperline($ctx->{DEPEND_LIST});

	return unless $ctx->{TARGET};

	$self->output("$ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST = $tmpdepend\n");
	$self->output("$ctx->{TARGET}: ");
	$self->output("\$($ctx->{TYPE}_$ctx->{NAME}_DEPEND_LIST) \$($ctx->{TYPE}_$ctx->{NAME}_OBJS)\n");
	$self->output("\t\@touch $ctx->{TARGET}\n");
}

sub StaticLibrary($$)
{
	my ($self,$ctx) = @_;

	push (@{$self->{static_libs}}, $ctx->{OUTPUT});

	my $tmpdepend = array2oneperline($ctx->{DEPEND_LIST});
	my $tmpstlink = array2oneperline($ctx->{LINK_LIST});
	my $tmpstflag = array2oneperline($ctx->{LINK_FLAGS});

	$self->output(<< "__EOD__"
LIBRARY_$ctx->{NAME}_DEPEND_LIST =$tmpdepend
#
LIBRARY_$ctx->{NAME}_STATIC_LINK_LIST =$tmpstlink
#
$ctx->{TARGET}: \$(LIBRARY_$ctx->{NAME}_DEPEND_LIST) \$(LIBRARY_$ctx->{NAME}_OBJS) bin/.dummy
	\@echo Linking \$@
	\@\$(STLD) \$(STLD_FLAGS) \$@ \\
		\$(LIBRARY_$ctx->{NAME}_STATIC_LINK_LIST)

library_$ctx->{NAME}: basics $ctx->{TARGET}

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

	unless (defined($ctx->{INSTALLDIR})) {
	} elsif ($ctx->{INSTALLDIR} eq "SBINDIR") {
		push (@{$self->{sbin_progs}}, $ctx->{TARGET});
	} elsif ($ctx->{INSTALLDIR} eq "BINDIR") {
		push (@{$self->{bin_progs}}, $ctx->{TARGET});
	}

	my $tmpdepend = array2oneperline($ctx->{DEPEND_LIST});
	my $tmplink = array2oneperline($ctx->{LINK_LIST});
	my $tmpflag = array2oneperline($ctx->{LINK_FLAGS});

	$self->output(<< "__EOD__"
#
BINARY_$ctx->{NAME}_DEPEND_LIST =$tmpdepend
BINARY_$ctx->{NAME}_LINK_LIST =$tmplink
BINARY_$ctx->{NAME}_LINK_FLAGS =$tmpflag
#
bin/$ctx->{BINARY}: bin/.dummy \$(BINARY_$ctx->{NAME}_DEPEND_LIST) \$(BINARY_$ctx->{NAME}_OBJS)
	\@echo Linking \$\@
	\@\$(CC) \$(LD_FLAGS) -o \$\@ \\
		\$\(BINARY_$ctx->{NAME}_LINK_FLAGS) \\
		\$\(BINARY_$ctx->{NAME}_LINK_LIST) \\
		\$\(BINARY_$ctx->{NAME}_LINK_FLAGS)
binary_$ctx->{BINARY}: basics bin/$ctx->{BINARY}

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

sub write($$)
{
	my ($self,$file) = @_;

	$self->output("MANPAGES = ".array2oneperline($self->{manpages})."\n");
	$self->output("BIN_PROGS = " . array2oneperline($self->{bin_progs}) . "\n");
	$self->output("SBIN_PROGS = " . array2oneperline($self->{sbin_progs}) . "\n");
	$self->output("STATIC_LIBS = " . array2oneperline($self->{static_libs}) . "\n");
	$self->output("SHARED_LIBS = " . array2oneperline($self->{shared_libs}) . "\n");
	$self->output("PUBLIC_HEADERS = " . array2oneperline($self->{headers}) . "\n");

	if ($self->{developer}) {
		$self->output(<<__EOD__
#-include \$(_ALL_OBJS_OBJS:.o=.d)
IDL_FILES = \$(wildcard librpc/idl/*.idl)
\$(patsubst librpc/idl/%.idl,librpc/gen_ndr/ndr_%.c,\$(IDL_FILES)) \\
\$(patsubst librpc/idl/%.idl,librpc/gen_ndr/ndr_\%_c.c,\$(IDL_FILES)) \\
\$(patsubst librpc/idl/%.idl,librpc/gen_ndr/ndr_%.h,\$(IDL_FILES)): idl
__EOD__
);
	}

	$self->output($self->{mkfile});

	open(MAKEFILE,">$file") || die ("Can't open $file\n");
	print MAKEFILE $self->{output};
	close(MAKEFILE);

	print "build/smb_build/main.pl: creating $file\n";
	return;	
}

1;
