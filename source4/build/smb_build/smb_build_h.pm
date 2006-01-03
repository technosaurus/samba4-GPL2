# SMB Build System
# - create output for build.h
#
#  Copyright (C) Stefan (metze) Metzmacher 2004
#  Released under the GNU GPL

package smb_build_h;
use strict;

sub _add_define_section($)
{
	my $DEFINE = shift;
	my $output = "";

	$output .= "
/* $DEFINE->{COMMENT} */
#define $DEFINE->{KEY} $DEFINE->{VAL}
";

	return $output;
}

sub _prepare_build_h($)
{
	my $depend = shift;
	my @defines = ();
	my $output = "";

	foreach my $key (values %{$depend}) {
		my $DEFINE = ();
		next if ($key->{TYPE} ne "LIBRARY" and $key->{TYPE} ne "SUBSYSTEM");
		next unless defined($key->{INIT_FUNCTIONS});
		
		$DEFINE->{COMMENT} = "$key->{TYPE} $key->{NAME} INIT";
		$DEFINE->{KEY} = "STATIC_$key->{NAME}_MODULES";
		$DEFINE->{VAL} = "{ \\\n";
		foreach (@{$key->{INIT_FUNCTIONS}}) {
			$DEFINE->{VAL} .= "\t$_, \\\n";
			$output .= "NTSTATUS $_(void);\n";
		}

		$DEFINE->{VAL} .= "\tNULL \\\n }";

		push(@defines,$DEFINE);
	}

	#
	# Shared modules
	#
	foreach my $key (values %{$depend}) {
		next if $key->{TYPE} ne "MODULE";
		next if $key->{ENABLE} ne "YES";
		next if $key->{OUTPUT_TYPE} ne "SHARED_LIBRARY";

		my $name = $key->{NAME};
		next if not defined($key->{INIT_FUNCTION});

		my $DEFINE = ();
		
		$DEFINE->{COMMENT} = "$name is built shared";
		$DEFINE->{KEY} = $key->{INIT_FUNCTION};
		$DEFINE->{VAL} = "init_module";

		push(@defines,$DEFINE);
	}

	#
	# loop over all BUILD_H define sections
	#
	foreach (@defines) { $output .= _add_define_section($_); }

	return $output;
}

###########################################################
# This function creates include/build.h from the SMB_BUILD 
# context
#
# create_build_h($SMB_BUILD_CTX)
#
# $SMB_BUILD_CTX -	the global SMB_BUILD context
#
# $output -		the resulting output buffer
sub create_smb_build_h($$)
{
	my ($CTX, $file) = @_;

	open(SMB_BUILD_H,">$file") || die ("Can't open `$file'\n");
	print SMB_BUILD_H "/* autogenerated by build/smb_build/main.pl */\n";
	print SMB_BUILD_H _prepare_build_h($CTX);
	close(SMB_BUILD_H);

	print __FILE__.": creating $file\n";
}
1;
