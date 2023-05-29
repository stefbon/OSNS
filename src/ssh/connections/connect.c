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

int connect_ssh_connection(struct ssh_connection_s *sshc, struct ip_address_s *ip, struct network_port_s *port, struct beventloop_s *loop)
{

    if (set_address_osns_connection(&sshc->connection, ip, port)==0) {

        if (create_connection(&sshc->connection, loop, (void *) sshc)>=0) {
	    struct osns_socket_s *sock=&sshc->connection.sock;

	    set_osns_socket_nonblocking(sock);
	    set_ssh_socket_behaviour(sock, "init");
	    return (* sock->get_unix_fd)(sock);

        }

    }

    return -1;
}
