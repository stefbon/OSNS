/*
  2010, 2011, 2012, 2103, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#include <sys/socket.h>
#include <netdb.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-network.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-threads.h"

#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-utils.h"
#include "ssh-receive.h"

#define _SSH_BEVENTLOOP_NAME			"SSH"

int create_ssh_networksocket(struct ssh_connection_s *connection, char *address, unsigned int port)
{
    int fd=-1;

    out:
    return fd;
}

int connect_ssh_connection(struct ssh_connection_s *connection, struct host_address_s *address, struct network_port_s *port, struct beventloop_s *loop)
{
    struct connection_address_s caddr;
    struct network_peer_s remote;

    memset(&caddr, 0, sizeof(struct connection_address_s));
    memcpy(&remote.host, address, sizeof(struct host_address_s));
    remote.port.nr=port->nr;
    remote.port.type=port->type;
    caddr.target.peer=&remote;

    if (create_connection(&connection->connection, &caddr, loop)>=0) {
	struct system_socket_s *sock=&connection->connection.sock;

	set_system_socket_nonblocking(sock);

	return (* sock->sops.get_unix_fd)(sock);

    }

    return -1;
}

void disconnect_ssh_connection(struct ssh_connection_s *connection)
{
    struct connection_s *c=&connection->connection;

    if (c->status & CONNECTION_STATUS_CONNECTED) {

	(* c->ops.client.disconnect)(c, 0);
	c->status&=~CONNECTION_STATUS_CONNECTED;

    }

}

int add_ssh_connection_eventloop(struct ssh_connection_s *connection, unsigned int fd, unsigned int *error)
{
    return -1;
}

void remove_ssh_connection_eventloop(struct ssh_connection_s *connection)
{
    struct connection_s *c=&connection->connection;

    (* c->ops.client.disconnect)(c, 0);

}
