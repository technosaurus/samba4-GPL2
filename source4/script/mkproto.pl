#!/usr/bin/perl

use strict;
use warnings;

my $header_name = '_PROTO_H_';

if ($ARGV[0] eq '-h') {
	shift @ARGV;
	$header_name = shift @ARGV;
}


sub print_header {
	print "#ifndef $header_name\n";
	print "#define $header_name\n\n";
	print "/* This file is automatically generated with \"make proto\". DO NOT EDIT */\n\n";
}

sub print_footer {
	printf "\n#endif /*  %s  */\n", $header_name;
}


sub handle_loadparm {
	my $line = shift;

	if ($line =~ /^FN_GLOBAL_STRING/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "char *$fnName(void);\n";
	} elsif ($line =~ /^FN_LOCAL_STRING/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "char *$fnName(int );\n";
	} elsif ($line =~ /^FN_GLOBAL_BOOL/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "BOOL $fnName(void);\n";
	}
	elsif ($line =~ /^FN_LOCAL_BOOL/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "BOOL $fnName(int );\n";
	}
	elsif ($line =~ /^FN_GLOBAL_INTEGER/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "int $fnName(void);\n";
	}
	elsif ($line =~ /^FN_LOCAL_INTEGER/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "int $fnName(int );\n";
	}
	elsif ($line =~ /^FN_GLOBAL_LIST/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "const char **$fnName(void);\n";
	}
	elsif ($line =~ /^FN_LOCAL_LIST/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "const char **$fnName(int );\n";
	}
	elsif ($line =~ /^FN_GLOBAL_CONST_STRING/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "const char *$fnName(void);\n";
	}
	elsif ($line =~ /^FN_LOCAL_CONST_STRING/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "const char *$fnName(int );\n";
	}
	elsif ($line =~ /^FN_LOCAL_CHAR/o) {
		my $fnName = (split(/[\(,]/, $line))[1];
		print "char $fnName(int );\n";
	}
}


sub process_files {
	my $line;
	my $inheader;
	my $gotstart;

      FILE: foreach my $filename (@ARGV) {
		next FILE unless (open(FH, "< $filename")); # skip over file unless it can be opened
		print "\n/* The following definitions come from $filename  */\n\n";

		$inheader = 0;
		$gotstart = 0;
	      LINE: while (defined($line = <FH>)) {

			if ($inheader) {
				# this chomp is somewhat expensive, so don't do it unless we know
				# that we probably want to use it
				chomp $line;
				if ($line =~ /\)\s*$/o) {
					$inheader = 0;
					print "$line;\n";
				} else {
					print "$line\n";
				}
				next LINE;
			}

			$gotstart = 0;

			# ignore static and extern declarations
			if ($line =~ /^static|^extern/o ||
			    $line !~ /^[a-zA-Z]/o ||
			    $line =~ /[;]/o) {
				next LINE;
			}


			if ($line =~ /^FN_/) {
				handle_loadparm($line);
			}


			# I'm going to leave these as is for now - perl can probably handle larger regex, though -- vance
			# I've also sort of put these in approximate order of most commonly called

			if ( $line =~ /
			     ^void|^BOOL|^int|^struct|^char|^const|^\w+_[tT]\s|^uint|^unsigned|^long|
			     ^NTSTATUS|^ADS_STATUS|^enum\s.*\(|^DATA_BLOB|^WERROR|^XFILE|^FILE|^DIR|
			     ^double|^TDB_CONTEXT|^TDB_DATA|^TALLOC_CTX|^NTTIME
			     /x) {
				$gotstart = 1;
			}


			# goto next line if we don't have a start
			next LINE unless $gotstart;

			if ( $line =~ /\(.*\)\s*$/o ) {
			# now that we're here, we know we
				chomp $line;
				print "$line;\n";
				next LINE;
			}
			elsif ( $line =~ /\(/o ) {

				$inheader = 1;
				# line hasn't been chomped, so we can assume it already has the \n
				print $line;
				next LINE;
			}
		}
	}
}

print_header();
process_files();
print_footer();
