#!/usr/bin/perl

use strict;

# don't use warnings module as it is not portable enough
# use warnings;


use Getopt::Long;

my $public_file = undef;
my $private_file = undef;
my $public_define = undef;
my $private_define = undef;
my $public_fd = \*STDOUT;
my $private_fd = \*STDOUT;

GetOptions(
	'public=s' => sub { my ($f,$v) = @_; $public_file = $v; },
	'private=s' => sub { my ($f,$v) = @_; $private_file = $v; },
	'define=s' => sub { 
		my ($f,$v) = @_; 
		$public_define = $v; 
		$private_define = "$v\_PRIVATE"; 
	},
	'public-define=s' => \$public_define,
	'private-define=s' => \$private_define
);

if (not defined($public_define) and defined($public_file)) {
	$public_define = $public_file;
	$public_define =~ tr{./}{__};
} elsif (not defined($public_define)) {
	$public_define = '_PROTO_H_';
}

if (not defined($private_define) and defined($private_file)) {
	$private_define = $private_file;
	$private_define =~ tr{./}{__};
} elsif (not defined($public_define)) {
	$public_define = '_PROTO_H_';
}

if (defined($public_file)) {
	open PUBLIC, ">$public_file"; 
	$public_fd = \*PUBLIC;
}

if ($private_file eq $public_file) {
	$private_fd = $public_fd;
} elsif (not defined($private_file)) {
	open PRIVATE, ">$private_file"; 
	$private_fd = \*PRIVATE;
}

sub print_header($$)
{
	my ($file, $header_name) = @_;
	print $file "#ifndef $header_name\n";
	print $file "#define $header_name\n\n";
	print $file "/* This file is automatically generated with \"make proto\". DO NOT EDIT */\n\n";
}

sub print_footer($$) 
{
	my ($file, $header_name) = @_;
	printf $file "\n#endif /*  %s  */\n", $header_name;
}

sub handle_loadparm($$) 
{
	my ($file,$line) = @_;

	if ($line =~ /^FN_(GLOBAL|LOCAL)_(CONST_STRING|STRING|BOOL|CHAR|INTEGER|LIST)\((\w+),.*\)/o) {
		my $scope = $1;
		my $type = $2;
		my $name = $3;

		my %tmap = (
			    "BOOL" => "BOOL ",
			    "CONST_STRING" => "const char *",
			    "STRING" => "const char *",
			    "INTEGER" => "int ",
			    "CHAR" => "char ",
			    "LIST" => "const char **",
			    );

		my %smap = (
			    "GLOBAL" => "void",
			    "LOCAL" => "int "
			    );

		print $file "$tmap{$type}$name($smap{$scope});\n";
	}
}

sub process_file($$$) 
{
	my ($public_file, $private_file, $filename) = @_;

	$filename =~ s/\.o$/\.c/g;

	open(FH, "< $filename") || die "Failed to open $filename";

	print $private_file "\n/* The following definitions come from $filename  */\n\n";

	while (my $line = <FH>) {	      
		my $target = $private_file;

		# these are ordered for maximum speed
		next if ($line =~ /^\s/);
	      
		next unless ($line =~ /\(/);

		next if ($line =~ /^\/|[;]/);

		next unless ( $line =~ /
			      ^void|^BOOL|^int|^struct|^char|^const|^\w+_[tT]\s|^uint|^unsigned|^long|
			      ^NTSTATUS|^ADS_STATUS|^enum\s.*\(|^DATA_BLOB|^WERROR|^XFILE|^FILE|^DIR|
			      ^double|^TDB_CONTEXT|^TDB_DATA|^TALLOC_CTX|^NTTIME|^FN_|^init_module|
			      ^GtkWidget|^GType|^smb_ucs2_t
			      /xo);

		next if ($line =~ /^int\s*main/);

		if ($line =~ /^FN_/) {
			handle_loadparm($public_file, $line);
			next;
		}

		if ($line =~ s/_PUBLIC_//xo) {
			$target = $public_file;
		}

		if ( $line =~ /\(.*\)\s*$/o ) {
			chomp $line;
			print $target "$line;\n";
			next;
		}

		print $target $line;

		while ($line = <FH>) {
			if ($line =~ /\)\s*$/o) {
				chomp $line;
				print $target "$line;\n";
				last;
			}
			print $target $line;
		}
	}

	close(FH);
}

if ($public_file ne $private_file) {
	print_header($private_fd, $private_define);
}
print_header($public_fd, $public_define);
process_file($public_fd, $private_fd, $_) foreach (@ARGV);
print_footer($public_fd, $public_define);
if ($public_file ne $private_file) {
	print_footer($private_fd, $private_define);
}
