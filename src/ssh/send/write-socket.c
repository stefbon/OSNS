/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-threads.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-send.h"
#include "ssh-utils.h"

int write_ssh_socket(struct ssh_connection_s *connection, struct ssh_packet_s *packet, unsigned int *error)
{
    struct osns_socket_s *sock=&connection->connection.sock;
    ssize_t written=0;
    char *pos=packet->buffer;
    int left=(int) packet->size;

    writesocket:

    written=(* sock->sops.connection.send)(sock, pos, left, 0);
    logoutput_debug("write_ssh_socket: seq %i len %i written %i", packet->sequence, left, written);

    if (written==-1) {

	if (errno==EAGAIN || errno==EWOULDBLOCK) goto writesocket;

	*error=errno;
	return -1;

    }

    pos+=written;
    left-=written;
    if (left>0) goto writesocket;

    return (int)(pos - packet->buffer);
}
