#!/usr/bin/perl
# Bootstrap Samba and run a number of tests against it.
# Copyright (C) 2005-2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later.

package SocketWrapper;

use Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(setup_dir setup_pcap set_default_iface);

use strict;
use FindBin qw($RealBin);

sub setup_dir($)
{
	my ($dir) = @_;
	$ENV{SOCKET_WRAPPER_DIR} = $dir;
	return $dir;
}

sub setup_pcap($)
{
	my ($pcap_file) = @_;

}

sub set_default_iface($)
{
	my ($i) = @_;
	$ENV{SOCKET_WRAPPER_DEFAULT_IFACE} = $i;
}

1;
