###################################################
# Samba3 client generator for IDL structures
# on top of Samba4 style NDR functions
# Copyright jelmer@samba.org 2005-2006
# released under the GNU GPL

package Parse::Pidl::Samba3::ClientNDR;

use strict;
use Parse::Pidl::Typelist qw(hasType getType mapType scalar_is_reference);
use Parse::Pidl::Util qw(has_property ParseExpr is_constant);
use Parse::Pidl::NDR qw(GetPrevLevel GetNextLevel ContainsDeferred);
use Parse::Pidl::Samba4 qw(DeclLong);

use vars qw($VERSION);
$VERSION = '0.01';

my $res;
my $res_hdr;
my $tabs = "";
sub indent() { $tabs.="\t"; }
sub deindent() { $tabs = substr($tabs, 1); }
sub pidl($) { $res .= $tabs.(shift)."\n"; }
sub pidl_hdr($) { $res_hdr .= (shift)."\n"; }
sub fatal($$) { my ($e,$s) = @_; die("$e->{ORIGINAL}->{FILE}:$e->{ORIGINAL}->{LINE}: $s\n"); }
sub warning($$) { my ($e,$s) = @_; warn("$e->{ORIGINAL}->{FILE}:$e->{ORIGINAL}->{LINE}: $s\n"); }
sub fn_declare($) { my ($n) = @_; pidl $n; pidl_hdr "$n;"; }

sub ParseFunction($$)
{
	my ($if,$fn) = @_;

	my $inargs = "";
	my $defargs = "";
	my $uif = uc($if->{NAME});
	my $ufn = "DCERPC_".uc($fn->{NAME});

	foreach (@{$fn->{ELEMENTS}}) {
		$defargs .= ", " . DeclLong($_);
	}
	fn_declare "NTSTATUS rpccli_$fn->{NAME}(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx$defargs)";
	pidl "{";
	indent;
	pidl "struct $fn->{NAME} r;";
	pidl "NTSTATUS status;";
	pidl "";
	pidl "/* In parameters */";

	foreach (@{$fn->{ELEMENTS}}) {
		if (grep(/in/, @{$_->{DIRECTION}})) {
			pidl "r.in.$_->{NAME} = $_->{NAME};";
		} 
	}

	pidl "";
	pidl "if (DEBUGLEVEL >= 10)";
	pidl "\tNDR_PRINT_IN_DEBUG($fn->{NAME}, &r);";
	pidl "";
	pidl "status = cli_do_rpc_ndr(cli, mem_ctx, PI_$uif, $ufn, &r, (ndr_pull_flags_fn_t)ndr_pull_$fn->{NAME}, (ndr_push_flags_fn_t)ndr_push_$fn->{NAME});";
	pidl "";
	pidl "if (DEBUGLEVEL >= 10)";
	pidl "\tNDR_PRINT_OUT_DEBUG($fn->{NAME}, &r);";
	pidl "";
	pidl "if (NT_STATUS_IS_ERR(status)) {";
	pidl "\treturn status;";
	pidl "}";
	pidl "";
	pidl "/* Return variables */";
	foreach my $e (@{$fn->{ELEMENTS}}) {
		next unless (grep(/out/, @{$e->{DIRECTION}}));

		fatal($e, "[out] argument is not a pointer or array") if ($e->{LEVELS}[0]->{TYPE} ne "POINTER" and $e->{LEVELS}[0]->{TYPE} ne "ARRAY");

		pidl "*$e->{NAME} = *r.out.$e->{NAME};";
	}

	pidl"";
	pidl "/* Return result */";
	if (not $fn->{RETURN_TYPE}) {
		pidl "return NT_STATUS_OK;";
	} elsif ($fn->{RETURN_TYPE} eq "NTSTATUS") {
		pidl "return r.out.result;";
	} elsif ($fn->{RETURN_TYPE} eq "WERROR") {
		pidl "return werror_to_ntstatus(r.out.result);";
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

	my $uif = uc($if->{NAME});

	pidl_hdr "#ifndef __CLI_$uif\__";
	pidl_hdr "#define __CLI_$uif\__";
	ParseFunction($if, $_) foreach (@{$if->{FUNCTIONS}});
	pidl_hdr "#endif /* __CLI_$uif\__ */";
}

sub Parse($$$)
{
	my($ndr,$header,$ndr_header) = @_;

	$res = "";
	$res_hdr = "";

	pidl "/*";
	pidl " * Unix SMB/CIFS implementation.";
	pidl " * client auto-generated by pidl. DO NOT MODIFY!";
	pidl " */";
	pidl "";
	pidl "#include \"includes.h\"";
	pidl "#include \"$header\"";
	pidl_hdr "#include \"$ndr_header\"";
	pidl "";
	
	foreach (@$ndr) {
		ParseInterface($_) if ($_->{TYPE} eq "INTERFACE");
	}

	return ($res, $res_hdr);
}

1;
