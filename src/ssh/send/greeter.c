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

#include "libosns-log.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-connections.h"

unsigned int create_greeter(char *pos)
{
    unsigned int len0=strlen("SSH-2.0-osns_");
    unsigned int len1=strlen("1.0a1");

    if (pos) {

	memcpy(pos, "SSH-2.0-osns_", len0);
	pos+=len0;
	memcpy(pos, "1.0a1", len1);
	pos+=len1;

    }

    return len0+len1;
}


int send_ssh_greeter(struct ssh_connection_s *connection)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct system_socket_s *sock=&connection->connection.sock;
    struct ssh_string_s *greeter=&session->data.greeter_client;
    unsigned int len=create_greeter(NULL);
    char line[len+2];
    int fd=-1;

    len=create_greeter(&line[0]);
    line[len]=0;

    logoutput("send_greeter: sending %s", line);

    line[len]=(unsigned char) 13;
    line[len+1]=(unsigned char) 10;

    if (socket_send(sock, line, len+2, 0)==-1) {

	logoutput("send_greeter: error %i:%s", errno, strerror(errno));
	return -1;

    }

    if (create_ssh_string(&greeter, len, line, SSH_STRING_FLAG_ALLOC)) {

	change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_GREETER, SSH_GREETER_FLAG_C2S, 0, NULL, NULL);

    } else {

	return -1;

    }

    return 0;

}
