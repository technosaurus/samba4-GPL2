###################################################
# DCOM proxy generator
# Copyright jelmer@samba.org 2003
# released under the GNU GPL

package IdlProxy;

use strict;

my($res);

sub ParseVTable($$)
{
	my $interface = shift;
	my $name = shift;

	# Generate the vtable
	$res .="\tstruct dcom_$interface->{NAME}_vtable $name = {";

	if (defined($interface->{BASE})) {
		$res .= "\n\t\t{},";
	}

	my $data = $interface->{DATA};

	foreach my $d (@{$data}) {
		if ($d->{TYPE} eq "FUNCTION") {
		    $res .= "\n\t\tdcom_proxy_$interface->{NAME}_$d->{NAME}";
			$res .= ",";
		}
	}

	$res .= "\n\t};\n\n";
}

sub ParseRegFunc($)
{
	my $interface = shift;

	$res .= "static NTSTATUS dcom_$interface->{NAME}_init(void)
{
	struct dcom_interface iface;
";
	
	ParseVTable($interface, "proxy");

	if (defined($interface->{BASE})) {
		$res.= "
	const void *base_vtable;

	GUID_from_string(DCERPC_" . (uc $interface->{BASE}) . "_UUID, &iface.base_iid);

	base_vtable = dcom_proxy_vtable_by_iid(&iface.base_iid);
	if (base_vtable == NULL) {
		return NT_STATUS_FOOBAR;
	}

	proxy.base = *((const struct dcom_$interface->{BASE}_vtable *)base_vtable);
	";
	} else {
		$res .= "\tZERO_STRUCT(iface.base_iid);\n";
	}

	$res.= "
	iface.num_methods = DCERPC_" . (uc $interface->{NAME}) . "_CALL_COUNT;
	GUID_from_string(DCERPC_" . (uc $interface->{NAME}) . "_UUID, &iface.iid);
	iface.proxy_vtable = talloc_memdup(talloc_autofree_context(), &proxy, sizeof(struct dcom_$interface->{NAME}_vtable));

	return dcom_register_interface(&iface);
}\n\n";
}

#####################################################################
# parse a function
sub ParseFunction($$)
{
	my $interface = shift;
	my $fn = shift;
	my $name = $fn->{NAME};
	my $uname = uc $name;

	if (util::has_property($fn, "local")) {
		$res .= "
static NTSTATUS dcom_proxy_$interface->{NAME}_$name(struct dcom_interface_p *d, TALLOC_CTX *mem_ctx, struct $name *r)
{
	/* FIXME */
	return NT_STATUS_NOT_SUPPORTED;
}\n";
	} else {
		$res .= "
static struct rpc_request *dcom_proxy_$interface->{NAME}_$name\_send(struct dcom_interface_p *d, TALLOC_CTX *mem_ctx, struct $name *r)
{
	struct dcerpc_pipe *p;
	NTSTATUS status = dcom_get_pipe(d, &p);

	if (NT_STATUS_IS_ERR(status)) {
		return NULL;
	}

	ZERO_STRUCT(r->in.ORPCthis);
	r->in.ORPCthis.version.MajorVersion = COM_MAJOR_VERSION;
	r->in.ORPCthis.version.MinorVersion = COM_MINOR_VERSION;

	if (p->flags & DCERPC_DEBUG_PRINT_IN) {
		NDR_PRINT_IN_DEBUG($name, r);		
	}

	return dcerpc_ndr_request_send(p, &d->ipid, &dcerpc_table_$interface->{NAME}, DCERPC_$uname, mem_ctx, r);
}

static NTSTATUS dcom_proxy_$interface->{NAME}_$name(struct dcom_interface_p *d, TALLOC_CTX *mem_ctx, struct $name *r)
{
	struct dcerpc_pipe *p;
	NTSTATUS status = dcom_get_pipe(d, &p);
	struct rpc_request *req;

	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	req = dcom_proxy_$interface->{NAME}_$name\_send(d, mem_ctx, r);
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
}";
	}

	$res .=" 
NTSTATUS dcom_$interface->{NAME}_$name (struct dcom_interface_p *d, TALLOC_CTX *mem_ctx, struct $name *r)
{
	return ((const struct dcom_$interface->{NAME}_vtable *)d->vtable)->$name (d, mem_ctx, r);
}
";
}


#####################################################################
# parse the interface definitions
sub ParseInterface($)
{
	my($interface) = shift;
	my($data) = $interface->{DATA};
	$res = "/* DCOM stubs generated by pidl */\n\n";
	foreach my $d (@{$data}) {
		($d->{TYPE} eq "FUNCTION") && 
		ParseFunction($interface, $d);
	}

	ParseRegFunc($interface);
}

sub RegistrationFunction($$)
{
	my $idl = shift;
	my $basename = shift;

	my $res = "NTSTATUS dcom_$basename\_init(void)\n";
	$res .= "{\n";
	$res .="\tNTSTATUS status = NT_STATUS_OK;\n";
	foreach my $interface (@{$idl}) {
		next if $interface->{TYPE} ne "INTERFACE";
		next if not util::has_property($interface, "object");

		my $data = $interface->{INHERITED_DATA};
		my $count = 0;
		foreach my $d (@{$data}) {
			if ($d->{TYPE} eq "FUNCTION") { $count++; }
		}

		next if ($count == 0);

		$res .= "\tstatus = dcom_$interface->{NAME}_init();\n";
		$res .= "\tif (NT_STATUS_IS_ERR(status)) {\n";
		$res .= "\t\treturn status;\n";
		$res .= "\t}\n\n";
	}
	$res .= "\treturn status;\n";
	$res .= "}\n\n";

	return $res;
}

1;
