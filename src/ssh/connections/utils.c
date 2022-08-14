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

struct ssh_connection_s *get_main_ssh_connection(struct ssh_connections_s *connections)
{
    struct list_header_s *h=&connections->header;
    struct list_element_s *list=get_list_head(h, 0);

    return ((list) ? ((struct ssh_connection_s *)((char *) list - offsetof(struct ssh_connection_s, list))) : NULL);

}

struct ssh_connection_s *get_next_ssh_connection(struct ssh_connections_s *connections, struct ssh_connection_s *connection, unsigned char remove)
{
    struct list_element_s *next=NULL;

    if (remove) {

	next=get_list_head(&connections->header, SIMPLE_LIST_FLAG_REMOVE);

    } else {

	next=(connection) ? get_next_element(&connection->list) : get_list_head(&connections->header, 0);

    }

    return (next) ? (struct ssh_connection_s *)(((char *) next) - offsetof(struct ssh_connection_s, list)) : NULL;
}

unsigned int get_status_ssh_connection(struct ssh_connection_s *connection)
{
    return get_status_osns_socket(&connection->connection.sock);
}

void get_ssh_connection_expire_init(struct ssh_connection_s *c, struct system_timespec_s *expire)
{
    struct connection_s *connection=&c->connection;

    get_current_time_system_time(expire);
    system_time_add(expire, SYSTEM_TIME_ADD_ZERO, c->connection.expire); /* connection expire is only in sec */
}

void get_ssh_connection_expire_session(struct ssh_connection_s *c, struct system_timespec_s *expire)
{
    struct ssh_session_s *session=get_ssh_connection_session(c);

    get_current_time_system_time(expire);
    system_time_add(expire, SYSTEM_TIME_ADD_ZERO, session->config.connection_expire); /* connection expire is only in sec */
}

void get_ssh_connection_expire_userauth(struct ssh_connection_s *c, struct system_timespec_s *expire)
{
    struct ssh_session_s *session=get_ssh_connection_session(c);

    get_current_time_system_time(expire);
    system_time_add(expire, SYSTEM_TIME_ADD_ZERO, session->config.userauth_expire); /* connection expire is only in sec */
}

void signal_ssh_connections(struct ssh_session_s *session)
{
    struct ssh_connections_s *c=&session->connections;

    /* signal any waiting thread for a payload (this is done via signal) */

    signal_lock(c->signal);
    signal_broadcast(c->signal);
    signal_unlock(c->signal);

}

static void common_refcount_ssh_connection(struct ssh_connection_s *connection, signed char step)
{
    struct ssh_connections_s *connections=get_ssh_connection_connections(connection);

    logoutput("common_refcount_ssh_connection: step %i con %s sig %s ", step, (connections) ? "defined" : "null", (connections && connections->signal) ? "defined" : "null");

    /* signal any waiting thread for a payload (this is done via signal) */

    signal_lock(connections->signal);

    if (step<0 && connection->refcount <= abs(step)) {

	connection->refcount=0;

    } else {

	connection->refcount+=step;

    }

    signal_unlock(connections->signal);
}

void increase_refcount_ssh_connection(struct ssh_connection_s *connection)
{
    common_refcount_ssh_connection(connection, 1);
}

void decrease_refcount_ssh_connection(struct ssh_connection_s *connection)
{
    common_refcount_ssh_connection(connection, -1);
}

struct ssh_session_s *get_ssh_connection_session(struct ssh_connection_s *connection)
{
    struct list_header_s *h=connection->list.h;

    if (h) {
	struct ssh_connections_s *connections=(struct ssh_connections_s *)(((char *) h) - offsetof(struct ssh_connections_s, header));

	return (struct ssh_session_s *)(((char *) connections) - offsetof(struct ssh_session_s, connections));

    }

    return NULL;
}

struct ssh_connections_s *get_ssh_connection_connections(struct ssh_connection_s *connection)
{
    struct list_header_s *h=connection->list.h;
    return ((h) ? (struct ssh_connections_s *)(((char *) h) - offsetof(struct ssh_connections_s, header)) : NULL);
}

struct connection_s *get_session_connection(struct ssh_session_s *s)
{
    struct ssh_connection_s *conn=s->connections.main;
    return (conn) ? &conn->connection : NULL;
}
