/*
 
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#include <sys/uio.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-eventloop.h"
#include "libosns-error.h"
#include "libosns-socket.h"

#include "connection.h"

/* server cb's */

static struct connection_s *accept_peer_cb_default(struct connection_s *c_conn, struct connection_s *s_conn)
{

    /* default: accept nothing */
    return NULL;
}

/* client cb's */

void disconnect_cb_default(struct connection_s *conn, unsigned char remote)
{
    struct system_socket_s *sock=&conn->sock;

    if (signal_set_flag(conn->signal, &conn->status, CONNECTION_STATUS_DISCONNECTING)) {

	/* remove connection from server list */

	if ((conn->sock.flags & SYSTEM_SOCKET_FLAG_ENDPOINT)==0 && (conn->sock.flags & SYSTEM_SOCKET_FLAG_SERVER)) {

	    if (signal_unset_flag(conn->signal, &conn->status, CONNECTION_STATUS_SERVERLIST)) {

		remove_list_element(&conn->list);

	    }

	}

	(* sock->sops.close)(sock);
	signal_set_flag(conn->signal, &conn->status, CONNECTION_STATUS_DISCONNECTED);

    }

    if (signal_unset_flag(conn->signal, &conn->status, CONNECTION_STATUS_EVENTLOOP)) {

	if (sock->event.type==SOCKET_EVENT_TYPE_BEVENT) {
	    struct bevent_s *bevent=sock->event.link.bevent;

	    remove_bevent_watch(bevent, BEVENT_REMOVE_FLAG_UNSET);
    	    free_bevent(&bevent);
	    sock->event.link.bevent=NULL;
	    sock->event.type=0;

	}

    }

}

static void error_cb_default(struct connection_s *conn, struct generic_error_s *error)
{

    if (error->type==0) {

	/* TODO: get from a call like getsockopt
	    and add error handling in reading/writing socket -> add a socket_error_s struct
	*/

    }

}

static void dataavail_cb_default(struct connection_s *conn)
{
    /* TODO: read data ... this cb should not be the one which is called
    */
}

void init_connection(struct connection_s *c, unsigned char ctype, unsigned char crole, unsigned int cflags)
{
    unsigned int stype=0;
    unsigned int sflags=0;

    /* translate the connection ctype, crole and cflags into type and flags that apply to a system socket */

    switch (ctype) {

	case CONNECTION_TYPE_LOCAL:

	    stype=SYSTEM_SOCKET_TYPE_LOCAL;
	    break;

	case CONNECTION_TYPE_NETWORK:

	    stype=SYSTEM_SOCKET_TYPE_NET;

	    if (cflags & CONNECTION_FLAG_IPv4) {

		sflags |= SYSTEM_SOCKET_FLAG_IPv4;

	    } else if (cflags & CONNECTION_FLAG_IPv6) {

		sflags |= SYSTEM_SOCKET_FLAG_IPv6;

	    }

	    if (cflags & CONNECTION_FLAG_UDP) sflags |= SYSTEM_SOCKET_FLAG_UDP;
	    break;

    }

    if (stype==0) {

	logoutput("init_connection: type %i not supported", ctype);
	return;

    }

    switch (crole) {

	case CONNECTION_ROLE_SERVER:

	    sflags |= SYSTEM_SOCKET_FLAG_SERVER;
	    break;

	case CONNECTION_ROLE_SOCKET:

	    sflags |= SYSTEM_SOCKET_FLAG_ENDPOINT;
	    break;

	case CONNECTION_ROLE_CLIENT:

	    break;

	default:

	    logoutput("init_connection: role %i not supported", crole);
	    return;

    }

    memset(c, 0, sizeof(struct connection_s));
    c->status=CONNECTION_STATUS_INIT;
    c->error=0;
    c->expire=0;
    c->data=NULL;
    c->signal=get_default_shared_signal();

    init_list_element(&c->list, NULL);
    init_system_socket(&c->sock, stype, sflags, NULL);

    if (crole==CONNECTION_ROLE_SOCKET) {

	c->ops.server.accept_peer=accept_peer_cb_default;
	init_list_header(&c->ops.server.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	c->ops.server.unique=0;

    } else {

	if (ctype==CONNECTION_TYPE_LOCAL) {

	    c->ops.client.peer.local.uid=(uid_t) -1;
	    c->ops.client.peer.local.gid=(gid_t) -1;
	    c->ops.client.peer.local.pid=0; /* not possible value for client in userspace */

	} else if (ctype==CONNECTION_TYPE_NETWORK) {

	    init_host_address(&c->ops.client.peer.network.host);
	    c->ops.client.peer.network.port.nr=0;
	    c->ops.client.peer.network.port.type=0;

	}

	c->ops.client.disconnect=disconnect_cb_default;
	c->ops.client.error=error_cb_default;
	c->ops.client.dataavail=dataavail_cb_default;
	c->ops.client.server=NULL;
	c->ops.client.unique=0;

    }

}

void clear_connection(struct connection_s *c)
{
}

void close_connection(struct connection_s *c)
{
    struct system_socket_s *sock=&c->sock;
    (* sock->sops.close)(sock);
}

static void handle_connection_close_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct connection_s *client=(struct connection_s *) bevent->ptr;

    /* connection is closed by remote side */
    (* client->ops.client.disconnect)(client, 1);
}

static void handle_connection_error_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct connection_s *client=(struct connection_s *) bevent->ptr;
    struct generic_error_s error=GENERIC_ERROR_INIT;

    /* TODO: convert arg->error to generic error in a generic way */

#ifdef __linux__

    error.value.errnum=arg->error.error;

#endif

    (* client->ops.client.error)(client, &error);

}

static void handle_connection_data_event(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct connection_s *client=(struct connection_s *) bevent->ptr;
    (* client->ops.client.dataavail)(client);
}

static void accept_connection(struct bevent_s *bevent, unsigned int flag, struct bevent_argument_s *arg)
{
    struct connection_s *server=(struct connection_s *) bevent->ptr;
    struct connection_s c_conn;
    struct connection_s *client=NULL;
    unsigned int stype=0;

    if (signal_is_error(arg) || signal_is_close(arg)) {

	/* in case of error first find out what kind of error? */
	logoutput("accept_connection: signal is error and/or close");
	goto disconnectLabel;

    }

    logoutput("accept_connection");

    if (server->sock.type & SYSTEM_SOCKET_TYPE_NET) {

	stype=CONNECTION_TYPE_NETWORK;

    } else if (server->sock.type & SYSTEM_SOCKET_TYPE_LOCAL) {

	stype=CONNECTION_TYPE_LOCAL;

    } else {

	goto disconnectLabel;

    }

    init_connection(&c_conn, stype, CONNECTION_ROLE_SERVER, 0);

    if (socket_accept(&server->sock, &c_conn.sock, 0)) {

	if (server->sock.type & SYSTEM_SOCKET_TYPE_LOCAL) {
	    struct local_peer_s *peer=&c_conn.ops.client.peer.local;

	    if (get_local_peer_properties(&c_conn.sock, peer)==0) {

		logoutput("accept_connection: found uid=%u gid=%u pid=%u", (unsigned int) peer->uid, (unsigned int) peer->gid, (unsigned int) peer->pid);

	    } else {

		logoutput_warning("accept_connection: not able to get peer properties like uid and pid");
		goto notacceptLabel;

	    }

	} else if (server->sock.type & SYSTEM_SOCKET_TYPE_NET) {
	    struct network_peer_s *peer=&c_conn.ops.client.peer.network;

	    peer->host.flags |= (HOST_ADDRESS_FLAG_IP | HOST_ADDRESS_FLAG_HOSTNAME);

	    if (get_network_peer_properties(&c_conn.sock, peer, "remote")==0) {

		/* todo ... */
		logoutput("accept_connection: found ip=");

	    } else {

		logoutput_warning("accept_connection: not able to get peer properties like ip address");
		goto notacceptLabel;

	    }

	}

	/* call the accept_peer cb set by context of server to check the context accepts this connection */

	client=(* server->ops.server.accept_peer)(&c_conn, server);

	if (client) {
	    struct bevent_s *c_bevent=NULL;
	    struct beventloop_s *loop=get_eventloop_bevent(bevent); /* use same loop as server */

	    client->ops.client.server=server;

	    /* add to eventloop */

	    c_bevent=create_fd_bevent(loop, (void *) client);

	    if (c_bevent) {

		set_bevent_cb(c_bevent, BEVENT_FLAG_CB_DATAAVAIL, handle_connection_data_event);
		set_bevent_cb(c_bevent, BEVENT_FLAG_CB_CLOSE, handle_connection_close_event);
		set_bevent_cb(c_bevent, BEVENT_FLAG_CB_ERROR, handle_connection_error_event);
		set_bevent_system_socket(c_bevent, &client->sock);
		add_bevent_watch(c_bevent);

		signal_set_flag(client->signal, &client->status, CONNECTION_STATUS_EVENTLOOP);

	    } else {

		close_connection(client);
		free(client);
		return;

	    }

	    if (signal_set_flag(client->signal, &client->status, CONNECTION_STATUS_SERVERLIST)) {

		add_list_element_first(&server->ops.server.header, &client->list);
		client->ops.client.unique=server->ops.server.unique;
		server->ops.server.unique++;

	    }

	} else {

	    logoutput_warning("accept_connection: new connection not accepted by server ctx");
	    goto notacceptLabel;

	}

    } else {

	logoutput_warning("accept_connection: new connection not accepted by system");
	goto notacceptLabel;

    }

    out:
    return;

    notacceptLabel:
    close_connection(&c_conn);
    return;

    disconnectLabel:
    close_connection(server);
    return;

    error:
    logoutput_warning("accept_connection: error ...");

}

int create_serversocket(struct connection_s *server, struct beventloop_s *loop, struct connection_s *(* accept_cb)(struct connection_s *c_conn, struct connection_s *s_conn), struct connection_address_s *address, struct generic_error_s *error)
{
    struct _generic_server_sops_s *sops=NULL;
    struct bevent_s *bevent=NULL;
    int result=-1;

    if (server==NULL) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    } else if ((server->sock.flags & SYSTEM_SOCKET_FLAG_ENDPOINT)==0 ) {

	logoutput("create_serversocket: connection is not a server");
	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    }

    if (server->sock.type & SYSTEM_SOCKET_TYPE_LOCAL) {

	/* bind to local socket in filesystem */

	if (address->target.path==NULL) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	}

	logoutput("create_serversocket: set path");

	if (set_path_system_sockaddr(&server->sock, address->target.path)<=0) {

	    logoutput("create_serversocket: not able to set path");
	    goto out;

	}

    } else if (server->sock.type & SYSTEM_SOCKET_TYPE_NET) {
	struct network_peer_s *peer=address->target.peer;

	/* bind to (local) network address and port
	    make sure the address and the type of connection match */

	if (peer==NULL) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	} else if (((server->sock.flags & SYSTEM_SOCKET_FLAG_IPv4) && (peer->host.ip.family != AF_INET)) ||
		    ((server->sock.flags & SYSTEM_SOCKET_FLAG_IPv6) && (peer->host.ip.family != AF_INET6))) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	}

	/* if there are more familys coming (ipv8 ???) this does not work anymore (this handles tow situations: ipv4 of ipv6, not more) */

	set_address_system_sockaddr(&server->sock, &peer->host.ip, peer->port.nr);

    } else {

	logoutput("create_serversocket: socket type %i not supported.", server->sock.type);
	goto out;

    }

    if (socket_bind(&server->sock)==-1) {

    	set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
    	logoutput_debug("create_serversocket: unable to bind socket");
    	goto out;

    }

    /* listen */

    if (socket_listen(&server->sock, LISTEN_BACKLOG)==-1 ) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
        logoutput_debug("create_serversocket: unable to listen socket");
        goto out;

    }

    if (loop==NULL) loop=get_default_mainloop();
    bevent=create_fd_bevent(loop, (void *) server);
    if (bevent==NULL) goto out;
    set_bevent_cb(bevent, BEVENT_FLAG_CB_DATAAVAIL | BEVENT_FLAG_CB_CLOSE | BEVENT_FLAG_CB_ERROR, accept_connection);
    set_bevent_system_socket(bevent, &server->sock);

    if (add_bevent_watch(bevent)==0) {

    	logoutput("create_serversocket: socket added to eventloop");
	result=0;
	server->ops.server.accept_peer=((accept_cb) ? accept_cb : accept_peer_cb_default);
	signal_set_flag(server->signal, &server->status, CONNECTION_STATUS_EVENTLOOP);

    } else {

    	logoutput("create_serversocket: error adding socket to eventloop.");
	set_generic_error_system(error, EIO, __PRETTY_FUNCTION__);
	unset_bevent_system_socket(bevent, &server->sock);
	free_bevent(&bevent);

    }

    out:
    return result;

}

struct connection_s *get_next_connection(struct connection_s *s_conn, struct connection_s *c_conn)
{
    struct list_element_s *list=NULL;

    if (s_conn==NULL || (s_conn->sock.flags & SYSTEM_SOCKET_FLAG_ENDPOINT)==0) return NULL;
    list=(c_conn) ? get_next_element(&c_conn->list) : get_list_head(&s_conn->ops.server.header, 0);
    return ((list) ? ((struct connection_s *) ((char *) list - offsetof(struct connection_s, list))) : NULL);
}

void disconnect_connection(struct connection_s *client)
{
    struct system_socket_s *sock=&client->sock;

    logoutput_debug("disconnect_connection");

    if ((sock->flags & SYSTEM_SOCKET_FLAG_ENDPOINT)==0) {

	(* client->ops.client.disconnect)(client, 0);

    } else {

	disconnect_cb_default(client, 0);

    }

}

int create_connection(struct connection_s *client, struct connection_address_s *address, struct beventloop_s *loop)
{
    struct system_socket_s *sock=&client->sock;

    if ((sock->flags & (SYSTEM_SOCKET_FLAG_ENDPOINT | SYSTEM_SOCKET_FLAG_SERVER))) {

	logoutput_warning("create_connection: invalid socket");
	return -1;

    }

    if (sock->type & SYSTEM_SOCKET_TYPE_LOCAL) {

	if (address->target.path==NULL) {

	    logoutput_warning("create_connection: path sot set");
	    goto erroroutLabel;

	}

	if (set_path_system_sockaddr(sock, address->target.path)<=0) {

	    logoutput_warning("create_connection: not able to set path");
	    goto erroroutLabel;

	}

    } else if (sock->type & SYSTEM_SOCKET_TYPE_NET) {
	struct network_peer_s *peer=address->target.peer;

	if (peer==NULL) {

	    logoutput_warning("create_connection: peer sot set");
	    goto erroroutLabel;

	} else if (((client->sock.flags & SYSTEM_SOCKET_FLAG_IPv4) && (peer->host.ip.family != AF_INET)) ||
		    ((client->sock.flags & SYSTEM_SOCKET_FLAG_IPv6) && (peer->host.ip.family != AF_INET6))) {

	    logoutput_warning("create_connection: wrong address");
	    goto erroroutLabel;

	}

	/* if there are more familys coming (ipv8 ???) this does not work anymore (this handles tow situations: ipv4 of ipv6, not more) */

	set_address_system_sockaddr(&client->sock, &peer->host.ip, peer->port.nr);

    } else {

	logoutput("create_connection: socket type %i not supported.", client->sock.type);
	goto erroroutLabel;

    }

    if (socket_connect(sock)==0) {
	struct bevent_s *bevent=NULL;

	logoutput("create_osns_connection: connected");

	bevent=create_fd_bevent(loop, (void *) client);
	if (bevent==NULL) goto erroroutLabel;

	set_bevent_cb(bevent, BEVENT_FLAG_CB_ERROR, handle_connection_error_event);
	set_bevent_cb(bevent, BEVENT_FLAG_CB_CLOSE, handle_connection_close_event);
	set_bevent_cb(bevent, BEVENT_FLAG_CB_DATAAVAIL, handle_connection_data_event);

	set_bevent_system_socket(bevent, sock);
	// enable_bevent_write_watch(bevent);

	add_bevent_watch(bevent);
	signal_set_flag(client->signal, &client->status, CONNECTION_STATUS_EVENTLOOP);

    } else {

	logoutput("create_osns_connection: unable to connect");
	goto erroroutLabel;

    }

    return 0;

    erroroutLabel:
    disconnect_cb_default(client, 0);
    return -1;

}

int create_local_connection(struct connection_s *client, char *runpath, struct beventloop_s *loop)
{
    struct fs_location_path_s rundir=FS_LOCATION_PATH_INIT;
    unsigned int size=0;

    if ((client->sock.type & SYSTEM_SOCKET_TYPE_LOCAL)==0) {

	logoutput_warning("create_local_connection: invalid socket");
	return -1;

    }

    set_location_path(&rundir, 'c', runpath);
    size=append_location_path_get_required_size(&rundir, 'c', "system.sock");

    if (size>0) {
	char buffer[size];
	struct fs_location_path_s socketpath=FS_LOCATION_PATH_INIT;
	struct system_socket_s *sock=&client->sock;
	struct connection_address_s address;

	assign_buffer_location_path(&socketpath, buffer, size);
	combine_location_path(&socketpath, &rundir, 'c', "system.sock");

	address.target.path=&socketpath;
	return create_connection(client, &address, loop);

    }

    return -1;

}

void set_connection_shared_signal(struct connection_s *c, struct shared_signal_s *signal)
{
    c->signal=signal;
}
