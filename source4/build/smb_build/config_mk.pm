###########################################################
### SMB Build System					###
### - config.mk parsing functions			###
###							###
###  Copyright (C) Stefan (metze) Metzmacher 2004	###
###  Released under the GNU GPL				###
###########################################################

package config_mk;
use input;

use strict;

###########################################################
# The parsing function which parses the file
#
# $result = _parse_config_mk($filename)
#
# $filename -	the path of the config.mk file
#		which should be parsed
#
# $result -	the resulting structure
#
# $result->{ERROR_CODE} -	the error_code, '0' means success
# $result->{ERROR_STR} -	the error string
#
# $result->{$key}{KEY} -	the key == the variable which was parsed
# $result->{$key}{VAL} -	the value of the variable
sub _parse_config_mk($)
{
	my $filename = shift;
	my $result;
	my $linenum = -1;
	my $waiting = 0;
	my $section = "GLOBAL";
	my $key;

	$result->{ERROR_CODE} = -1;

	open(CONFIG_MK, "< $filename") || die ("Can't open $filename\n");

	while (<CONFIG_MK>) {
		my $line = $_;
		my $val;

		$linenum++;

		#
		# lines beginnig with '#' are ignored
		# 
		if ($line =~ /^\#.*$/) {
			next;
		}

		#
		#
		#
		if (($waiting == 0) && ($line =~ /^\[([a-zA-Z0-9_:]+)\][\t ]*$/)) {
			$section = $1;
			next;
		}
		
		#
		# 1.)	lines with an alphanumeric character indicate
		# 	a new variable, 
		# 2.)	followed by zero or more whitespaces or tabs
		# 3.)	then one '=' character
		# 4.)	followed by the value of the variable
		# 5.)	a newline ('\n') can be escaped by a '\' before the newline
		#	and the next line needs to start with a tab ('\t')
		#
		if (($waiting == 0) && ($line =~ /^([a-zA-Z0-9_]+)[\t ]*=(.*)$/)) {
			$key = $1;
			$val = $2;

			#
			# when we have a '\' before the newline 
			# then skip it and wait for the next line.
			#
			if ($val =~ /(.*)(\\)$/) {
				$val = $1;
				$waiting = 1;		
			} else {
				$waiting = 0;
			}

			$result->{$section}{$key}{KEY} = $key;
			$result->{$section}{$key}{VAL} = $val;
			next;
		}

		#
		# when we are waiting for a value to continue then
		# check if it has a leading tab.
		#
		if (($waiting == 1) && ($line =~ /^\t(.*)$/)) {
			$val = $1;

			#
			# when we have a '\' before the newline 
			# then skip it and wait for the next line.
			#
			if ($val =~ /(.*)( \\)$/) {
				$val = $1;
				$waiting = 1;		
			} else {
				$waiting = 0;
			}

			$result->{$section}{$key}{VAL} .= " ";
			$result->{$section}{$key}{VAL} .= $val;
			next;
		}

		#
		# catch empty lines they're ignored
		# and we're no longer waiting for the value to continue
		#
		if ($line =~ /^$/) {
			$waiting = 0;
			next;
		}

		close(CONFIG_MK);

		$result->{ERROR_STR} = "Bad line while parsing $filename\n$filename:$linenum: $line";

		return $result;
	}

	close(CONFIG_MK);

	$result->{ERROR_CODE} = 0;

	return $result;
}

###########################################################
# A caching function to avoid to parse
# a file twice or more
#
# $result = _get_parse_results($filename)
#
# $filename -	the path of the config.mk file
#		which should be parsed
#
# $result -	the resulting structure
#
# $result->{ERROR_CODE} -	the error_code, '0' means success
# $result->{ERROR_STR} -	the error string
#
# $result->{$key}{KEY} -	the key == the variable which was parsed
# $result->{$key}{VAL} -	the value of the variable
my $_get_parse_results_cache;
sub _get_parse_results($)
{
	my $filename = shift;

	if ((!defined($_get_parse_results_cache->{$filename}{ERROR_CODE}))
		||($_get_parse_results_cache->{$filename}{ERROR_CODE} != 0)) {
		$_get_parse_results_cache->{$filename} = _parse_config_mk($filename);
	}

	return $_get_parse_results_cache->{$filename};
}

###########################################################
# The fetching function to fetch the value of a variable 
# out of the file
#
# $value = _fetch_var_from_config_mk($filename,$section,$variable)
#
# $filename -	the path of the config.mk file
#		which should be parsed
#
# $section  -	the section name of the variable
#
# $variable -	the variable name of which we want the value
#
# $value -	the value of the variable
sub _fetch_var_from_config_mk($$$)
{
	my $filename = shift;
	my $section = shift;
	my $key = shift;
	my $val = "";
	my $result;

	$result = _get_parse_results($filename);

	if ($result->{ERROR_CODE} != 0) {
		die ($result->{ERROR_STR});
	}

	if (defined($result->{$section}{$key})) {
		$val = input::strtrim($result->{$section}{$key}{VAL});
	} elsif (defined($result->{DEFAULT}{$key})) {
		$val = input::strtrim($result->{DEFAULT}{$key}{VAL});
	}

	return $val;
}

###########################################################
# The fetching function to fetch the array of values of a variable 
# out of the file
#
# $array = _fetch_array_from_config_mk($filename,$section,$variable)
#
# $filename -	the path of the config.mk file
#		which should be parsed
#
# $section  -	the section name of the variable
#
# $variable -	the variable name of which we want the value
#
# $array -	the array of values of the variable
sub _fetch_array_from_config_mk($$$)
{
	my $filename = shift;
	my $section = shift;
	my $key = shift;
	my $result;
	my $val = "";

	$result = _get_parse_results($filename);

	if ($result->{ERROR_CODE} != 0) {
		die ($result->{ERROR_STR});
	}

	if (defined($result->{$section}{$key})) {
		$val = $result->{$section}{$key}{VAL};
	} elsif (defined($result->{DEFAULT}{$key})) {
		$val = $result->{DEFAULT}{$key}{VAL};
	} 

	return input::str2array($val);
}

sub import_file($$)
{
	my $input = shift;
	my $filename = shift;

	my $result = _parse_config_mk($filename);

	die ($result->{ERROR_STR}) unless $result->{ERROR_CODE} == 0;

	foreach my $section (keys %{$result}) {
		next if ($section eq "ERROR_CODE");
		my ($type, $name) = split(/::/, $section, 2);
		
		$input->{$name}{NAME} = $name;
		$input->{$name}{TYPE} = $type;

		foreach my $key (values %{$result->{$section}}) {
			$input->{$name}{$key->{KEY}} = [input::str2array($key->{VAL})];
		}
	}
}
1;
