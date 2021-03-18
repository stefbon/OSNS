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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>

#include "log.h"
#include "main.h"
#include "misc.h"
#include "network.h"
#include "eventloop.h"
#include "workspace-interface.h"
#include "threads.h"

#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh-receive.h"

static int init_ssh_connection(struct ssh_session_s *session, struct ssh_connection_s *connection, unsigned char type)
{
    unsigned int error=0;

    memset(connection, 0, sizeof(struct ssh_connection_s));

    switch (type) {

    case FS_CONNECTION_TYPE_TCP4:
    case FS_CONNECTION_TYPE_TCP6:
    case FS_CONNECTION_TYPE_UDP4:
    case FS_CONNECTION_TYPE_UDP6:

	init_connection(&connection->connection, type, FS_CONNECTION_ROLE_CLIENT);
	break;

    default:

	return -1;

    }

    connection->flags=0;
    connection->refcount=0;
    init_list_element(&connection->list, &session->connections.header);
    connection->connection.expire=session->config.connection_expire;

    for (int i=0; i<256; i++) connection->cb[i]=msg_not_supported;

    if (init_ssh_connection_send(connection)==-1) return -1;
    if (init_ssh_connection_receive(connection, &error)==-1) return -1;
    init_ssh_connection_setup(connection, "init", 0);
    connection->setup.mutex=session->connections.mutex;
    connection->setup.cond=session->connections.cond;
    return 0;

}

static struct ssh_connection_s *new_ssh_connection(struct ssh_session_s *session, unsigned char type)
{
    struct ssh_connection_s *connection=malloc(sizeof(struct ssh_connection_s));

    if (connection) {

	if (init_ssh_connection(session, connection, type)==0) return connection;
	free_ssh_connection(&connection);

    }

    return connection;
}

int get_ssh_connections_unlocked(struct ssh_session_s *session)
{
    struct ssh_connections_s *connections=&session->connections;

    while (connections->flags & SSH_CONNECTIONS_FLAG_LOCKED) {

	int result=pthread_cond_wait(connections->cond, connections->mutex);

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
    connections->flags -= SSH_CONNECTIONS_FLAG_LOCKED;
}

int add_ssh_connection(struct ssh_session_s *session, unsigned char type, unsigned int flags)
{
    struct ssh_connections_s *connections=&session->connections;
    struct ssh_connection_s *connection=NULL;
    int result=1;

    pthread_mutex_lock(connections->mutex);

    if (get_ssh_connections_unlocked(session)==-1) {

	pthread_mutex_unlock(connections->mutex);
	return -1;

    }

    pthread_mutex_unlock(connections->mutex);

    if (flags & SSH_CONNECTION_FLAG_MAIN) {

	if (connections->main) return 0;

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

    pthread_mutex_lock(connections->mutex);
    set_ssh_connections_unlocked(session);
    pthread_cond_broadcast(connections->cond);
    pthread_mutex_unlock(connections->mutex);

    return result;

}

int add_main_ssh_connection(struct ssh_session_s *session)
{
    return add_ssh_connection(session, FS_CONNECTION_TYPE_TCP4, SSH_CONNECTION_FLAG_MAIN);
}

void remove_ssh_connection(struct ssh_session_s *session, struct ssh_connection_s *connection)
{
    struct ssh_connections_s *connections=&session->connections;

    pthread_mutex_lock(connections->mutex);

    if (get_ssh_connections_unlocked(session)==-1) {

	pthread_mutex_unlock(connections->mutex);
	return;

    }

    pthread_mutex_unlock(connections->mutex);

    remove_list_element(&connection->list);
    if (connections->main==connection) connections->main=NULL;

    pthread_mutex_lock(connections->mutex);
    set_ssh_connections_unlocked(session);
    pthread_cond_broadcast(connections->cond);
    pthread_mutex_unlock(connections->mutex);
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
    connections->mutex=NULL;
    connections->cond=NULL;
    connections->main=NULL;
    init_list_header(&connections->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
}

int set_ssh_connections_signal(struct ssh_session_s *session, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
    struct ssh_connections_s *connections=&session->connections;

    /* one central signal (=mutex and cond) for all connections:
	- status of setup (init, keyexchange, transport, connected, disconnect...
	- arriving of messages
    */

    if (mutex==NULL || cond==NULL) {

	mutex=malloc(sizeof(pthread_mutex_t));
	cond=malloc(sizeof(pthread_cond_t));

	if (mutex && cond) {

	    pthread_mutex_init(mutex, NULL);
	    pthread_cond_init(cond, NULL);
	    connections->flags |= SSH_CONNECTIONS_FLAG_SIGNAL_ALLOCATED;

	} else {

	    if (mutex) free(mutex);
	    if (cond) free(cond);
	    return -1;

	}

    }

    connections->mutex=mutex;
    connections->cond=cond;
    return 0;

}

void free_ssh_connections(struct ssh_session_s *session)
{
    struct ssh_connections_s *connections=&session->connections;
    struct ssh_connection_s *connection=NULL, *next=NULL;

    connection=get_next_ssh_connection(connections, NULL, "remove");

    while (connection) {

	remove_ssh_connection_eventloop(connection);
	disconnect_ssh_connection(connection);
	free_ssh_connection(&connection);
	connection=get_next_ssh_connection(connections, NULL, "remove");

    }

    if (connections->flags & SSH_CONNECTIONS_FLAG_SIGNAL_ALLOCATED) {

	pthread_mutex_destroy(connections->mutex);
	pthread_cond_destroy(connections->cond);
	free(connections->mutex);
	free(connections->cond);
	connections->flags -= SSH_CONNECTIONS_FLAG_SIGNAL_ALLOCATED;

    }

    connections->mutex=NULL;
    connections->cond=NULL;
    connections->main=NULL;

}
