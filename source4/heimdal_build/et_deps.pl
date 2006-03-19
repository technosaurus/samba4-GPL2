#!/usr/bin/perl

use File::Basename;

my $file = shift;
my $dirname = dirname($file);
my $basename = basename($file);

my $header = $file; $header =~ s/\.et$/.h/;
my $source = $file; $source =~ s/\.et$/.c/;
my $short_header = $header; $short_header =~ s/(.*)\///g;
print "$short_header $header $source: $file bin/compile_et\n";
print "\t\@echo \"Compiling error table $file\"\n";
print "\t\@cd $dirname && ../../../bin/compile_et $basename\n\n";
