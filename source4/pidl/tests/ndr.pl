#!/usr/bin/perl
# (C) 2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU General Public License
use strict;
use warnings;

use Test::More tests => 26;
use FindBin qw($RealBin);
use lib "$RealBin";
use Util;
use Parse::Pidl::Util qw(MyDumper);
use Parse::Pidl::NDR qw(GetElementLevelTable ParseElement align_type mapToScalar ParseType);

# Case 1

my $e = {
	'FILE' => 'foo.idl',
	'NAME' => 'v',
	'PROPERTIES' => {},
	'POINTERS' => 0,
	'TYPE' => 'uint8',
	'PARENT' => { TYPE => 'STRUCT' },
	'LINE' => 42 };

is_deeply(GetElementLevelTable($e), [
	{
		'IS_DEFERRED' => 0,
		'LEVEL_INDEX' => 0,
		'DATA_TYPE' => 'uint8',
		'CONTAINS_DEFERRED' => 0,
		'TYPE' => 'DATA',
		'IS_SURROUNDING' => 0,
	}
]);

my $ne = ParseElement($e, undef);
is($ne->{ORIGINAL}, $e);
is($ne->{NAME}, "v");
is($ne->{ALIGN}, 1);
is($ne->{TYPE}, "uint8");
is_deeply($ne->{LEVELS},  [
	{
		'IS_DEFERRED' => 0,
		'LEVEL_INDEX' => 0,
		'DATA_TYPE' => 'uint8',
		'CONTAINS_DEFERRED' => 0,
		'TYPE' => 'DATA',
		'IS_SURROUNDING' => 0,
	}
]);

# Case 2 : pointers
#
$e = {
	'FILE' => 'foo.idl',
	'NAME' => 'v',
	'PROPERTIES' => {"unique" => 1},
	'POINTERS' => 1,
	'PARENT' => { TYPE => 'STRUCT' },
	'TYPE' => 'uint8',
	'LINE' => 42 };

is_deeply(GetElementLevelTable($e), [
	{
		LEVEL_INDEX => 0,
		IS_DEFERRED => 0,
		TYPE => 'POINTER',
		POINTER_TYPE => "unique",
		POINTER_INDEX => 0,
		LEVEL => 'EMBEDDED'
	},
	{
		'IS_DEFERRED' => 1,
		'LEVEL_INDEX' => 1,
		'DATA_TYPE' => 'uint8',
		'CONTAINS_DEFERRED' => 0,
		'TYPE' => 'DATA',
		'IS_SURROUNDING' => 0,
	}
]);

# Case 3 : double pointers
#
$e = {
	'FILE' => 'foo.idl',
	'NAME' => 'v',
	'PROPERTIES' => {"unique" => 1},
	'POINTERS' => 2,
	'TYPE' => 'uint8',
	'PARENT' => { TYPE => 'STRUCT' },
	'LINE' => 42 };

is_deeply(GetElementLevelTable($e), [
	{
		LEVEL_INDEX => 0,
		IS_DEFERRED => 0,
		TYPE => 'POINTER',
		POINTER_TYPE => "unique",
		POINTER_INDEX => 0,
		LEVEL => 'EMBEDDED'
	},
	{
		LEVEL_INDEX => 1,
		IS_DEFERRED => 1,
		TYPE => 'POINTER',
		POINTER_TYPE => "unique",
		POINTER_INDEX => 1,
		LEVEL => 'EMBEDDED'
	},
	{
		'IS_DEFERRED' => 1,
		'LEVEL_INDEX' => 2,
		'DATA_TYPE' => 'uint8',
		'CONTAINS_DEFERRED' => 0,
		'TYPE' => 'DATA',
		'IS_SURROUNDING' => 0,
	}
]);

# Case 3 : ref pointers
#
$e = {
	'FILE' => 'foo.idl',
	'NAME' => 'v',
	'PROPERTIES' => {"ref" => 1},
	'POINTERS' => 1,
	'TYPE' => 'uint8',
	'PARENT' => { TYPE => 'STRUCT' },
	'LINE' => 42 };

is_deeply(GetElementLevelTable($e), [
	{
		LEVEL_INDEX => 0,
		IS_DEFERRED => 0,
		TYPE => 'POINTER',
		POINTER_TYPE => "ref",
		POINTER_INDEX => 0,
		LEVEL => 'EMBEDDED'
	},
	{
		'IS_DEFERRED' => 1,
		'LEVEL_INDEX' => 1,
		'DATA_TYPE' => 'uint8',
		'CONTAINS_DEFERRED' => 0,
		'TYPE' => 'DATA',
		'IS_SURROUNDING' => 0,
	}
]);


# Case 4 : top-level ref pointers
#
$e = {
	'FILE' => 'foo.idl',
	'NAME' => 'v',
	'PROPERTIES' => {"ref" => 1},
	'POINTERS' => 1,
	'TYPE' => 'uint8',
	'PARENT' => { TYPE => 'FUNCTION' },
	'LINE' => 42 };

is_deeply(GetElementLevelTable($e), [
	{
		LEVEL_INDEX => 0,
		IS_DEFERRED => 0,
		TYPE => 'POINTER',
		POINTER_TYPE => "ref",
		POINTER_INDEX => 0,
		LEVEL => 'TOP'
	},
	{
		'IS_DEFERRED' => 0,
		'LEVEL_INDEX' => 1,
		'DATA_TYPE' => 'uint8',
		'CONTAINS_DEFERRED' => 0,
		'TYPE' => 'DATA',
		'IS_SURROUNDING' => 0,
	}
]);

# representation_type
$e = {
	'FILE' => 'foo.idl',
	'NAME' => 'v',
	'PROPERTIES' => { represent_as => "bar" },
	'POINTERS' => 0,
	'TYPE' => 'uint8',
	'PARENT' => { TYPE => 'STRUCT' },
	'LINE' => 42 };

$ne = ParseElement($e, undef);
is($ne->{REPRESENTATION_TYPE}, "bar");

# representation_type
$e = {
	'FILE' => 'foo.idl',
	'NAME' => 'v',
	'PROPERTIES' => { },
	'POINTERS' => 0,
	'TYPE' => 'uint8',
	'PARENT' => { TYPE => 'STRUCT' },
	'LINE' => 42 };

$ne = ParseElement($e, undef);
is($ne->{REPRESENTATION_TYPE}, "uint8");

is(align_type("hyper"), 8);
is(align_type("uint32"), 4);
is(align_type("uint16"), 2);
is(align_type("uint8"), 1);
is(align_type({ TYPE => "STRUCT", "NAME" => "bla", 
			    ELEMENTS => [ { TYPE => "uint16" } ] }), 4);
is(align_type({ TYPE => "STRUCT", 
			    ELEMENTS => [ { TYPE => "hyper" } ] }), 8);
is(align_type({ TYPE => "DECLARE", DATA => { 
				TYPE => "STRUCT", 
			    ELEMENTS => [ { TYPE => "hyper" } ] }}), 8);
is(align_type({ TYPE => "STRUCT", "NAME" => "bla", 
			    ELEMENTS => [ { TYPE => "uint8" } ] }), 4);

is(mapToScalar("someverymuchnotexistingtype"), undef);
is(mapToScalar("uint32"), "uint32");
is(mapToScalar({TYPE => "ENUM", PARENT => { PROPERTIES => { enum8bit => 1 } } }), "uint8");
is(mapToScalar({TYPE => "BITMAP", PROPERTIES => { bitmap64bit => 1 } }),
	"hyper");
is(mapToScalar({TYPE => "TYPEDEF", DATA => {TYPE => "ENUM", PARENT => { PROPERTIES => { enum8bit => 1 } } }}), "uint8");

is_deeply(ParseType({TYPE => "STRUCT", NAME => "foo" }, "ref"), 
	{TYPE => "STRUCT", NAME => "foo" });
