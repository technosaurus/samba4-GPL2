###################################################
# Samba4 parser generator for IDL structures
# Copyright tridge@samba.org 2000-2003
# Copyright tpot@samba.org 2001
# released under the GNU GPL

package IdlParser;

use Data::Dumper;

my($res);

# the list of needed functions
my %needed;

#####################################################################
# parse a properties list
sub ParseProperties($)
{
    my($props) = shift;
    foreach my $d (@{$props}) {
	if (ref($d) ne "HASH") {
	    $res .= "[$d] ";
	} else {
	    foreach my $k (keys %{$d}) {
		$res .= "[$k($d->{$k})] ";
	    }
	}
    }
}

####################################################################
# work out the name of a size_is() variable
sub find_size_var($$)
{
	my($e) = shift;
	my($size) = shift;
	my($fn) = $e->{PARENT};

	if (util::is_constant($size)) {
		return $size;
	}
	
	if ($fn->{TYPE} ne "FUNCTION") {
		return "r->$size";
	}

	for my $e2 (@{$fn->{DATA}}) {
		if ($e2->{NAME} eq $size) {
			if (util::has_property($e2, "in")) {
				return "r->in.$size";
			}
			if (util::has_property($e2, "out")) {
				return "r->out.$size";
			}
		}
	}
	die "invalid variable in $size for element $e->{NAME} in $fn->{NAME}\n";
}


#####################################################################
# work out the correct alignment for a structure
sub struct_alignment($)
{
	my $s = shift;
	# why do we need a minimum alignment of 4 ?? 
	my $align = 4;
	for my $e (@{$s->{ELEMENTS}}) {
		if ($align < util::type_align($e)) {
			$align = util::type_align($e);
		}
	}
	return $align;
}

#####################################################################
# parse an array - push side
sub ParseArrayPush($$)
{
	my $e = shift;
	my $var_prefix = shift;
	my $size = find_size_var($e, util::array_size($e));
	my $const = "";

	if (util::is_constant($size)) {
		$const = "_const";
	}

	if (util::is_scalar_type($e->{TYPE})) {
		$res .= "\t\tNDR_CHECK(ndr_push$const\_array_$e->{TYPE}(ndr, $var_prefix$e->{NAME}, $size));\n";
	} else {
		$res .= "\t\tNDR_CHECK(ndr_push$const\_array(ndr, ndr_flags, $var_prefix$e->{NAME}, sizeof($var_prefix$e->{NAME}\[0]), $size, (ndr_push_flags_fn_t)ndr_push_$e->{TYPE}));\n";
	}
}

#####################################################################
# print an array
sub ParseArrayPrint($$)
{
	my $e = shift;
	my $var_prefix = shift;
	my $size = find_size_var($e, util::array_size($e));

	if (util::is_scalar_type($e->{TYPE})) {
		$res .= "\t\tndr_print_array_$e->{TYPE}(ndr, \"$e->{NAME}\", $var_prefix$e->{NAME}, $size);\n";
	} else {
		$res .= "\t\tndr_print_array(ndr, \"$e->{NAME}\", $var_prefix$e->{NAME}, sizeof($var_prefix$e->{NAME}\[0]), $size, (ndr_print_fn_t)ndr_print_$e->{TYPE});\n";
	}
}

#####################################################################
# parse an array - pull side
sub ParseArrayPull($$)
{
	my $e = shift;
	my $var_prefix = shift;
	my $size = find_size_var($e, util::array_size($e));
	my $const = "";

	if (util::is_constant($size)) {
		$const = "_const";
	}

	if (util::need_alloc($e) && !util::is_constant($size)) {
		$res .= "\t\tNDR_ALLOC_N_SIZE(ndr, $var_prefix$e->{NAME}, $size, sizeof($var_prefix$e->{NAME}\[0]));\n";
	}
	if (util::is_scalar_type($e->{TYPE})) {
		$res .= "\t\tNDR_CHECK(ndr_pull$const\_array_$e->{TYPE}(ndr, $var_prefix$e->{NAME}, $size));\n";
	} else {
		$res .= "\t\tNDR_CHECK(ndr_pull$const\_array(ndr, ndr_flags, (void **)$var_prefix$e->{NAME}, sizeof($var_prefix$e->{NAME}\[0]), $size, (ndr_pull_flags_fn_t)ndr_pull_$e->{TYPE}));\n";
	}
}


#####################################################################
# parse scalars in a structure element
sub ParseElementPushScalar($$$)
{
	my($e) = shift;
	my($var_prefix) = shift;
	my($ndr_flags) = shift;
	my $cprefix = util::c_push_prefix($e);

	if (defined $e->{VALUE}) {
		$res .= "\tNDR_CHECK(ndr_push_$e->{TYPE}(ndr, $e->{VALUE}));\n";
	} elsif (util::need_wire_pointer($e)) {
		$res .= "\tNDR_CHECK(ndr_push_ptr(ndr, $var_prefix$e->{NAME}));\n";
	} elsif (util::is_builtin_type($e->{TYPE})) {
		$res .= "\tNDR_CHECK(ndr_push_$e->{TYPE}(ndr, $cprefix$var_prefix$e->{NAME}));\n";
	} else {
		$res .= "\tNDR_CHECK(ndr_push_$e->{TYPE}(ndr, $ndr_flags, $cprefix$var_prefix$e->{NAME}));\n";
	}
}

#####################################################################
# print scalars in a structure element
sub ParseElementPrintScalar($$)
{
	my($e) = shift;
	my($var_prefix) = shift;
	my $cprefix = util::c_push_prefix($e);

	if (util::has_property($e, "struct_len")) {
		return;
	}

	if (defined $e->{VALUE}) {
		$res .= "\tndr_print_$e->{TYPE}(ndr, \"$e->{NAME}\", $e->{VALUE});\n";
	} elsif (util::has_direct_buffers($e)) {
		$res .= "\tndr_print_ptr(ndr, \"$e->{NAME}\", $var_prefix$e->{NAME});\n";
		$res .= "\tndr->depth++;\n";
		ParseElementPrintBuffer($e, "r->");
		$res .= "\tndr->depth--;\n";
	} else {
		$res .= "\tndr_print_$e->{TYPE}(ndr, \"$e->{NAME}\", $cprefix$var_prefix$e->{NAME});\n";
	}
}

#####################################################################
# parse scalars in a structure element - pull size
sub ParseElementPullSwitch($$$$)
{
	my($e) = shift;
	my($var_prefix) = shift;
	my($ndr_flags) = shift;
	my $switch = shift;
	my $switch_var = find_size_var($e, $switch);

	my $cprefix = util::c_pull_prefix($e);

	$res .= "\t{ uint16 _level;\n";
	$res .= "\tNDR_CHECK(ndr_pull_$e->{TYPE}(ndr, $ndr_flags, &_level, $cprefix$var_prefix$e->{NAME}));\n";
	$res .= "\tif (_level != $switch_var) return NT_STATUS_INVALID_LEVEL;\n";
	$res .= "\t}\n";
}


#####################################################################
# parse scalars in a structure element - pull size
sub ParseElementPullScalar($$$)
{
	my($e) = shift;
	my($var_prefix) = shift;
	my($ndr_flags) = shift;
	my $cprefix = util::c_pull_prefix($e);

	if (defined $e->{VALUE}) {
		$res .= "\tNDR_CHECK(ndr_pull_$e->{TYPE}(ndr, $e->{VALUE}));\n";
	} elsif (util::need_wire_pointer($e)) {
		$res .= "\tNDR_CHECK(ndr_pull_uint32(ndr, &_ptr_$e->{NAME}));\n";
		$res .= "\tif (_ptr_$e->{NAME}) {\n";
		$res .= "\t\tNDR_ALLOC(ndr, $var_prefix$e->{NAME});\n";
		$res .= "\t} else {\n";
		$res .= "\t\t$var_prefix$e->{NAME} = NULL;\n";
		$res .= "\t}\n";
	} elsif (util::need_alloc($e)) {
		# no scalar component
	} elsif (my $switch = util::has_property($e, "switch_is")) {
		ParseElementPullSwitch($e, $var_prefix, $ndr_flags, $switch);
	} elsif (util::is_builtin_type($e->{TYPE})) {
		$res .= "\tNDR_CHECK(ndr_pull_$e->{TYPE}(ndr, $cprefix$var_prefix$e->{NAME}));\n";
	} else {
		$res .= "\tNDR_CHECK(ndr_pull_$e->{TYPE}(ndr, $ndr_flags, $cprefix$var_prefix$e->{NAME}));\n";
	}
}

#####################################################################
# parse buffers in a structure element
sub ParseElementPushBuffer($$)
{
	my($e) = shift;
	my($var_prefix) = shift;
	my $cprefix = util::c_push_prefix($e);

	if (util::is_pure_scalar($e)) {
		return;
	}

	if (util::need_wire_pointer($e)) {
		$res .= "\tif ($var_prefix$e->{NAME}) {\n";
	}
	    
	if (util::array_size($e)) {
		ParseArrayPush($e, "r->");
	} elsif (util::is_builtin_type($e->{TYPE})) {
		$res .= "\t\tNDR_CHECK(ndr_push_$e->{TYPE}(ndr, $cprefix$var_prefix$e->{NAME}));\n";
	} else {
		$res .= "\t\tNDR_CHECK(ndr_push_$e->{TYPE}(ndr, ndr_flags, $cprefix$var_prefix$e->{NAME}));\n";
	}

	if (util::need_wire_pointer($e)) {
		$res .= "\t}\n";
	}	
}

#####################################################################
# print buffers in a structure element
sub ParseElementPrintBuffer($$)
{
	my($e) = shift;
	my($var_prefix) = shift;
	my $cprefix = util::c_push_prefix($e);

	if (util::is_pure_scalar($e)) {
		return;
	}

	if (util::need_wire_pointer($e)) {
		$res .= "\tif ($var_prefix$e->{NAME}) {\n";
	}
	    
	if (util::array_size($e)) {
		ParseArrayPrint($e, "r->");
	} else {
		$res .= "\t\tndr_print_$e->{TYPE}(ndr, \"$e->{NAME}\", $cprefix$var_prefix$e->{NAME});\n";
	}

	if (util::need_wire_pointer($e)) {
		$res .= "\t}\n";
	}	
}


#####################################################################
# parse buffers in a structure element - pull side
sub ParseElementPullBuffer($$$)
{
	my($e) = shift;
	my($var_prefix) = shift;
	my($ndr_flags) = shift;
	my $cprefix = util::c_pull_prefix($e);

	if (util::is_pure_scalar($e)) {
		return;
	}

	if (util::need_wire_pointer($e)) {
		$res .= "\tif ($var_prefix$e->{NAME}) {\n";
	}
	    
	if (util::array_size($e)) {
		ParseArrayPull($e, "r->");
	} elsif (my $switch = util::has_property($e, "switch_is")) {
		ParseElementPullSwitch($e, $var_prefix, $ndr_flags, $switch);
	} elsif (util::is_builtin_type($e->{TYPE})) {
		$res .= "\t\tNDR_CHECK(ndr_pull_$e->{TYPE}(ndr, $cprefix$var_prefix$e->{NAME}));\n";
	} elsif ($e->{POINTERS}) {
		$res .= "\t\tNDR_CHECK(ndr_pull_$e->{TYPE}(ndr, NDR_SCALARS|NDR_BUFFERS, $cprefix$var_prefix$e->{NAME}));\n";
	} else {
		$res .= "\t\tNDR_CHECK(ndr_pull_$e->{TYPE}(ndr, $ndr_flags, $cprefix$var_prefix$e->{NAME}));\n";
	}

	if (util::need_wire_pointer($e)) {
		$res .= "\t}\n";
	}	
}

#####################################################################
# parse a struct
sub ParseStructPush($)
{
	my($struct) = shift;
	my($struct_len);

	if (! defined $struct->{ELEMENTS}) {
		return;
	}

	# see if we have a structure length
	foreach my $e (@{$struct->{ELEMENTS}}) {
		$e->{PARENT} = $struct;
		if (util::has_property($e, "struct_len")) {
			$struct_len = $e;
			$e->{VALUE} = "0";
		}
	}	

	if (defined $struct_len) {
		$res .= "\tstruct ndr_push_save _save1, _save2, _save3;\n";
		$res .= "\tndr_push_save(ndr, &_save1);\n";
	}

	my $align = struct_alignment($struct);
	$res .= "\tNDR_CHECK(ndr_push_align(ndr, $align));\n";

	$res .= "\tif (!(ndr_flags & NDR_SCALARS)) goto buffers;\n";

	foreach my $e (@{$struct->{ELEMENTS}}) {
		if (defined($struct_len) && $e == $struct_len) {
			$res .= "\tNDR_CHECK(ndr_push_align(ndr, sizeof($e->{TYPE})));\n";
			$res .= "\tndr_push_save(ndr, &_save2);\n";
		}
		ParseElementPushScalar($e, "r->", "NDR_SCALARS");
	}	

	$res .= "buffers:\n";
	$res .= "\tif (!(ndr_flags & NDR_BUFFERS)) goto done;\n";
	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPushBuffer($e, "r->");
	}

	if (defined $struct_len) {
		$res .= "\tndr_push_save(ndr, &_save3);\n";
		$res .= "\tndr_push_restore(ndr, &_save2);\n";
		$struct_len->{VALUE} = "_save3.offset - _save1.offset";
		ParseElementPushScalar($struct_len, "r->", "NDR_SCALARS");
		$res .= "\tndr_push_restore(ndr, &_save3);\n";
	}

	$res .= "done:\n";
}

#####################################################################
# generate a struct print function
sub ParseStructPrint($)
{
	my($struct) = shift;

	if (! defined $struct->{ELEMENTS}) {
		return;
	}

	$res .= "\tndr->depth++;\n";
	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPrintScalar($e, "r->");
	}
	$res .= "\tndr->depth--;\n";
}

#####################################################################
# parse a struct - pull side
sub ParseStructPull($)
{
	my($struct) = shift;
	my($struct_len);

	if (! defined $struct->{ELEMENTS}) {
		return;
	}

	# declare any internal pointers we need
	foreach my $e (@{$struct->{ELEMENTS}}) {
		$e->{PARENT} = $struct;
		if (util::need_wire_pointer($e)) {
			$res .= "\tuint32 _ptr_$e->{NAME};\n";
		}
	}


	# see if we have a structure length. If we do then we need to advance
	# the ndr_pull offset to that length past the front of the structure
	# when we have finished with the structure
	# we also need to make sure that we limit the size of our parsing
	# of this structure to the given size
	foreach my $e (@{$struct->{ELEMENTS}}) {
		if (util::has_property($e, "struct_len")) {
			$struct_len = $e;
			$e->{VALUE} = "&_size";
		}
	}	

	if (defined $struct_len) {
		$res .= "\tuint32 _size;\n";
		$res .= "\tstruct ndr_pull_save _save;\n";
		$res .= "\tndr_pull_save(ndr, &_save);\n";
	}

	my $align = struct_alignment($struct);
	$res .= "\tNDR_CHECK(ndr_pull_align(ndr, $align));\n";

	$res .= "\tif (!(ndr_flags & NDR_SCALARS)) goto buffers;\n";
	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPullScalar($e, "r->", "NDR_SCALARS");
		if (defined($struct_len) && $e == $struct_len) {
			$res .= "\tNDR_CHECK(ndr_pull_limit_size(ndr, _size, 4));\n";
		}
	}	

	$res .= "buffers:\n";
	$res .= "\tif (!(ndr_flags & NDR_BUFFERS)) goto done;\n";
	foreach my $e (@{$struct->{ELEMENTS}}) {
		ParseElementPullBuffer($e, "r->", "ndr_flags");
	}

	if (defined $struct_len) {
		$res .= "\tndr_pull_restore(ndr, &_save);\n";
		$res .= "\tNDR_CHECK(ndr_pull_advance(ndr, _size));\n";
	}

	$res .= "done:\n";
}


#####################################################################
# parse a union - push side
sub ParseUnionPush($)
{
	my $e = shift;
	print "WARNING! union push  not done\n";	
}

#####################################################################
# print a union
sub ParseUnionPrint($)
{
	my $e = shift;

	$res .= "\tswitch (level) {\n";
	foreach my $el (@{$e->{DATA}}) {
		$res .= "\tcase $el->{CASE}:\n";
		ParseElementPrintScalar($el->{DATA}, "r->");
		$res .= "\tbreak;\n\n";
	}
	$res .= "\tdefault:\n\t\tndr_print_bad_level(ndr, name, level);\n";
	$res .= "\t}\n";
}

#####################################################################
# parse a union - pull side
sub ParseUnionPull($)
{
	my $e = shift;

	$res .= "\tNDR_CHECK(ndr_pull_uint16(ndr, level));\n";
	$res .= "\tif (!(ndr_flags & NDR_SCALARS)) goto buffers;\n";
	$res .= "\tswitch (*level) {\n";
	foreach my $el (@{$e->{DATA}}) {
		$res .= "\tcase $el->{CASE}:\n";
		ParseElementPullScalar($el->{DATA}, "r->", "NDR_SCALARS");		
		$res .= "\tbreak;\n\n";
	}
	$res .= "\tdefault:\n\t\treturn NT_STATUS_INVALID_LEVEL;\n";
	$res .= "\t}\n";
	$res .= "buffers:\n";
	$res .= "\tif (!(ndr_flags & NDR_BUFFERS)) goto done;\n";
	$res .= "\tswitch (*level) {\n";
	foreach my $el (@{$e->{DATA}}) {
		$res .= "\tcase $el->{CASE}:\n";
		ParseElementPullBuffer($el->{DATA}, "r->", "NDR_BUFFERS");
		$res .= "\tbreak;\n\n";
	}
	$res .= "\tdefault:\n\t\treturn NT_STATUS_INVALID_LEVEL;\n";
	$res .= "\t}\n";
	$res .= "done:\n";
}

#####################################################################
# parse a type
sub ParseTypePush($)
{
	my($data) = shift;

	if (ref($data) eq "HASH") {
		($data->{TYPE} eq "STRUCT") &&
		    ParseStructPush($data);
		($data->{TYPE} eq "UNION") &&
		    ParseUnionPush($data);
	}
}

#####################################################################
# generate a print function for a type
sub ParseTypePrint($)
{
	my($data) = shift;

	if (ref($data) eq "HASH") {
		($data->{TYPE} eq "STRUCT") &&
		    ParseStructPrint($data);
		($data->{TYPE} eq "UNION") &&
		    ParseUnionPrint($data);
	}
}

#####################################################################
# parse a type
sub ParseTypePull($)
{
	my($data) = shift;

	if (ref($data) eq "HASH") {
		($data->{TYPE} eq "STRUCT") &&
		    ParseStructPull($data);
		($data->{TYPE} eq "UNION") &&
		    ParseUnionPull($data);
	}
}

#####################################################################
# parse a typedef - push side
sub ParseTypedefPush($)
{
	my($e) = shift;

	if (! $needed{"push_$e->{NAME}"}) {
#		print "push_$e->{NAME} not needed\n";
		return;
	}

	if ($e->{DATA}->{TYPE} eq "STRUCT") {
		$res .= "static NTSTATUS ndr_push_$e->{NAME}(struct ndr_push *ndr, int ndr_flags, struct $e->{NAME} *r)";
		$res .= "\n{\n";
		ParseTypePush($e->{DATA});
		$res .= "\treturn NT_STATUS_OK;\n";
		$res .= "}\n\n";
	}

	if ($e->{DATA}->{TYPE} eq "UNION") {
		$res .= "static NTSTATUS ndr_push_$e->{NAME}(struct ndr_push *ndr, int ndr_flags, uint16 level, union $e->{NAME} *r)";
		$res .= "\n{\n";
		ParseTypePush($e->{DATA});
		$res .= "\treturn NT_STATUS_OK;\n";
		$res .= "}\n\n";
	}
}


#####################################################################
# parse a typedef - pull side
sub ParseTypedefPull($)
{
	my($e) = shift;

	if (! $needed{"pull_$e->{NAME}"}) {
#		print "pull_$e->{NAME} not needed\n";
		return;
	}

	if ($e->{DATA}->{TYPE} eq "STRUCT") {
		$res .= "static NTSTATUS ndr_pull_$e->{NAME}(struct ndr_pull *ndr, int ndr_flags, struct $e->{NAME} *r)";
		$res .= "\n{\n";
		ParseTypePull($e->{DATA});
		$res .= "\treturn NT_STATUS_OK;\n";
		$res .= "}\n\n";
	}

	if ($e->{DATA}->{TYPE} eq "UNION") {
		$res .= "static NTSTATUS ndr_pull_$e->{NAME}(struct ndr_pull *ndr, int ndr_flags, uint16 *level, union $e->{NAME} *r)";
		$res .= "\n{\n";
		ParseTypePull($e->{DATA});
		$res .= "\treturn NT_STATUS_OK;\n";
		$res .= "}\n\n";
	}
}


#####################################################################
# parse a typedef - push side
sub ParseTypedefPrint($)
{
	my($e) = shift;

	if ($e->{DATA}->{TYPE} eq "STRUCT") {
		$res .= "void ndr_print_$e->{NAME}(struct ndr_print *ndr, const char *name, struct $e->{NAME} *r)";
		$res .= "\n{\n";
		$res .= "\tndr_print_struct(ndr, name, \"$e->{NAME}\");\n";
		ParseTypePrint($e->{DATA});
		$res .= "}\n\n";
	}

	if ($e->{DATA}->{TYPE} eq "UNION") {
		$res .= "void ndr_print_$e->{NAME}(struct ndr_print *ndr, const char *name, uint16 level, union $e->{NAME} *r)";
		$res .= "\n{\n";
		$res .= "\tndr_print_union(ndr, name, level, \"$e->{NAME}\");\n";
		ParseTypePrint($e->{DATA});
		$res .= "}\n\n";
	}
}


#####################################################################
# parse a function
sub ParseFunctionPush($)
{ 
	my($function) = shift;

	# Input function
	$res .= "NTSTATUS ndr_push_$function->{NAME}(struct ndr_push *ndr, struct $function->{NAME} *r)\n{\n";

	foreach my $e (@{$function->{DATA}}) {
		if (util::has_property($e, "in")) {
			$e->{PARENT} = $function;
			if (util::array_size($e)) {
				$res .= "\tif (r->in.$e->{NAME}) {\n";
				if (!util::is_scalar_type($e->{TYPE})) {
					$res .= "\t\tint ndr_flags = NDR_SCALARS|NDR_BUFFERS;\n";
				}
				ParseArrayPush($e, "r->in.");
				$res .= "\t}\n";
			} else {
				ParseElementPushScalar($e, "r->in.", "NDR_SCALARS|NDR_BUFFERS");
				ParseElementPushBuffer($e, "r->in.");
			}
		}
	}
    
	$res .= "\n\treturn NT_STATUS_OK;\n}\n\n";
}

#####################################################################
# parse a function
sub ParseFunctionPull($)
{ 
	my($fn) = shift;

	# pull function args
	$res .= "NTSTATUS ndr_pull_$fn->{NAME}(struct ndr_pull *ndr, struct $fn->{NAME} *r)\n{\n";

	# declare any internal pointers we need
	foreach my $e (@{$fn->{DATA}}) {
		if (util::has_property($e, "out")) {
			if (util::need_wire_pointer($e)) {
				$res .= "\tuint32 _ptr_$e->{NAME};\n";
			}
		}
	}

	foreach my $e (@{$fn->{DATA}}) {
		if (util::has_property($e, "out")) {
			$e->{PARENT} = $fn;
			if (util::array_size($e)) {
				$res .= "\tif (r->out.$e->{NAME}) {\n";
				if (!util::is_scalar_type($e->{TYPE})) {
					$res .= "\t\tint ndr_flags = NDR_SCALARS|NDR_BUFFERS;\n";
				}
				ParseArrayPull($e, "r->out.");
				$res .= "\t}\n";
			} else {
				ParseElementPullScalar($e, "r->out.", "NDR_SCALARS|NDR_BUFFERS");
				ParseElementPullBuffer($e, "r->out.", "NDR_SCALARS|NDR_BUFFERS");
			}
		}
	}

	if ($fn->{RETURN_TYPE} && $fn->{RETURN_TYPE} ne "void") {
		$res .= "\tNDR_CHECK(ndr_pull_$fn->{RETURN_TYPE}(ndr, &r->out.result));\n";
	}

    
	$res .= "\n\treturn NT_STATUS_OK;\n}\n\n";
}

#####################################################################
# parse a typedef
sub ParseTypedef($)
{
	my($e) = shift;
	ParseTypedefPush($e);
	ParseTypedefPull($e);
	ParseTypedefPrint($e);
}

#####################################################################
# parse a function
sub ParseFunction($)
{
	my $i = shift;
	ParseFunctionPush($i);
	ParseFunctionPull($i);
}

#####################################################################
# parse the interface definitions
sub ParseInterface($)
{
	my($interface) = shift;
	my($data) = $interface->{DATA};
	foreach my $d (@{$data}) {
		($d->{TYPE} eq "TYPEDEF") &&
		    ParseTypedef($d);
		($d->{TYPE} eq "FUNCTION") && 
		    ParseFunction($d);
	}
}

sub NeededFunction($)
{
	my $fn = shift;
	$needed{"pull_$fn->{NAME}"} = 1;
	$needed{"push_$fn->{NAME}"} = 1;
	foreach my $e (@{$fn->{DATA}}) {
		if (util::has_property($e, "out")) {
			$needed{"pull_$e->{TYPE}"} = 1;
		}
		if (util::has_property($e, "in")) {
			$needed{"push_$e->{TYPE}"} = 1;
		}
	}
}

sub NeededTypedef($)
{
	my $t = shift;
	if ($t->{DATA}->{TYPE} eq "STRUCT") {
		for my $e (@{$t->{DATA}->{ELEMENTS}}) {
				if ($needed{"pull_$t->{NAME}"}) {
					$needed{"pull_$e->{TYPE}"} = 1;
				}
				if ($needed{"push_$t->{NAME}"}) {
					$needed{"push_$e->{TYPE}"} = 1;
				}
			}
		}
	if ($t->{DATA}->{TYPE} eq "UNION") {
		for my $e (@{$t->{DATA}->{DATA}}) {
			if ($needed{"pull_$t->{NAME}"}) {
				$needed{"pull_$e->{DATA}->{TYPE}"} = 1;
			}
			if ($needed{"push_$t->{NAME}"}) {
				$needed{"push_$e->{DATA}->{TYPE}"} = 1;
			}
		}
	}
}

#####################################################################
# work out what parse functions are needed
sub BuildNeeded($)
{
	my($interface) = shift;
	my($data) = $interface->{DATA};
	foreach my $d (@{$data}) {
		($d->{TYPE} eq "FUNCTION") && 
		    NeededFunction($d);
	}
	foreach my $d (reverse @{$data}) {
		($d->{TYPE} eq "TYPEDEF") &&
		    NeededTypedef($d);
	}
}


#####################################################################
# parse a parsed IDL structure back into an IDL file
sub Parse($)
{
	my($idl) = shift;
	$res = "/* parser auto-generated by pidl */\n\n";
	$res .= "#include \"includes.h\"\n\n";
	foreach my $x (@{$idl}) {
		if ($x->{TYPE} eq "INTERFACE") { 
			BuildNeeded($x);
			ParseInterface($x);
		}
	}
	return $res;
}

1;
