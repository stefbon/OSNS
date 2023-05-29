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

void init_connection(struct connection_s *c, unsigned char ctype, unsigned char crole, unsigned int cflags)
{
    unsigned int stype=0;
    unsigned int sflags=0;

    /* translate the connection ctype, crole and cflags into type and flags that apply to a system socket */

    switch (ctype) {

	case CONNECTION_TYPE_LOCAL:

	    stype=OSNS_SOCKET_TYPE_CONNECTION;
	    sflags=OSNS_SOCKET_FLAG_LOCAL;
	    break;

	case CONNECTION_TYPE_NETWORK:

	    stype=OSNS_SOCKET_TYPE_CONNECTION;
	    sflags=OSNS_SOCKET_FLAG_NET;

	    if (cflags & CONNECTION_FLAG_IPv4) {

		sflags |= OSNS_SOCKET_FLAG_IPv4;

	    } else if (cflags & CONNECTION_FLAG_IPv6) {

		sflags |= OSNS_SOCKET_FLAG_IPv6;

	    }

	    if (cflags & CONNECTION_FLAG_UDP) sflags |= OSNS_SOCKET_FLAG_UDP;
	    break;

    }

    if (stype==0) {

	logoutput("init_connection: type %i not supported", ctype);
	return;

    }

    switch (crole) {

	case CONNECTION_ROLE_SERVER:

	    sflags |= OSNS_SOCKET_FLAG_SERVER;
	    break;

	case CONNECTION_ROLE_SOCKET:

	    sflags |= OSNS_SOCKET_FLAG_ENDPOINT;
	    break;

	case CONNECTION_ROLE_CLIENT:

	    break;

	default:

	    logoutput("init_connection: role %i not supported", crole);
	    return;

    }

    memset(c, 0, sizeof(struct connection_s));
    c->status=CONNECTION_STATUS_INIT;
    c->flags=cflags;
    c->error=0;
    c->expire=0;
    c->data=NULL;
    c->signal=get_default_shared_signal();
    c->ctr = 0;

    init_list_element(&c->list, NULL);
    init_osns_socket(&c->sock, stype, sflags);

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

	c->ops.client.server=NULL;
	c->ops.client.unique=0;

    }

}

void clear_connection(struct connection_s *c)
{
}

void close_connection(struct connection_s *c)
{
    struct osns_socket_s *sock=&c->sock;
    (* sock->close)(sock);
}

static void accept_connection(struct connection_s *server)
{
    struct osns_socket_s *sock=&server->sock;
    struct connection_s c_conn;
    struct connection_s *client=NULL;
    unsigned int stype=0;
    unsigned int cflags=0;

    logoutput("accept_connection");

    if (sock->type==OSNS_SOCKET_TYPE_CONNECTION) {

	if (sock->flags & OSNS_SOCKET_FLAG_NET) {

	    stype=CONNECTION_TYPE_NETWORK;

	} else if (sock->flags & OSNS_SOCKET_FLAG_LOCAL) {

	    stype=CONNECTION_TYPE_LOCAL;

	}

    }

    if (stype==0) goto disconnectLabel;
    if (server->flags & CONNECTION_FLAG_UDP) cflags |= CONNECTION_FLAG_UDP;
    init_connection(&c_conn, stype, CONNECTION_ROLE_SERVER, cflags);

    if ((* sock->sops.endpoint.accept)(sock, &c_conn.sock, 0)) {

	if (sock->type == OSNS_SOCKET_TYPE_CONNECTION) {

            if (sock->flags & OSNS_SOCKET_FLAG_LOCAL) {
	        struct local_peer_s *peer=&c_conn.ops.client.peer.local;

	        if (get_local_peer_properties(&c_conn.sock, peer)==0) {

		    logoutput("accept_connection: found uid=%u gid=%u pid=%u", (unsigned int) peer->uid, (unsigned int) peer->gid, (unsigned int) peer->pid);

	        } else {

		    logoutput_warning("accept_connection: not able to get peer properties like uid and pid");
		    goto notacceptLabel;

	        }

	    } else if (sock->flags & OSNS_SOCKET_FLAG_NET) {
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

	}

	/* call the accept_peer cb set by context of server to check the context accepts this connection */
	client=(* server->ops.server.accept_peer)(&c_conn, server);

	if (client) {
	    struct beventloop_s *loop=osns_socket_get_eventloop(sock); /* use same loop as server */
	    struct osns_socket_s *csock=&client->sock;

	    client->ops.client.server=server;

            if (add_osns_socket_eventloop(csock, loop, (void *) client, 0)==0) {

                logoutput_debug("accept_connection: client socket added to eventloop");

	    } else {

                logoutput_debug("accept_connection: unable to add client socket to eventloop");
		close_connection(client);
		free(client);
		return;

	    }

	    if (signal_set_flag(client->signal, &client->status, CONNECTION_STATUS_SERVERLIST)) {
	        struct list_header_s *h=&server->ops.server.header;

                write_lock_list_header(h);
		add_list_element_first(h, &client->list);
		client->ops.client.unique=server->ops.server.unique;
		server->ops.server.unique++;
		write_unlock_list_header(h);

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

/* functions to link the socket cb's to the server connection cb's */

static void handle_socket_data_server(struct osns_socket_s *sock, char *header, char *data, struct socket_control_data_s *ctrl, void *ptr)
{
    struct connection_s *server=(struct connection_s *) ((char *) sock - offsetof(struct connection_s, sock));
    accept_connection(server);
}

int create_serversocket(struct connection_s *server, struct beventloop_s *loop, struct connection_s *(* accept_cb)(struct connection_s *c_conn, struct connection_s *s_conn), struct connection_address_s *address, struct generic_error_s *error)
{
    struct _generic_server_sops_s *sops=NULL;
    struct osns_socket_s *sock=NULL;
    int result=-1;

    if (server==NULL) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    } else if ((server->sock.flags & OSNS_SOCKET_FLAG_ENDPOINT)==0 ) {

	logoutput("create_serversocket: connection is not a server");
	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    }

    sock=&server->sock;

    if (sock->flags & OSNS_SOCKET_FLAG_LOCAL) {

	/* bind to local socket in filesystem */

	if (address->target.path==NULL) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	}

	if (set_path_osns_sockaddr(sock, address->target.path)<=0) {

	    logoutput("create_serversocket: not able to set path");
	    goto out;

	}

    } else if (sock->flags & OSNS_SOCKET_FLAG_NET) {
	struct network_peer_s *peer=address->target.peer;

	/* bind to (local) network address and port
	    make sure the address and the type of connection match */

	if (peer==NULL) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	} else if (((sock->flags & OSNS_SOCKET_FLAG_IPv4) && (peer->host.ip.family != AF_INET)) ||
		    ((sock->flags & OSNS_SOCKET_FLAG_IPv6) && (peer->host.ip.family != AF_INET6))) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	}

	/* if there are more familys coming (ipv8 ???) this does not work anymore (this handles tow situations: ipv4 of ipv6, not more) */

	set_address_osns_sockaddr(sock, &peer->host.ip, peer->port.nr);

    } else {

	logoutput("create_serversocket: socket type %i not supported.", sock->type);
	goto out;

    }

    if ((* sock->sops.endpoint.open)(sock)==-1) {

    	set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
    	logoutput_debug("create_serversocket: unable to open socket");
    	goto out;

    }

    if ((* sock->sops.endpoint.bind)(sock)==-1) {

    	set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
    	logoutput_debug("create_serversocket: unable to bind socket");
    	goto out;

    }

    /* listen */

    if ((* sock->sops.endpoint.listen)(sock, LISTEN_BACKLOG)==-1 ) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
        logoutput_debug("create_serversocket: unable to listen socket");
        goto out;

    }

    if (loop==NULL) loop=get_default_mainloop();

    if (add_osns_socket_eventloop(sock, loop, (void *) server, OSNS_SOCKET_ENABLE_CUSTOM_READ)==0) {

        logoutput_debug("create_serversocket: client socket added to eventloop");
        sock->ctx.process_data=handle_socket_data_server;
        server->ops.server.accept_peer=((accept_cb) ? accept_cb : accept_peer_cb_default);
        result=0;

    } else {

        logoutput_debug("create_serversocket: unable to add client socket to eventloop");
    }

    out:
    return result;

}

struct connection_s *get_next_connection(struct connection_s *s_conn, struct connection_s *c_conn)
{
    struct list_element_s *list=NULL;

    if (s_conn==NULL || (s_conn->sock.flags & OSNS_SOCKET_FLAG_ENDPOINT)==0) return NULL;
    list=(c_conn) ? get_next_element(&c_conn->list) : get_list_head(&s_conn->ops.server.header);
    return ((list) ? ((struct connection_s *) ((char *) list - offsetof(struct connection_s, list))) : NULL);
}

void disconnect_connection(struct connection_s *c)
{
    logoutput_debug("disconnect_connection");
}

int set_address_osns_connection(struct connection_s *c, struct ip_address_s *ip, struct network_port_s *port)
{
    struct osns_socket_s *sock=NULL;

    if ((c==NULL) || (ip==NULL) || (port==NULL)) return -1;
    sock=&c->sock;

    if (sock->flags & OSNS_SOCKET_FLAG_NET) {

	if (((sock->flags & OSNS_SOCKET_FLAG_IPv4) && (ip->family != AF_INET)) ||
		    ((sock->flags & OSNS_SOCKET_FLAG_IPv6) && (ip->family != AF_INET6))) {

	    logoutput_warning("set_address_osns_connection: wrong address (socket flags %u ip family %u)", sock->flags, ip->family);
	    return -1;

	}

        c->ops.client.peer.network.host.flags=HOST_ADDRESS_FLAG_IP;
        memcpy(&c->ops.client.peer.network.host.ip, ip, sizeof(struct ip_address_s));
	set_address_osns_sockaddr(sock, ip, port->nr);

	if (port->type) {

            if (port->type == _NETWORK_PORT_TCP) {

                sock->flags &= ~OSNS_SOCKET_FLAG_UDP;

            } else if (port->type == _NETWORK_PORT_UDP) {

                sock->flags |= OSNS_SOCKET_FLAG_UDP;

            }

        }

	return 0;

    }

    return -1;
}

int set_path_osns_connection(struct connection_s *c, struct fs_location_path_s *path)
{

    if ((c==NULL) || (path==NULL)) return -1;

    if (c->sock.flags & OSNS_SOCKET_FLAG_LOCAL) {

	if (set_path_osns_sockaddr(&c->sock, path)>0) return 0;

    }

    logoutput_warning("set_path_osns_connection: not able to set path");
    return -1;

}

int create_connection(struct connection_s *client, struct beventloop_s *loop, void *ptr)
{
    struct osns_socket_s *sock=&client->sock;

    if ((sock->flags & (OSNS_SOCKET_FLAG_ENDPOINT | OSNS_SOCKET_FLAG_SERVER))) {

	logoutput_warning("create_connection: invalid socket");
	return -1;

    }

    if ((sock->flags & (OSNS_SOCKET_FLAG_LOCAL | OSNS_SOCKET_FLAG_NET))==0) {

	logoutput("create_connection: socket type %u flags %u not supported.", sock->type, sock->flags);
	goto erroroutLabel;

    }

    if ((* sock->sops.connection.open)(sock)==-1) {

	logoutput_debug("create_connection: unable to open");
	return -1;

    }

    if ((* sock->sops.connection.connect)(sock)==0) {
	unsigned int flags=(client->flags & CONNECTION_FLAG_CTRL_DATA) ? OSNS_SOCKET_ENABLE_CTRL_DATA : 0;

	logoutput("create_connection: connected");

        if (add_osns_socket_eventloop(sock, loop, ptr, flags)==0) {

            logoutput_debug("create_connection: client socket added to eventloop");

	} else {

            logoutput_debug("create_connection: unable to add client socket to eventloop");
            goto erroroutLabel;

        }

    } else {

	logoutput("create_connection: unable to connect");
	goto erroroutLabel;

    }

    return 0;

    erroroutLabel:
    return -1;

}

void set_connection_shared_signal(struct connection_s *c, struct shared_signal_s *signal)
{
    c->signal=signal;
}
