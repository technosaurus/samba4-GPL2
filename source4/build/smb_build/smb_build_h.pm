###########################################################
### SMB Build System					###
### - create output for smb_build.h			###
###							###
###  Copyright (C) Stefan (metze) Metzmacher 2004	###
###  Released under the GNU GPL				###
###########################################################

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

sub _prepare_smb_build_h($)
{
	my $depend = shift;
	my @defines = ();

	#
	# loop over all binaries
	#
	foreach my $key (values %{$depend}) {
		next if ($key->{TYPE} ne "BINARY");

		my $NAME = $key->{NAME};
		my $DEFINE = ();
		my $name = lc($NAME);

		#
		# Static modules
		# 
		$DEFINE->{COMMENT} = "BINARY $NAME INIT";
		$DEFINE->{KEY} = $name . "_init_subsystems";
		$DEFINE->{VAL} = "do { \\\n";
		foreach my $subkey (@{$key->{SUBSYSTEM_INIT_FUNCTIONS}}) {
			$DEFINE->{VAL} .= "\t\textern NTSTATUS $subkey(void); \\\n";
		}
	
		foreach my $subkey (@{$key->{SUBSYSTEM_INIT_FUNCTIONS}}) {
			$DEFINE->{VAL} .= "\t\tif (NT_STATUS_IS_ERR($subkey())) exit(1); \\\n";
		}
		$DEFINE->{VAL} .= "\t} while(0)";
		
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
		my $func = $key->{INIT_FUNCTION};
		next if $func eq "";

		my $DEFINE = ();
		
		$DEFINE->{COMMENT} = "$name is built shared";
		$DEFINE->{KEY} = $func;
		$DEFINE->{VAL} = "init_module";

		push(@defines,$DEFINE);
	}

	#
	# loop over all SMB_BUILD_H define sections
	#
	my $output = "";
	foreach my $key (@defines) {
		$output .= _add_define_section($key);
	}

	return $output;
}

###########################################################
# This function creates include/smb_build.h from the SMB_BUILD 
# context
#
# create_smb_build_h($SMB_BUILD_CTX)
#
# $SMB_BUILD_CTX -	the global SMB_BUILD context
#
# $output -		the resulting output buffer
sub create_smb_build_h($)
{
	my $CTX = shift;
	my $output = "/* autogenerated by config.smb_build.pl */\n";

	$output .= _prepare_smb_build_h($CTX);

	open(SMB_BUILD_H,"> include/smb_build.h") || die ("Can't open include/smb_build.h\n");

	print SMB_BUILD_H $output;

	close(SMB_BUILD_H);

	print "config.smb_build.pl: creating include/smb_build.h\n";
	return;	
}
1;
