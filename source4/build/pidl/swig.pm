###################################################
# Samba4 parser generator for swig wrappers
# Copyright tpot@samba.org 2004
# released under the GNU GPL

package IdlSwig;

use strict;
use Data::Dumper;

my($res);
my($name);

sub ParseFunction($)
{
    my($fn) = shift;

#    print Dumper($fn);

    # Input typemap

    $res .= "%typemap(in) struct $fn->{NAME} * (struct $fn->{NAME} temp) {\n";
    $res .= "\tif (!PyDict_Check(\$input)) {\n";
    $res .= "\t\tPyErr_SetString(PyExc_TypeError, \"dict arg expected\");\n";
    $res .= "\t\treturn NULL;\n";
    $res .= "\t}\n\n";
    $res .= "\tmemset(&temp, 0, sizeof(temp));\n\n";
    $res .= "\t/* store input params in dict */\n\n";
    $res .= "\t\$1 = &temp;\n";
    $res .= "}\n\n";

    # Output typemap

    $res .= "%typemap(argout) struct $fn->{NAME} * {\n";
    $res .= "\tlong status = PyLong_AsLong(resultobj);\n";
    $res .= "\tPyObject *dict;\n";
    $res .= "\n";
    $res .= "\tif (status != 0) {\n";
    $res .= "\t\tset_ntstatus_exception(status);\n";
    $res .= "\t\treturn NULL;\n";
    $res .= "\t}\n";
    $res .= "\n";
    $res .= "\tdict = PyDict_New();\n\n";
    $res .= "\t/* store output params in dict */\n\n";
    $res .= "\tresultobj = dict;\n";
    $res .= "}\n\n";

    # Function definitions

    $res .= "%rename($fn->{NAME}) dcerpc_$fn->{NAME};\n";
    $res .= "$fn->{RETURN_TYPE} dcerpc_$fn->{NAME}(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, struct $fn->{NAME} *r);\n\n";
}

sub ParseInheritedData($)
{
    my($data) = shift;

    foreach my $e (@{$data}) {
	($e->{TYPE} eq "FUNCTION") && ParseFunction($e);
    }
}

sub ParseHeader($)
{
    my($hdr) = shift;

    $name = $hdr->{NAME};
    $res .= "#define DCERPC_" . uc($name) . "_UUID \"$hdr->{PROPERTIES}->{uuid}\"\n";
    $res .= "const int DCERPC_" . uc($name) . "_VERSION = " . $hdr->{PROPERTIES}->{version} . ";\n";
    $res .= "#define DCERPC_" . uc($name) . "_NAME \"" . $name . "\"\n";
    $res .= "\n";

    ParseInheritedData($hdr->{INHERITED_DATA});    
}

sub Parse($)
{
    my($idl) = shift;
    
    $res = "/* auto-generated by pidl */\n\n";

    foreach my $x (@{$idl}) {
	($x->{TYPE} eq "INTERFACE") && ParseHeader($x);
    }

    return $res;
}

1;
