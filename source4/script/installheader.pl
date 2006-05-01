#!/usr/bin/perl
# Copyright (C) 2006 Jelmer Vernooij
use strict;
use File::Basename;

my $includedir = shift;


sub read_headermap($)
{
	my ($fn) = @_;
	my %map = ();
	my $ln = 0;
	open(MAP, "<headermap.txt");
	while(<MAP>) {
		$ln++;
		s/#.*$//g;
		next if (/^\s*$/);
		if (! /^(.*): (.*)$/) {
			print STDERR "headermap.txt:$ln: Malformed line\n";
			next;
		}
		$map{$1} = $2;
	}

	close(MAP);

	return %map;
}

my %map = read_headermap("headermap.txt");

sub findmap($)
{
	$_ = shift;
	s/^\.\///g;

	if (! -f $_ && -f "lib/$_") { $_ = "lib/$_"; }
	
	return $map{$_};
}

sub rewrite_include($$)
{
	my ($pos,$d) = @_;

	my $n = findmap($d);
	return $n if $n; 
	return $d;
}

sub install_header($$)
{
	my ($src,$dst) = @_;

	my $lineno = 0;

	open(IN, "<$src");
	open(OUT, ">$dst");

	while (<IN>) {
		$lineno++;
		if (/^#include \"(.*)\"/) {
			print OUT "#include <" . rewrite_include("$src:$lineno", $1) . ">\n";
		} else {
			print OUT $_;
		}
	}

	close(OUT);
	close(IN);
}

foreach my $p (@ARGV)
{
	my $p2 = findmap($p);
	unless ($p2) {
	    warn("Unable to map $p");
	    next;
	}
 	print "Installing $p as $includedir/$p2\n";

	my $dirname = dirname($p2);

	if (! -d "$includedir/$dirname") {
		mkdir("$includedir/$dirname", 0777);
	}

	if ( -f "$includedir/$p2" ) {
		unlink("$includedir/$p2.old");
		rename("$includedir/$p2", "$includedir/$p2.old");
	}

	install_header($p,"$includedir/$p2");
}

print <<EOF;
======================================================================
The headers are installed. You may restore the old headers (if there
were any) using the command "make revert". You may uninstall the headers
using the command "make uninstallheader" or "make uninstall" to uninstall
binaries, man pages and shell scripts.
======================================================================
EOF

exit 0;
