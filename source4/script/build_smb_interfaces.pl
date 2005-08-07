#!/usr/bin/perl
#
# Create ejs interfaces for structures in a C header file
#

use File::Basename;
use Data::Dumper;

#
# Generate parse tree for header file
#

my $file = shift;
require smb_interfaces;
my $parser = new smb_interfaces;
$header = $parser->parse($file);

#
# Make second pass over tree to make it easier to process.
#

sub flatten_structs($) {
  my $obj = shift;
  my $s = { %$obj };

  # Map NAME, STRUCT_NAME and UNION_NAME elements into a more likeable
  # property.

  if (defined($obj->{STRUCT_NAME}) or defined($obj->{UNION_NAME})) {

    $s->{TYPE_DEFINED} = defined($obj->{STRUCT_NAME}) ? $obj->{STRUCT_NAME} 
      : $obj->{UNION_NAME};

    delete $s->{STRUCT_NAME};
    delete $s->{UNION_NAME};
  }

  # Create a new list of structure fields with flattened names

  foreach my $elt (@{$obj->{DATA}}) {
    foreach my $name (@{$elt->{NAME}}) {
      my $new_elt = { %$elt };
      $new_elt->{NAME} = $name;
      $new_elt->{PARENT} = $s;
      push(@{$s->{FIELDS}}, flatten_structs($new_elt));
    }
  }

  delete $s->{DATA};

  return $s;
}

@newheader = map { flatten_structs($_) } @{$header};

#
# Generate implementation
#

open(FILE, ">libcli/gen_raw/ejs_${basename}.c");

print FILE "/* EJS wrapper functions auto-generated by build_smb_interfaces.pl */\n\n";

print FILE "#include \"includes.h\"\n";
print FILE "#include \"lib/appweb/ejs/ejs.h\"\n";
print FILE "#include \"scripting/ejs/ejsrpc.h\"\n"; # TODO: remove this
print FILE "\n";

# Top level call functions

foreach my $s (@newheader) {

  if ($s->{TYPE} eq "struct") {

    # Top level struct

    print FILE "static NTSTATUS ejs_$s->{TYPE_DEFINED}(int eid, int argc, struct MprVar **argv)\n";
    print FILE "{\n";
    print FILE "\treturn NT_STATUS_OK;\n";
    print FILE "}\n\n";

  } else {

    # Top level union

    foreach my $arm (@{$s->{FIELDS}}) {

      print FILE "static NTSTATUS ejs_$s->{TYPE_DEFINED}_$arm->{NAME}(int eid, int argc, struct MprVar **argv)\n";
      print FILE "{\n";
      print FILE "\treturn NT_STATUS_OK;\n";
      print FILE "}\n\n";
    }
  }
}

# Module initialisation

print FILE "static int ejs_${basename}_init(int eid, int argc, struct MprVar **argv)\n";
print FILE "{\n";
print FILE "\tstruct MprVar *obj = mprInitObject(eid, \"${basename}\", argc, argv);\n\n";

foreach my $s (@newheader) {
  if ($s->{TYPE} eq "struct") {
    print FILE "\tmprSetCFunction(obj, \"$s->{TYPE_DEFINED}\", ejs_$s->{TYPE_DEFINED});\n";
  } else {
    foreach my $arm (@{$s->{FIELDS}}) {
      print FILE "\tmprSetCFunction(obj, \"$s->{TYPE_DEFINED}_$arm->{NAME}\", ejs_$s->{TYPE_DEFINED});\n";
    }
  }
}

print FILE "}\n\n";

print FILE "NTSTATUS ejs_init_${basename}(void)\n";
print FILE "{\n";
print FILE "\treturn smbcalls_register_ejs(\"${basename}_init\", ejs_${basename}_init);\n";
print FILE "}\n";

close(FILE);

exit;

foreach my $x (@{$header}) {
  next, if $x->{STRUCT_NAME} eq "";

  $raw_name = $x->{STRUCT_NAME};
  $raw_name =~ s/smb_/smb_raw_/;

  print FILE "static int ejs_$x->{STRUCT_NAME}(int eid, int argc, struct MprVar **argv)\n";
  print FILE "{\n";
  print FILE "\tstruct $x->{STRUCT_NAME} params;\n";
  print FILE "\tstruct smbcli_tree *tree;\n";
  print FILE "\tNTSTATUS result;\n\n";

  $output = << "__HERE__";
	if (argc != 1 || argv[0]->type != MPR_TYPE_OBJECT) {
		ejsSetErrorMsg(eid, "invalid arguments");
		return -1;
	}

	tree = mprGetThisPtr(eid, "tree");

	if (!tree) {
		ejsSetErrorMsg(eid, "invalid tree");
		return -1;
	}

__HERE__

  print FILE $output;
  print FILE "\tresult = $raw_name(tree, &params);\n\n";

  print FILE "\tmpr_Return(eid, mprNTSTATUS(status));\n";
  print FILE "\tif (NT_STATUS_EQUAL(status, NT_STATUS_INTERNAL_ERROR)) {\n";
  print FILE "\t\treturn -1;\n";
  print FILE "\t}\n\n";
  print FILE "\treturn 0;\n";

  print FILE "}\n\n";
}

