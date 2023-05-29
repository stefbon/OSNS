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

static int open_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection, int (* open_socket)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags))
{
    struct osns_socket_s *sock=NULL;

    /* STDIN */

    sock=&connection->type.std.stdin;
    (* sock->set_unix_fd)(sock, fileno(stdin));
    if ((* open_socket)(connection, sock, SSH_SUBSYSTEM_SOCKET_TYPE_IN, SSH_SUBSYSTEM_SOCKET_FLAG_IN)<0) goto errorstdin;

    /* STDOUT */

    sock=&connection->type.std.stdout;
    (* sock->set_unix_fd)(sock, fileno(stdout));
    if ((* open_socket)(connection, sock, SSH_SUBSYSTEM_SOCKET_TYPE_OUT, SSH_SUBSYSTEM_SOCKET_FLAG_OUT)<0) goto errorstdout;

    /* STDERR */

    sock=&connection->type.std.stderr;
    (* sock->set_unix_fd)(sock, fileno(stderr));
    if ((* open_socket)(connection, sock, SSH_SUBSYSTEM_SOCKET_TYPE_ERROR, (SSH_SUBSYSTEM_SOCKET_FLAG_ERROR | SSH_SUBSYSTEM_SOCKET_FLAG_IN | SSH_SUBSYSTEM_SOCKET_FLAG_OUT))<0) goto errorstderr;

    return 0;

    errorstderr:
    process_socket_close_default(&connection->type.std.stderr, SOCKET_LEVEL_LOCAL, NULL);

    errorstdout:
    process_socket_close_default(&connection->type.std.stdout, SOCKET_LEVEL_LOCAL, NULL);

    errorstdin:
    process_socket_close_default(&connection->type.std.stdin, SOCKET_LEVEL_LOCAL, NULL);

    return -1;

}

static void close_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection, unsigned int type)
{

    /* STDIN */

    if ((type==0) || (type == SSH_SUBSYSTEM_SOCKET_TYPE_IN)) process_socket_close_default(&connection->type.std.stdin, SOCKET_LEVEL_LOCAL, NULL);

    /* STDOUT */

    if ((type==0) || (type == SSH_SUBSYSTEM_SOCKET_TYPE_OUT)) process_socket_close_default(&connection->type.std.stdout, SOCKET_LEVEL_LOCAL, NULL);

    /* STDERR */

    if ((type==0) || (type == SSH_SUBSYSTEM_SOCKET_TYPE_ERROR)) process_socket_close_default(&connection->type.std.stderr, SOCKET_LEVEL_LOCAL, NULL);

}

static int write_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection, char *data, unsigned int size)
{
    struct osns_socket_s *sock=&connection->type.std.stdout;
    return (* sock->sops.device.write)(sock, data, size);
}

int init_ssh_subsystem_std(struct ssh_subsystem_connection_s *connection, void (* init_socket)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags))
{

    connection->flags |= SSH_SUBSYSTEM_CONNECTION_FLAG_STD;

    init_osns_socket(&connection->type.std.stdin, OSNS_SOCKET_TYPE_DEVICE, 0);
    (* init_socket)(connection, &connection->type.std.stdin, SSH_SUBSYSTEM_SOCKET_TYPE_IN, SSH_SUBSYSTEM_SOCKET_FLAG_IN);

    init_osns_socket(&connection->type.std.stdout, OSNS_SOCKET_TYPE_DEVICE, OSNS_SOCKET_FLAG_WRONLY);
    (* init_socket)(connection, &connection->type.std.stdout, SSH_SUBSYSTEM_SOCKET_TYPE_OUT, SSH_SUBSYSTEM_SOCKET_FLAG_OUT);

    init_osns_socket(&connection->type.std.stderr, OSNS_SOCKET_TYPE_DEVICE, OSNS_SOCKET_FLAG_RDWR);
    (* init_socket)(connection, &connection->type.std.stderr, SSH_SUBSYSTEM_SOCKET_TYPE_ERROR, (SSH_SUBSYSTEM_SOCKET_FLAG_ERROR | SSH_SUBSYSTEM_SOCKET_FLAG_IN | SSH_SUBSYSTEM_SOCKET_FLAG_OUT));

    connection->open=open_ssh_subsystem_std;
    connection->close=close_ssh_subsystem_std;
    connection->write=write_ssh_subsystem_std;

    return 0;

}

