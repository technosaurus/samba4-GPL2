#!/usr/bin/perl
# (C) 2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU General Public License
use strict;
use warnings;

use Test::More tests => 9;
use FindBin qw($RealBin);
use lib "$RealBin";
use Util;
use Parse::Pidl::Util qw(MyDumper);
use Parse::Pidl::Samba4::Header;
use Parse::Pidl::IDL qw(parse_string);

sub parse_idl($)
{
	my $text = shift;
	my $idl = Parse::Pidl::IDL::parse_string($text, "nofile");
	return Parse::Pidl::Samba4::Header::Parse($idl);
}

is("/* header auto-generated by pidl */\n\n#include <core.h>\n\n", parse_idl(""), "includes work");
is("/* header auto-generated by pidl */\n\n#include <core.h>\n\n", parse_idl("interface x {}"), "simple empty interface doesn't cause overhead");
like(parse_idl("interface p { typedef struct { int y; } x; };"),
     qr/.*#ifndef _HEADER_p\n#define _HEADER_p\n.+\n#endif \/\* _HEADER_p \*\/.*/ms, "ifdefs are created");
like(parse_idl("interface p { typedef struct { int y; } x; };"),
     qr/struct x.*{.*int32_t y;.*}.*;/sm, "interface member generated properly");
like(parse_idl("interface x { void foo (void); };"),
     qr/struct foo.*{\s+int _dummy_element;\s+};/sm, "void fn contains dummy element");
like(parse_idl("interface x { void foo ([in] uint32 x); };"),
     qr/struct foo.*{\s+struct\s+{\s+uint32_t x;\s+} in;\s+};/sm, "fn in arg works");
like(parse_idl("interface x { void foo ([out] uint32 x); };"),
     qr/struct foo.*{.*struct\s+{\s+uint32_t x;\s+} out;.*};/sm, "fn out arg works");
like(parse_idl("interface x { void foo ([in,out] uint32 x); };"),
     qr/struct foo.*{.*struct\s+{\s+uint32_t x;\s+} in;\s+struct\s+{\s+uint32_t x;\s+} out;.*};/sm, "fn in,out arg works");
like(parse_idl("interface x { void foo (uint32 x); };"), qr/struct foo.*{.*struct\s+{\s+uint32_t x;\s+} in;\s+struct\s+{\s+uint32_t x;\s+} out;.*};/sm, "fn with no props implies in,out");
