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

static struct system_socket_s *socket_accept_dummy(struct system_socket_s *s, struct system_socket_s *c, int flags)
{
    return NULL;
}

static int socket_bind_dummy(struct system_socket_s *s)
{
    return -1;
}

static int socket_listen_dummy(struct system_socket_s *s, int len)
{
    return -1;
}

static int socket_bind_common(struct system_socket_s *sock)
{
    int result=-1;

    if (check_status_hlpr(sock->status, (SOCKET_STATUS_INIT | SOCKET_STATUS_OPEN), SOCKET_STATUS_BIND)==0) {

#ifdef __linux__

	int fd=(* sock->sops.get_unix_fd)(sock);

	if (bind(fd, sock->sockaddr.addr, sock->sockaddr.len)==0) {

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

static int socket_listen_common(struct system_socket_s *sock, int len)
{
    int result=-1;

    if (check_status_hlpr(sock->status, (SOCKET_STATUS_INIT | SOCKET_STATUS_OPEN | SOCKET_STATUS_BIND), SOCKET_STATUS_LISTEN)==0) {

#ifdef __linux__

	int fd=(* sock->sops.get_unix_fd)(sock);

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

static void set_connection_cb(struct system_socket_s *sock)
{
    sock->sops.type.socket.connection.recv=socket_recv_common;
    sock->sops.type.socket.connection.send=socket_send_common;
    sock->sops.type.socket.connection.writev=socket_writev_common;
    sock->sops.type.socket.connection.readv=socket_readv_common;
    sock->sops.type.socket.connection.recvmsg=socket_recvmsg_common;
    sock->sops.type.socket.connection.sendmsg=socket_sendmsg_common;
}

static struct system_socket_s *socket_accept_common(struct system_socket_s *server, struct system_socket_s *client, int flags)
{
    struct system_socket_s *result=NULL;

    if (check_status_hlpr(server->status, (SOCKET_STATUS_INIT | SOCKET_STATUS_OPEN | SOCKET_STATUS_BIND | SOCKET_STATUS_LISTEN), 0)==0 &&
	(client==NULL || (check_status_hlpr(client->status, SOCKET_STATUS_INIT, SOCKET_STATUS_ACCEPT)==0))) {

#ifdef __linux__

	int fds=(* server->sops.get_unix_fd)(server);

	if (fds>=0) {
	    int fdc=-1;
	    unsigned char allocated=0;

	    flags &= SOCK_CLOEXEC;

	    if (client==NULL) {

		client=malloc(sizeof(struct system_socket_s));
		if (client==NULL) return NULL;
		allocated=1;
		init_system_socket(client, get_client_type_from_server(server), get_client_flags_from_server(server), NULL);

	    }

	    fdc=accept4(fds, client->sockaddr.addr, &client->sockaddr.len, flags);

	    if (fdc>=0) {

		logoutput_debug("socket_accept: accepted fdc %i", fdc);
		(* client->sops.set_unix_fd)(client, fdc);
		result=client;
		client->status |= (SOCKET_STATUS_ACCEPT | SOCKET_STATUS_OPEN);
		set_connection_cb(client);

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

static int socket_connect_common(struct system_socket_s *sock)
{
    int result=-1;

    if ((sock->flags & SYSTEM_SOCKET_FLAG_ENDPOINT)==0) {

	if ((sock->flags & SYSTEM_SOCKET_FLAG_SERVER)==0) {

#ifdef __linux__

	    int fdc=(* sock->sops.get_unix_fd)(sock);

	    if (connect(fdc, sock->sockaddr.addr, sock->sockaddr.len)==0) {

		result=0;
		sock->status |= SOCKET_STATUS_CONNECT;
		logoutput_debug("socket_connect: %i connected", fdc);

	    } else {

		logoutput_debug("socket_connect: unable to connect %i:%s", errno, strerror(errno));

	    }

#else

	    logoutput_debug("socket_connect: not supported");

#endif


	}

    }

    if (result==0) {

	set_connection_cb(sock);

    }

    return result;
}

void init_system_socket_connection(struct system_socket_s *sock)
{
    int domain=0;
    int type=0;

    logoutput_debug("init_system_socket_connection");

    if (sock->flags & SYSTEM_SOCKET_FLAG_ENDPOINT) {

	sock->sops.type.socket.endpoint.bind=socket_bind_dummy;
	sock->sops.type.socket.endpoint.listen=socket_listen_dummy;
	sock->sops.type.socket.endpoint.accept=socket_accept_dummy;

    } else {

	sock->sops.type.socket.connection.connect=socket_connect_dummy;
	sock->sops.type.socket.connection.recv=socket_recvsend_dummy;
	sock->sops.type.socket.connection.send=socket_recvsend_dummy;
	sock->sops.type.socket.connection.writev=socket_writevreadv_dummy;
	sock->sops.type.socket.connection.readv=socket_writevreadv_dummy;
	sock->sops.type.socket.connection.recvmsg=socket_recvmsg_dummy;
	sock->sops.type.socket.connection.sendmsg=socket_sendmsg_dummy;

    }

    if (sock->type & SYSTEM_SOCKET_TYPE_LOCAL) {

	sock->sockaddr.len=sizeof(struct sockaddr_un);
	sock->sockaddr.data.local.sun_family=AF_UNIX;
	sock->sockaddr.addr=(struct sockaddr *) &sock->sockaddr.data.local;
	domain=AF_UNIX;

    } else if (sock->type & SYSTEM_SOCKET_TYPE_NET) {

	if (sock->flags & SYSTEM_SOCKET_FLAG_IPv4) {

	    sock->sockaddr.addr=(struct sockaddr *) &sock->sockaddr.data.net4;
	    sock->sockaddr.len=sizeof(struct sockaddr_in);
	    sock->sockaddr.data.net4.sin_family=AF_INET;
	    domain=AF_INET;

	} else if (sock->flags & SYSTEM_SOCKET_FLAG_IPv6) {

	    sock->sockaddr.addr=(struct sockaddr *) &sock->sockaddr.data.net6;
	    sock->sockaddr.len=sizeof(struct sockaddr_in6);
	    sock->sockaddr.data.net6.sin6_family=AF_INET6;
	    domain=AF_INET6;

	}

    }

    type=((sock->flags & SYSTEM_SOCKET_FLAG_UDP) ? SOCK_DGRAM : SOCK_STREAM);
    if (sock->flags & SYSTEM_SOCKET_FLAG_NOOPEN) return;

    if ((sock->flags & SYSTEM_SOCKET_FLAG_ENDPOINT) || ((sock->flags & SYSTEM_SOCKET_FLAG_SERVER)==0)) {

#ifdef __linux__

	/* only create a filedescriptor here when dealing with a client or serversocket
	    the serverpart of a connection is created using accept */

	sock->backend.fd=socket(domain, type, 0);

	if (sock->backend.fd>=0) {

	    logoutput("init_system_socket_common: fd %i", sock->backend.fd);

	    if (sock->flags & SYSTEM_SOCKET_FLAG_ENDPOINT) {

		sock->sops.type.socket.endpoint.bind=socket_bind_common;
		sock->sops.type.socket.endpoint.listen=socket_listen_common;
		sock->sops.type.socket.endpoint.accept=socket_accept_common;

	    } else {

		sock->sops.type.socket.connection.connect=socket_connect_common;

	    }

	    sock->status |= SOCKET_STATUS_OPEN;

	}

#endif

    } else {

	sock->sops.type.socket.connection.connect=socket_connect_common;

    }

}

int socket_connect(struct system_socket_s *sock)
{
    int result=-1;

    if (sock->type & SYSTEM_SOCKET_TYPE_CONNECTION) {

	if (check_status_hlpr(sock->status, (SOCKET_STATUS_INIT | SOCKET_STATUS_OPEN), SOCKET_STATUS_CONNECT)==0) {

	    result=(* sock->sops.type.socket.connection.connect)(sock);

	}

    }

    return result;
}

int socket_send(struct system_socket_s *sock, char *buffer, unsigned int size, unsigned char flags)
{
    return (* sock->sops.type.socket.connection.send)(sock, buffer, size, flags);
}

int socket_recv(struct system_socket_s *sock, char *buffer, unsigned int size, unsigned int flags)
{
    return (* sock->sops.type.socket.connection.recv)(sock, buffer, size, flags);
}

int socket_writev(struct system_socket_s *sock, struct iovec *iov, unsigned int count)
{
    return (* sock->sops.type.socket.connection.writev)(sock, iov, count);
}

int socket_readv(struct system_socket_s *sock, struct iovec *iov, unsigned int count)
{
    return (* sock->sops.type.socket.connection.readv)(sock, iov, count);
}

int socket_sendmsg(struct system_socket_s *sock, const struct msghdr *msg)
{
    return (* sock->sops.type.socket.connection.sendmsg)(sock, msg);
}

int socket_recvmsg(struct system_socket_s *sock, struct msghdr *msg)
{
    return (* sock->sops.type.socket.connection.recvmsg)(sock, msg);
}

/* SOCKET Endpoint */

int socket_bind(struct system_socket_s *sock)
{
    return (* sock->sops.type.socket.endpoint.bind)(sock);
}

int socket_listen(struct system_socket_s *sock, int len)
{
    return (* sock->sops.type.socket.endpoint.listen)(sock, len);
}

struct system_socket_s *socket_accept(struct system_socket_s *sock, struct system_socket_s *client, int flags)
{
    return (* sock->sops.type.socket.endpoint.accept)(sock, client, flags);
}
