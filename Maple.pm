#!/usr/bin/perl -w
# vi:set ts=4:

package Maple;
use strict;

our (@ISA,@EXPORT);

BEGIN {
	use Exporter;
	use Socket;

	@ISA    = qw(Exporter);
	@EXPORT = qw(MAPLE_CMD_DEVINFO, MAPLE_CMD_ALLINFO, MAPLE_CMD_RESET, 
	             MAPLE_CMD_SHUTDOWN, MAPLE_RESP_DEVINFO, MAPLE_RESP_DEVINFOEX,
	             MAPLE_RESP_ACK,MAPLE_RESP_DATAXFER, MAPLE_CMD_GETCOND,
	             MAPLE_CMD_GETMEMINFO, MAPLE_CMD_READBLOCK, 
	             MAPLE_CMD_WRITEBLOCK, MAPLE_CMD_SYNCBLOCK, MAPLE_CMD_SETCOND,
	             MAPLE_CMD_MICROPHONE, MAPLE_CMD_CAMERA, MAPLE_RESP_ERR,
	             MAPLE_RESP_BADFUNC, MAPLE_RESP_BADCMD, MAPLE_RESP_AGAIN,
	             MAPLE_RESP_FILEERR, MAPLE_FUNC_CONTROLLER, MAPLE_FUNC_MEMORY,
	             MAPLE_FUNC_LCD, MAPLE_FUNC_CLOCK, MAPLE_FUNC_MICROPHONE,
	             MAPLE_FUNC_AR_GUN, MAPLE_FUNC_KEYBOARD, MAPLE_FUNC_LIGHT_GUN,
	             MAPLE_FUNC_PURU_PURU, MAPLE_FUNC_MOUSE, MAPLE_FUNC_CAMERA,
	             send_maple_packet);
}

=head1 NAME

Maple - (Very) Simple Dreamcast Maple-over-UDP Client

=head1 DESCRIPTION

C<Maple> provides a function to send Maple packets (and read the response) 
from a Dreamcast running a modified version of dcload-ip, available from
L<http://github.com/thentenaar/dc-load-ip>.

A summary of the maple protocol is available at: 
L<http://mc.pp.se/dc/maplebus.html>

=head1 SYNOPSIS

	use Maple;

	my %cmd  = (
		'cmd'    => MAPLE_CMD_RESET,
		'port'   => 0,
		'slot'   => 1,
		'last'   => 0,
		'length' => 0
	);

	my $resp = send_maple_packet('127.0.0.1',\%cmd);

This example sends a B<Reset> command to the device on port 0, slot 1 of the
Dreamcast with the IP B<127.0.0.1>.

	use Maple;

	my %cmd  = (
		'cmd'    => MAPLE_CMD_GETMEMINFO,
		'port'   => 0,
		'slot'   => 1,
		'last'   => 0,
		'length' => 2,
		'data'   => [ MAPLE_FUNC_MEMORY, 0 ]
	);

	my $resp = send_maple_packet('127.0.0.1',\%cmd);

This example retrieves the memory info for partition 0 of the block
device at port 0, slot 1. The C<data> parameter should be specified
as an array of 32-bit unsigned integers.

The B<MAPLE_CMD_*>, B<MAPLE_RESP_*>, and B<MAPLE_FUNC_*> constants can
be found within this module.

=cut

# Maple Commands/Responses
use constant MAPLE_CMD_DEVINFO     => 0x01;
use constant MAPLE_CMD_ALLINFO     => 0x02;
use constant MAPLE_CMD_RESET       => 0x03;
use constant MAPLE_CMD_SHUTDOWN    => 0x04;
use constant MAPLE_RESP_DEVINFO    => 0x05;
use constant MAPLE_RESP_DEVINFOEX  => 0x06;
use constant MAPLE_RESP_ACK        => 0x07;
use constant MAPLE_RESP_DATAXFER   => 0x08;
use constant MAPLE_CMD_GETCOND     => 0x09;
use constant MAPLE_CMD_GETMEMINFO  => 0x0a;
use constant MAPLE_CMD_READBLOCK   => 0x0b;
use constant MAPLE_CMD_WRITEBLOCK  => 0x0c;
use constant MAPLE_CMD_SYNCBLOCK   => 0x0d;
use constant MAPLE_CMD_SETCOND     => 0x0e;
use constant MAPLE_CMD_MICROPHONE  => 0x0f;
use constant MAPLE_CMD_CAMERA      => 0x11;
use constant MAPLE_RESP_ERR        => -1;
use constant MAPLE_RESP_BADFUNC    => -2;
use constant MAPLE_RESP_BADCMD     => -3;
use constant MAPLE_RESP_AGAIN      => -4;
use constant MAPLE_RESP_FILEERR    => -5;

# Maple Functions
use constant MAPLE_FUNC_CONTROLLER => 0x00000001;
use constant MAPLE_FUNC_MEMORY     => 0x00000002;
use constant MAPLE_FUNC_LCD        => 0x00000004;
use constant MAPLE_FUNC_CLOCK      => 0x00000008;
use constant MAPLE_FUNC_MICROPHONE => 0x00000010;
use constant MAPLE_FUNC_AR_GUN     => 0x00000020;
use constant MAPLE_FUNC_KEYBOARD   => 0x00000040;
use constant MAPLE_FUNC_LIGHT_GUN  => 0x00000080;
use constant MAPLE_FUNC_PURU_PURU  => 0x00000100;
use constant MAPLE_FUNC_MOUSE      => 0x00000200;
use constant MAPLE_FUNC_CAMERA     => 0x00000800;

=head1 FUNCTIONS

=head2 send_maple_packet(IP,MAPLE_DATA)

Sends a maple packet, and reads the response.

B<IP> is the IP address of the Dreamcast as a string.
B<MAPLE_DATA> is a hashref of values, as in the synopsis.

The response is returned as a hashref with the following fields:
    B<Status> - Maple status code (One of B<MAPLE_RESP_*>)
    B<To>     - Destination maple address
    B<From>   - Source maple address
    B<Length> - Length of data (number of 32-bit words)
    B<Data>   - Data (string of B<Length>*4 bytes)

B<undef> is returned if the B<bind> syscall fails.

=cut

sub send_maple_packet($$) {
	my ($ip,$maple) = @_;
	return undef unless (defined $ip && defined $maple);

	# Create/Bind the socket
	my $sin = sockaddr_in('31313',inet_aton($ip));
	my $slo = sockaddr_in('31314',inet_aton('0.0.0.0'));
	socket(MSOCK,PF_INET,SOCK_DGRAM,getprotobyname('udp'));
	bind(MSOCK,$slo) || return undef;

	# Build the packet
	my $bufi;
	my $data = 'MAPL' . 
	         pack('c5',$maple->{'port'},$maple->{'slot'},$maple->{'cmd'},$maple->{'last'},$maple->{'length'});
	$data .= pack('N' . int($maple->{'length'}),@{$maple->{'data'}}) if (defined($maple->{'data'}) && $maple->{'length'} > 0);

	# Send packet and get response
	send(MSOCK,$data,0,$sin);
	recv(MSOCK,$bufi,1024,0);
	close(MSOCK);

	# Parse Response
	my %resp = ();
	($resp{'status'},$resp{'to'},$resp{'from'},$resp{'length'}) = unpack('c4',$bufi);
	$resp{'data'} = substr($bufi,4,$resp{'length'} << 2) if ($resp{'status'} > 0 && $resp{'length'} > 0);
	return \%resp;
}

=head1 AUTHOR

Tim Hentenaar E<lt>tim@hentenaar.comE<gt>

=head1 VERSION

1.0 (Jun 23 2011)

=head1 COPYRIGHT

Copyright (C) 2011 Tim Hentenaar.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

=cut

1;
