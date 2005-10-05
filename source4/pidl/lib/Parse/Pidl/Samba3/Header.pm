###################################################
# Samba3 NDR header generator for IDL structures
# Copyright jelmer@samba.org 2005
# released under the GNU GPL

package Parse::Pidl::Samba3::Header;

use strict;
use Parse::Pidl::Typelist qw(hasType getType);
use Parse::Pidl::Util qw(has_property ParseExpr);
use Parse::Pidl::NDR qw(GetPrevLevel GetNextLevel ContainsDeferred);
use Parse::Pidl::Samba3::Types qw(DeclShort);

use vars qw($VERSION);
$VERSION = '0.01';

my $res = "";
my $tabs = "";
sub indent() { $tabs.="\t"; }
sub deindent() { $tabs = substr($tabs, 1); }
sub pidl($) { $res .= $tabs.(shift)."\n"; }
sub fatal($$) { my ($e,$s) = @_; die("$e->{FILE}:$e->{LINE}: $s\n"); }
sub warning($$) { my ($e,$s) = @_; warn("$e->{FILE}:$e->{LINE}: $s\n"); }

sub ParseElement($)
{
	my $e = shift;

	foreach my $l (@{$e->{LEVELS}}) {
		if ($l->{TYPE} eq "POINTER") {
			return if ($l->{POINTER_TYPE} eq "ref" and $l->{LEVEL} eq "top");
			pidl "\tuint32 ptr_$e->{NAME};";
		} elsif ($l->{TYPE} eq "SWITCH") {
			pidl "\tuint32 level_$e->{NAME};";
		} elsif ($l->{TYPE} eq "DATA") {
			pidl "\t" . DeclShort($e) . ";";
		} elsif ($l->{TYPE} eq "ARRAY") {
			if ($l->{IS_CONFORMANT}) {
				pidl "\tuint32 size_$e->{NAME};";
			}
			if ($l->{IS_VARYING}) {
				pidl "\tuint32 length_$e->{NAME};";
				pidl "\tuint32 offset_$e->{NAME};";
			}
		}
	}
}

sub CreateStruct($$$$)
{
	my ($if,$fn,$n,$t) = @_;

	pidl "typedef struct $n {";
	ParseElement($_) foreach (@$t);

	if (not @$t) {
		# Some compilers don't like empty structs
		pidl "\tuint32 dummy;";
	}

	pidl "} " . uc($n) . ";";
	pidl "";
}

sub ParseFunction($$)
{
	my ($if,$fn) = @_;

	my @in = ();
	my @out = ();

	foreach (@{$fn->{ELEMENTS}}) {
		push (@in, $_) if (grep(/in/, @{$_->{DIRECTION}}));
		push (@out, $_) if (grep(/out/, @{$_->{DIRECTION}}));
	}

	if (defined($fn->{RETURN_TYPE})) {
		push (@out, { 
			NAME => "status", 
			TYPE => $fn->{RETURN_TYPE},
			LEVELS => [
				{
					TYPE => "DATA",
					DATA_TYPE => $fn->{RETURN_TYPE}
				}
			]
		} );
	}

	#  define Q + R structures for functions

	CreateStruct($if, $fn, "$if->{NAME}_q_$fn->{NAME}", \@in);
	CreateStruct($if, $fn, "$if->{NAME}_r_$fn->{NAME}", \@out);
}

sub ParseStruct($$$)
{
	my ($if,$s,$n) = @_;

	CreateStruct($if, $s, "$if->{NAME}_$n", $s->{ELEMENTS});
}

sub ParseUnion($$$)
{
	my ($if,$u,$n) = @_;

	my $extra = {};
	
	unless (has_property($u, "nodiscriminant")) {
		$extra->{switch_value} = 1;
	}

	foreach my $e (@{$u->{ELEMENTS}}) {
		foreach my $l (@{$e->{LEVELS}}) {
			if ($l->{TYPE} eq "ARRAY") {
				if ($l->{IS_CONFORMANT}) {
					$extra->{"size"} = 1;
				}
				if ($l->{IS_VARYING}) {
					$extra->{"length"} = $extra->{"offset"} = 1;
				}
			} elsif ($l->{TYPE} eq "POINTER") {
				$extra->{"ptr"} = 1;
			} elsif ($l->{TYPE} eq "SWITCH") {
				$extra->{"level"} = 1;
			}
		}
	}

	pidl "typedef struct $if->{NAME}_$n\_ctr {";
	indent;
	pidl "uint32 $_;" foreach (keys %$extra);
	pidl "union {";
	indent;
	foreach (@{$u->{ELEMENTS}}) {
		next if ($_->{TYPE} eq "EMPTY");
		pidl "\t" . DeclShort($_) . ";";
	}
	deindent;
	pidl "} u;";
	deindent;
	pidl "} ".uc("$if->{NAME}_$n\_ctr") .";";
	pidl "";
}

sub ParseEnum($$$)
{
	my ($if,$s,$n) = @_;

	pidl "typedef enum {";
	pidl "$_," foreach (@{$s->{ELEMENTS}});
	pidl "} $n;";
}

sub ParseBitmap($$$)
{
	my ($if,$s,$n) = @_;

	pidl "#define $_" foreach (@{$s->{ELEMENTS}});
}

sub ParseInterface($)
{
	my $if = shift;

	my $def = "_RPC_" . uc($if->{NAME}) . "_H";

	pidl "";

	pidl "\#ifndef $def";
	pidl "\#define $def";

	pidl "";
	
	foreach (@{$if->{FUNCTIONS}}) {
		pidl "\#define " . uc($_->{NAME}) . " $_->{OPNUM}" ;
	}

	pidl "";

	foreach (@{$if->{TYPEDEFS}}) {
		ParseStruct($if, $_->{DATA}, $_->{NAME}) if ($_->{DATA}->{TYPE} eq "STRUCT");
		ParseEnum($if, $_->{DATA}, $_->{NAME}) if ($_->{DATA}->{TYPE} eq "ENUM");
		ParseBitmap($if, $_->{DATA}, $_->{NAME}) if ($_->{DATA}->{TYPE} eq "BITMAP");
		ParseUnion($if, $_->{DATA}, $_->{NAME}) if ($_->{DATA}->{TYPE} eq "UNION");
	}

	ParseFunction($if, $_) foreach (@{$if->{FUNCTIONS}});

	foreach (@{$if->{CONSTS}}) {
		pidl "$_->{NAME} ($_->{VALUE})";
	}

	pidl "\#endif /* $def */";
}

sub Parse($$)
{
	my($ndr,$filename) = @_;

	$res = "";
	$tabs = "";

	pidl "/*";
	pidl " * Unix SMB/CIFS implementation.";
	pidl " * header auto-generated by pidl. DO NOT MODIFY!";
	pidl " */";
	pidl "";

	# Loop over interfaces
	foreach (@{$ndr}) {
		ParseInterface($_) if ($_->{TYPE} eq "INTERFACE");
	}
	return $res;
}

1;
