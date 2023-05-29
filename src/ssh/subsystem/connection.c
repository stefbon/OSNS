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

static int open_default(struct ssh_subsystem_connection_s *connection, int (* cb)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags))
{
    return -1;
}

static void close_default(struct ssh_subsystem_connection_s *connection, unsigned int type)
{
}

static int write_default(struct ssh_subsystem_connection_s *connection, char *data, unsigned int size)
{
    return -1;
}

int init_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection, unsigned char type, struct shared_signal_s *signal, void (* init_socket)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags))
{
    int result=-1;

    connection->flags=0;
    connection->errcode=0;
    connection->signal=signal;

    connection->open=open_default;
    connection->close=close_default;
    connection->write=write_default;

    /* default is std io */

    if (type==0) type=SSH_SUBSYSTEM_CONNECTION_TYPE_STD;

    /* depending type init connection */

    if (type==SSH_SUBSYSTEM_CONNECTION_TYPE_STD) {

	result=init_ssh_subsystem_std(connection, init_socket);

    } else {

	logoutput_warning("connect_ssh_subsystem_connection: type %i not reckognized: cannot continue", type);

    }

    return result;

}

int open_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection, int (* open_socket)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags))
{
    return (connection->open)(connection, open_socket);
}
