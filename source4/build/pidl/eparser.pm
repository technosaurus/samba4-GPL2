###################################################
# parser generator for IDL structures
# Copyright tpot@samba.org 2001
# Copyright tridge@samba.org 2000
# released under the GNU GPL

package IdlEParser;

use strict;
use dump;
use Data::Dumper;

sub ParamSimpleNdrType($)
{
    my($p) = shift;
    my($res);

    $res .= "\toffset = dissect_ndr_$p->{TYPE}(tvb, offset, pinfo, tree, drep, hf_$p->{NAME}, NULL);\n";

    return $res;
}

sub ParamPolicyHandle($)
{
    my($p) = shift;
    my($res);

    $res .= "\toffset = dissect_nt_policy_handle(tvb, offset, pinfo, tree, drep, hf_policy_hnd, NULL, NULL, FALSE, FALSE);\n";

    return $res;
}

my %param_handlers = (
		      'uint16' => \&ParamSimpleNdrType,
		      'uint32' => \&ParamSimpleNdrType,
		      'policy_handle' => \&ParamPolicyHandle,
		      );

#####################################################################
# parse a function
sub ParseParameter($)
{ 
    my($p) = shift;
    my($res);

    if (defined($param_handlers{$p->{TYPE}})) {
	$res .= &{$param_handlers{$p->{TYPE}}}($p);
	return $res;
    }

    $res .= "\t/* Unhandled IDL type '$p->{TYPE}' in $p->{PARENT}->{NAME} */\n";

    return $res;
    # exit(1);
}

#####################################################################
# parse a function
sub ParseFunction($)
{ 
    my($f) = shift;
    my($res);

    $res .= "/*\n\n";
    $res .= IdlDump::DumpFunction($f);
    $res .= "*/\n\n";

    # Request function

    $res .= "static int\n";
    $res .= "$f->{NAME}_rqst(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree, guint8 *drep)\n";
    $res .= "{\n";

    my($d);
    foreach $d (@{$f->{DATA}}) {
	$res .= ParseParameter($d), if defined($d->{PROPERTIES}{in});
    }

    $res .= "\treturn offset;\n";
    $res .= "}\n\n";

    # Response function

    $res .= "static int\n";
    $res .= "$f->{NAME}_resp(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree, guint8 *drep)\n";
    $res .= "{\n";

    foreach $d (@{$f->{DATA}}) {
	$res .= ParseParameter($d), if defined($d->{PROPERTIES}{out});
    }


    $res .= "\treturn offset;\n";
    $res .= "}\n\n";

    return $res;
}

#####################################################################
# parse the interface definitions
sub ParseInterface($)
{
    my($interface) = shift;
    my($data) = $interface->{DATA};
    my($res) = "";

    foreach my $d (@{$data}) {
	$res .= ParseFunction($d), if $d->{TYPE} eq "FUNCTION";
    }

    return $res;
}


#####################################################################
# parse a parsed IDL structure back into an IDL file
sub Parse($)
{
    my($idl) = shift;
    my($res);

    $res = "/* parser auto-generated by pidl */\n\n";
    foreach my $d (@{$idl}) {
	$res .= ParseInterface($d), if $d->{TYPE} eq "INTERFACE";
    }

    return $res;
}

1;
