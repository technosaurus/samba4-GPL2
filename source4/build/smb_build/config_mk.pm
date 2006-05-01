# Samba Build System
# - config.mk parsing functions
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Copyright (C) Jelmer Vernooij 2005
#  Released under the GNU GPL
#

package smb_build::config_mk;
use smb_build::input;
use File::Basename;

use strict;

my $section_types = {
	"EXT_LIB" => {
		"LIBS"			=> "list",
		"CFLAGS"		=> "string",
		"CPPFLAGS"		=> "list",
		"LDFLAGS"		=> "list",
		},
	"SUBSYSTEM" => {
		"OBJ_FILES"		=> "list",

		"PRIVATE_DEPENDENCIES"	=> "list",
		"PUBLIC_DEPENDENCIES"	=> "list",

		"ENABLE"		=> "bool",

		"MANPAGE"		=> "string",

		"PUBLIC_PROTO_HEADER"	=> "string",
		"PRIVATE_PROTO_HEADER"	=> "string",

		"PUBLIC_HEADERS"	=> "list",

		"CFLAGS"		=> "string",
		"LDFLAGS"		=> "list",
		"STANDARD_VISIBILITY"	=> "string"
		},
	"MODULE" => {
		"SUBSYSTEM"		=> "string",

		"INIT_FUNCTION"		=> "string",
		"OBJ_FILES"		=> "list",

		"PUBLIC_DEPENDENCIES"	=> "list",
		"PRIVATE_DEPENDENCIES"	=> "list",

		"ALIASES" => "list",

		"ENABLE"		=> "bool",

		"OUTPUT_TYPE"		=> "string",

		"MANPAGE"		=> "string",
		"PRIVATE_PROTO_HEADER"	=> "string",
		"PUBLIC_PROTO_HEADER"	=> "string",


		"PUBLIC_HEADERS"	=> "list",

		"CFLAGS"		=> "string"
		},
	"BINARY" => {
		"OBJ_FILES"		=> "list",

		"PRIVATE_DEPENDENCIES"	=> "list",

		"ENABLE"		=> "bool",

		"MANPAGE"		=> "string",
		"INSTALLDIR"		=> "string",
		"PRIVATE_PROTO_HEADER"	=> "string",
		"PUBLIC_PROTO_HEADER"	=> "string",
		"PUBLIC_HEADERS"	=> "list", 

		"CFLAGS"		=> "string",
		"STANDARD_VISIBILITY"	=> "string"
		},
	"LIBRARY" => {
		"VERSION"		=> "string",
		"SO_VERSION"		=> "string",
		"LIBRARY_REALNAME" => "string",
		
		"INIT_FUNCTION_TYPE"	=> "string",

		"OBJ_FILES"		=> "list",

		"DESCRIPTION"		=> "string",

		"PRIVATE_DEPENDENCIES"	=> "list",
		"PUBLIC_DEPENDENCIES"	=> "list",

		"ENABLE"		=> "bool",

		"MANPAGE"		=> "string",

		"PUBLIC_HEADERS"	=> "list",

		"PUBLIC_PROTO_HEADER"	=> "string",
		"PRIVATE_PROTO_HEADER"	=> "string",

		"CFLAGS"		=> "string",
		"LDFLAGS"		=> "list",
		"STANDARD_VISIBILITY"	=> "string"
		}
};

use vars qw(@parsed_files);

@parsed_files = ();

###########################################################
# The parsing function which parses the file
#
# $result = _parse_config_mk($filename)
#
# $filename -	the path of the config.mk file
#		which should be parsed
sub run_config_mk($$$$)
{
	sub run_config_mk($$$$);
	my ($input, $srcdir, $builddir, $filename) = @_;
	my $result;
	my $linenum = -1;
	my $infragment = 0;
	my $section = "GLOBAL";
	my $makefile = "";

	my $parsing_file = $builddir."/".$filename;

	$ENV{samba_builddir} = $builddir;
	$ENV{samba_srcdir} = $srcdir;
	
	if (!open(CONFIG_MK, $parsing_file)) { 
		$parsing_file = $srcdir."/".$filename;
		open(CONFIG_MK, $parsing_file) or 
		    die("Can't open neither `$builddir."/".$filename' nor `$srcdir/$filename'\n");
	}
	
	push (@parsed_files, $parsing_file);
	
	
	my @lines = <CONFIG_MK>;
	close(CONFIG_MK);

	my $line = "";
	my $prev = "";

	foreach (@lines) {
		$linenum++;

		# lines beginning with '#' are ignored
		next if (/^\#.*$/);
		
		if (/^(.*)\\$/) {
			$prev .= $1;
			next;
		} else {
			$line = "$prev$_";
			$prev = "";
		}

		if ($line =~ /^\[([-a-zA-Z0-9_:]+)\][\t ]*$/) 
		{
			$section = $1;
			$infragment = 0;
			next;
		}

		# include
		if ($line =~ /^include (.*)$/) {
			$makefile .= run_config_mk($input, $srcdir, $builddir, dirname($filename)."/$1");
			next;
		}

		# empty line
		if ($line =~ /^[ \t]*$/) {
			$section = "GLOBAL";
			if ($infragment) { $makefile.="\n"; }
			next;
		}

		# global stuff is considered part of the makefile
		if ($section eq "GLOBAL") {
			if (!$infragment) { $makefile.="\n"; }
			$makefile .= $line;
			$infragment = 1;
			next;
		}

		
		# Assignment
		if ($line =~ /^([a-zA-Z0-9_]+)[\t ]*=(.*)$/) {
			$result->{$section}{$1}{VAL} = $2;
			$result->{$section}{$1}{KEY} = $1;
		
			next;
		}

		die("$srcdir."/".$filename:$linenum: Bad line while parsing $srcdir."/".$filename");
	}

	foreach my $section (keys %{$result}) {
		my ($type, $name) = split(/::/, $section, 2);

		my $sectype = $section_types->{$type};
		if (not defined($sectype)) {
			die($srcdir."/".$filename.":[".$section."] unknown section type \"".$type."\"!");
		}

		$input->{$name}{NAME} = $name;
		$input->{$name}{TYPE} = $type;
		$input->{$name}{MK_FILE} = $srcdir."/".$filename;
		$input->{$name}{BASEDIR} = dirname($filename);

		foreach my $key (values %{$result->{$section}}) {
			$key->{VAL} = smb_build::input::strtrim($key->{VAL});
			my $vartype = $sectype->{$key->{KEY}};
			if (not defined($vartype)) {
				die($srcdir."/".$filename.":[".$section."]: unknown attribute type \"$key->{KEY}\"!");
			}
			if ($vartype eq "string") {
				$input->{$name}{$key->{KEY}} = $key->{VAL};
			} elsif ($vartype eq "list") {
				$input->{$name}{$key->{KEY}} = [smb_build::input::str2array($key->{VAL})];
			} elsif ($vartype eq "bool") {
				if (($key->{VAL} ne "YES") and ($key->{VAL} ne "NO")) {
					die("Invalid value for bool attribute $key->{KEY}: $key->{VAL} in section $section");
				}
				$input->{$name}{$key->{KEY}} = $key->{VAL};
			}
		}
	}

	return $makefile;
}

1;
