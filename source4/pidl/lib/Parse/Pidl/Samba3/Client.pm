###################################################
# Samba3 NDR client generator for IDL structures
# Copyright jelmer@samba.org 2005
# released under the GNU GPL

package Parse::Pidl::Samba3::Client;

use strict;
use Parse::Pidl::Typelist qw(hasType getType mapType);
use Parse::Pidl::Util qw(has_property ParseExpr);
use Parse::Pidl::NDR qw(GetPrevLevel GetNextLevel ContainsDeferred);
use Parse::Pidl::Samba3::Types qw(DeclLong);

use vars qw($VERSION);
$VERSION = '0.01';

my $res = "";
my $tabs = "";
sub indent() { $tabs.="\t"; }
sub deindent() { $tabs = substr($tabs, 1); }
sub pidl($) { $res .= $tabs.(shift)."\n"; }
sub fatal($$) { my ($e,$s) = @_; die("$e->{FILE}:$e->{LINE}: $s\n"); }
sub warning($$) { my ($e,$s) = @_; warn("$e->{FILE}:$e->{LINE}: $s\n"); }

sub CopyLevel($$$$)
{
	sub CopyLevel($$$$);
	my ($e,$l,$argument,$member) = @_;

	if ($l->{TYPE} eq "DATA") {
		pidl "*$argument = $member;";
	} elsif ($l->{TYPE} eq "POINTER") {
		pidl "if (r.ptr$l->{POINTER_INDEX}_$e->{NAME}) {";
		indent;
		pidl "*$argument = talloc_size(mem_ctx, sizeof(void *));";
		CopyLevel($e,GetNextLevel($e,$l),"*$argument", $member);
		deindent;
		pidl "}";
	} elsif ($l->{TYPE} eq "SWITCH") {
		CopyLevel($e,GetNextLevel($e,$l),$argument,$member);	
	} elsif ($l->{TYPE} eq "ARRAY") {
		pidl "*$argument = $member;";
	}
}

sub ParseFunction($$)
{
	my ($if,$fn) = @_;

	my $inargs = "";
	my $defargs = "";
	foreach (@{$fn->{ELEMENTS}}) {
		$defargs .= ", " . DeclLong($_);
		if (grep(/in/, @{$_->{DIRECTION}})) {
			$inargs .= ", $_->{NAME}";
		} 
	}

	my $uif = uc($if->{NAME});
	my $ufn = uc($fn->{NAME});

	pidl "NTSTATUS rpccli_$fn->{NAME}(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx$defargs)";
	pidl "{";
	indent;
	pidl "prs_struct qbuf, rbuf;";
	pidl "$uif\_Q_$ufn q;";
	pidl "$uif\_R_$ufn r;";
	pidl "";
	pidl "ZERO_STRUCT(q);";
	pidl "ZERO_STRUCT(r);";
	pidl "";
	pidl "/* Marshall data and send request */";
	pidl "";
	pidl "init_$if->{NAME}_q_$fn->{NAME}(&q$inargs);";
	pidl "";
	pidl "CLI_DO_RPC(cli, mem_ctx, PI_$uif, $ufn,";
	pidl "\tq, r,";
	pidl "\tqbuf, rbuf, ";
	pidl "\t$if->{NAME}_io_q_$fn->{NAME},";
	pidl "\t$if->{NAME}_io_r_$fn->{NAME},";
	pidl "\tNT_STATUS_UNSUCCESSFUL);";
	pidl "";
	pidl "/* Return variables */";
	foreach my $e (@{$fn->{ELEMENTS}}) {
		next unless (grep(/out/, @{$e->{DIRECTION}}));
		
		if ($e->{LEVELS}[0]->{TYPE} ne "POINTER") {
			warning($e->{ORIGINAL}, "First element not a pointer for [out] argument");
			next;
		}
		CopyLevel($e, $e->{LEVELS}[1], $e->{NAME}, "r.$e->{NAME}");
	}

	pidl"";
	pidl "/* Return result */";
	if (not $fn->{RETURN_TYPE}) {
		pidl "return NT_STATUS_OK;";
	} elsif ($fn->{RETURN_TYPE} eq "NTSTATUS") {
		pidl "return r.status;";
	} elsif ($fn->{RETURN_TYPE} eq "WERROR") {
		pidl "return werror_to_ntstatus(r.status);";
	} else {
		pidl "/* Sorry, don't know how to convert $fn->{RETURN_TYPE} to NTSTATUS */";
		pidl "return NT_STATUS_OK;";
	}

	deindent;
	pidl "}";
	pidl "";
}

sub ParseInterface($)
{
	my $if = shift;

	ParseFunction($if, $_) foreach (@{$if->{FUNCTIONS}});
}

sub Parse($$)
{
	my($ndr,$filename) = @_;

	$res = "";

	pidl "/*";
	pidl " * Unix SMB/CIFS implementation.";
	pidl " * client auto-generated by pidl. DO NOT MODIFY!";
	pidl " */";
	pidl "";
	pidl "#include \"includes.h\"";
	pidl "";
	
	foreach (@$ndr) {
		ParseInterface($_) if ($_->{TYPE} eq "INTERFACE");
	}

	return $res;
}

1;
