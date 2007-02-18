###################################################
# Samba4 NDR parser generator for IDL structures
# Copyright tridge@samba.org 2000-2003
# Copyright tpot@samba.org 2001
# Copyright jelmer@samba.org 2004-2006
# released under the GNU GPL

package Parse::Pidl::Samba4::NDR::Parser;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(is_charset_array);
@EXPORT_OK = qw(check_null_pointer GenerateFunctionInEnv 
   GenerateFunctionOutEnv EnvSubstituteValue GenerateStructEnv NeededFunction
   NeededElement NeededType);

use strict;
use Parse::Pidl::Typelist qw(hasType getType mapType);
use Parse::Pidl::Util qw(has_property ParseExpr ParseExprExt print_uuid);
use Parse::Pidl::NDR qw(GetPrevLevel GetNextLevel ContainsDeferred);
use Parse::Pidl::Samba4 qw(is_intree choose_header);
use Parse::Pidl qw(warning);

use vars qw($VERSION);
$VERSION = '0.01';

# list of known types
my %typefamily;

sub get_typefamily($)
{
	my $n = shift;
	return $typefamily{$n};
}

sub append_prefix($$)
{
	my ($e, $var_name) = @_;
	my $pointers = 0;

	foreach my $l (@{$e->{LEVELS}}) {
		if ($l->{TYPE} eq "POINTER") {
			$pointers++;
		} elsif ($l->{TYPE} eq "ARRAY") {
			if (($pointers == 0) and 
			    (not $l->{IS_FIXED}) and
			    (not $l->{IS_INLINE})) {
				return get_value_of($var_name); 
			}
		} elsif ($l->{TYPE} eq "DATA") {
			if (Parse::Pidl::Typelist::scalar_is_reference($l->{DATA_TYPE})) {
				return get_value_of($var_name) unless ($pointers);
			}
		}
	}
	
	return $var_name;
}

sub has_fast_array($$)
{
	my ($e,$l) = @_;

	return 0 if ($l->{TYPE} ne "ARRAY");

	my $nl = GetNextLevel($e,$l);
	return 0 unless ($nl->{TYPE} eq "DATA");
	return 0 unless (hasType($nl->{DATA_TYPE}));

	my $t = getType($nl->{DATA_TYPE});

	# Only uint8 and string have fast array functions at the moment
	return ($t->{NAME} eq "uint8") or ($t->{NAME} eq "string");
}

sub is_charset_array($$)
{
	my ($e,$l) = @_;

	return 0 if ($l->{TYPE} ne "ARRAY");

	my $nl = GetNextLevel($e,$l);

	return 0 unless ($nl->{TYPE} eq "DATA");

	return has_property($e, "charset");
}

sub get_pointer_to($)
{
	my $var_name = shift;
	
	if ($var_name =~ /^\*(.*)$/) {
		return $1;
	} elsif ($var_name =~ /^\&(.*)$/) {
		return "&($var_name)";
	} else {
		return "&$var_name";
	}
}

sub get_value_of($)
{
	my $var_name = shift;

	if ($var_name =~ /^\&(.*)$/) {
		return $1;
	} else {
		return "*$var_name";
	}
}

my $res;
my $deferred = [];
my $tabs = "";

####################################
# pidl() is our basic output routine
sub pidl($)
{
	my $d = shift;
	if ($d) {
		$res .= $tabs;
		$res .= $d;
	}
	$res .="\n";
}

my $res_hdr;

sub pidl_hdr ($) { my $d = shift; $res_hdr .= "$d\n"; }

####################################
# defer() is like pidl(), but adds to 
# a deferred buffer which is then added to the 
# output buffer at the end of the structure/union/function
# This is needed to cope with code that must be pushed back
# to the end of a block of elements
my $defer_tabs = "";
sub defer_indent() { $defer_tabs.="\t"; }
sub defer_deindent() { $defer_tabs=substr($defer_tabs, 0, -1); }

sub defer($)
{
	my $d = shift;
	if ($d) {
		push(@$deferred, $defer_tabs.$d);
	}
}

########################################
# add the deferred content to the current
# output
sub add_deferred()
{
	pidl $_ foreach (@$deferred);
	$deferred = [];
	$defer_tabs = "";
}

sub indent()
{
	$tabs .= "\t";
}

sub deindent()
{
	$tabs = substr($tabs, 0, -1);
}

#####################################################################
# declare a function public or static, depending on its attributes
sub fn_declare($$$)
{
	my ($type,$fn,$decl) = @_;

	if (has_property($fn, "no$type")) {
		pidl_hdr "$decl;";
		return 0;
	}

	if (has_property($fn, "public")) {
		pidl_hdr "$decl;";
		pidl "_PUBLIC_ $decl";
	} else {
		pidl "static $decl";
	}

	return 1;
}

###################################################################
# setup any special flags for an element or structure
sub start_flags($)
{
	my $e = shift;
	my $flags = has_property($e, "flag");
	if (defined $flags) {
		pidl "{";
		indent;
		pidl "uint32_t _flags_save_$e->{TYPE} = ndr->flags;";
		pidl "ndr_set_flags(&ndr->flags, $flags);";
	}
}

###################################################################
# end any special flags for an element or structure
sub end_flags($)
{
	my $e = shift;
	my $flags = has_property($e, "flag");
	if (defined $flags) {
		pidl "ndr->flags = _flags_save_$e->{TYPE};";
		deindent;
		pidl "}";
	}
}

sub GenerateStructEnv($)
{
	my $x = shift;
	my %env;

	foreach my $e (@{$x->{ELEMENTS}}) {
		$env{$e->{NAME}} = "r->$e->{NAME}";
	}

	$env{"this"} = "r";

	return \%env;
}

sub EnvSubstituteValue($$)
{
	my ($env,$s) = @_;

	# Substitute the value() values in the env
	foreach my $e (@{$s->{ELEMENTS}}) {
		next unless (defined(my $v = has_property($e, "value")));
		
		$env->{$e->{NAME}} = ParseExpr($v, $env, $e);
	}

	return $env;
}

sub GenerateFunctionInEnv($)
{
	my $fn = shift;
	my %env;

	foreach my $e (@{$fn->{ELEMENTS}}) {
		if (grep (/in/, @{$e->{DIRECTION}})) {
			$env{$e->{NAME}} = "r->in.$e->{NAME}";
		}
	}

	return \%env;
}

sub GenerateFunctionOutEnv($)
{
	my $fn = shift;
	my %env;

	foreach my $e (@{$fn->{ELEMENTS}}) {
		if (grep (/out/, @{$e->{DIRECTION}})) {
			$env{$e->{NAME}} = "r->out.$e->{NAME}";
		} elsif (grep (/in/, @{$e->{DIRECTION}})) {
			$env{$e->{NAME}} = "r->in.$e->{NAME}";
		}
	}

	return \%env;
}

#####################################################################
# parse the data of an array - push side
sub ParseArrayPushHeader($$$$$)
{
	my ($e,$l,$ndr,$var_name,$env) = @_;

	my $size;
	my $length;

	if ($l->{IS_ZERO_TERMINATED}) {
		if (has_property($e, "charset")) {
			$size = $length = "ndr_charset_length($var_name, CH_$e->{PROPERTIES}->{charset})";
		} else {
			$size = $length = "ndr_string_length($var_name, sizeof(*$var_name))";
		}
	} else {
		$size = ParseExpr($l->{SIZE_IS}, $env, $e);
		$length = ParseExpr($l->{LENGTH_IS}, $env, $e);
	}

	if ((!$l->{IS_SURROUNDING}) and $l->{IS_CONFORMANT}) {
		pidl "NDR_CHECK(ndr_push_uint32($ndr, NDR_SCALARS, $size));";
	}
	
	if ($l->{IS_VARYING}) {
		pidl "NDR_CHECK(ndr_push_uint32($ndr, NDR_SCALARS, 0));";  # array offset
		pidl "NDR_CHECK(ndr_push_uint32($ndr, NDR_SCALARS, $length));";
	} 

	return $length;
}

sub check_fully_dereferenced($$)
{
	my ($element, $env) = @_;

	return sub ($) {
		my $origvar = shift;
		my $check = 0;

		# Figure out the number of pointers in $ptr
		my $expandedvar = $origvar;
		$expandedvar =~ s/^(\**)//;
		my $ptr = $1;

		my $var = undef;
		foreach (keys %$env) {
			if ($env->{$_} eq $expandedvar) {
				$var = $_;
				last;
			}
		}
		
		return($origvar) unless (defined($var));
		my $e;
		foreach (@{$element->{PARENT}->{ELEMENTS}}) {
			if ($_->{NAME} eq $var) {
				$e = $_;
				last;
			}
		}

		$e or die("Environment doesn't match siblings");

		# See if pointer at pointer level $level
		# needs to be checked.
		my $nump = 0;
		foreach (@{$e->{LEVELS}}) {
			if ($_->{TYPE} eq "POINTER") {
				$nump = $_->{POINTER_INDEX}+1;
			}
		}
		warning($element->{ORIGINAL}, "Got pointer for `$e->{NAME}', expected fully derefenced variable") if ($nump > length($ptr));
		return ($origvar);
	}
}	

sub check_null_pointer($$$$)
{
	my ($element, $env, $print_fn, $return) = @_;

	return sub ($) {
		my $expandedvar = shift;
		my $check = 0;

		# Figure out the number of pointers in $ptr
		$expandedvar =~ s/^(\**)//;
		my $ptr = $1;

		my $var = undef;
		foreach (keys %$env) {
			if ($env->{$_} eq $expandedvar) {
				$var = $_;
				last;
			}
		}
		
		if (defined($var)) {
			my $e;
			# lookup ptr in $e
			foreach (@{$element->{PARENT}->{ELEMENTS}}) {
				if ($_->{NAME} eq $var) {
					$e = $_;
					last;
				}
			}

			$e or die("Environment doesn't match siblings");

			# See if pointer at pointer level $level
			# needs to be checked.
			foreach my $l (@{$e->{LEVELS}}) {
				if ($l->{TYPE} eq "POINTER" and 
					$l->{POINTER_INDEX} == length($ptr)) {
					# No need to check ref pointers
					$check = ($l->{POINTER_TYPE} ne "ref");
					last;
				}

				if ($l->{TYPE} eq "DATA") {
					warning($element, "too much dereferences for `$var'");
				}
			}
		} else {
			warning($element, "unknown dereferenced expression `$expandedvar'");
			$check = 1;
		}
		
		$print_fn->("if ($ptr$expandedvar == NULL) $return") if $check;
	}
}

#####################################################################
# parse an array - pull side
sub ParseArrayPullHeader($$$$$)
{
	my ($e,$l,$ndr,$var_name,$env) = @_;

	my $length;
	my $size;

	if ($l->{IS_CONFORMANT}) {
		$length = $size = "ndr_get_array_size($ndr, " . get_pointer_to($var_name) . ")";
	} elsif ($l->{IS_ZERO_TERMINATED}) { # Noheader arrays
		$length = $size = "ndr_get_string_size($ndr, sizeof(*$var_name))";
	} else {
		$length = $size = ParseExprExt($l->{SIZE_IS}, $env, $e->{ORIGINAL}, 
		    check_null_pointer($e, $env, \&pidl, "return NT_STATUS_INVALID_PARAMETER_MIX;"), check_fully_dereferenced($e, $env));
	}

	if ((!$l->{IS_SURROUNDING}) and $l->{IS_CONFORMANT}) {
		pidl "NDR_CHECK(ndr_pull_array_size(ndr, " . get_pointer_to($var_name) . "));";
	}

	if ($l->{IS_VARYING}) {
		pidl "NDR_CHECK(ndr_pull_array_length($ndr, " . get_pointer_to($var_name) . "));";
		$length = "ndr_get_array_length($ndr, " . get_pointer_to($var_name) .")";
	}

	if ($length ne $size) {
		pidl "if ($length > $size) {";
		indent;
		pidl "return ndr_pull_error($ndr, NDR_ERR_ARRAY_SIZE, \"Bad array size %u should exceed array length %u\", $size, $length);";
		deindent;
		pidl "}";
	}

	if ($l->{IS_CONFORMANT} and not $l->{IS_ZERO_TERMINATED}) {
		defer "if ($var_name) {";
		defer_indent;
		my $size = ParseExprExt($l->{SIZE_IS}, $env, $e->{ORIGINAL}, check_null_pointer($e, $env, \&defer, "return NT_STATUS_INVALID_PARAMETER_MIX;"), check_fully_dereferenced($e, $env));
		defer "NDR_CHECK(ndr_check_array_size(ndr, (void*)" . get_pointer_to($var_name) . ", $size));";
		defer_deindent;
		defer "}";
	}

	if ($l->{IS_VARYING} and not $l->{IS_ZERO_TERMINATED}) {
		defer "if ($var_name) {";
		defer_indent;
		my $length = ParseExprExt($l->{LENGTH_IS}, $env, $e->{ORIGINAL}, 
			check_null_pointer($e, $env, \&defer, "return NT_STATUS_INVALID_PARAMETER_MIX;"), 
			check_fully_dereferenced($e, $env));
		defer "NDR_CHECK(ndr_check_array_length(ndr, (void*)" . get_pointer_to($var_name) . ", $length));";
		defer_deindent;
		defer "}"
	}

	if (not $l->{IS_FIXED} and not is_charset_array($e, $l)) {
		AllocateArrayLevel($e,$l,$ndr,$env,$size);
	}

	return $length;
}

sub compression_alg($$)
{
	my ($e, $l) = @_;
	my ($alg, $clen, $dlen) = split(/ /, $l->{COMPRESSION});

	return $alg;
}

sub compression_clen($$$)
{
	my ($e, $l, $env) = @_;
	my ($alg, $clen, $dlen) = split(/ /, $l->{COMPRESSION});

	return ParseExpr($clen, $env, $e->{ORIGINAL});
}

sub compression_dlen($$$)
{
	my ($e,$l,$env) = @_;
	my ($alg, $clen, $dlen) = split(/ /, $l->{COMPRESSION});

	return ParseExpr($dlen, $env, $e->{ORIGINAL});
}

sub ParseCompressionPushStart($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $comndr = "$ndr\_compressed";
	my $alg = compression_alg($e, $l);
	my $dlen = compression_dlen($e, $l, $env);

	pidl "{";
	indent;
	pidl "struct ndr_push *$comndr;";
	pidl "NDR_CHECK(ndr_push_compression_start($ndr, &$comndr, $alg, $dlen));";

	return $comndr;
}

sub ParseCompressionPushEnd($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $comndr = "$ndr\_compressed";
	my $alg = compression_alg($e, $l);
	my $dlen = compression_dlen($e, $l, $env);

	pidl "NDR_CHECK(ndr_push_compression_end($ndr, $comndr, $alg, $dlen));";
	deindent;
	pidl "}";
}

sub ParseCompressionPullStart($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $comndr = "$ndr\_compressed";
	my $alg = compression_alg($e, $l);
	my $dlen = compression_dlen($e, $l, $env);

	pidl "{";
	indent;
	pidl "struct ndr_pull *$comndr;";
	pidl "NDR_CHECK(ndr_pull_compression_start($ndr, &$comndr, $alg, $dlen));";

	return $comndr;
}

sub ParseCompressionPullEnd($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $comndr = "$ndr\_compressed";
	my $alg = compression_alg($e, $l);
	my $dlen = compression_dlen($e, $l, $env);

	pidl "NDR_CHECK(ndr_pull_compression_end($ndr, $comndr, $alg, $dlen));";
	deindent;
	pidl "}";
}

sub ParseSubcontextPushStart($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $subndr = "_ndr_$e->{NAME}";
	my $subcontext_size = ParseExpr($l->{SUBCONTEXT_SIZE}, $env, $e->{ORIGINAL});

	pidl "{";
	indent;
	pidl "struct ndr_push *$subndr;";
	pidl "NDR_CHECK(ndr_push_subcontext_start($ndr, &$subndr, $l->{HEADER_SIZE}, $subcontext_size));";

	if (defined $l->{COMPRESSION}) {
		$subndr = ParseCompressionPushStart($e, $l, $subndr, $env);
	}

	return $subndr;
}

sub ParseSubcontextPushEnd($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $subndr = "_ndr_$e->{NAME}";
	my $subcontext_size = ParseExpr($l->{SUBCONTEXT_SIZE}, $env, $e->{ORIGINAL});

	if (defined $l->{COMPRESSION}) {
		ParseCompressionPushEnd($e, $l, $subndr, $env);
	}

	pidl "NDR_CHECK(ndr_push_subcontext_end($ndr, $subndr, $l->{HEADER_SIZE}, $subcontext_size));";
	deindent;
	pidl "}";
}

sub ParseSubcontextPullStart($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $subndr = "_ndr_$e->{NAME}";
	my $subcontext_size = ParseExpr($l->{SUBCONTEXT_SIZE}, $env, $e->{ORIGINAL});

	pidl "{";
	indent;
	pidl "struct ndr_pull *$subndr;";
	pidl "NDR_CHECK(ndr_pull_subcontext_start($ndr, &$subndr, $l->{HEADER_SIZE}, $subcontext_size));";

	if (defined $l->{COMPRESSION}) {
		$subndr = ParseCompressionPullStart($e, $l, $subndr, $env);
	}

	return $subndr;
}

sub ParseSubcontextPullEnd($$$$)
{
	my ($e,$l,$ndr,$env) = @_;
	my $subndr = "_ndr_$e->{NAME}";
	my $subcontext_size = ParseExpr($l->{SUBCONTEXT_SIZE}, $env, $e->{ORIGINAL});

	if (defined $l->{COMPRESSION}) {
		ParseCompressionPullEnd($e, $l, $subndr, $env);
	}

	pidl "NDR_CHECK(ndr_pull_subcontext_end($ndr, $subndr, $l->{HEADER_SIZE}, $subcontext_size));";
	deindent;
	pidl "}";
}

sub ParseElementPushLevel
{
	my ($e,$l,$ndr,$var_name,$env,$primitives,$deferred) = @_;

	my $ndr_flags = CalcNdrFlags($l, $primitives, $deferred);

	if ($l->{TYPE} eq "ARRAY" and ($l->{IS_CONFORMANT} or $l->{IS_VARYING})) {
		$var_name = get_pointer_to($var_name);
	}

	if (defined($ndr_flags)) {
		if ($l->{TYPE} eq "SUBCONTEXT") {
			my $subndr = ParseSubcontextPushStart($e, $l, $ndr, $env);
			ParseElementPushLevel($e, GetNextLevel($e, $l), $subndr, $var_name, $env, 1, 1);
			ParseSubcontextPushEnd($e, $l, $ndr, $env);
		} elsif ($l->{TYPE} eq "POINTER") {
			ParsePtrPush($e, $l, $var_name);
		} elsif ($l->{TYPE} eq "ARRAY") {
			my $length = ParseArrayPushHeader($e, $l, $ndr, $var_name, $env); 

			my $nl = GetNextLevel($e, $l);

			# Allow speedups for arrays of scalar types
			if (is_charset_array($e,$l)) {
				pidl "NDR_CHECK(ndr_push_charset($ndr, $ndr_flags, $var_name, $length, sizeof(" . mapType($nl->{DATA_TYPE}) . "), CH_$e->{PROPERTIES}->{charset}));";
				return;
			} elsif (has_fast_array($e,$l)) {
				pidl "NDR_CHECK(ndr_push_array_$nl->{DATA_TYPE}($ndr, $ndr_flags, $var_name, $length));";
				return;
			} 
		} elsif ($l->{TYPE} eq "SWITCH") {
			ParseSwitchPush($e, $l, $ndr, $var_name, $ndr_flags, $env);
		} elsif ($l->{TYPE} eq "DATA") {
			ParseDataPush($e, $l, $ndr, $var_name, $ndr_flags);
		}
	}

	if ($l->{TYPE} eq "POINTER" and $deferred) {
		if ($l->{POINTER_TYPE} ne "ref") {
			pidl "if ($var_name) {";
			indent;
			if ($l->{POINTER_TYPE} eq "relative") {
				pidl "NDR_CHECK(ndr_push_relative_ptr2(ndr, $var_name));";
			}
		}
		$var_name = get_value_of($var_name);
		ParseElementPushLevel($e, GetNextLevel($e, $l), $ndr, $var_name, $env, 1, 1);

		if ($l->{POINTER_TYPE} ne "ref") {
			deindent;
			pidl "}";
		}
	} elsif ($l->{TYPE} eq "ARRAY" and not has_fast_array($e,$l) and
		not is_charset_array($e, $l)) {
		my $length = ParseExpr($l->{LENGTH_IS}, $env, $e->{ORIGINAL});
		my $counter = "cntr_$e->{NAME}_$l->{LEVEL_INDEX}";

		$var_name = $var_name . "[$counter]";

		if (($primitives and not $l->{IS_DEFERRED}) or ($deferred and $l->{IS_DEFERRED})) {
			pidl "for ($counter = 0; $counter < $length; $counter++) {";
			indent;
			ParseElementPushLevel($e, GetNextLevel($e, $l), $ndr, $var_name, $env, 1, 0);
			deindent;
			pidl "}";
		}

		if ($deferred and ContainsDeferred($e, $l)) {
			pidl "for ($counter = 0; $counter < $length; $counter++) {";
			indent;
			ParseElementPushLevel($e, GetNextLevel($e, $l), $ndr, $var_name, $env, 0, 1);
			deindent;
			pidl "}";
		}	
	} elsif ($l->{TYPE} eq "SWITCH") {
		ParseElementPushLevel($e, GetNextLevel($e, $l), $ndr, $var_name, $env, $primitives, $deferred);
	}
}

#####################################################################
# parse scalars in a structure element
sub ParseElementPush($$$$$)
{
	my ($e,$ndr,$env,$primitives,$deferred) = @_;
	my $subndr = undef;

	my $var_name = $env->{$e->{NAME}};

	return unless $primitives or ($deferred and ContainsDeferred($e, $e->{LEVELS}[0]));

	# Representation type is different from transmit_as
	if ($e->{REPRESENTATION_TYPE} ne $e->{TYPE}) {
		pidl "{";
		indent;
		my $transmit_name = "_transmit_$e->{NAME}";
		pidl mapType($e->{TYPE}) ." $transmit_name;";
		pidl "NDR_CHECK(ndr_$e->{REPRESENTATION_TYPE}_to_$e->{TYPE}($var_name, " . get_pointer_to($transmit_name) . "));";
		$var_name = $transmit_name;
	}

	$var_name = append_prefix($e, $var_name);

	start_flags($e);

	if (defined(my $value = has_property($e, "value"))) {
		$var_name = ParseExpr($value, $env, $e->{ORIGINAL});
	}

	ParseElementPushLevel($e, $e->{LEVELS}[0], $ndr, $var_name, $env, $primitives, $deferred);

	end_flags($e);

	if ($e->{REPRESENTATION_TYPE} ne $e->{TYPE}) {
		deindent;
		pidl "}";
	}
}

#####################################################################
# parse a pointer in a struct element or function
sub ParsePtrPush($$$)
{
	my ($e,$l,$var_name) = @_;

	if ($l->{POINTER_TYPE} eq "ref") {
		pidl "if ($var_name == NULL) return NT_STATUS_INVALID_PARAMETER_MIX;";
		if ($l->{LEVEL} eq "EMBEDDED") {
			pidl "NDR_CHECK(ndr_push_ref_ptr(ndr));";
		}
	} elsif ($l->{POINTER_TYPE} eq "relative") {
		pidl "NDR_CHECK(ndr_push_relative_ptr1(ndr, $var_name));";
	} elsif ($l->{POINTER_TYPE} eq "unique") {
		pidl "NDR_CHECK(ndr_push_unique_ptr(ndr, $var_name));";
	} elsif ($l->{POINTER_TYPE} eq "full") {
		pidl "NDR_CHECK(ndr_push_full_ptr(ndr, $var_name));";
	} else {
		die("Unhandled pointer type $l->{POINTER_TYPE}");
	}
}

#####################################################################
# print scalars in a structure element
sub ParseElementPrint($$$)
{
	my($e, $var_name, $env) = @_;

	return if (has_property($e, "noprint"));

	if ($e->{REPRESENTATION_TYPE} ne $e->{TYPE}) {
		pidl "ndr_print_$e->{REPRESENTATION_TYPE}(ndr, \"$e->{NAME}\", $var_name);";
		return;
	}

	$var_name = append_prefix($e, $var_name);

	if (defined(my $value = has_property($e, "value"))) {
		$var_name = "(ndr->flags & LIBNDR_PRINT_SET_VALUES)?" . ParseExpr($value,$env, $e->{ORIGINAL}) . ":$var_name";
	}

	foreach my $l (@{$e->{LEVELS}}) {
		if ($l->{TYPE} eq "POINTER") {
			pidl "ndr_print_ptr(ndr, \"$e->{NAME}\", $var_name);";
			pidl "ndr->depth++;";
			if ($l->{POINTER_TYPE} ne "ref") {
				pidl "if ($var_name) {";
				indent;
			}
			$var_name = get_value_of($var_name);
		} elsif ($l->{TYPE} eq "ARRAY") {
			my $length;

			if ($l->{IS_CONFORMANT} or $l->{IS_VARYING}) {
				$var_name = get_pointer_to($var_name); 
			}
			
			if ($l->{IS_ZERO_TERMINATED}) {
				$length = "ndr_string_length($var_name, sizeof(*$var_name))";
			} else {
				$length = ParseExprExt($l->{LENGTH_IS}, $env, $e->{ORIGINAL}, 
							check_null_pointer($e, $env, \&pidl, "return;"), check_fully_dereferenced($e, $env));
			}

			if (is_charset_array($e,$l)) {
				pidl "ndr_print_string(ndr, \"$e->{NAME}\", $var_name);";
				last;
			} elsif (has_fast_array($e, $l)) {
				my $nl = GetNextLevel($e, $l);
				pidl "ndr_print_array_$nl->{DATA_TYPE}(ndr, \"$e->{NAME}\", $var_name, $length);";
				last;
			} else {
				my $counter = "cntr_$e->{NAME}_$l->{LEVEL_INDEX}";

				pidl "ndr->print(ndr, \"\%s: ARRAY(\%d)\", \"$e->{NAME}\", $length);";
				pidl 'ndr->depth++;';
				pidl "for ($counter=0;$counter<$length;$counter++) {";
				indent;
				pidl "char *idx_$l->{LEVEL_INDEX}=NULL;";
				pidl "asprintf(&idx_$l->{LEVEL_INDEX}, \"[\%d]\", $counter);";
				pidl "if (idx_$l->{LEVEL_INDEX}) {";
				indent;

				$var_name = $var_name . "[$counter]";
			}
		} elsif ($l->{TYPE} eq "DATA") {
			if (not Parse::Pidl::Typelist::is_scalar($l->{DATA_TYPE}) or Parse::Pidl::Typelist::scalar_is_reference($l->{DATA_TYPE})) {
				$var_name = get_pointer_to($var_name);
			}
			pidl "ndr_print_$l->{DATA_TYPE}(ndr, \"$e->{NAME}\", $var_name);";
		} elsif ($l->{TYPE} eq "SWITCH") {
			my $switch_var = ParseExprExt($l->{SWITCH_IS}, $env, $e->{ORIGINAL}, 
						check_null_pointer($e, $env, \&pidl, "return;"), check_fully_dereferenced($e, $env));
			pidl "ndr_print_set_switch_value(ndr, " . get_pointer_to($var_name) . ", $switch_var);";
		} 
	}

	foreach my $l (reverse @{$e->{LEVELS}}) {
		if ($l->{TYPE} eq "POINTER") {
			if ($l->{POINTER_TYPE} ne "ref") {
				deindent;
				pidl "}";
			}
			pidl "ndr->depth--;";
		} elsif (($l->{TYPE} eq "ARRAY")
			and not is_charset_array($e,$l)
			and not has_fast_array($e,$l)) {
			pidl "free(idx_$l->{LEVEL_INDEX});";
			deindent;
			pidl "}";
			deindent;
			pidl "}";
			pidl "ndr->depth--;";
		}
	}
}

#####################################################################
# parse scalars in a structure element - pull size
sub ParseSwitchPull($$$$$$)
{
	my($e,$l,$ndr,$var_name,$ndr_flags,$env) = @_;
	my $switch_var = ParseExprExt($l->{SWITCH_IS}, $env, $e->{ORIGINAL}, 
		check_null_pointer($e, $env, \&pidl, "return NT_STATUS_INVALID_PARAMETER_MIX;"), check_fully_dereferenced($e, $env));

	$var_name = get_pointer_to($var_name);
	pidl "NDR_CHECK(ndr_pull_set_switch_value($ndr, $var_name, $switch_var));";
}

#####################################################################
# push switch element
sub ParseSwitchPush($$$$$$)
{
	my($e,$l,$ndr,$var_name,$ndr_flags,$env) = @_;
	my $switch_var = ParseExprExt($l->{SWITCH_IS}, $env, $e->{ORIGINAL}, 
		check_null_pointer($e, $env, \&pidl, "return NT_STATUS_INVALID_PARAMETER_MIX;"), check_fully_dereferenced($e, $env));

	$var_name = get_pointer_to($var_name);
	pidl "NDR_CHECK(ndr_push_set_switch_value($ndr, $var_name, $switch_var));";
}

sub ParseDataPull($$$$$)
{
	my ($e,$l,$ndr,$var_name,$ndr_flags) = @_;

	if (Parse::Pidl::Typelist::scalar_is_reference($l->{DATA_TYPE})) {
		$var_name = get_pointer_to($var_name);
	}

	$var_name = get_pointer_to($var_name);

	pidl "NDR_CHECK(ndr_pull_$l->{DATA_TYPE}($ndr, $ndr_flags, $var_name));";

	if (my $range = has_property($e, "range")) {
		$var_name = get_value_of($var_name);
		my ($low, $high) = split(/ /, $range, 2);
		pidl "if ($var_name < $low || $var_name > $high) {";
		pidl "\treturn ndr_pull_error($ndr, NDR_ERR_RANGE, \"value out of range\");";
		pidl "}";
	}
}

sub ParseDataPush($$$$$)
{
	my ($e,$l,$ndr,$var_name,$ndr_flags) = @_;

	# strings are passed by value rather than reference
	if (not Parse::Pidl::Typelist::is_scalar($l->{DATA_TYPE}) or Parse::Pidl::Typelist::scalar_is_reference($l->{DATA_TYPE})) {
		$var_name = get_pointer_to($var_name);
	}

	pidl "NDR_CHECK(ndr_push_$l->{DATA_TYPE}($ndr, $ndr_flags, $var_name));";
}

sub CalcNdrFlags($$$)
{
	my ($l,$primitives,$deferred) = @_;

	my $scalars = 0;
	my $buffers = 0;

	# Add NDR_SCALARS if this one is deferred 
	# and deferreds may be pushed
	$scalars = 1 if ($l->{IS_DEFERRED} and $deferred);

	# Add NDR_SCALARS if this one is not deferred and 
	# primitives may be pushed
	$scalars = 1 if (!$l->{IS_DEFERRED} and $primitives);
	
	# Add NDR_BUFFERS if this one contains deferred stuff
	# and deferreds may be pushed
	$buffers = 1 if ($l->{CONTAINS_DEFERRED} and $deferred);

	return "NDR_SCALARS|NDR_BUFFERS" if ($scalars and $buffers);
	return "NDR_SCALARS" if ($scalars);
	return "NDR_BUFFERS" if ($buffers);
	return undef;
}

sub ParseMemCtxPullStart($$$)
{
	my ($e, $l, $ptr_name) = @_;

	my $mem_r_ctx = "_mem_save_$e->{NAME}_$l->{LEVEL_INDEX}";
	my $mem_c_ctx = $ptr_name;
	my $mem_c_flags = "0";

	return if ($l->{TYPE} eq "ARRAY" and $l->{IS_FIXED});

	if (($l->{TYPE} eq "POINTER") and ($l->{POINTER_TYPE} eq "ref")) {
		my $nl = GetNextLevel($e, $l);
		my $next_is_array = ($nl->{TYPE} eq "ARRAY");
		my $next_is_string = (($nl->{TYPE} eq "DATA") and 
					($nl->{DATA_TYPE} eq "string"));
		if ($next_is_array or $next_is_string) {
			return;
		} else {
			$mem_c_flags = "LIBNDR_FLAG_REF_ALLOC";
		}
	}

	pidl "$mem_r_ctx = NDR_PULL_GET_MEM_CTX(ndr);";
	pidl "NDR_PULL_SET_MEM_CTX(ndr, $mem_c_ctx, $mem_c_flags);";
}

sub ParseMemCtxPullEnd($$)
{
	my $e = shift;
	my $l = shift;

	my $mem_r_ctx = "_mem_save_$e->{NAME}_$l->{LEVEL_INDEX}";
	my $mem_r_flags = "0";

	return if ($l->{TYPE} eq "ARRAY" and $l->{IS_FIXED});

	if (($l->{TYPE} eq "POINTER") and ($l->{POINTER_TYPE} eq "ref")) {
		my $nl = GetNextLevel($e, $l);
		my $next_is_array = ($nl->{TYPE} eq "ARRAY");
		my $next_is_string = (($nl->{TYPE} eq "DATA") and 
					($nl->{DATA_TYPE} eq "string"));
		if ($next_is_array or $next_is_string) {
			return;
		} else {
			$mem_r_flags = "LIBNDR_FLAG_REF_ALLOC";
		}
	}

	pidl "NDR_PULL_SET_MEM_CTX(ndr, $mem_r_ctx, $mem_r_flags);";
}

sub CheckStringTerminator($$$$)
{
	my ($ndr,$e,$l,$length) = @_;
	my $nl = GetNextLevel($e, $l);

	# Make sure last element is zero!
	pidl "NDR_CHECK(ndr_check_string_terminator($ndr, $length, sizeof($nl->{DATA_TYPE}_t)));";
}

sub ParseElementPullLevel
{
	my($e,$l,$ndr,$var_name,$env,$primitives,$deferred) = @_;

	my $ndr_flags = CalcNdrFlags($l, $primitives, $deferred);

	if ($l->{TYPE} eq "ARRAY" and ($l->{IS_VARYING} or $l->{IS_CONFORMANT})) {
		$var_name = get_pointer_to($var_name);
	}

	# Only pull something if there's actually something to be pulled
	if (defined($ndr_flags)) {
		if ($l->{TYPE} eq "SUBCONTEXT") {
			my $subndr = ParseSubcontextPullStart($e, $l, $ndr, $env);
			ParseElementPullLevel($e, GetNextLevel($e,$l), $subndr, $var_name, $env, 1, 1);
			ParseSubcontextPullEnd($e, $l, $ndr, $env);
		} elsif ($l->{TYPE} eq "ARRAY") {
			my $length = ParseArrayPullHeader($e, $l, $ndr, $var_name, $env);

			my $nl = GetNextLevel($e, $l);

			if (is_charset_array($e,$l)) {
				if ($l->{IS_ZERO_TERMINATED}) {
					CheckStringTerminator($ndr, $e, $l, $length);
				}
				pidl "NDR_CHECK(ndr_pull_charset($ndr, $ndr_flags, ".get_pointer_to($var_name).", $length, sizeof(" . mapType($nl->{DATA_TYPE}) . "), CH_$e->{PROPERTIES}->{charset}));";
				return;
			} elsif (has_fast_array($e, $l)) {
				if ($l->{IS_ZERO_TERMINATED}) {
					CheckStringTerminator($ndr,$e,$l,$length);
				}
				pidl "NDR_CHECK(ndr_pull_array_$nl->{DATA_TYPE}($ndr, $ndr_flags, $var_name, $length));";
				return;
			}
		} elsif ($l->{TYPE} eq "POINTER") {
			ParsePtrPull($e, $l, $ndr, $var_name);
		} elsif ($l->{TYPE} eq "SWITCH") {
			ParseSwitchPull($e, $l, $ndr, $var_name, $ndr_flags, $env);
		} elsif ($l->{TYPE} eq "DATA") {
			ParseDataPull($e, $l, $ndr, $var_name, $ndr_flags);
		}
	}

	# add additional constructions
	if ($l->{TYPE} eq "POINTER" and $deferred) {
		if ($l->{POINTER_TYPE} ne "ref") {
			pidl "if ($var_name) {";
			indent;

			if ($l->{POINTER_TYPE} eq "relative") {
				pidl "struct ndr_pull_save _relative_save;";
				pidl "ndr_pull_save(ndr, &_relative_save);";
				pidl "NDR_CHECK(ndr_pull_relative_ptr2(ndr, $var_name));";
			}
		}

		ParseMemCtxPullStart($e, $l, $var_name);

		$var_name = get_value_of($var_name);
		ParseElementPullLevel($e, GetNextLevel($e,$l), $ndr, $var_name, $env, 1, 1);

		ParseMemCtxPullEnd($e,$l);

		if ($l->{POINTER_TYPE} ne "ref") {
    			if ($l->{POINTER_TYPE} eq "relative") {
				pidl "ndr_pull_restore(ndr, &_relative_save);";
			}
			deindent;
			pidl "}";
		}
	} elsif ($l->{TYPE} eq "ARRAY" and 
			not has_fast_array($e,$l) and not is_charset_array($e, $l)) {
		my $length = ParseExpr($l->{LENGTH_IS}, $env, $e->{ORIGINAL});
		my $counter = "cntr_$e->{NAME}_$l->{LEVEL_INDEX}";
		my $array_name = $var_name;

		$var_name = $var_name . "[$counter]";

		ParseMemCtxPullStart($e, $l, $array_name);

		if (($primitives and not $l->{IS_DEFERRED}) or ($deferred and $l->{IS_DEFERRED})) {
			my $nl = GetNextLevel($e,$l);

			if ($l->{IS_ZERO_TERMINATED}) {
				CheckStringTerminator($ndr,$e,$l,$length);
			}

			pidl "for ($counter = 0; $counter < $length; $counter++) {";
			indent;
			ParseElementPullLevel($e, $nl, $ndr, $var_name, $env, 1, 0);
			deindent;
			pidl "}";
		}

		if ($deferred and ContainsDeferred($e, $l)) {
			pidl "for ($counter = 0; $counter < $length; $counter++) {";
			indent;
			ParseElementPullLevel($e,GetNextLevel($e,$l), $ndr, $var_name, $env, 0, 1);
			deindent;
			pidl "}";
		}

		ParseMemCtxPullEnd($e, $l);

	} elsif ($l->{TYPE} eq "SWITCH") {
		ParseElementPullLevel($e, GetNextLevel($e,$l), $ndr, $var_name, $env, $primitives, $deferred);
	}
}

#####################################################################
# parse scalars in a structure element - pull size
sub ParseElementPull($$$$$)
{
	my($e,$ndr,$env,$primitives,$deferred) = @_;

	my $var_name = $env->{$e->{NAME}};
	my $represent_name;
	my $transmit_name;

	return unless $primitives or ($deferred and ContainsDeferred($e, $e->{LEVELS}[0]));

	if ($e->{REPRESENTATION_TYPE} ne $e->{TYPE}) {
		pidl "{";
		indent;
		$represent_name = $var_name;
		$transmit_name = "_transmit_$e->{NAME}";
		$var_name = $transmit_name;
		pidl mapType($e->{TYPE})." $var_name;";
	}

	$var_name = append_prefix($e, $var_name);

	start_flags($e);

	ParseElementPullLevel($e,$e->{LEVELS}[0],$ndr,$var_name,$env,$primitives,$deferred);

	end_flags($e);

	# Representation type is different from transmit_as
	if ($e->{REPRESENTATION_TYPE} ne $e->{TYPE}) {
		pidl "NDR_CHECK(ndr_$e->{TYPE}_to_$e->{REPRESENTATION_TYPE}($transmit_name, ".get_pointer_to($represent_name)."));";
		deindent;
		pidl "}";
	}
}

#####################################################################
# parse a pointer in a struct element or function
sub ParsePtrPull($$$$)
{
	my($e,$l,$ndr,$var_name) = @_;

	my $nl = GetNextLevel($e, $l);
	my $next_is_array = ($nl->{TYPE} eq "ARRAY");
	my $next_is_string = (($nl->{TYPE} eq "DATA") and 
						 ($nl->{DATA_TYPE} eq "string"));

	if ($l->{POINTER_TYPE} eq "ref") {
		if ($l->{LEVEL} eq "EMBEDDED") {
			pidl "NDR_CHECK(ndr_pull_ref_ptr($ndr, &_ptr_$e->{NAME}));";
		}

		if (!$next_is_array and !$next_is_string) {
			pidl "if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {";
			pidl "\tNDR_PULL_ALLOC($ndr, $var_name);"; 
			pidl "}";
		}
		
		return;
	} elsif (($l->{POINTER_TYPE} eq "unique") or 
		 ($l->{POINTER_TYPE} eq "relative") or
		 ($l->{POINTER_TYPE} eq "full")) {
		pidl "NDR_CHECK(ndr_pull_generic_ptr($ndr, &_ptr_$e->{NAME}));";
		pidl "if (_ptr_$e->{NAME}) {";
		indent;
	} else {
		die("Unhandled pointer type $l->{POINTER_TYPE}");
	}

	# Don't do this for arrays, they're allocated at the actual level 
	# of the array
	unless ($next_is_array or $next_is_string) { 
		pidl "NDR_PULL_ALLOC($ndr, $var_name);"; 
	} else {
		# FIXME: Yes, this is nasty.
		# We allocate an array twice
		# - once just to indicate that it's there,
		# - then the real allocation...
		pidl "NDR_PULL_ALLOC($ndr, $var_name);";
	}

	#pidl "memset($var_name, 0, sizeof($var_name));";
	if ($l->{POINTER_TYPE} eq "relative") {
		pidl "NDR_CHECK(ndr_pull_relative_ptr1($ndr, $var_name, _ptr_$e->{NAME}));";
	}
	deindent;
	pidl "} else {";
	pidl "\t$var_name = NULL;";
	pidl "}";
}

#####################################################################
# parse a struct
sub ParseStructPush($$)
{
	my($struct,$name) = @_;
	
	return unless defined($struct->{ELEMENTS});

	my $env = GenerateStructEnv($struct);

	EnvSubstituteValue($env, $struct);

	# save the old relative_base_offset
	pidl "uint32_t _save_relative_base_offset = ndr_push_get_relative_base_offset(ndr);" if defined($struct->{PROPERTIES}{relative_base});

	foreach my $e (@{$struct->{ELEMENTS}}) { 
		DeclareArrayVariables($e);
	}

	start_flags($struct);

	# see if the structure contains a conformant array. If it
	# does, then it must be the last element of the structure, and
	# we need to push the conformant length early, as it fits on
	# the wire before the structure (and even before the structure
	# alignment)
	if (defined($struct->{SURROUNDING_ELEMENT})) {
		my $e = $struct->{SURROUNDING_ELEMENT};

		if (defined($e->{LEVELS}[0]) and 
			$e->{LEVELS}[0]->{TYPE} eq "ARRAY") {
			my $size;
			
			if ($e->{LEVELS}[0]->{IS_ZERO_TERMINATED}) {
				if (has_property($e, "charset")) {
					$size = "ndr_charset_length(r->$e->{NAME}, CH_$e->{PROPERTIES}->{charset})";
				} else {
					$size = "ndr_string_length(r->$e->{NAME}, sizeof(*r->$e->{NAME}))";
				}
			} else {
				$size = ParseExpr($e->{LEVELS}[0]->{SIZE_IS}, $env, $e->{ORIGINAL});
			}

			pidl "NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, $size));";
		} else {
			pidl "NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, ndr_string_array_size(ndr, r->$e->{NAME})));";
		}
	}

	pidl "if (ndr_flags & NDR_SCALARS) {";
	indent;

	pidl "NDR_CHECK(ndr_push_align(ndr, $struct->{ALIGN}));";

	if (defined($struct->{PROPERTIES}{relative_base})) {
		# set the current offset as base for relative pointers
		# and store it based on the toplevel struct/union
		pidl "NDR_CHECK(ndr_push_setup_relative_base_offset1(ndr, r, ndr->offset));";
	}

	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPush($e, "ndr", $env, 1, 0);
	}	

	deindent;
	pidl "}";

	pidl "if (ndr_flags & NDR_BUFFERS) {";
	indent;
	if (defined($struct->{PROPERTIES}{relative_base})) {
		# retrieve the current offset as base for relative pointers
		# based on the toplevel struct/union
		pidl "NDR_CHECK(ndr_push_setup_relative_base_offset2(ndr, r));";
	}
	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPush($e, "ndr", $env, 0, 1);
	}

	deindent;
	pidl "}";

	end_flags($struct);
	# restore the old relative_base_offset
	pidl "ndr_push_restore_relative_base_offset(ndr, _save_relative_base_offset);" if defined($struct->{PROPERTIES}{relative_base});
}

#####################################################################
# generate a push function for an enum
sub ParseEnumPush($$)
{
	my($enum,$name) = @_;
	my($type_fn) = $enum->{BASE_TYPE};

	start_flags($enum);
	pidl "NDR_CHECK(ndr_push_$type_fn(ndr, NDR_SCALARS, r));";
	end_flags($enum);
}

#####################################################################
# generate a pull function for an enum
sub ParseEnumPull($$)
{
	my($enum,$name) = @_;
	my($type_fn) = $enum->{BASE_TYPE};
	my($type_v_decl) = mapType($type_fn);

	pidl "$type_v_decl v;";
	start_flags($enum);
	pidl "NDR_CHECK(ndr_pull_$type_fn(ndr, NDR_SCALARS, &v));";
	pidl "*r = v;";

	end_flags($enum);
}

#####################################################################
# generate a print function for an enum
sub ParseEnumPrint($$)
{
	my($enum,$name) = @_;

	pidl "const char *val = NULL;";
	pidl "";

	start_flags($enum);

	pidl "switch (r) {";
	indent;
	my $els = \@{$enum->{ELEMENTS}};
	foreach my $i (0 .. $#{$els}) {
		my $e = ${$els}[$i];
		chomp $e;
		if ($e =~ /^(.*)=/) {
			$e = $1;
		}
		pidl "case $e: val = \"$e\"; break;";
	}

	deindent;
	pidl "}";
	
	pidl "ndr_print_enum(ndr, name, \"$enum->{TYPE}\", val, r);";

	end_flags($enum);
}

sub DeclEnum($$$)
{
	my ($e,$t,$name) = @_;
	return "enum $name " . 
		($t eq "pull"?"*":"") . "r";
}

$typefamily{ENUM} = {
	DECL => \&DeclEnum,
	PUSH_FN_BODY => \&ParseEnumPush,
	PULL_FN_BODY => \&ParseEnumPull,
	PRINT_FN_BODY => \&ParseEnumPrint,
};

#####################################################################
# generate a push function for a bitmap
sub ParseBitmapPush($$)
{
	my($bitmap,$name) = @_;
	my($type_fn) = $bitmap->{BASE_TYPE};

	start_flags($bitmap);

	pidl "NDR_CHECK(ndr_push_$type_fn(ndr, NDR_SCALARS, r));";

	end_flags($bitmap);
}

#####################################################################
# generate a pull function for an bitmap
sub ParseBitmapPull($$)
{
	my($bitmap,$name) = @_;
	my $type_fn = $bitmap->{BASE_TYPE};
	my($type_decl) = mapType($bitmap->{BASE_TYPE});

	pidl "$type_decl v;";
	start_flags($bitmap);
	pidl "NDR_CHECK(ndr_pull_$type_fn(ndr, NDR_SCALARS, &v));";
	pidl "*r = v;";

	end_flags($bitmap);
}

#####################################################################
# generate a print function for an bitmap
sub ParseBitmapPrintElement($$$)
{
	my($e,$bitmap,$name) = @_;
	my($type_decl) = mapType($bitmap->{BASE_TYPE});
	my($type_fn) = $bitmap->{BASE_TYPE};
	my($flag);

	if ($e =~ /^(\w+) .*$/) {
		$flag = "$1";
	} else {
		die "Bitmap: \"$name\" invalid Flag: \"$e\"";
	}

	pidl "ndr_print_bitmap_flag(ndr, sizeof($type_decl), \"$flag\", $flag, r);";
}

#####################################################################
# generate a print function for an bitmap
sub ParseBitmapPrint($$)
{
	my($bitmap,$name) = @_;
	my($type_decl) = mapType($bitmap->{TYPE});
	my($type_fn) = $bitmap->{BASE_TYPE};

	start_flags($bitmap);

	pidl "ndr_print_$type_fn(ndr, name, r);";

	pidl "ndr->depth++;";
	foreach my $e (@{$bitmap->{ELEMENTS}}) {
		ParseBitmapPrintElement($e, $bitmap, $name);
	}
	pidl "ndr->depth--;";

	end_flags($bitmap);
}

sub DeclBitmap($$$)
{
	my ($e,$t,$name) = @_;
	return mapType(Parse::Pidl::Typelist::bitmap_type_fn($e)) . 
		($t eq "pull"?" *":" ") . "r";
}

$typefamily{BITMAP} = {
	DECL => \&DeclBitmap,
	PUSH_FN_BODY => \&ParseBitmapPush,
	PULL_FN_BODY => \&ParseBitmapPull,
	PRINT_FN_BODY => \&ParseBitmapPrint,
};

#####################################################################
# generate a struct print function
sub ParseStructPrint($$)
{
	my($struct,$name) = @_;

	return unless defined $struct->{ELEMENTS};

	my $env = GenerateStructEnv($struct);

	EnvSubstituteValue($env, $struct);

	DeclareArrayVariables($_) foreach (@{$struct->{ELEMENTS}});

	pidl "ndr_print_struct(ndr, name, \"$name\");";

	start_flags($struct);

	pidl "ndr->depth++;";
	
	ParseElementPrint($_, "r->$_->{NAME}", $env) foreach (@{$struct->{ELEMENTS}});
	pidl "ndr->depth--;";

	end_flags($struct);
}

sub DeclarePtrVariables($)
{
	my $e = shift;
	foreach my $l (@{$e->{LEVELS}}) {
		if ($l->{TYPE} eq "POINTER" and 
			not ($l->{POINTER_TYPE} eq "ref" and $l->{LEVEL} eq "TOP")) {
			pidl "uint32_t _ptr_$e->{NAME};";
			last;
		}
	}
}

sub DeclareArrayVariables($)
{
	my $e = shift;

	foreach my $l (@{$e->{LEVELS}}) {
		next if has_fast_array($e,$l);
		next if is_charset_array($e,$l);
		if ($l->{TYPE} eq "ARRAY") {
			pidl "uint32_t cntr_$e->{NAME}_$l->{LEVEL_INDEX};";
		}
	}
}

sub need_decl_mem_ctx($$)
{
	my ($e,$l) = @_;

	return 0 if has_fast_array($e,$l);
	return 0 if is_charset_array($e,$l);
	return 1 if (($l->{TYPE} eq "ARRAY") and not $l->{IS_FIXED});

	if (($l->{TYPE} eq "POINTER") and ($l->{POINTER_TYPE} eq "ref")) {
		my $nl = GetNextLevel($e, $l);
		my $next_is_array = ($nl->{TYPE} eq "ARRAY");
		my $next_is_string = (($nl->{TYPE} eq "DATA") and 
					($nl->{DATA_TYPE} eq "string"));
		return 0 if ($next_is_array or $next_is_string);
	}
	return 1 if ($l->{TYPE} eq "POINTER");

	return 0;
}

sub DeclareMemCtxVariables($)
{
	my $e = shift;
	foreach my $l (@{$e->{LEVELS}}) {
		if (need_decl_mem_ctx($e, $l)) {
			pidl "TALLOC_CTX *_mem_save_$e->{NAME}_$l->{LEVEL_INDEX};";
		}
	}
}

#####################################################################
# parse a struct - pull side
sub ParseStructPull($$)
{
	my($struct,$name) = @_;

	return unless defined $struct->{ELEMENTS};

	my $env = GenerateStructEnv($struct);

	# declare any internal pointers we need
	foreach my $e (@{$struct->{ELEMENTS}}) {
		DeclarePtrVariables($e);
		DeclareArrayVariables($e);
		DeclareMemCtxVariables($e);
	}

	# save the old relative_base_offset
	pidl "uint32_t _save_relative_base_offset = ndr_pull_get_relative_base_offset(ndr);" if defined($struct->{PROPERTIES}{relative_base});

	start_flags($struct);

	pidl "if (ndr_flags & NDR_SCALARS) {";
	indent;

	if (defined $struct->{SURROUNDING_ELEMENT}) {
		pidl "NDR_CHECK(ndr_pull_array_size(ndr, &r->$struct->{SURROUNDING_ELEMENT}->{NAME}));";
	}

	pidl "NDR_CHECK(ndr_pull_align(ndr, $struct->{ALIGN}));";

	if (defined($struct->{PROPERTIES}{relative_base})) {
		# set the current offset as base for relative pointers
		# and store it based on the toplevel struct/union
		pidl "NDR_CHECK(ndr_pull_setup_relative_base_offset1(ndr, r, ndr->offset));";
	}

	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPull($e, "ndr", $env, 1, 0);
	}	

	add_deferred();

	deindent;
	pidl "}";
	pidl "if (ndr_flags & NDR_BUFFERS) {";
	indent;
	if (defined($struct->{PROPERTIES}{relative_base})) {
		# retrieve the current offset as base for relative pointers
		# based on the toplevel struct/union
		pidl "NDR_CHECK(ndr_pull_setup_relative_base_offset2(ndr, r));";
	}
	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPull($e, "ndr", $env, 0, 1);
	}

	add_deferred();

	deindent;
	pidl "}";

	end_flags($struct);
	# restore the old relative_base_offset
	pidl "ndr_pull_restore_relative_base_offset(ndr, _save_relative_base_offset);" if defined($struct->{PROPERTIES}{relative_base});
}

#####################################################################
# calculate size of ndr struct
sub ParseStructNdrSize($$)
{
	my ($t, $name) = @_;
	my $sizevar;

	if (my $flags = has_property($t, "flag")) {
		pidl "flags |= $flags;";
	}
	pidl "return ndr_size_struct(r, flags, (ndr_push_flags_fn_t)ndr_push_$name);";
}

sub DeclStruct($$$)
{
	my ($e,$t,$name) = @_;
	return ($t ne "pull"?"const ":"") . "struct $name *r";
}

sub ArgsStructNdrSize($$)
{
	my ($d, $name) = @_;
	return "const struct $name *r, int flags";
}

$typefamily{STRUCT} = {
	PUSH_FN_BODY => \&ParseStructPush,
	DECL => \&DeclStruct,
	PULL_FN_BODY => \&ParseStructPull,
	PRINT_FN_BODY => \&ParseStructPrint,
	SIZE_FN_BODY => \&ParseStructNdrSize,
	SIZE_FN_ARGS => \&ArgsStructNdrSize,
};

#####################################################################
# calculate size of ndr struct
sub ParseUnionNdrSize($$)
{
	my ($t, $name) = @_;
	my $sizevar;

	if (my $flags = has_property($t, "flag")) {
		pidl "flags |= $flags;";
	}

	pidl "return ndr_size_union(r, flags, level, (ndr_push_flags_fn_t)ndr_push_$name);";
}

#####################################################################
# parse a union - push side
sub ParseUnionPush($$)
{
	my ($e,$name) = @_;
	my $have_default = 0;

	# save the old relative_base_offset
	pidl "uint32_t _save_relative_base_offset = ndr_push_get_relative_base_offset(ndr);" if defined($e->{PROPERTIES}{relative_base});
	pidl "int level;";

	start_flags($e);

	pidl "level = ndr_push_get_switch_value(ndr, r);";

	pidl "if (ndr_flags & NDR_SCALARS) {";
	indent;

	if (defined($e->{SWITCH_TYPE})) {
		pidl "NDR_CHECK(ndr_push_$e->{SWITCH_TYPE}(ndr, NDR_SCALARS, level));";
	}

	pidl "switch (level) {";
	indent;
	foreach my $el (@{$e->{ELEMENTS}}) {
		if ($el->{CASE} eq "default") {
			$have_default = 1;
		}
		pidl "$el->{CASE}:";

		if ($el->{TYPE} ne "EMPTY") {
			indent;
			if (defined($e->{PROPERTIES}{relative_base})) {
				pidl "NDR_CHECK(ndr_push_align(ndr, $el->{ALIGN}));";
				# set the current offset as base for relative pointers
				# and store it based on the toplevel struct/union
				pidl "NDR_CHECK(ndr_push_setup_relative_base_offset1(ndr, r, ndr->offset));";
			}
			DeclareArrayVariables($el);
			ParseElementPush($el, "ndr", {$el->{NAME} => "r->$el->{NAME}"}, 1, 0);
			deindent;
		}
		pidl "break;";
		pidl "";
	}
	if (! $have_default) {
		pidl "default:";
		pidl "\treturn ndr_push_error(ndr, NDR_ERR_BAD_SWITCH, \"Bad switch value \%u\", level);";
	}
	deindent;
	pidl "}";
	deindent;
	pidl "}";
	pidl "if (ndr_flags & NDR_BUFFERS) {";
	indent;
	if (defined($e->{PROPERTIES}{relative_base})) {
		# retrieve the current offset as base for relative pointers
		# based on the toplevel struct/union
		pidl "NDR_CHECK(ndr_push_setup_relative_base_offset2(ndr, r));";
	}
	pidl "switch (level) {";
	indent;
	foreach my $el (@{$e->{ELEMENTS}}) {
		pidl "$el->{CASE}:";
		if ($el->{TYPE} ne "EMPTY") {
			indent;
			ParseElementPush($el, "ndr", {$el->{NAME} => "r->$el->{NAME}"}, 0, 1);
			deindent;
		}
		pidl "break;";
		pidl "";
	}
	if (! $have_default) {
		pidl "default:";
		pidl "\treturn ndr_push_error(ndr, NDR_ERR_BAD_SWITCH, \"Bad switch value \%u\", level);";
	}
	deindent;
	pidl "}";

	deindent;
	pidl "}";
	end_flags($e);
	# restore the old relative_base_offset
	pidl "ndr_push_restore_relative_base_offset(ndr, _save_relative_base_offset);" if defined($e->{PROPERTIES}{relative_base});
}

#####################################################################
# print a union
sub ParseUnionPrint($$)
{
	my ($e,$name) = @_;
	my $have_default = 0;

	pidl "int level;";
	foreach my $el (@{$e->{ELEMENTS}}) {
		DeclareArrayVariables($el);
	}

	start_flags($e);

	pidl "level = ndr_print_get_switch_value(ndr, r);";

	pidl "ndr_print_union(ndr, name, level, \"$name\");";

	pidl "switch (level) {";
	indent;
	foreach my $el (@{$e->{ELEMENTS}}) {
		if ($el->{CASE} eq "default") {
			$have_default = 1;
		}
		pidl "$el->{CASE}:";
		if ($el->{TYPE} ne "EMPTY") {
			indent;
			ParseElementPrint($el, "r->$el->{NAME}", {});
			deindent;
		}
		pidl "break;";
		pidl "";
	}
	if (! $have_default) {
		pidl "default:";
		pidl "\tndr_print_bad_level(ndr, name, level);";
	}
	deindent;
	pidl "}";

	end_flags($e);
}

#####################################################################
# parse a union - pull side
sub ParseUnionPull($$)
{
	my ($e,$name) = @_;
	my $have_default = 0;
	my $switch_type = $e->{SWITCH_TYPE};

	# save the old relative_base_offset
	pidl "uint32_t _save_relative_base_offset = ndr_pull_get_relative_base_offset(ndr);" if defined($e->{PROPERTIES}{relative_base});
	pidl "int level;";
	if (defined($switch_type)) {
		if (Parse::Pidl::Typelist::typeIs($switch_type, "ENUM")) {
			$switch_type = Parse::Pidl::Typelist::enum_type_fn(getType($switch_type)->{DATA});
		}
		pidl mapType($switch_type) . " _level;";
	}

	my %double_cases = ();
	foreach my $el (@{$e->{ELEMENTS}}) {
		next if ($el->{TYPE} eq "EMPTY");
		next if ($double_cases{"$el->{NAME}"});
		DeclareMemCtxVariables($el);
		$double_cases{"$el->{NAME}"} = 1;
	}

	start_flags($e);

	pidl "level = ndr_pull_get_switch_value(ndr, r);";

	pidl "if (ndr_flags & NDR_SCALARS) {";
	indent;

	if (defined($switch_type)) {
		pidl "NDR_CHECK(ndr_pull_$switch_type(ndr, NDR_SCALARS, &_level));";
		pidl "if (_level != level) {"; 
		pidl "\treturn ndr_pull_error(ndr, NDR_ERR_BAD_SWITCH, \"Bad switch value %u for $name\", _level);";
		pidl "}";
	}

	pidl "switch (level) {";
	indent;
	foreach my $el (@{$e->{ELEMENTS}}) {
		if ($el->{CASE} eq "default") {
			$have_default = 1;
		} 
		pidl "$el->{CASE}: {";

		if ($el->{TYPE} ne "EMPTY") {
			indent;
			DeclarePtrVariables($el);
			DeclareArrayVariables($el);
			if (defined($e->{PROPERTIES}{relative_base})) {
				pidl "NDR_CHECK(ndr_pull_align(ndr, $el->{ALIGN}));";
				# set the current offset as base for relative pointers
				# and store it based on the toplevel struct/union
				pidl "NDR_CHECK(ndr_pull_setup_relative_base_offset1(ndr, r, ndr->offset));";
			}
			ParseElementPull($el, "ndr", {$el->{NAME} => "r->$el->{NAME}"}, 1, 0);
			deindent;
		}
		pidl "break; }";
		pidl "";
	}
	if (! $have_default) {
		pidl "default:";
		pidl "\treturn ndr_pull_error(ndr, NDR_ERR_BAD_SWITCH, \"Bad switch value \%u\", level);";
	}
	deindent;
	pidl "}";
	deindent;
	pidl "}";
	pidl "if (ndr_flags & NDR_BUFFERS) {";
	indent;
	if (defined($e->{PROPERTIES}{relative_base})) {
		# retrieve the current offset as base for relative pointers
		# based on the toplevel struct/union
		pidl "NDR_CHECK(ndr_pull_setup_relative_base_offset2(ndr, r));";
	}
	pidl "switch (level) {";
	indent;
	foreach my $el (@{$e->{ELEMENTS}}) {
		pidl "$el->{CASE}:";
		if ($el->{TYPE} ne "EMPTY") {
			indent;
			ParseElementPull($el, "ndr", {$el->{NAME} => "r->$el->{NAME}"}, 0, 1);
			deindent;
		}
		pidl "break;";
		pidl "";
	}
	if (! $have_default) {
		pidl "default:";
		pidl "\treturn ndr_pull_error(ndr, NDR_ERR_BAD_SWITCH, \"Bad switch value \%u\", level);";
	}
	deindent;
	pidl "}";

	deindent;
	pidl "}";

	add_deferred();

	end_flags($e);
	# restore the old relative_base_offset
	pidl "ndr_pull_restore_relative_base_offset(ndr, _save_relative_base_offset);" if defined($e->{PROPERTIES}{relative_base});
}

sub DeclUnion($$$)
{
	my ($e,$t,$name) = @_;
	return ($t ne "pull"?"const ":"") . "union $name *r";
}

sub ArgsUnionNdrSize($$)
{
	my ($d,$name) = @_;
	return "const union $name *r, uint32_t level, int flags";
}

$typefamily{UNION} = {
	PUSH_FN_BODY => \&ParseUnionPush,
	DECL => \&DeclUnion,
	PULL_FN_BODY => \&ParseUnionPull,
	PRINT_FN_BODY => \&ParseUnionPrint,
	SIZE_FN_ARGS => \&ArgsUnionNdrSize,
	SIZE_FN_BODY => \&ParseUnionNdrSize,
};
	
#####################################################################
# parse a typedef - push side
sub ParseTypedefPush($$)
{
	my($e,$name) = @_;

	$typefamily{$e->{DATA}->{TYPE}}->{PUSH_FN_BODY}->($e->{DATA}, $name);
}

#####################################################################
# parse a typedef - pull side
sub ParseTypedefPull($$)
{
	my($e,$name) = @_;

	$typefamily{$e->{DATA}->{TYPE}}->{PULL_FN_BODY}->($e->{DATA}, $name);
}

#####################################################################
# parse a typedef - print side
sub ParseTypedefPrint($$)
{
	my($e,$name) = @_;

	$typefamily{$e->{DATA}->{TYPE}}->{PRINT_FN_BODY}->($e->{DATA}, $name);
}

#####################################################################
## calculate the size of a structure
sub ParseTypedefNdrSize($$)
{
	my($t,$name) = @_;

	$typefamily{$t->{DATA}->{TYPE}}->{SIZE_FN_BODY}->($t->{DATA}, $name);
}

sub DeclTypedef($$$)
{
	my ($e, $t, $name) = @_;
	
	return $typefamily{$e->{DATA}->{TYPE}}->{DECL}->($e->{DATA}, $t, $name);
}

sub ArgsTypedefNdrSize($$)
{
	my ($d, $name) = @_;
	return $typefamily{$d->{DATA}->{TYPE}}->{SIZE_FN_ARGS}->($d->{DATA}, $name);
}

$typefamily{TYPEDEF} = {
	PUSH_FN_BODY => \&ParseTypedefPush,
	DECL => \&DeclTypedef,
	PULL_FN_BODY => \&ParseTypedefPull,
	PRINT_FN_BODY => \&ParseTypedefPrint,
	SIZE_FN_ARGS => \&ArgsTypedefNdrSize,
	SIZE_FN_BODY => \&ParseTypedefNdrSize,
};

#####################################################################
# parse a function - print side
sub ParseFunctionPrint($)
{
	my($fn) = shift;

	pidl_hdr "void ndr_print_$fn->{NAME}(struct ndr_print *ndr, const char *name, int flags, const struct $fn->{NAME} *r);";

	return if has_property($fn, "noprint");

	pidl "_PUBLIC_ void ndr_print_$fn->{NAME}(struct ndr_print *ndr, const char *name, int flags, const struct $fn->{NAME} *r)";
	pidl "{";
	indent;

	foreach my $e (@{$fn->{ELEMENTS}}) {
		DeclareArrayVariables($e);
	}

	pidl "ndr_print_struct(ndr, name, \"$fn->{NAME}\");";
	pidl "ndr->depth++;";

	pidl "if (flags & NDR_SET_VALUES) {";
	pidl "\tndr->flags |= LIBNDR_PRINT_SET_VALUES;";
	pidl "}";

	pidl "if (flags & NDR_IN) {";
	indent;
	pidl "ndr_print_struct(ndr, \"in\", \"$fn->{NAME}\");";
	pidl "ndr->depth++;";

	my $env = GenerateFunctionInEnv($fn);
	EnvSubstituteValue($env, $fn);

	foreach my $e (@{$fn->{ELEMENTS}}) {
		if (grep(/in/,@{$e->{DIRECTION}})) {
			ParseElementPrint($e, $env->{$e->{NAME}}, $env);
		}
	}
	pidl "ndr->depth--;";
	deindent;
	pidl "}";
	
	pidl "if (flags & NDR_OUT) {";
	indent;
	pidl "ndr_print_struct(ndr, \"out\", \"$fn->{NAME}\");";
	pidl "ndr->depth++;";

	$env = GenerateFunctionOutEnv($fn);
	foreach my $e (@{$fn->{ELEMENTS}}) {
		if (grep(/out/,@{$e->{DIRECTION}})) {
			ParseElementPrint($e, $env->{$e->{NAME}}, $env);
		}
	}
	if ($fn->{RETURN_TYPE}) {
		pidl "ndr_print_$fn->{RETURN_TYPE}(ndr, \"result\", r->out.result);";
	}
	pidl "ndr->depth--;";
	deindent;
	pidl "}";
	
	pidl "ndr->depth--;";
	deindent;
	pidl "}";
	pidl "";
}

#####################################################################
# parse a function
sub ParseFunctionPush($)
{ 
	my($fn) = shift;

	fn_declare("push", $fn, "NTSTATUS ndr_push_$fn->{NAME}(struct ndr_push *ndr, int flags, const struct $fn->{NAME} *r)") or return;

	return if has_property($fn, "nopush");

	pidl "{";
	indent;

	foreach my $e (@{$fn->{ELEMENTS}}) { 
		DeclareArrayVariables($e);
	}

	pidl "if (flags & NDR_IN) {";
	indent;

	my $env = GenerateFunctionInEnv($fn);

	EnvSubstituteValue($env, $fn);

	foreach my $e (@{$fn->{ELEMENTS}}) {
		if (grep(/in/,@{$e->{DIRECTION}})) {
			ParseElementPush($e, "ndr", $env, 1, 1);
		}
	}

	deindent;
	pidl "}";

	pidl "if (flags & NDR_OUT) {";
	indent;

	$env = GenerateFunctionOutEnv($fn);
	foreach my $e (@{$fn->{ELEMENTS}}) {
		if (grep(/out/,@{$e->{DIRECTION}})) {
			ParseElementPush($e, "ndr", $env, 1, 1);
		}
	}

	if ($fn->{RETURN_TYPE}) {
		pidl "NDR_CHECK(ndr_push_$fn->{RETURN_TYPE}(ndr, NDR_SCALARS, r->out.result));";
	}
    
	deindent;
	pidl "}";
	pidl "return NT_STATUS_OK;";
	deindent;
	pidl "}";
	pidl "";
}

sub AllocateArrayLevel($$$$$)
{
	my ($e,$l,$ndr,$env,$size) = @_;

	my $var = ParseExpr($e->{NAME}, $env, $e->{ORIGINAL});

	my $pl = GetPrevLevel($e, $l);
	if (defined($pl) and 
	    $pl->{TYPE} eq "POINTER" and 
	    $pl->{POINTER_TYPE} eq "ref"
	    and not $l->{IS_ZERO_TERMINATED}) {
		pidl "if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {";
		pidl "\tNDR_PULL_ALLOC_N($ndr, $var, $size);";
		pidl "}";
		if (grep(/in/,@{$e->{DIRECTION}}) and
		    grep(/out/,@{$e->{DIRECTION}})) {
			pidl "memcpy(r->out.$e->{NAME}, r->in.$e->{NAME}, $size * sizeof(*r->in.$e->{NAME}));";
		}
		return;
	}

	pidl "NDR_PULL_ALLOC_N($ndr, $var, $size);";
}

#####################################################################
# parse a function
sub ParseFunctionPull($)
{ 
	my($fn) = shift;

	# pull function args
	fn_declare("pull", $fn, "NTSTATUS ndr_pull_$fn->{NAME}(struct ndr_pull *ndr, int flags, struct $fn->{NAME} *r)") or return;

	pidl "{";
	indent;

	# declare any internal pointers we need
	foreach my $e (@{$fn->{ELEMENTS}}) { 
		DeclarePtrVariables($e);
		DeclareArrayVariables($e);
	}

	my %double_cases = ();
	foreach my $e (@{$fn->{ELEMENTS}}) {
		next if ($e->{TYPE} eq "EMPTY");
		next if ($double_cases{"$e->{NAME}"});
		DeclareMemCtxVariables($e);
		$double_cases{"$e->{NAME}"} = 1;
	}

	pidl "if (flags & NDR_IN) {";
	indent;

	# auto-init the out section of a structure. I originally argued that
	# this was a bad idea as it hides bugs, but coping correctly
	# with initialisation and not wiping ref vars is turning
	# out to be too tricky (tridge)
	foreach my $e (@{$fn->{ELEMENTS}}) {
		next unless grep(/out/, @{$e->{DIRECTION}});
		pidl "ZERO_STRUCT(r->out);";
		pidl "";
		last;
	}

	my $env = GenerateFunctionInEnv($fn);

	foreach my $e (@{$fn->{ELEMENTS}}) {
		next unless (grep(/in/, @{$e->{DIRECTION}}));
		ParseElementPull($e, "ndr", $env, 1, 1);
	}

	# allocate the "simple" out ref variables. FIXME: Shouldn't this have it's
	# own flag rather than be in NDR_IN ?

	foreach my $e (@{$fn->{ELEMENTS}}) {
		next unless (grep(/out/, @{$e->{DIRECTION}}));
		next unless ($e->{LEVELS}[0]->{TYPE} eq "POINTER" and 
		             $e->{LEVELS}[0]->{POINTER_TYPE} eq "ref");
		next if (($e->{LEVELS}[1]->{TYPE} eq "DATA") and 
				 ($e->{LEVELS}[1]->{DATA_TYPE} eq "string"));
		next if (($e->{LEVELS}[1]->{TYPE} eq "ARRAY") 
			and   $e->{LEVELS}[1]->{IS_ZERO_TERMINATED});

		if ($e->{LEVELS}[1]->{TYPE} eq "ARRAY") {
			my $size = ParseExprExt($e->{LEVELS}[1]->{SIZE_IS}, $env, $e->{ORIGINAL}, check_null_pointer($e, $env, \&pidl, "return NT_STATUS_INVALID_PARAMETER_MIX;"), 
				check_fully_dereferenced($e, $env));
			
			pidl "NDR_PULL_ALLOC_N(ndr, r->out.$e->{NAME}, $size);";

			if (grep(/in/, @{$e->{DIRECTION}})) {
				pidl "memcpy(r->out.$e->{NAME}, r->in.$e->{NAME}, $size * sizeof(*r->in.$e->{NAME}));";
			} else {
				pidl "memset(r->out.$e->{NAME}, 0, $size * sizeof(*r->out.$e->{NAME}));";
			}
		} else {
			pidl "NDR_PULL_ALLOC(ndr, r->out.$e->{NAME});";
		
			if (grep(/in/, @{$e->{DIRECTION}})) {
				pidl "*r->out.$e->{NAME} = *r->in.$e->{NAME};";
			} else {
				pidl "ZERO_STRUCTP(r->out.$e->{NAME});";
			}
		}
	}

	add_deferred();
	deindent;
	pidl "}";
	
	pidl "if (flags & NDR_OUT) {";
	indent;

	$env = GenerateFunctionOutEnv($fn);
	foreach my $e (@{$fn->{ELEMENTS}}) {
		next unless grep(/out/, @{$e->{DIRECTION}});
		ParseElementPull($e, "ndr", $env, 1, 1);
	}

	if ($fn->{RETURN_TYPE}) {
		pidl "NDR_CHECK(ndr_pull_$fn->{RETURN_TYPE}(ndr, NDR_SCALARS, &r->out.result));";
	}

	add_deferred();
	deindent;
	pidl "}";

	pidl "return NT_STATUS_OK;";
	deindent;
	pidl "}";
	pidl "";
}

#####################################################################
# produce a function call table
sub FunctionTable($)
{
	my($interface) = shift;
	my $count = 0;
	my $uname = uc $interface->{NAME};

	return if ($#{$interface->{FUNCTIONS}}+1 == 0);
	return unless defined ($interface->{PROPERTIES}->{uuid});

	pidl "static const struct dcerpc_interface_call $interface->{NAME}\_calls[] = {";
	foreach my $d (@{$interface->{FUNCTIONS}}) {
		next if not defined($d->{OPNUM});
		pidl "\t{";
		pidl "\t\t\"$d->{NAME}\",";
		pidl "\t\tsizeof(struct $d->{NAME}),";
		pidl "\t\t(ndr_push_flags_fn_t) ndr_push_$d->{NAME},";
		pidl "\t\t(ndr_pull_flags_fn_t) ndr_pull_$d->{NAME},";
		pidl "\t\t(ndr_print_function_t) ndr_print_$d->{NAME},";
		pidl "\t\t".($d->{ASYNC}?"True":"False").",";
		pidl "\t},";
		$count++;
	}
	pidl "\t{ NULL, 0, NULL, NULL, NULL, False }";
	pidl "};";
	pidl "";

	pidl "static const char * const $interface->{NAME}\_endpoint_strings[] = {";
	foreach my $ep (@{$interface->{ENDPOINTS}}) {
		pidl "\t$ep, ";
	}
	my $endpoint_count = $#{$interface->{ENDPOINTS}}+1;
	
	pidl "};";
	pidl "";

	pidl "static const struct dcerpc_endpoint_list $interface->{NAME}\_endpoints = {";
	pidl "\t.count\t= $endpoint_count,";
	pidl "\t.names\t= $interface->{NAME}\_endpoint_strings";
	pidl "};";
	pidl "";

	if (! defined $interface->{PROPERTIES}->{authservice}) {
		$interface->{PROPERTIES}->{authservice} = "\"host\"";
	}

	my @a = split / /, $interface->{PROPERTIES}->{authservice};
	my $authservice_count = $#a + 1;

	pidl "static const char * const $interface->{NAME}\_authservice_strings[] = {";
	foreach my $ap (@a) {
		pidl "\t$ap, ";
	}
	pidl "};";
	pidl "";

	pidl "static const struct dcerpc_authservice_list $interface->{NAME}\_authservices = {";
	pidl "\t.count\t= $endpoint_count,";
	pidl "\t.names\t= $interface->{NAME}\_authservice_strings";
	pidl "};";
	pidl "";

	pidl "\nconst struct dcerpc_interface_table dcerpc_table_$interface->{NAME} = {";
	pidl "\t.name\t\t= \"$interface->{NAME}\",";
	pidl "\t.syntax_id\t= {";
	pidl "\t\t" . print_uuid($interface->{UUID}) .",";
	pidl "\t\tDCERPC_$uname\_VERSION";
	pidl "\t},";
	pidl "\t.helpstring\t= DCERPC_$uname\_HELPSTRING,";
	pidl "\t.num_calls\t= $count,";
	pidl "\t.calls\t\t= $interface->{NAME}\_calls,";
	pidl "\t.endpoints\t= &$interface->{NAME}\_endpoints,";
	pidl "\t.authservices\t= &$interface->{NAME}\_authservices";
	pidl "};";
	pidl "";

}

#####################################################################
# generate include statements for imported idl files
sub HeaderImport
{
	my @imports = @_;
	foreach (@imports) {
		s/\.idl\"$//;
		s/^\"//;
		pidl choose_header("librpc/gen_ndr/ndr_$_\.h", "gen_ndr/ndr_$_.h");
	}
}

#####################################################################
# generate include statements for included header files
sub HeaderInclude
{
	my @includes = @_;
	foreach (@includes) {
		pidl_hdr "#include $_";
	}
}

#####################################################################
# generate prototypes and defines for the interface definitions
# FIXME: these prototypes are for the DCE/RPC client functions, not the 
# NDR parser and so do not belong here, technically speaking
sub HeaderInterface($)
{
	my($interface) = shift;

	my $count = 0;

	pidl_hdr choose_header("librpc/ndr/libndr.h", "ndr.h");

	if (has_property($interface, "object")) {
		pidl choose_header("librpc/gen_ndr/ndr_orpc.h", "ndr/orpc.h");
	}

	if (defined $interface->{PROPERTIES}->{helper}) {
		HeaderInclude(split / /, $interface->{PROPERTIES}->{helper});
	}

	if (defined $interface->{PROPERTIES}->{uuid}) {
		my $name = uc $interface->{NAME};
		pidl_hdr "#define DCERPC_$name\_UUID " . 
		Parse::Pidl::Util::make_str(lc($interface->{PROPERTIES}->{uuid}));

		if(!defined $interface->{PROPERTIES}->{version}) { $interface->{PROPERTIES}->{version} = "0.0"; }
		pidl_hdr "#define DCERPC_$name\_VERSION $interface->{PROPERTIES}->{version}";

		pidl_hdr "#define DCERPC_$name\_NAME \"$interface->{NAME}\"";

		if(!defined $interface->{PROPERTIES}->{helpstring}) { $interface->{PROPERTIES}->{helpstring} = "NULL"; }
		pidl_hdr "#define DCERPC_$name\_HELPSTRING $interface->{PROPERTIES}->{helpstring}";

		pidl_hdr "extern const struct dcerpc_interface_table dcerpc_table_$interface->{NAME};";
		pidl_hdr "NTSTATUS dcerpc_server_$interface->{NAME}_init(void);";
	}

	foreach (@{$interface->{FUNCTIONS}}) {
		next if has_property($_, "noopnum");
		next if grep(/$_->{NAME}/,@{$interface->{INHERITED_FUNCTIONS}});
		my $u_name = uc $_->{NAME};
	
		my $val = sprintf("0x%02x", $count);
		if (defined($interface->{BASE})) {
			$val .= " + DCERPC_" . uc $interface->{BASE} . "_CALL_COUNT";
		}
		
		pidl_hdr "#define DCERPC_$u_name ($val)";

		pidl_hdr "";
		$count++;
	}

	my $val = $count;

	if (defined($interface->{BASE})) {
		$val .= " + DCERPC_" . uc $interface->{BASE} . "_CALL_COUNT";
	}

	pidl_hdr "#define DCERPC_" . uc $interface->{NAME} . "_CALL_COUNT ($val)";

}

sub ParseTypePush($)
{
	my ($e) = @_;

	my $args = $typefamily{$e->{TYPE}}->{DECL}->($e, "push", $e->{NAME});
	fn_declare("push", $e, "NTSTATUS ndr_push_$e->{NAME}(struct ndr_push *ndr, int ndr_flags, $args)") or return;

	pidl "{";
	indent;
	$typefamily{$e->{TYPE}}->{PUSH_FN_BODY}->($e, $e->{NAME});
	pidl "return NT_STATUS_OK;";
	deindent;
	pidl "}";
	pidl "";;
}

sub ParseTypePull($)
{
	my ($e) = @_;

	my $args = $typefamily{$e->{TYPE}}->{DECL}->($e, "pull", $e->{NAME});

	fn_declare("pull", $e, "NTSTATUS ndr_pull_$e->{NAME}(struct ndr_pull *ndr, int ndr_flags, $args)") or return;

	pidl "{";
	indent;
	$typefamily{$e->{TYPE}}->{PULL_FN_BODY}->($e, $e->{NAME});
	pidl "return NT_STATUS_OK;";
	deindent;
	pidl "}";
	pidl "";
}

sub ParseTypePrint($)
{
	my ($e) = @_;
	my $args = $typefamily{$e->{TYPE}}->{DECL}->($e, "print", $e->{NAME});

	pidl_hdr "void ndr_print_$e->{NAME}(struct ndr_print *ndr, const char *name, $args);";

	return if (has_property($e, "noprint"));

	pidl "_PUBLIC_ void ndr_print_$e->{NAME}(struct ndr_print *ndr, const char *name, $args)";
	pidl "{";
	indent;
	$typefamily{$e->{TYPE}}->{PRINT_FN_BODY}->($e, $e->{NAME});
	deindent;
	pidl "}";
	pidl "";
}

sub ParseTypeNdrSize($)
{
	my ($t) = @_;

	my $tf = $typefamily{$t->{TYPE}};
	my $args = $tf->{SIZE_FN_ARGS}->($t, $t->{NAME});

	fn_declare("size", $t, "size_t ndr_size_$t->{NAME}($args)") or return;

	pidl "{";
	indent;
	$typefamily{$t->{TYPE}}->{SIZE_FN_BODY}->($t, $t->{NAME});
	deindent;
	pidl "}";
	pidl "";
}

#####################################################################
# parse the interface definitions
sub ParseInterface($$)
{
	my($interface,$needed) = @_;

	pidl_hdr "#ifndef _HEADER_NDR_$interface->{NAME}";
	pidl_hdr "#define _HEADER_NDR_$interface->{NAME}";

	pidl_hdr "";

	if ($needed->{"compression"}) {
		pidl choose_header("librpc/ndr/ndr_compression.h", "ndr/compression.h");
	}

	HeaderInterface($interface);

	# Typedefs
	foreach my $d (@{$interface->{TYPES}}) {
		($needed->{"push_$d->{NAME}"}) && ParseTypePush($d);
		($needed->{"pull_$d->{NAME}"}) && ParseTypePull($d);
		($needed->{"print_$d->{NAME}"}) && ParseTypePrint($d);

		# Make sure we don't generate a function twice...
		$needed->{"push_$d->{NAME}"} = $needed->{"pull_$d->{NAME}"} = 
			$needed->{"print_$d->{NAME}"} = 0;

		($needed->{"ndr_size_$d->{NAME}"}) && ParseTypeNdrSize($d);
	}

	# Functions
	foreach my $d (@{$interface->{FUNCTIONS}}) {
		($needed->{"push_$d->{NAME}"}) && ParseFunctionPush($d);
		($needed->{"pull_$d->{NAME}"}) && ParseFunctionPull($d);
		($needed->{"print_$d->{NAME}"}) && ParseFunctionPrint($d);

		# Make sure we don't generate a function twice...
		$needed->{"push_$d->{NAME}"} = $needed->{"pull_$d->{NAME}"} = 
			$needed->{"print_$d->{NAME}"} = 0;
	}

	FunctionTable($interface);

	pidl_hdr "#endif /* _HEADER_NDR_$interface->{NAME} */";
}

sub GenerateIncludes()
{
	if (is_intree()) {
		pidl "#include \"includes.h\"";
	} else {
		pidl "#define _GNU_SOURCE";
		pidl "#include <stdint.h>";
		pidl "#include <stdlib.h>";
		pidl "#include <stdio.h>";
		pidl "#include <stdbool.h>";
		pidl "#include <stdarg.h>";
		pidl "#include <string.h>";
	}

	# Samba3 has everything in include/includes.h
	if (is_intree() != 3) {
		pidl choose_header("libcli/util/nterr.h", "core/nterr.h");
		pidl choose_header("librpc/gen_ndr/ndr_misc.h", "gen_ndr/ndr_misc.h");
		pidl choose_header("librpc/gen_ndr/ndr_dcerpc.h", "gen_ndr/ndr_dcerpc.h");
		pidl choose_header("librpc/rpc/dcerpc.h", "dcerpc.h"); #FIXME: This shouldn't be here!
	}
}

#####################################################################
# parse a parsed IDL structure back into an IDL file
sub Parse($$$)
{
	my($ndr,$gen_header,$ndr_header) = @_;

	$tabs = "";
	$res = "";

	$res_hdr = "";
	pidl_hdr "/* header auto-generated by pidl */";
	pidl_hdr "";
	pidl_hdr "#include \"$gen_header\"" if ($gen_header);
	pidl_hdr "";

	pidl "/* parser auto-generated by pidl */";
	pidl "";
	GenerateIncludes();
	pidl "#include \"$ndr_header\"" if ($ndr_header);
	pidl "";

	my %needed = ();

	foreach (@{$ndr}) {
		($_->{TYPE} eq "INTERFACE") && NeededInterface($_, \%needed);
	}

	foreach (@{$ndr}) {
		($_->{TYPE} eq "INTERFACE") && ParseInterface($_, \%needed);
		($_->{TYPE} eq "IMPORT") && HeaderImport(@{$_->{PATHS}});
		($_->{TYPE} eq "INCLUDE") && HeaderInclude(@{$_->{PATHS}});
	}

	return ($res_hdr, $res);
}

sub NeededElement($$$)
{
	my ($e, $dir, $needed) = @_;

	return if ($e->{TYPE} eq "EMPTY");

	my @fn = ();
	if ($dir eq "print") {
		push(@fn, "print_$e->{REPRESENTATION_TYPE}");
	} elsif ($dir eq "pull") {
		push (@fn, "pull_$e->{TYPE}");
		push (@fn, "ndr_$e->{TYPE}_to_$e->{REPRESENTATION_TYPE}")
			if ($e->{REPRESENTATION_TYPE} ne $e->{TYPE});
	} elsif ($dir eq "push") {
		push (@fn, "push_$e->{TYPE}");
		push (@fn, "ndr_$e->{REPRESENTATION_TYPE}_to_$e->{TYPE}")
			if ($e->{REPRESENTATION_TYPE} ne $e->{TYPE});
	} else {
		die("invalid direction `$dir'");
	}

	foreach (@fn) {
		unless (defined($needed->{$_})) {
			$needed->{$_} = 1;
		}
	}
}

sub NeededFunction($$)
{
	my ($fn,$needed) = @_;
	$needed->{"pull_$fn->{NAME}"} = 1;
	$needed->{"push_$fn->{NAME}"} = 1;
	$needed->{"print_$fn->{NAME}"} = 1;
	foreach my $e (@{$fn->{ELEMENTS}}) {
		$e->{PARENT} = $fn;
		NeededElement($e, $_, $needed) foreach ("pull", "push", "print");
	}
}

sub NeededType($$)
{
	my ($t,$needed) = @_;
	if (has_property($t, "public")) {
		$needed->{"pull_$t->{NAME}"} = 1;
		$needed->{"push_$t->{NAME}"} = 1;
		$needed->{"print_$t->{NAME}"} = 1;
	}

	if ($t->{DATA}->{TYPE} eq "STRUCT" or $t->{DATA}->{TYPE} eq "UNION") {
		if (has_property($t, "gensize")) {
			$needed->{"ndr_size_$t->{NAME}"} = 1;
		}

		for my $e (@{$t->{DATA}->{ELEMENTS}}) {
			$e->{PARENT} = $t->{DATA};
			if (has_property($e, "compression")) { 
				$needed->{"compression"} = 1;
			}
			NeededElement($e, "pull", $needed) if ($needed->{"pull_$t->{NAME}"});
			NeededElement($e, "push", $needed) if ($needed->{"push_$t->{NAME}"});
			NeededElement($e, "print", $needed) if ($needed->{"print_$t->{NAME}"});
		}
	}
}

#####################################################################
# work out what parse functions are needed
sub NeededInterface($$)
{
	my ($interface,$needed) = @_;
	NeededFunction($_, $needed) foreach (@{$interface->{FUNCTIONS}});
	NeededType($_, $needed) foreach (reverse @{$interface->{TYPES}});
}

1;
