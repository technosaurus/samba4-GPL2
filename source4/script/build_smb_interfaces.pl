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
#      $new_elt->{PARENT} = $s;
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

my $basename = basename($file, ".h");
stat "libcli/gen_raw" || mkdir("libcli/gen_raw") || die("mkdir");

open(FILE, ">libcli/gen_raw/ejs_${basename}.c");

print FILE "/* EJS wrapper functions auto-generated by build_smb_interfaces.pl */\n\n";

print FILE "#include \"includes.h\"\n";
print FILE "#include \"lib/appweb/ejs/ejs.h\"\n";
print FILE "#include \"scripting/ejs/ejsrpc.h\"\n"; # TODO: remove this
print FILE "\n";

sub transfer_element($$$) {
  my $dir = shift;
  my $prefix = shift;
  my $elt = shift;

  print FILE "\tejs_${dir}_$elt->{TYPE}(ejs, v, \"$prefix.$elt->{NAME}\")\n";
}

sub transfer_struct($$) {
  my $dir = shift;
  my $struct = shift;

  foreach my $field (@{$struct->{FIELDS}}) {
    next if $dir eq "pull" and $field->{NAME} eq "out";
    next if $dir eq "push" and $field->{NAME} eq "in";

    if ($field->{TYPE} eq "struct") {
      foreach $subfield (@{$field->{FIELDS}}) {
	transfer_element($dir, $field->{NAME}, $subfield);
      }
    } else {
      transfer_element($dir, $struct->{NAME}, $field);
    }
  }
}

# Top level call functions

foreach my $s (@newheader) {

  if ($s->{TYPE} eq "struct") {

    # Top level struct

    print FILE "static int ejs_$s->{TYPE_DEFINED}(int eid, int argc, struct MprVar **argv)\n";
    print FILE "{\n";
    print FILE "\tstruct $s->{TYPE_DEFINED} params;\n";
    print FILE "\tstruct smbcli_tree *tree;\n";
    print FILE "\tNTSTATUS result;\n\n";

    print FILE "\tif (argc != 1 || argv[0]->type != MPR_TYPE_OBJECT) {\n";
    print FILE "\t\tejsSetErrorMsg(eid, \"invalid arguments\");\n";
    print FILE "\t\treturn -1;\n";
    print FILE "\t}\n\n";

    transfer_struct("pull", $s);

    my $fn = $s->{TYPE_DEFINED};
    $fn =~ s/^smb_/smb_raw_/;

    print FILE "\n\tresult = $fn(tree, &params);\n\n";

    transfer_struct("push", $s);

    print FILE "\n\tmpr_Return(eid, mprNTSTATUS(result));\n\n";
    print FILE "\tif (NT_STATUS_EQUAL(status, NT_STATUS_INTERNAL_ERROR)) {\n";
    print FILE "\t\treturn -1;\n";
    print FILE "\t}\n\n";
    print FILE "\treturn 0;\n";

    print FILE "}\n\n";

  } else {

    # Top level union

    foreach my $arm (@{$s->{FIELDS}}) {

      print FILE "static int ejs_$s->{TYPE_DEFINED}_$arm->{NAME}(int eid, int argc, struct MprVar **argv)\n";
      print FILE "{\n";
      print FILE "\tunion $s->{TYPE_DEFINED} params;\n";
      print FILE "\tstruct smbcli_tree *tree;\n";
      print FILE "\tNTSTATUS result;\n\n";

      print FILE "\tif (argc != 1 || argv[0]->type != MPR_TYPE_OBJECT) {\n";
      print FILE "\t\tejsSetErrorMsg(eid, \"invalid arguments\");\n";
      print FILE "\t\treturn -1;\n";
      print FILE "\t}\n\n";

      transfer_struct("pull", $arm);

      my $fn = $s->{TYPE_DEFINED};
      $fn =~ s/^smb_/smb_raw_/;

      print FILE "\n\tresult = $fn(tree, &params);\n\n";

      transfer_struct("push", $arm);

      print FILE "\n\tmpr_Return(eid, mprNTSTATUS(result));\n\n";
      print FILE "\tif (NT_STATUS_EQUAL(status, NT_STATUS_INTERNAL_ERROR)) {\n";
      print FILE "\t\treturn -1;\n";
      print FILE "\t}\n\n";
      print FILE "\treturn 0;\n";

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
