###################################################
# client calls generator
# Copyright tridge@samba.org 2003
# released under the GNU GPL

package IdlClient;

use Data::Dumper;

my($res);

#####################################################################
# parse a function
sub ParseFunction($)
{
	my $fn = shift;
	my $name = $fn->{NAME};
	my $uname = uc $name;

	if ($fn->{RETURN_TYPE} ne "NTSTATUS") {
		$res .= "
NTSTATUS dcerpc_$name(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct $name *r)
{
	return dcerpc_ndr_request(p, DCERPC_$uname, mem_ctx,
				  (ndr_push_fn_t) ndr_push_$name,
				  (ndr_pull_fn_t) ndr_pull_$name,
				  r);
}
";
	} else {
		$res .= "
NTSTATUS dcerpc_$name(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct $name *r)
{
	NTSTATUS status;
	status = dcerpc_ndr_request(p, DCERPC_$uname, mem_ctx,
				    (ndr_push_fn_t) ndr_push_$name,
				    (ndr_pull_fn_t) ndr_pull_$name,
				    r);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	return r->out.result;
}
";
}
}

#####################################################################
# parse the interface definitions
sub ParseInterface($)
{
	my($interface) = shift;
	my($data) = $interface->{DATA};
	foreach my $d (@{$data}) {
		($d->{TYPE} eq "FUNCTION") && 
		    ParseFunction($d);
	}
}


#####################################################################
# parse a parsed IDL structure back into an IDL file
sub Parse($)
{
	my($idl) = shift;
	$res = "/* dcerpc client calls auto-generated by pidl */\n\n";
	$res .= "#include \"includes.h\"\n\n";
	foreach my $x (@{$idl}) {
		($x->{TYPE} eq "INTERFACE") && 
		    ParseInterface($x);
	}
	return $res;
}

1;
