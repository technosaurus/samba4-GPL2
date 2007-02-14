#!/usr/bin/perl
# (C) 2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU General Public License
use strict;
use warnings;

use Test::More tests => 20;
use FindBin qw($RealBin);
use lib "$RealBin";
use Util;
use Parse::Pidl::Util qw(MyDumper);
use Parse::Pidl::Samba4::NDR::Parser qw(check_null_pointer GenerateFunctionInEnv GenerateFunctionOutEnv GenerateStructEnv EnvSubstituteValue); 

my $output;
sub print_fn($) { my $x = shift; $output.=$x; }

# Test case 1: Simple unique pointer dereference

$output = "";
my $fn = check_null_pointer({ 
	PARENT => {
		ELEMENTS => [
			{ 
				NAME => "bla",
				LEVELS => [
					{ TYPE => "POINTER",
					  POINTER_INDEX => 0,
					  POINTER_TYPE => "unique" },
					{ TYPE => "DATA" }
				],
			},
		]
	}
}, { bla => "r->in.bla" }, \&print_fn, "return;"); 


test_warnings("", sub { $fn->("r->in.bla"); });

is($output, "if (r->in.bla == NULL) return;");

# Test case 2: Simple ref pointer dereference

$output = "";
$fn = check_null_pointer({ 
	PARENT => {
		ELEMENTS => [
			{ 
				NAME => "bla",
				LEVELS => [
					{ TYPE => "POINTER",
					  POINTER_INDEX => 0,
					  POINTER_TYPE => "ref" },
					{ TYPE => "DATA" }
				],
			},
		]
	}
}, { bla => "r->in.bla" }, \&print_fn, undef); 

test_warnings("", sub { $fn->("r->in.bla"); });

is($output, "");

# Test case 3: Illegal dereference

$output = "";
$fn = check_null_pointer({ 
	FILE => "nofile",
	LINE => 1,
	PARENT => {
		ELEMENTS => [
			{ 
				NAME => "bla",
				LEVELS => [
					{ TYPE => "DATA" }
				],
			},
		]
	}
}, { bla => "r->in.bla" }, \&print_fn, undef); 

test_warnings("nofile:1: too much dereferences for `bla'\n", 
	          sub { $fn->("r->in.bla"); });

is($output, "");

# Test case 4: Double pointer dereference

$output = "";
$fn = check_null_pointer({ 
	PARENT => {
		ELEMENTS => [
			{ 
				NAME => "bla",
				LEVELS => [
					{ TYPE => "POINTER",
					  POINTER_INDEX => 0,
					  POINTER_TYPE => "unique" },
					{ TYPE => "POINTER",
					  POINTER_INDEX => 1,
					  POINTER_TYPE => "unique" },
					{ TYPE => "DATA" }
				],
			},
		]
	}
}, { bla => "r->in.bla" }, \&print_fn, "return;"); 

test_warnings("",
	          sub { $fn->("*r->in.bla"); });

is($output, "if (*r->in.bla == NULL) return;");

# Test case 5: Unknown variable

$output = "";
$fn = check_null_pointer({ 
	FILE => "nofile",
	LINE => 2,
	PARENT => {
		ELEMENTS => [
			{ 
				NAME => "bla",
				LEVELS => [
					{ TYPE => "DATA" }
				],
			},
		]
	}
}, { }, \&print_fn, "return;"); 

test_warnings("nofile:2: unknown dereferenced expression `r->in.bla'\n",
	          sub { $fn->("r->in.bla"); });

is($output, "if (r->in.bla == NULL) return;");

# Make sure GenerateFunctionInEnv and GenerateFunctionOutEnv work
$fn = { ELEMENTS => [ { DIRECTION => ["in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->in.foo" }, GenerateFunctionInEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->out.foo" }, GenerateFunctionOutEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out", "in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->in.foo" }, GenerateFunctionInEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out", "in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->out.foo" }, GenerateFunctionOutEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["in"], NAME => "foo" } ] };
is_deeply({ "foo" => "r->in.foo" }, GenerateFunctionOutEnv($fn));

$fn = { ELEMENTS => [ { DIRECTION => ["out"], NAME => "foo" } ] };
is_deeply({ }, GenerateFunctionInEnv($fn));

$fn = { ELEMENTS => [ { NAME => "foo" }, { NAME => "bar" } ] };
is_deeply({ foo => "r->foo", bar => "r->bar", this => "r" }, GenerateStructEnv($fn));

$fn = { ELEMENTS => [ { NAME => "foo", PROPERTIES => { value => 3 }} ] };

my $env = GenerateStructEnv($fn);
EnvSubstituteValue($env, $fn);
is_deeply($env, { foo => 3, this => "r" });

$fn = { ELEMENTS => [ { NAME => "foo" }, { NAME => "bar" } ] };
$env = GenerateStructEnv($fn);
EnvSubstituteValue($env, $fn);
is_deeply($env, { foo => 'r->foo', bar => 'r->bar', this => "r" });

$fn = { ELEMENTS => [ { NAME => "foo", PROPERTIES => { value => 0 }} ] };

$env = GenerateStructEnv($fn);
EnvSubstituteValue($env, $fn);
is_deeply($env, { foo => 0, this => "r" });
