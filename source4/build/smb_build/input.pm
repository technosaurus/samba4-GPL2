# Samba Build System
# - the input checking functions
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Copyright (C) Jelmer Vernooij 2004
#  Released under the GNU GPL

use config;
use strict;
package smb_build::input;

my $srcdir = $config::config{srcdir};

sub strtrim($)
{
	$_ = shift;
	s/^[\t\n ]*//g;
	s/[\t\n ]*$//g;
	return $_;
}

sub str2array($)
{
	$_ = shift;
	s/^[\t\n ]*//g;
	s/[\t\n ]*$//g;
	s/([\t\n ]+)/ /g;

	return () if (length($_)==0);
	return split /[ \t\n]/;
}

sub check_subsystem($$$)
{
	my ($INPUT, $subsys, $default_ot) = @_;
	return if ($subsys->{ENABLE} ne "YES");
	
	unless(defined($subsys->{OUTPUT_TYPE})) {
		$subsys->{OUTPUT_TYPE} = $default_ot;
	}
}

sub check_module($$$)
{
	my ($INPUT, $mod, $default_ot) = @_;

	die("Module $mod->{NAME} does not have a SUBSYSTEM set") if not defined($mod->{SUBSYSTEM});

	my $use_default = 0;
	
	if (not exists($INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTIONS})) {
		$INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTIONS} = [];
	}

	if (!(defined($INPUT->{$mod->{SUBSYSTEM}}))) {
		$mod->{ENABLE} = "NO";
		return;
	}

	return if ($mod->{ENABLE} ne "YES");

	if (exists($INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTION_TYPE})) {
		$mod->{INIT_FUNCTION_TYPE} = $INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTION_TYPE};
	} else {
		$mod->{INIT_FUNCTION_TYPE} = "NTSTATUS (*) (void)";
	}

	if (defined($mod->{CHOSEN_BUILD}) and $mod->{CHOSEN_BUILD} ne "DEFAULT") 
	{
		$mod->{OUTPUT_TYPE} = $mod->{CHOSEN_BUILD};
	} elsif (not defined($mod->{OUTPUT_TYPE})) {
		$mod->{OUTPUT_TYPE} = $default_ot;
	}

	if ($mod->{OUTPUT_TYPE} eq "SHARED_LIBRARY") {
		$mod->{INSTALLDIR} = "MODULESDIR/$mod->{SUBSYSTEM}";
		push (@{$mod->{PRIVATE_DEPENDENCIES}}, $mod->{SUBSYSTEM}) unless 
			$INPUT->{$mod->{SUBSYSTEM}}->{TYPE} eq "BINARY";
	} else {
		push (@{$INPUT->{$mod->{SUBSYSTEM}}{PRIVATE_DEPENDENCIES}}, $mod->{NAME});
		push (@{$INPUT->{$mod->{SUBSYSTEM}}{INIT_FUNCTIONS}}, $mod->{INIT_FUNCTION}) if defined($mod->{INIT_FUNCTION});
	}
}

sub check_library($$$)
{
	my ($INPUT, $lib, $default_ot) = @_;

	return if ($lib->{ENABLE} ne "YES");

	$lib->{OUTPUT_TYPE} = $default_ot;

	if (defined($lib->{VERSION}) and not defined($lib->{SO_VERSION})) {
		print "$lib->{NAME}: Please specify SO_VERSION when specifying VERSION\n";
		return;
	}

	if (defined($lib->{SO_VERSION}) and not defined($lib->{VERSION})) {
		print "$lib->{NAME}: Please specify VERSION when specifying SO_VERSION\n";
		return;
	}

	unless (defined($lib->{INIT_FUNCTION_TYPE})) {
		$lib->{INIT_FUNCTION_TYPE} = "NTSTATUS (*) (void)";
	}

	$lib->{INSTALLDIR} = "LIBDIR";
}

sub check_binary($$)
{
	my ($INPUT, $bin) = @_;

	return if ($bin->{ENABLE} ne "YES");

	($bin->{BINARY} = (lc $bin->{NAME})) if not defined($bin->{BINARY});

	$bin->{OUTPUT_TYPE} = "BINARY";
}

my $level = "";

sub calc_unique_deps($$$$)
{
	sub calc_unique_deps($$$$);
	my ($name, $deps, $udeps, $withlibs) = @_;

	print "$level-> $name\n" if ($ENV{SMB_BUILD_VERBOSE});
	$level.=" ";

	foreach my $dep (@{$deps}) {
		next if defined($udeps->{$$dep->{NAME}});

 		if (defined ($$dep->{OUTPUT_TYPE}) && 
			($withlibs or ($$dep->{OUTPUT_TYPE} eq "OBJ_LIST") or ($$dep->{OUTPUT_TYPE} eq "MERGEDOBJ"))) {
   			        $udeps->{$$dep->{NAME}} = "BUSY";
			        calc_unique_deps($$dep->{NAME}, $$dep->{DEPENDENCIES}, $udeps, $withlibs);
	        }

		$udeps->{$$dep->{NAME}} = $$dep;
	}
	
	$level = substr($level, 1);
}

sub check($$$$$)
{
	my ($INPUT, $enabled, $subsys_ot, $lib_ot, $module_ot) = @_;

	foreach my $part (values %$INPUT) {
		unless (defined($part->{STANDARD_VISIBILITY})) {
			if ($part->{TYPE} eq "MODULE" or $part->{TYPE} eq "BINARY") {
				$part->{STANDARD_VISIBILITY} = "hidden";
			} else {
				$part->{STANDARD_VISIBILITY} = "default";
			}
		}

		unless (defined($part->{CFLAGS})) {
			$part->{CFLAGS} = "";
		}

		unless (defined($part->{PUBLIC_HEADERS})) {
			$part->{PUBLIC_HEADERS} = [];
		}
		
		if (defined($part->{PUBLIC_PROTO_HEADER})) {
			push (@{$part->{PUBLIC_HEADERS}}, $part->{PUBLIC_PROTO_HEADER});
		}

		if (defined($enabled->{$part->{NAME}})) { 
			$part->{ENABLE} = $enabled->{$part->{NAME}};
			next;
		}
		
		unless(defined($part->{ENABLE})) {
			if ($part->{TYPE} eq "EXT_LIB") {
				$part->{ENABLE} = "NO";
			} else {
				$part->{ENABLE} = "YES";
			}
		}
	}

	foreach my $k (keys %$INPUT) {
		my $part = $INPUT->{$k};

		check_subsystem($INPUT, $part, $subsys_ot) if ($part->{TYPE} eq "SUBSYSTEM");
		check_module($INPUT, $part, $module_ot) if ($part->{TYPE} eq "MODULE");
		check_library($INPUT, $part, $lib_ot) if ($part->{TYPE} eq "LIBRARY");
		check_binary($INPUT, $part) if ($part->{TYPE} eq "BINARY");
	}

	my %depend = %$INPUT;

	foreach my $part (values %depend) {
		
		# Generate list of dependencies
		$part->{DEPENDENCIES} = [];

		foreach my $key (@{$part->{PUBLIC_DEPENDENCIES}},
				 @{$part->{PRIVATE_DEPENDENCIES}}) {
			die("$part->{NAME} has undefined dependency $key\n") if not defined($depend{$key});
			push (@{$part->{DEPENDENCIES}}, \$depend{$key});
		}

#		delete ($part->{PRIVATE_DEPENDENCIES});
#		delete ($part->{PUBLIC_DEPENDENCIES});
	}

	foreach my $part (values %depend) {
		$part->{UNIQUE_DEPENDENCIES} = {};
		calc_unique_deps($part->{NAME}, $part->{DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES}, 0);
	}

	foreach my $part (values %depend) {
		$part->{UNIQUE_DEPENDENCIES_ALL} = {};
		calc_unique_deps($part->{NAME}, $part->{DEPENDENCIES}, $part->{UNIQUE_DEPENDENCIES_ALL}, 1);
	}

	return \%depend;
}

1;
