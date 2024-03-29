###################################################
# Common Samba4 functions
# Copyright jelmer@samba.org 2006
# released under the GNU GPL

package Parse::Pidl::Samba4;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(is_intree choose_header DeclLong);

use Parse::Pidl::Util qw(has_property is_constant);
use Parse::Pidl::Typelist qw(mapTypeName scalar_is_reference);
use strict;

use vars qw($VERSION);
$VERSION = '0.01';

sub is_intree()
{
	my $srcdir = $ENV{srcdir};
	$srcdir = $srcdir ? "$srcdir/" : "";
	return 4 if (-f "${srcdir}kdc/kdc.c");
	return 3 if (-f "${srcdir}include/smb.h");
	return 0;
}

# Return an #include line depending on whether this build is an in-tree
# build or not.
sub choose_header($$)
{
	my ($in,$out) = @_;
	return "#include \"$in\"" if (is_intree());
	return "#include <$out>";
}

sub DeclLong($)
{
	my($element) = shift;
	my $ret = "";

	if (has_property($element, "represent_as")) {
		$ret.=mapTypeName($element->{PROPERTIES}->{represent_as})." ";
	} else {
		if (has_property($element, "charset")) {
			$ret.="const char";
		} else {
			$ret.=mapTypeName($element->{TYPE});
		}

		$ret.=" ";
		my $numstar = $element->{ORIGINAL}->{POINTERS};
		if ($numstar >= 1) {
			$numstar-- if scalar_is_reference($element->{TYPE});
		}
		foreach (@{$element->{ORIGINAL}->{ARRAY_LEN}})
		{
			next if is_constant($_) and 
				not has_property($element, "charset");
			$numstar++;
		}
		$ret.="*" foreach (1..$numstar);
	}
	$ret.=$element->{NAME};
	foreach (@{$element->{ARRAY_LEN}}) {
		next unless (is_constant($_) and not has_property($element, "charset"));
		$ret.="[$_]";
	}

	return $ret;
}

1;
