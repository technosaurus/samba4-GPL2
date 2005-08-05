###################################################
# parse an ethereal conformance file
# Copyright jelmer@samba.org 2005
# released under the GNU GPL

package Parse::Pidl::Ethereal::Conformance;

require Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw(EmitProhibited FindDissectorParam %hf_renames);

use strict;

use Parse::Pidl::Util qw(has_property);

sub handle_union_tag_size($$)
{
	#FIXME	
}

sub handle_type($$$$$$$)
{
	my ($name,$dissectorname,$ft_type,$base_type,$mask,$valsstring,$alignment) = @_;
	#FIXME
}

use vars qw(%hf_renames);

sub handle_hf_rename($$)
{
	my ($old,$new) = @_;
	$hf_renames{$old} = $new;
}

my %dissectorparams = ();

sub handle_param_value($$)
{
	my ($dissector_name,$value) = @_;

	$dissectorparams{$dissector_name} = $value;

}

sub handle_hf_field($$$$$$$$)
{
	my ($hf,$title,$filter,$ft_type,$base_type,$valsstring,$mask,$blub) = @_;

}

sub handle_strip_prefix($)
{
	#FIXME
}

my @noemit = ();

sub handle_noemit($)
{
	my $type = shift;

	push (@noemit, $type);
}

my %field_handlers = (
	UNION_TAG_SIZE => \&handle_union_tag_size,
	TYPE => \&handle_type,
	NOEMIT => \&handle_noemit, 
	PARAM_VALUE => \&handle_param_value, 
	HF_FIELD => \&handle_hf_field, 
	HF_RENAME => \&handle_hf_rename, 
	STRIP_PREFIX => \&handle_strip_prefix
);

sub Parse($)
{
	my $f = shift;

	open(IN,$f) or return undef;

	foreach (<IN>) {
		next if (/^#.*$/);
		next if (/^$/);

		my @fields = split(/ /);
		
		$field_handlers{$fields[0]}(@fields);
	}

	close(IN);
}

sub EmitProhibited($)
{
	my $type = shift;

	return 1 if (grep(/$type/,@noemit));

	return 0;
}

sub FindDissectorParam($)
{
	my $type = shift;

	return $dissectorparams{$type} if defined ($dissectorparams{$type});

	return 0;
}

1;
