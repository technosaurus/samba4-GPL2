###################################################
# client calls generator
# Copyright tridge@samba.org 2003
# released under the GNU GPL

package IdlClient;

use strict;

my($res);

#####################################################################
# parse a function
sub ParseFunction($)
{
	my $fn = shift;
	my $name = $fn->{NAME};
	my $uname = uc $name;

	return if (util::has_property($fn, "local"));

	$res .= 
"
struct rpc_request *dcerpc_$name\_send(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct $name *r)
{
        if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG($name, r);		
	}

	return dcerpc_ndr_request_send(p, DCERPC_$uname, mem_ctx,
				    (ndr_push_flags_fn_t) ndr_push_$name,
				    (ndr_pull_flags_fn_t) ndr_pull_$name,
				    r, sizeof(*r));
}

";

	$res .= 
"
NTSTATUS dcerpc_$name(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct $name *r)
{
	struct rpc_request *req = dcerpc_$name\_send(p, mem_ctx, r);
	NTSTATUS status;
	if (req == NULL) return NT_STATUS_NO_MEMORY;

	status = dcerpc_ndr_request_recv(req);

        if (NT_STATUS_IS_OK(status) && (p->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_OUT_DEBUG($name, r);		
	}
";
        if ($fn->{RETURN_TYPE} eq "NTSTATUS") {
             $res .= "\tif (NT_STATUS_IS_OK(status)) status = r->out.result;\n";
        }
	$res .= 
"
	return status;
}
";
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
	$res = "/* dcerpc client calls generated by pidl */\n\n";
	foreach my $x (@{$idl}) {
		($x->{TYPE} eq "INTERFACE") && 
		    ParseInterface($x);
	}
	return $res;
}

1;
