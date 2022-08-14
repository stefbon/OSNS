/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

#include "connection.h"
#include "std.h"

int init_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection, unsigned char type, struct shared_signal_s *signal, void (* read_cb)(struct ssh_subsystem_connection_s *c, struct osns_socket_s *sock))
{
    int result=-1;

    connection->flags=0;
    connection->error=0;
    connection->signal=signal;

    init_osns_locking(&connection->locking, 0);
    connection->read=read_cb;

    /* default */

    if (type==0) type=SSH_SUBSYSTEM_CONNECTION_TYPE_STD;

    /* depending type make connection */

    if (type==SSH_SUBSYSTEM_CONNECTION_TYPE_STD) {

	result=init_ssh_subsystem_std(connection);

    } else {

	logoutput_warning("connect_ssh_subsystem_connection: type %i not reckognized: cannot continue", type);
	return -1;

    }

    return 0;

}

int connect_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection)
{

    if ((* connection->open)(connection)>=0) {

	logoutput_info("connect_ssh_subsystem_connection: connected");
	return 0;

    }

    logoutput_warning("connect_ssh_subsystem_connection: failed to connect");
    return -1;

}

void clear_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection)
{

    if (connection->flags & SSH_SUBSYSTEM_CONNECTION_FLAG_STD) {

	clear_ssh_subsystem_std(connection);

    }

}
