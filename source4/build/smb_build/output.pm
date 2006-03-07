# SMB Build System
# - the output generating functions
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Copyright (C) Jelmer Vernooij 2004
#  Released under the GNU GPL

package output;
use strict;

sub add_dir($$)
{
	my ($dir,$files) = @_;
	my @ret = ();

	$dir =~ s/^\.\///g;
	
	foreach (@$files) {
		$_ = "$dir/$_";
		s/([^\/\.]+)\/\.\.\///g;
		push (@ret, $_);
	}
	
	return @ret;
}

sub generate_mergedobj($)
{
	my $subsys = shift;

	$subsys->{OUTPUT} = $subsys->{TARGET} = "bin/subsystems/$subsys->{TYPE}_$subsys->{NAME}.o";
}

sub generate_objlist($)
{
	my $subsys = shift;

	$subsys->{TARGET} = "bin/.$subsys->{TYPE}_$subsys->{NAME}";
	$subsys->{OUTPUT} = "\$($subsys->{TYPE}_$subsys->{NAME}_OBJ_LIST)";
}

sub generate_shared_library($)
{
	my $lib = shift;
	my $link_name;

	@{$lib->{DEPEND_LIST}} = ();
	@{$lib->{LINK_LIST}} = ("\$($lib->{TYPE}_$lib->{NAME}\_OBJ_LIST)");

	$link_name = $lib->{NAME};
	$link_name =~ s/^LIB//;
	$link_name = lc($link_name);

	$lib->{LIBRARY_REALNAME} = $lib->{LIBRARY_NAME} = "lib$link_name.\$(SHLIBEXT)";
	$lib->{TARGET} = "bin/$lib->{LIBRARY_NAME}";
	if (defined($lib->{MAJOR_VERSION})) {
		$lib->{LIBRARY_SONAME} = $lib->{LIBRARY_NAME}.".$lib->{MAJOR_VERSION}";
		$lib->{LIBRARY_REALNAME} = $lib->{LIBRARY_SONAME}.".$lib->{MINOR_VERSION}.$lib->{RELEASE_VERSION}";
		$lib->{TARGET} = "bin/$lib->{LIBRARY_REALNAME}";
		@{$lib->{LINK_FLAGS}} = ("\$(SONAMEFLAG)$lib->{LIBRARY_SONAME}");
	}
	$lib->{OUTPUT} = "-l$link_name";
}

sub generate_static_library($)
{
	my $lib = shift;
	my $link_name;

	@{$lib->{DEPEND_LIST}} = ();

	$link_name = $lib->{NAME};
	$link_name =~ s/^LIB//;

	$lib->{LIBRARY_NAME} = "lib".lc($link_name).".a";
	@{$lib->{LINK_LIST}} = ("\$($lib->{TYPE}_$lib->{NAME}\_OBJ_LIST)");
	@{$lib->{LINK_FLAGS}} = ();

	$lib->{TARGET} = "bin/$lib->{LIBRARY_NAME}";
	$lib->{OUTPUT} = "-l".lc($link_name);
}

sub generate_binary($)
{
	my $bin = shift;

	@{$bin->{DEPEND_LIST}} = ();
	@{$bin->{LINK_LIST}} = ("\$($bin->{TYPE}_$bin->{NAME}\_OBJ_LIST)");
	@{$bin->{LINK_FLAGS}} = ();

	$bin->{TARGET} = $bin->{OUTPUT} = "bin/$bin->{NAME}";
	$bin->{BINARY} = $bin->{NAME};
}

sub create_output($$)
{
	my ($depend, $config) = @_;
	my $part;

	foreach $part (values %{$depend}) {
		next if not defined($part->{OUTPUT_TYPE});

		# Combine object lists
		push(@{$part->{OBJ_LIST}}, add_dir($part->{BASEDIR}, $part->{INIT_OBJ_FILES})) if defined($part->{INIT_OBJ_FILES});
		push(@{$part->{OBJ_LIST}}, add_dir($part->{BASEDIR}, $part->{ADD_OBJ_FILES})) if defined($part->{ADD_OBJ_FILES});
		push(@{$part->{OBJ_LIST}}, add_dir($part->{BASEDIR}, $part->{OBJ_FILES})) if defined($part->{OBJ_FILES});

		if ((not defined($part->{OBJ_LIST}) or 
			scalar(@{$part->{OBJ_LIST}}) == 0) and 
			$part->{OUTPUT_TYPE} eq "MERGEDOBJ") {
			$part->{OUTPUT_TYPE} = "OBJLIST";
		}

		if (($part->{STANDARD_VISIBILITY} ne "default") and 
			($config->{visibility_attribute} eq "yes")) {
			$part->{EXTRA_CFLAGS} .= " -fvisibility=$part->{STANDARD_VISIBILITY}";
		}

		generate_binary($part) if $part->{OUTPUT_TYPE} eq "BINARY";
		generate_mergedobj($part) if $part->{OUTPUT_TYPE} eq "MERGEDOBJ";
		generate_objlist($part) if $part->{OUTPUT_TYPE} eq "OBJLIST";
		generate_shared_library($part) if $part->{OUTPUT_TYPE} eq "SHARED_LIBRARY";
		generate_static_library($part) if $part->{OUTPUT_TYPE} eq "STATIC_LIBRARY";

	}

	foreach $part (values %{$depend}) {
		next if not defined($part->{OUTPUT_TYPE});

		# Always import the CFLAGS and CPPFLAGS of the unique dependencies
		foreach my $elem (values %{$part->{UNIQUE_DEPENDENCIES}}) {
			next if $elem == $part;

			push(@{$part->{CPPFLAGS}}, @{$elem->{CPPFLAGS}}) if defined(@{$elem->{CPPFLAGS}});
			push(@{$part->{CFLAGS}}, @{$elem->{CFLAGS}}) if defined(@{$elem->{CFLAGS}});
			push(@{$part->{LINK_LIST}}, $elem->{OUTPUT}) if defined($elem->{OUTPUT});
			push(@{$part->{LINK_FLAGS}}, @{$elem->{LIBS}}) if defined($elem->{LIBS});
			push(@{$part->{LINK_FLAGS}},@{$elem->{LDFLAGS}}) if defined($elem->{LDFLAGS});
			if (defined($elem->{OUTPUT_TYPE}) and ($elem->{OUTPUT_TYPE} eq "SHARED_LIBRARY")) {
			    push(@{$part->{DEPEND_LIST}}, "bin/$elem->{LIBRARY_NAME}");			    
			} else { 
			    push(@{$part->{DEPEND_LIST}}, $elem->{TARGET}) if defined($elem->{TARGET});
			}
			push(@{$part->{SUBSYSTEM_INIT_FUNCTIONS}}, $elem->{INIT_FUNCTION}) if 
				defined($elem->{INIT_FUNCTION}) and 
				$elem->{TYPE} ne "MODULE" and 
				$part->{OUTPUT_TYPE} ne "SHARED_LIBRARY";
		}
	}

	return $depend;
}

1;
