###################################################
# Samba4 parser generator for swig wrappers
# Copyright tpot@samba.org 2004,2005
# Copyright jelmer@samba.org 2006
# released under the GNU GPL

package Parse::Pidl::Samba4::SWIG;

use vars qw($VERSION);
use Parse::Pidl::Samba4 qw(DeclLong);
use Parse::Pidl::Typelist qw(mapTypeName);
use Parse::Pidl::Util qw(has_property);
$VERSION = '0.01';

use strict;

my $ret = "";
my $tabs = "";

sub pidl($)
{
	my $p = shift;
	$ret .= $tabs. $p . "\n";
}

sub indent() { $tabs.="\t"; }
sub deindent() { $tabs = substr($tabs,0,-1); }

sub IgnoreInterface($$)
{
	my ($basename,$if) = @_;

	foreach (@{$if->{TYPES}}) {
		next unless (has_property($_, "public"));
		pidl "\%types($_->{NAME});";
	}
}

sub ParseInterface($$)
{
	my ($basename,$if) = @_;

	pidl "\%inline {";
	pidl "struct $if->{NAME} { struct dcerpc_pipe *pipe; };";
	pidl "}";
	pidl "";
	pidl "\%extend $if->{NAME} {";
	indent();
	pidl "$if->{NAME} (const char *binding, struct cli_credentials *cred = NULL, TALLOC_CTX *mem_ctx = NULL, struct event_context *event = NULL)";
	pidl "{";
	indent;
	pidl "struct $if->{NAME} *ret = talloc(mem_ctx, struct $if->{NAME});";
	pidl "NTSTATUS status;";
	pidl "";
	pidl "status = dcerpc_pipe_connect(mem_ctx, &ret->pipe, binding, &dcerpc_table_$if->{NAME}, cred, event);";
	pidl "if (NT_STATUS_IS_ERR(status)) {";
	pidl "\tntstatus_exception(status);";
	pidl "\treturn NULL;";
	pidl "}";
	pidl "";
	pidl "return ret;";
	deindent;
	pidl "}";
	pidl "";
	pidl "~$if->{NAME}() {";
	pidl "\ttalloc_free(self);";
	pidl "}";
	pidl "";

	foreach my $fn (@{$if->{FUNCTIONS}}) {
		pidl "/* $fn->{NAME} */";
		my $args = "";
		foreach (@{$fn->{ELEMENTS}}) {
			$args .= DeclLong($_) . ", ";
		}
		my $name = $fn->{NAME};
		$name =~ s/^$if->{NAME}_//g;
		$name =~ s/^$basename\_//g;
		$args .= "TALLOC_CTX *mem_ctx = NULL";
		pidl mapTypeName($fn->{RETURN_TYPE}) . " $name($args)";
		pidl "{";
		indent;
		pidl "struct $fn->{NAME} r;";
		pidl "NTSTATUS status;";
		pidl "";
		pidl "/* Fill r structure */";

		foreach (@{$fn->{ELEMENTS}}) {
			if (grep(/in/, @{$_->{DIRECTION}})) {
				pidl "r.in.$_->{NAME} = $_->{NAME};";
			} 
		}

		pidl "";
		pidl "status = dcerpc_$fn->{NAME}(self->pipe, mem_ctx, &r);";
		pidl "if (NT_STATUS_IS_ERR(status)) {";
		pidl "\tntstatus_exception(status);";
		if (defined($fn->{RETURN_TYPE})) {
			pidl "\treturn r.out.result;";
		} else {
			pidl "\treturn;";
		}
		pidl "}";
		pidl "";
		pidl "/* Set out arguments */";
		foreach (@{$fn->{ELEMENTS}}) {
			next unless (grep(/out/, @{$_->{DIRECTION}}));

			pidl ("/* FIXME: $_->{NAME} [out] argument is not a pointer */") if ($_->{LEVELS}[0]->{TYPE} ne "POINTER");

			pidl "*$_->{NAME} = *r.out.$_->{NAME};";
		}

		if (defined($fn->{RETURN_TYPE})) {
			pidl "return r.out.result;";
		}
		deindent;
		pidl "}";
		pidl "";
	}

	deindent();
	pidl "};";
	pidl "";

	foreach (@{$if->{TYPES}}) {
		pidl "/* $_->{NAME} */";
	}
	
	pidl "";
}

sub Parse($$$$)
{
    my($ndr,$basename,$header,$gen_header) = @_;

	$ret = "";

	pidl "/* This file is autogenerated by pidl. DO NOT EDIT */";

	pidl "\%module $basename";
	
	pidl "";

	pidl "\%{";
	pidl "#include \"includes.h\"";
	pidl "#include \"auth/credentials/credentials.h\"";
	pidl "#include \"$header\"";
	pidl "#include \"$gen_header\"";
	pidl "%}";
	pidl "\%import \"samba.i\"";
	pidl "";
	pidl "\%inline {";
	pidl "void ntstatus_exception(NTSTATUS status)"; 
	pidl "{";
	pidl "\t/* FIXME */";
	pidl "}";
	pidl "}";
	pidl "";
	foreach (@$ndr) {
		IgnoreInterface($basename, $_) if ($_->{TYPE} eq "INTERFACE");
	}
	pidl "";

	pidl "";

	foreach (@$ndr) {
		ParseInterface($basename, $_) if ($_->{TYPE} eq "INTERFACE");
	}
	#FIXME: Foreach ref pointer, set NONNULL
	#FIXME: Foreach unique/full pointer, set MAYBENULL
	#FIXME: Foreach [out] parameter, set OUTPARAM
	return $ret;
}

1;
