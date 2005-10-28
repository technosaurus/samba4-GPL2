###########################################################
### SMB Build System					###
### - the main program					###
###							###
###  Copyright (C) Stefan (metze) Metzmacher 2004	###
###  Copyright (C) Jelmer Vernooij 2005
###  Released under the GNU GPL				###
###########################################################

use smb_build::makefile;
use smb_build::smb_build_h;
use smb_build::input;
use smb_build::config_mk;
use smb_build::output;
use smb_build::env;
use config;
use strict;

my $INPUT = {};

my $mkfile = smb_build::config_mk::run_config_mk($INPUT, "main.mk");

if (defined($ENV{"SUBSYSTEM_OUTPUT_TYPE"})) {
	$smb_build::input::subsystem_output_type = $ENV{SUBSYSTEM_OUTPUT_TYPE};
} elsif ($config::config{BLDMERGED} eq "true") {
	$smb_build::input::subsystem_output_type = "MERGEDOBJ";
}

if (defined($ENV{"LIBRARY_OUTPUT_TYPE"})) {
	$smb_build::input::library_output_type = $ENV{LIBRARY_OUTPUT_TYPE};
} elsif ($config::config{BLDSHARED} eq "true") {
	#FIXME: This should eventually become SHARED_LIBRARY 
	# rather then MERGEDOBJ once I'm certain it works ok -- jelmer
	$smb_build::input::library_output_type = "MERGEDOBJ";
} elsif ($config::config{BLDMERGED} eq "true") {
	$smb_build::input::library_output_type = "MERGEDOBJ";
}

if (defined($ENV{"MODULE_OUTPUT_TYPE"})) {
	$smb_build::input::module_output_type = $ENV{MODULE_OUTPUT_TYPE};
} elsif ($config::config{BLDSHARED} eq "true") {
	#FIXME: This should eventually become SHARED_LIBRARY 
	# rather then MERGEDOBJ once I'm certain it works ok -- jelmer
	$smb_build::input::module_output_type = "MERGEDOBJ";
} elsif ($config::config{BLDMERGED} eq "true") {
	$smb_build::input::module_output_type = "MERGEDOBJ";
}

my $DEPEND = smb_build::input::check($INPUT, \%config::enabled);
my $OUTPUT = output::create_output($DEPEND);
my $mkenv = new smb_build::makefile(\%config::config, $mkfile);

foreach my $key (values %$OUTPUT) {
	next unless defined $key->{OUTPUT_TYPE};

	$mkenv->MergedObj($key) if $key->{OUTPUT_TYPE} eq "MERGEDOBJ";
	$mkenv->ObjList($key) if $key->{OUTPUT_TYPE} eq "OBJLIST";
	$mkenv->StaticLibrary($key) if $key->{OUTPUT_TYPE} eq "STATIC_LIBRARY";
	$mkenv->PkgConfig($key) if ($key->{OUTPUT_TYPE} eq "SHARED_LIBRARY") and
						defined($key->{MAJOR_VERSION});
	$mkenv->SharedLibrary($key) if $key->{OUTPUT_TYPE} eq "SHARED_LIBRARY";
	$mkenv->Binary($key) if $key->{OUTPUT_TYPE} eq "BINARY";
	$mkenv->Manpage($key) if defined($key->{MANPAGE});
	$mkenv->Header($key) if defined($key->{PUBLIC_HEADERS});
}

# FIXME: These two lines are a hack
$mkenv->ProtoHeader($OUTPUT->{PROTO});
$mkenv->ProtoHeader($OUTPUT->{ALL_OBJS});

$mkenv->write("Makefile");
smb_build_h::create_smb_build_h($OUTPUT, "include/smb_build.h");

1;
