###########################################################
### SMB Build System					###
### - config.mk parsing functions			###
###							###
###  Copyright (C) Stefan (metze) Metzmacher 2004	###
###  Released under the GNU GPL				###
###########################################################

package config_mk;
use smb_build::input;

use strict;

my %attribute_types = (
	"NOPROTO" => "bool",
   	"REQUIRED_SUBSYSTEMS" => "list",
	"OUTPUT_TYPE" => "string",
	"INIT_OBJ_FILES" => "list",
	"ADD_OBJ_FILES" => "list",
	"OBJ_FILES" => "list",
	"SUBSYSTEM" => "string",
	"CFLAGS" => "list",
	"CPPFLAGS" => "list",
	"LDFLAGS" => "list",
	"INSTALLDIR" => "string",
	"LIBS" => "list",
	"INIT_FUNCTION" => "string",
	"MAJOR_VERSION" => "string",
	"MINOR_VERSION" => "string",
	"RELEASE_VERSION" => "string",
	"ENABLE" => "bool",
	"CMD" => "string",
	"MANPAGE" => "string"
);

###########################################################
# The parsing function which parses the file
#
# $result = _parse_config_mk($filename)
#
# $filename -	the path of the config.mk file
#		which should be parsed
sub run_config_mk($$)
{
	my ($input, $filename) = @_;
	my $result;
	my $linenum = -1;
	my $infragment = 0;
	my $section = "GLOBAL";
	my $makefile = "";
	
	open(CONFIG_MK, $filename) or die("Can't open `$filename'\n");
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

		if ($line =~ /^\[([a-zA-Z0-9_:]+)\][\t ]*$/) 
		{
			$section = $1;
			$infragment = 0;
			next;
		}

		# include
		if ($line =~ /^include (.*)$/) {
			$makefile .= run_config_mk($input, $1);
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

		die("$filename:$linenum: Bad line while parsing $filename");
	}

	foreach my $section (keys %{$result}) {
		my ($type, $name) = split(/::/, $section, 2);
		
		$input->{$name}{NAME} = $name;
		$input->{$name}{TYPE} = $type;

		foreach my $key (values %{$result->{$section}}) {
			$key->{VAL} = smb_build::input::strtrim($key->{VAL});
			my $vartype = $attribute_types{$key->{KEY}};
			if (not defined($vartype)) {
				die("$filename:Unknown attribute $key->{KEY} with value $key->{VAL} in section $section");
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
