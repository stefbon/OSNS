/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Stef Bon <stefbon@gmail.com>

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

#include "socket.h"
#include "common.h"
#include "utils.h"

/* dummy server socket ops */

static struct osns_socket_s *socket_accept_dummy(struct osns_socket_s *s, struct osns_socket_s *c, int flags)
{
    return NULL;
}

static int socket_cb_dummy(struct osns_socket_s *s)
{
    return -1;
}

static int socket_listen_dummy(struct osns_socket_s *s, int len)
{
    return -1;
}

static int socket_bind_common(struct osns_socket_s *sock)
{
    int result=-1;

    if (check_status_hlpr(sock->status, (SOCKET_STATUS_INIT | SOCKET_STATUS_OPEN), SOCKET_STATUS_BIND)==0) {

#ifdef __linux__

	int fd=(* sock->get_unix_fd)(sock);

	if (bind(fd, sock->data.connection.sockaddr.addr, sock->data.connection.sockaddr.len)==0) {

	    result=0;
	    sock->status |= SOCKET_STATUS_BIND;

	} else {

	    logoutput_debug("socket_bind: error %i:%s binding fd %i", errno, strerror(errno), fd);

	}

#else

	logoutput_debug("socket_bind: not supported");

#endif

    } else {

	logoutput_debug("socket_bind: status %i", sock->status);

    }

    return result;
}

static int socket_listen_common(struct osns_socket_s *sock, int len)
{
    int result=-1;

    if (check_status_hlpr(sock->status, (SOCKET_STATUS_INIT | SOCKET_STATUS_OPEN | SOCKET_STATUS_BIND), SOCKET_STATUS_LISTEN)==0) {

#ifdef __linux__

	int fd=(* sock->get_unix_fd)(sock);

	if (listen(fd, len)==0) {

	    result=0;
	    sock->status |= SOCKET_STATUS_LISTEN;

	} else {

	    logoutput_debug("socket_listen: error %i:%s listning fd %i", errno, strerror(errno), fd);

	}

#else

	logoutput_debug("socket_listen: not supported");

#endif

    }

    return result;

}

static int socket_connect_common(struct osns_socket_s *sock)
{
    int result=-1;

    if ((sock->flags & (OSNS_SOCKET_FLAG_ENDPOINT | OSNS_SOCKET_FLAG_SERVER))==0) {

#ifdef __linux__

	int fdc=(* sock->get_unix_fd)(sock);

	if (connect(fdc, sock->data.connection.sockaddr.addr, sock->data.connection.sockaddr.len)==0) {

	    result=0;
	    sock->status |= SOCKET_STATUS_CONNECT;

	} else {

	    logoutput_debug("socket_connect: unable to connect %i:%s", errno, strerror(errno));

	}

#else

	logoutput_debug("socket_connect: not supported");

#endif

    }

    return result;
}

static int socket_open(struct osns_socket_s *sock);
static struct osns_socket_s *socket_accept_common(struct osns_socket_s *server, struct osns_socket_s *client, int flags);

static void set_connection_cb(struct osns_socket_s *sock, unsigned char enable)
{

    if (sock->flags & OSNS_SOCKET_FLAG_ENDPOINT) {

	sock->sops.endpoint.open=(enable ? socket_cb_dummy : socket_open);

	sock->sops.endpoint.accept=(enable ? socket_accept_common : socket_accept_dummy);
	sock->sops.endpoint.bind=(enable ? socket_bind_common: socket_cb_dummy);
	sock->sops.endpoint.listen=(enable ? socket_listen_common : socket_listen_dummy);

    } else {

	sock->sops.connection.open=(enable ? socket_cb_dummy : socket_open);

	sock->sops.connection.connect=(enable ? socket_connect_common : socket_connect_dummy);
	sock->sops.connection.recv=(enable ? socket_recv_common: socket_recvsend_dummy);
	sock->sops.connection.send=(enable ? socket_send_common: socket_recvsend_dummy);
	sock->sops.connection.writev=(enable ? socket_writev_common : socket_writevreadv_dummy);
	sock->sops.connection.readv=(enable ? socket_readv_common : socket_writevreadv_dummy);
	sock->sops.connection.recvmsg=(enable ? socket_recvmsg_common : socket_recvmsg_dummy);
	sock->sops.connection.sendmsg=(enable ? socket_sendmsg_common : socket_sendmsg_dummy);

    }

}

static unsigned int get_client_flags_from_server(struct osns_socket_s *server)
{
    unsigned int flags=0;

    if (server->flags & OSNS_SOCKET_FLAG_LOCAL) {

	flags |= OSNS_SOCKET_FLAG_LOCAL;

	if (server->flags & OSNS_SOCKET_FLAG_UDP) {

	    flags |= OSNS_SOCKET_FLAG_UDP;

	}

    } else if (server->flags & OSNS_SOCKET_FLAG_NET) {

	flags |= OSNS_SOCKET_FLAG_NET;

	if (server->flags & OSNS_SOCKET_FLAG_IPv4) {

	    flags |= OSNS_SOCKET_FLAG_IPv4;

	} else if (server->flags & OSNS_SOCKET_FLAG_IPv6) {

	    flags |= OSNS_SOCKET_FLAG_IPv6;

	}

    }

    if (server->flags & OSNS_SOCKET_FLAG_RDWR) {

	flags |= OSNS_SOCKET_FLAG_RDWR;

    } else if ((server->flags & OSNS_SOCKET_FLAG_WRONLY)==0) {

	flags |= OSNS_SOCKET_FLAG_WRONLY;

    }

    return flags;

}

static struct osns_socket_s *socket_accept_common(struct osns_socket_s *server, struct osns_socket_s *client, int flags)
{
    struct osns_socket_s *result=NULL;

    if (check_status_hlpr(server->status, (SOCKET_STATUS_INIT | SOCKET_STATUS_OPEN | SOCKET_STATUS_BIND | SOCKET_STATUS_LISTEN), 0)==0 &&
	(client==NULL || (check_status_hlpr(client->status, SOCKET_STATUS_INIT, SOCKET_STATUS_ACCEPT)==0))) {

#ifdef __linux__

	int fds=(* server->get_unix_fd)(server);

	if (fds>=0) {
	    int fdc=-1;
	    unsigned char allocated=0;

	    flags &= SOCK_CLOEXEC;

	    if (client==NULL) {

		client=malloc(sizeof(struct osns_socket_s));
		if (client==NULL) return NULL;
		allocated=1;
		init_osns_socket(client, server->type, get_client_flags_from_server(server));

	    }

	    fdc=accept4(fds, client->data.connection.sockaddr.addr, &client->data.connection.sockaddr.len, flags);

	    if (fdc>=0) {

		logoutput_debug("socket_accept: accepted fdc %i", fdc);
		(* client->set_unix_fd)(client, fdc);
		result=client;
		client->status |= (SOCKET_STATUS_ACCEPT | SOCKET_STATUS_OPEN);
		if (allocated) client->flags |= OSNS_SOCKET_FLAG_ALLOC;
		set_connection_cb(client, 1);

	    } else {

		if (allocated) free(client);
		logoutput_debug("socket_accept: error %i:%s", errno, strerror(errno));

	    }

	}

#else

	logoutput_debug("socket_accept: not supported");

#endif


    } else {

	logoutput_debug("socket_accept: check status negative");

    }

    return result;
}

static int socket_open(struct osns_socket_s *sock)
{
    int domain=0;
    int type=0;
    int fd=0;
    int result=-1;

    if (sock->flags & OSNS_SOCKET_FLAG_LOCAL) {

	domain=AF_UNIX;

    } else if (sock->flags & OSNS_SOCKET_FLAG_NET) {

	if (sock->flags & OSNS_SOCKET_FLAG_IPv4) {

	    domain=AF_INET;

	} else if (sock->flags & OSNS_SOCKET_FLAG_IPv6) {

	    domain=AF_INET6;

	}

    }

    type=((sock->flags & sock->flags & OSNS_SOCKET_FLAG_UDP) ? SOCK_DGRAM : SOCK_STREAM);

#ifdef __linux__

    fd=socket(domain, type, 0);

    if (fd>=0) {

	sock->fd=fd;
	sock->pid=getpid();
	set_connection_cb(sock, 1);
	sock->status |= SOCKET_STATUS_OPEN;
	result=0;

    }

#endif

    return result;

}

void init_osns_connection_socket(struct osns_socket_s *sock)
{
    int domain=0;
    int type=0;

    logoutput_debug("init_osns_connection_socket");
    set_connection_cb(sock, 0);

    if (sock->flags & OSNS_SOCKET_FLAG_LOCAL) {
	struct osns_sockaddr_s *sockaddr=&sock->data.connection.sockaddr;

	sockaddr->data.local.sun_family=AF_UNIX;
	sockaddr->len=sizeof(struct sockaddr_un);
	sockaddr->addr=(struct sockaddr *) &sockaddr->data.local;

    } else if (sock->flags & OSNS_SOCKET_FLAG_NET) {
	struct osns_sockaddr_s *sockaddr=&sock->data.connection.sockaddr;

	if (sock->flags & OSNS_SOCKET_FLAG_IPv4) {

	    sockaddr->data.net4.sin_family=AF_INET;
	    sockaddr->addr=(struct sockaddr *) &sockaddr->data.net4;
	    sockaddr->len=sizeof(struct sockaddr_in);

	} else if (sock->flags & OSNS_SOCKET_FLAG_IPv6) {

	    sockaddr->data.net6.sin6_family=AF_INET6;
	    sockaddr->addr=(struct sockaddr *) &sockaddr->data.net6;
	    sockaddr->len=sizeof(struct sockaddr_in6);

	}

    }

}
