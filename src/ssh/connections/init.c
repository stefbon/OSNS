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
#include "ssh-send.h"
#include "ssh-receive.h"

static int init_ssh_connection(struct ssh_session_s *session, struct ssh_connection_s *sshc, unsigned int flags)
{
    unsigned int error=0;

    memset(sshc, 0, sizeof(struct ssh_connection_s));

    init_connection(&sshc->connection, CONNECTION_TYPE_NETWORK, CONNECTION_ROLE_CLIENT, flags);
    sshc->connection.expire=session->config.connection_expire;
    sshc->setup.signal=session->signal;

    for (int i=0; i<256; i++) sshc->cb[i]=msg_not_supported;
    if (init_ssh_connection_send(sshc)==-1) return -1;
    if (init_ssh_connection_receive(session, sshc, &error)==-1) return -1;
    init_ssh_connection_setup(sshc, "init", 0);
    return 0;

}

static struct ssh_connection_s *new_ssh_connection(struct ssh_session_s *session)
{
    struct ssh_connection_s *connection=malloc(sizeof(struct ssh_connection_s));

    if (connection) {

	if (init_ssh_connection(session, connection, CONNECTION_FLAG_IPv4)==0) return connection;
	free_ssh_connection(&connection);

    }

    return connection;
}

int add_ssh_connection(struct ssh_session_s *session, unsigned int flags)
{
    struct list_header_s *h=&session->connections.header;
    struct ssh_connection_s *connection=NULL;
    int result=1;

    write_lock_list_header(h);

    if ((flags & SSH_CONNECTION_FLAG_MAIN) && session->connections.main) {

	result=0;
	goto unlock;

    }

    connection=new_ssh_connection(session);

    if (connection) {
	unsigned int all=SSH_CONNECTION_FLAG_MAIN;

	add_list_element_first(h, &connection->connection.list);
	connection->flags |= (flags & all);
	connection->unique=session->connections.unique;
	if (flags & SSH_CONNECTION_FLAG_MAIN) session->connections.main=connection;
	session->connections.unique++;
	result=0;

    }

    unlock:

    write_unlock_list_header(h);
    return result;

}

int add_main_ssh_connection(struct ssh_session_s *session)
{
    return add_ssh_connection(session, SSH_CONNECTION_FLAG_MAIN);
}

void remove_ssh_connection(struct ssh_session_s *session, struct ssh_connection_s *connection)
{
    struct list_header_s *h=&session->connections.header;

    write_lock_list_header(h);
    remove_list_element(&connection->connection.list);
    if (session->connections.main==connection) session->connections.main=NULL;
    write_unlock_list_header(h);

}

void free_ssh_connection(struct ssh_connection_s **p_connection)
{
    struct ssh_connection_s *connection=*p_connection;
    free_ssh_connection_send(connection);
    free_ssh_connection_receive(connection);
    init_ssh_connection_setup(connection, "free", 0);
    free(connection);
    *p_connection=NULL;
}

void init_ssh_connections(struct ssh_session_s *session)
{
    struct ssh_connections_s *connections=&session->connections;
    connections->flags=0;
    connections->unique=0;
    connections->signal=session->signal;
    connections->main=NULL;
    init_list_header(&connections->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

