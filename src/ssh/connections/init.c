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

static int init_ssh_connection(struct ssh_session_s *session, struct ssh_connection_s *connection, unsigned int type, unsigned int flags)
{
    unsigned int error=0;

    memset(connection, 0, sizeof(struct ssh_connection_s));

    switch (type) {

	case CONNECTION_TYPE_NETWORK:

	    init_connection(&connection->connection, type, CONNECTION_ROLE_CLIENT, flags);
	    break;

	default:

	    logoutput_debug("init_ssh_connection: type %u not reckognized", type);
	    return -1;

    }

    connection->flags=0;
    connection->refcount=0;
    init_list_element(&connection->list, &session->connections.header);
    connection->connection.expire=session->config.connection_expire;
    connection->setup.signal=session->signal;

    for (int i=0; i<256; i++) connection->cb[i]=msg_not_supported;

    if (init_ssh_connection_send(connection)==-1) return -1;
    if (init_ssh_connection_receive(connection, &error)==-1) return -1;
    init_ssh_connection_setup(connection, "init", 0);

    return 0;

}

static struct ssh_connection_s *new_ssh_connection(struct ssh_session_s *session, unsigned int type)
{
    struct ssh_connection_s *connection=malloc(sizeof(struct ssh_connection_s));

    if (connection) {

	if (init_ssh_connection(session, connection, type, CONNECTION_FLAG_IPv4)==0) return connection;
	free_ssh_connection(&connection);

    }

    return connection;
}

int get_ssh_connections_unlocked(struct ssh_session_s *session)
{
    struct ssh_connections_s *connections=&session->connections;
    struct shared_signal_s *signal=connections->signal;

    while (connections->flags & SSH_CONNECTIONS_FLAG_LOCKED) {

	int result=signal_condwait(signal);

	if ((connections->flags & SSH_CONNECTIONS_FLAG_LOCKED)==0) {

	    break;

	} else if (result>0) {

	    logoutput("get_ssh_connection_unlocked: error %i waiting for connections unlock (%s)", result, strerror(result));
	    return -1;

	}

    }

    connections->flags |= SSH_CONNECTIONS_FLAG_LOCKED;
    return 0;

}

void set_ssh_connections_unlocked(struct ssh_session_s *session)
{
    struct ssh_connections_s *connections=&session->connections;
    connections->flags &= ~SSH_CONNECTIONS_FLAG_LOCKED;
}

int add_ssh_connection(struct ssh_session_s *session, unsigned int type, unsigned int flags)
{
    struct ssh_connections_s *connections=&session->connections;
    struct shared_signal_s *signal=connections->signal;
    struct ssh_connection_s *connection=NULL;
    int result=1;

    signal_lock(signal);

    if (get_ssh_connections_unlocked(session)==-1) {

	signal_unlock(signal);
	return -1;

    }

    signal_unlock(signal);

    if ((flags & SSH_CONNECTION_FLAG_MAIN) && connections->main) {

	result=0;
	goto unlock;

    }

    connection=new_ssh_connection(session, type);

    if (connection) {
	unsigned int all=SSH_CONNECTION_FLAG_MAIN;

	add_list_element_first(&connections->header, &connection->list);
	connection->flags |= (flags & all);
	connection->unique=connections->unique;
	if (flags & SSH_CONNECTION_FLAG_MAIN) connections->main=connection;
	connections->unique++;
	result=0;

    }

    unlock:

    signal_lock(signal);
    set_ssh_connections_unlocked(session);
    signal_broadcast(signal);
    signal_unlock(signal);

    return result;

}

int add_main_ssh_connection(struct ssh_session_s *session)
{
    return add_ssh_connection(session, CONNECTION_TYPE_NETWORK, SSH_CONNECTION_FLAG_MAIN);
}

void remove_ssh_connection(struct ssh_session_s *session, struct ssh_connection_s *connection)
{
    struct ssh_connections_s *connections=&session->connections;
    struct shared_signal_s *signal=connections->signal;

    signal_lock(signal);

    if (get_ssh_connections_unlocked(session)==-1) {

	signal_unlock(signal);
	return;

    }

    signal_unlock(signal);

    remove_list_element(&connection->list);
    if (connections->main==connection) connections->main=NULL;

    signal_lock(signal);
    set_ssh_connections_unlocked(session);
    signal_broadcast(signal);
    signal_unlock(signal);
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
    connections->signal=NULL;
    connections->main=NULL;
    init_list_header(&connections->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

int set_ssh_connections_signal(struct ssh_session_s *session, struct shared_signal_s *signal)
{
    struct ssh_connections_s *connections=&session->connections;

    /* one central signal (=mutex and cond) for all connections:
	- status of setup (init, keyexchange, transport, connected, disconnect...
	- arriving of messages
    */

    if (signal==NULL) signal=get_default_shared_signal();
    connections->signal=signal;
    return 0;

}

void free_ssh_connections(struct ssh_session_s *session)
{
    struct ssh_connections_s *connections=&session->connections;
    struct ssh_connection_s *connection=NULL, *next=NULL;

    connection=get_next_ssh_connection(connections, NULL, 1);

    while (connection) {

	remove_ssh_connection_eventloop(connection);
	disconnect_ssh_connection(connection);
	free_ssh_connection(&connection);
	connection=get_next_ssh_connection(connections, NULL, 1);

    }

    connections->signal=NULL;
    connections->main=NULL;

}
