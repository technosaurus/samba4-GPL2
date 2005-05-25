###################################################
# client calls generator
# Copyright tridge@samba.org 2003
# released under the GNU GPL

package NdrClient;

use strict;

my($res);

#####################################################################
# parse a function
sub ParseFunction($$)
{
	my $interface = shift;
	my $fn = shift;
	my $name = $fn->{NAME};
	my $uname = uc $name;

	$res .= "
struct rpc_request *dcerpc_$name\_send(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct $name *r)
{
	if (p->conn->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG($name, r);
	}
	
	return dcerpc_ndr_request_send(p, NULL, &dcerpc_table_$interface->{NAME}, DCERPC_$uname, mem_ctx, r);
}

NTSTATUS dcerpc_$name(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct $name *r)
{
	struct rpc_request *req;
	NTSTATUS status;
	
	req = dcerpc_$name\_send(p, mem_ctx, r);
	if (req == NULL) return NT_STATUS_NO_MEMORY;

	status = dcerpc_ndr_request_recv(req);

        if (NT_STATUS_IS_OK(status) && (p->conn->flags & DCERPC_DEBUG_PRINT_OUT)) {
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

my %done;

#####################################################################
# parse the interface definitions
sub ParseInterface($)
{
	my($interface) = shift;
	my($data) = $interface->{DATA};
	$res = "/* Client functions generated by pidl */\n\n";
	
	foreach my $d (@{$data}) {
		if (($d->{TYPE} eq "FUNCTION") and not $done{$d->{NAME}}) {
			ParseFunction($interface, $d);
		}
		$done{$d->{NAME}} = 1;
	}
	return $res;
}

1;
