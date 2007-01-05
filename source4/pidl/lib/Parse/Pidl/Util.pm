###################################################
# utility functions to support pidl
# Copyright tridge@samba.org 2000
# released under the GNU GPL
package Parse::Pidl::Util;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(has_property property_matches ParseExpr is_constant make_str print_uuid);
use vars qw($VERSION);
$VERSION = '0.01';

use strict;

use Parse::Pidl::Expr;

#####################################################################
# a dumper wrapper to prevent dependence on the Data::Dumper module
# unless we actually need it
sub MyDumper($)
{
	require Data::Dumper;
	my $s = shift;
	return Data::Dumper::Dumper($s);
}

#####################################################################
# see if a pidl property list contains a given property
sub has_property($$)
{
	my($e, $p) = @_;

	return undef if (not defined($e->{PROPERTIES}));

	return $e->{PROPERTIES}->{$p};
}

#####################################################################
# see if a pidl property matches a value
sub property_matches($$$)
{
	my($e,$p,$v) = @_;

	if (!defined has_property($e, $p)) {
		return undef;
	}

	if ($e->{PROPERTIES}->{$p} =~ /$v/) {
		return 1;
	}

	return undef;
}

# return 1 if the string is a C constant
sub is_constant($)
{
	my $s = shift;
	return 1 if (defined $s && $s =~ /^\d+$/);
	return 1 if (defined $s && $s =~ /^0x[0-9A-Fa-f]+$/);
	return 0;
}

# return a "" quoted string, unless already quoted
sub make_str($)
{
	my $str = shift;
	if (substr($str, 0, 1) eq "\"") {
		return $str;
	}
	return "\"$str\"";
}

sub print_uuid($)
{
	my ($uuid) = @_;
	$uuid =~ s/"//g;
	my ($time_low,$time_mid,$time_hi,$clock_seq,$node) = split /-/, $uuid;
	return undef if not defined($node);

	my @clock_seq = $clock_seq =~ /(..)/g;
	my @node = $node =~ /(..)/g;

	return "{0x$time_low,0x$time_mid,0x$time_hi," .
		"{".join(',', map {"0x$_"} @clock_seq)."}," .
		"{".join(',', map {"0x$_"} @node)."}}";
}

# a hack to build on platforms that don't like negative enum values
my $useUintEnums = 0;
sub setUseUintEnums($)
{
	$useUintEnums = shift;
}
sub useUintEnums()
{
	return $useUintEnums;
}

sub ParseExpr($$)
{
	my($expr, $varlist) = @_;

	die("Undefined value in ParseExpr") if not defined($expr);

	my $x = new Parse::Pidl::Expr();
	
	return $x->Run($expr, sub { my $x = shift; die(MyDumper($x)); },
		# Lookup fn 
		sub { my $x = shift; 
			  return($varlist->{$x}) if (defined($varlist->{$x})); 
			  return $x;
		  },
		undef);
}

1;
