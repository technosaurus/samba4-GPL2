###################################################
# stub boilerplate generator
# Copyright jelmer@samba.org 2004
# Copyright tridge@samba.org 2003
# released under the GNU GPL

package IdlStub;

use strict;

my($res);

sub pidl($)
{
	$res .= shift;
}


#####################################################
# generate the switch statement for function dispatch
sub gen_dispatch_switch($)
{
	my $data = shift;

	my $count = 0;
	foreach my $d (@{$data}) {
		next if ($d->{TYPE} ne "FUNCTION");

		pidl "\tcase $count: {\n";
		if ($d->{RETURN_TYPE} && $d->{RETURN_TYPE} ne "void") {
			pidl "\t\tNTSTATUS result;\n";
		}
		pidl "\t\tstruct $d->{NAME} *r2 = r;\n";
		pidl "\t\tif (DEBUGLEVEL > 10) {\n";
		pidl "\t\t\tNDR_PRINT_FUNCTION_DEBUG($d->{NAME}, NDR_IN, r2);\n";
		pidl "\t\t}\n";
		if ($d->{RETURN_TYPE} && $d->{RETURN_TYPE} ne "void") {
			pidl "\t\tresult = vtable->$d->{NAME}(iface, mem_ctx, r2);\n";
		} else {
			pidl "\t\tvtable->$d->{NAME}(iface, mem_ctx, r2);\n";
		}
		pidl "\t\tif (DEBUGLEVEL > 10 && dce_call->fault_code == 0) {\n";
		pidl "\t\t\tNDR_PRINT_FUNCTION_DEBUG($d->{NAME}, NDR_OUT | NDR_SET_VALUES, r2);\n";
		pidl "\t\t}\n";
		pidl "\t\tif (dce_call->fault_code != 0) {\n";
		pidl "\t\t\tDEBUG(2,(\"dcerpc_fault 0x%x in $d->{NAME}\\n\", dce_call->fault_code));\n";
		pidl "\t\t}\n";
		pidl "\t\tbreak;\n\t}\n";
		$count++; 
	}
}


#####################################################################
# produce boilerplate code for a interface
sub Boilerplate_Iface($)
{
	my($interface) = shift;
	my($data) = $interface->{DATA};
	my $count = 0;
	my $name = $interface->{NAME};
	my $uname = uc $name;

	foreach my $d (@{$data}) {
		if ($d->{TYPE} eq "FUNCTION") { $count++; }
	}

	if ($count == 0) {
		return;
	}

	pidl "
static NTSTATUS $name\__op_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface)
{
#ifdef DCESRV_INTERFACE_$uname\_BIND
	return DCESRV_INTERFACE_$uname\_BIND(dce_call,iface);
#else
	return NT_STATUS_OK;
#endif
}

static void $name\__op_unbind(struct dcesrv_connection *dce_conn, const struct dcesrv_interface *iface)
{
#ifdef DCESRV_INTERFACE_$uname\_UNBIND
	DCESRV_INTERFACE_$uname\_UNBIND(dce_conn,iface);
#else
	return;
#endif
}

static NTSTATUS $name\__op_dispatch(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	uint16 opnum = dce_call->pkt.u.request.opnum;
	struct GUID ipid = dce_call->pkt.u.request.object.object;
	struct dcom_interface_p *iface = dcom_get_local_iface_p(&ipid);
	const struct dcom_$name\_vtable *vtable = iface->vtable;

	dce_call->fault_code = 0;

	switch (opnum) {
";
	gen_dispatch_switch($data);

pidl "
	default:
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		break;
	}

	if (dce_call->fault_code != 0) {
		return NT_STATUS_NET_WRITE_FAULT;
	}
	return NT_STATUS_OK;
}

static const struct dcesrv_interface $name\_interface = {
	&dcerpc_table_$name,
	$name\__op_bind,
	$name\__op_unbind,
	$name\__op_dispatch
};

";
}

#####################################################################
# produce boilerplate code for an endpoint server
sub Boilerplate_Ep_Server($)
{
	my($interface) = shift;
	my($data) = $interface->{DATA};
	my $count = 0;
	my $name = $interface->{NAME};
	my $uname = uc $name;

	foreach my $d (@{$data}) {
		if ($d->{TYPE} eq "FUNCTION") { $count++; }
	}

	if ($count == 0) {
		return;
	}

	pidl "
static NTSTATUS $name\__op_init_server(struct dcesrv_context *dce_ctx, const struct dcesrv_endpoint_server *ep_server)
{
	int i;

	for (i=0;i<$name\_interface.ndr->endpoints->count;i++) {
		NTSTATUS ret;
		const char *name = $name\_interface.ndr->endpoints->names[i];

		ret = dcesrv_interface_register(dce_ctx, name, &$name\_interface, NULL);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1,(\"$name\_op_init_server: failed to register endpoint \'%s\'\\n\",name));
			return ret;
		}
	}

	return NT_STATUS_OK;
}

static BOOL $name\__op_interface_by_uuid(struct dcesrv_interface *iface, const char *uuid, uint32 if_version)
{
	if ($name\_interface.ndr->if_version == if_version &&
		strcmp($name\_interface.ndr->uuid, uuid)==0) {
		memcpy(iface,&$name\_interface, sizeof(*iface));
		return True;
	}

	return False;
}

static BOOL $name\__op_interface_by_name(struct dcesrv_interface *iface, const char *name)
{
	if (strcmp($name\_interface.ndr->name, name)==0) {
		memcpy(iface,&$name\_interface, sizeof(*iface));
		return True;
	}

	return False;	
}
	
NTSTATUS dcerpc_server_$name\_init(void)
{
	NTSTATUS ret;
	struct dcesrv_endpoint_server ep_server;

	/* fill in our name */
	ep_server.name = \"$name\";

	/* fill in all the operations */
	ep_server.init_server = $name\__op_init_server;

	ep_server.interface_by_uuid = $name\__op_interface_by_uuid;
	ep_server.interface_by_name = $name\__op_interface_by_name;

	/* register ourselves with the DCERPC subsystem. */
    ret = dcerpc_register_ep_server(&ep_server);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,(\"Failed to register \'$name\' endpoint server!\\n\"));
		return ret;
	}

	return ret;
}

";
}

sub ParseInterface($)
{
	my($interface) = shift;
	$res = "/* dcom interface stub generated by pidl */\n\n";

	Boilerplate_Iface($interface);
	Boilerplate_Ep_Server($interface);

	return $res;
}

1;
