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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netdb.h>

#include <pthread.h>
#include <fcntl.h>
#include <sys/uio.h>

#include "log.h"
#include "main.h"
#include "misc.h"
#include "eventloop.h"

#include "utils.h"
#include "connection.h"
#include "error.h"

/*
    ZERO unix socket ops */

static int socket_zero_accept(int fd, struct sockaddr *addr, unsigned int *len)
{
    return -1;
}
static int socket_zero_bind(int fd, struct sockaddr *addr, int *len, int sock)
{
    return -1;
}
static int socket_zero_close(struct io_socket_s *s)
{
    return -1;
}
static int socket_zero_connect(struct io_socket_s *s, struct sockaddr *addr, int *len)
{
    return -1;
}
static int socket_zero_getpeername(int fd, struct sockaddr *addr, unsigned int *len)
{
    return -1;
}
static int socket_zero_getsockname(int fd, struct sockaddr *addr, unsigned int *len)
{
    return -1;
}
static int socket_zero_getsockopt(int fd, int level, int n, char *v, unsigned int *l)
{
    return -1;
}
static int socket_zero_setsockopt(int fd, int level, int n, char *v, unsigned int l)
{
    return -1;
}
static int socket_zero_listen(int fd, int blog)
{
    return -1;
}
static int socket_zero_socket(int af, int type, int protocol)
{
    return -1;
}
static int socket_zero_send(struct io_socket_s *s, char *buffer, unsigned int size, unsigned int flags)
{
    return -1;
}
static int socket_zero_recv(struct io_socket_s *s, char *buffer, unsigned int size, unsigned int flags)
{
    return -1;
}
static int socket_zero_start()
{
    return 0;
}
static int socket_zero_finish()
{
    return 0;
}
static struct socket_ops_s zero_sops = {
    .type				=	SOCKET_OPS_TYPE_ZERO,
    .accept				=	socket_zero_accept,
    .bind				=	socket_zero_bind,
    .close				=	socket_zero_close,
    .connect				=	socket_zero_connect,
    .getpeername			=	socket_zero_getpeername,
    .getsockname			=	socket_zero_getsockname,
    .getsockopt				=	socket_zero_getsockopt,
    .listen				=	socket_zero_listen,
    .setsockopt				=	socket_zero_setsockopt,
    .socket				=	socket_zero_socket,
    .send				=	socket_zero_send,
    .recv				=	socket_zero_recv,
    .start				=	socket_zero_start,
    .finish				=	socket_zero_finish,
};
void set_io_socket_ops_zero(struct io_socket_s *s)
{
    s->sops=&zero_sops;
}

/*
    DEFAULT unix socket ops	*/

static int socket_default_accept(int fd, struct sockaddr *addr, unsigned int *len)
{
    return accept(fd, addr, len);
}
static int socket_default_bind(int fd, struct sockaddr *addr, int *len, int sock)
{
    return bind(fd, addr, *len);
}
static int socket_default_close(struct io_socket_s *s)
{
    struct bevent_s *bevent=s->bevent;
    if (bevent) (* bevent->close)(bevent);
    return 0;
}
static int socket_default_connect(struct io_socket_s *s, struct sockaddr *addr, int *len)
{

    if (s->bevent) {
	int fd=get_bevent_unix_fd(s->bevent);
	return connect(fd, addr, *len);

    }

    return -1;
}
static int socket_default_getpeername(int fd, struct sockaddr *addr, unsigned int *len)
{
    return getpeername(fd, addr, len);
}
static int socket_default_getsockname(int fd, struct sockaddr *addr, unsigned int *len)
{
    return getsockname(fd, addr, len);
}
static int socket_default_getsockopt(int fd, int level, int n, char *v, unsigned int *l)
{
    return getsockopt(fd, level, n, v, l);
}
static int socket_default_listen(int fd, int len)
{
    return listen(fd, len);
}
static int socket_default_setsockopt(int fd, int level, int n, char *v, unsigned int l)
{
    return setsockopt(fd, level, n, v, l);
}
static int socket_default_socket(int af, int type, int protocol)
{
    return socket(af, type, protocol);
}
static int socket_default_recv(struct io_socket_s *s, char *buffer, unsigned int size, unsigned int flags)
{
    struct bevent_s *bevent=s->bevent;
    int fd=get_bevent_unix_fd(bevent);
    return recv(fd, buffer, size, flags);
}
static int socket_default_send(struct io_socket_s *s, char *buffer, unsigned int size, unsigned int flags)
{
    struct bevent_s *bevent=s->bevent;
    int fd=get_bevent_unix_fd(bevent);
    return send(fd, buffer, size, flags);
}
static int socket_default_start()
{
    return 0; /* default does not require initialization */
}
static int socket_default_finish()
{
    return 0; /* default does not require cleanup etc */
}
static struct socket_ops_s default_sops = {
    .type				=	SOCKET_OPS_TYPE_DEFAULT,
    .accept				=	socket_default_accept,
    .bind				=	socket_default_bind,
    .close				=	socket_default_close,
    .connect				=	socket_default_connect,
    .getpeername			=	socket_default_getpeername,
    .getsockname			=	socket_default_getsockname,
    .getsockopt				=	socket_default_getsockopt,
    .listen				=	socket_default_listen,
    .setsockopt				=	socket_default_setsockopt,
    .socket				=	socket_default_socket,
    .send				=	socket_default_send,
    .recv				=	socket_default_recv,
    .start				=	socket_default_start,
    .finish				=	socket_default_finish,
};

void set_io_socket_ops_default(struct io_socket_s *s)
{
    s->sops=&default_sops;
}

/* STD ops: connect and use the std fd's like stdin, stdout and stderr */

/* ZERO std ops */

static int std_zero_open(struct io_std_s *s, unsigned int flags)
{
    return -1;
}

static int std_zero_close(struct io_std_s *s)
{
    return 0;
}

static int std_zero_read(struct io_std_s *s, char *buffer, unsigned int size)
{
    return -1;
}

static int std_zero_write(struct io_std_s *s, char *data, unsigned int size)
{
    return -1;
}

struct std_ops_s zero_std_ops = {
    .type				= STD_OPS_TYPE_DEFAULT,
    .open				= std_zero_open,
    .close				= std_zero_close,
    .read				= std_zero_read,
    .write				= std_zero_write,
};

void set_io_std_ops_zero(struct io_std_s *s)
{
    s->sops=&zero_std_ops;
}

/* Default std ops */

static int std_default_open(struct io_std_s *s, unsigned int flags)
{

    if (s->type==STD_SOCKET_TYPE_STDIN) {

	return STDIN_FILENO;

    } else if (s->type==STD_SOCKET_TYPE_STDOUT) {

	return STDOUT_FILENO;

    } else if (s->type==STD_SOCKET_TYPE_STDERR) {

	return STDERR_FILENO;

    }

    /* everything else */

    return -1;

}

static int std_default_close(struct io_std_s *s)
{
    /* the std fd's are left open: it's up to the os to close them */
    return 0;
}

static int std_default_read(struct io_std_s *s, char *buffer, unsigned int size)
{
    struct bevent_s *bevent=s->bevent;
    int fd=get_bevent_unix_fd(bevent);
    return read(fd, buffer, size);
}

static int std_default_write(struct io_std_s *s, char *data, unsigned int size)
{
    struct bevent_s *bevent=s->bevent;
    int fd=get_bevent_unix_fd(bevent);
    logoutput("std_default_write: fs %i", fd);
    return write(fd, data, size);
}

struct std_ops_s default_std_ops = {
    .type				= STD_OPS_TYPE_DEFAULT,
    .open				= std_default_open,
    .close				= std_default_close,
    .read				= std_default_read,
    .write				= std_default_write,
};

void set_io_std_ops_default(struct io_std_s *s)
{
    s->sops=&default_std_ops;
}

void set_io_std_type(struct fs_connection_s *c, const char *what)
{
    struct io_std_s *s=&c->io.std;

    if (c->type==FS_CONNECTION_TYPE_STD) {

	if (strcmp(what, "stdin")==0) {

	    s->type=STD_SOCKET_TYPE_STDIN;

	} else if (strcmp(what, "stdout")==0) {

	    s->type=STD_SOCKET_TYPE_STDOUT;

	} else if (strcmp(what, "stderr")==0) {

	    s->type=STD_SOCKET_TYPE_STDERR;

	} else {

	    logoutput_warning("%s not reckognized", what);

	}

    } else {

	logoutput_warning("Cannot set type std, connection of type %i", c->type);

    }

}

/* FUSE socket operations */

/* ZERO fuse ops */

static int zero_fuse_open(char *path, unsigned int flags)
{
    return -1;
}
static int zero_fuse_close(struct io_fuse_s *s)
{
    return -1;
}
static ssize_t zero_fuse_writev(struct io_fuse_s *s, struct iovec *iov, int count)
{
    return -1;
}
static int zero_fuse_read(struct io_fuse_s *s, void *buffer, size_t size)
{
    return -1;
}
static struct fuse_ops_s zero_fuse_ops = {
    .type				=	FUSE_OPS_TYPE_ZERO,
    .open				=	zero_fuse_open,
    .close				=	zero_fuse_close,
    .writev				=	zero_fuse_writev,
    .read				=	zero_fuse_read,
};

void set_io_fuse_ops_zero(struct io_fuse_s *s)
{
    s->fops=&zero_fuse_ops;
}

/* DEFAULT fuse ops */

static int default_fuse_open(char *path, unsigned int flags)
{
    return open(path, flags);
}
static int default_fuse_close(struct io_fuse_s *s)
{
    struct bevent_s *bevent=s->bevent;
    (* bevent->close)(bevent);
    return 0;
}
static ssize_t default_fuse_writev(struct io_fuse_s *s, struct iovec *iov, int count)
{
    struct bevent_s *bevent=s->bevent;
    int fd=get_bevent_unix_fd(bevent);
    return writev(fd, iov, count);
}
static int default_fuse_read(struct io_fuse_s *s, void *buffer, size_t size)
{
    struct bevent_s *bevent=s->bevent;
    int fd=get_bevent_unix_fd(bevent);
    return read(fd, buffer, size);
}
static struct fuse_ops_s default_fuse_ops = {
    .type				=	FUSE_OPS_TYPE_DEFAULT,
    .open				=	default_fuse_open,
    .close				=	default_fuse_close,
    .writev				=	default_fuse_writev,
    .read				=	default_fuse_read,
};

void set_io_fuse_ops_default(struct io_fuse_s *s)
{
    s->fops=&default_fuse_ops;
}

struct fs_connection_s *accept_local_cb_dummy(uid_t uid, gid_t gid, pid_t pid, struct fs_connection_s *s_conn)
{
    return NULL;
}

static struct fs_connection_s *accept_network_cb_dummy(struct host_address_s *host, struct fs_connection_s *c)
{
    return NULL;
}

static void event_cb_dummy(struct fs_connection_s *conn, void *data, uint32_t events)
{
}
void disconnect_cb_dummy(struct fs_connection_s *conn, unsigned char remote)
{
    if (conn->role==FS_CONNECTION_ROLE_CLIENT) {
	struct fs_connection_s *s_conn=conn->ops.client.server;

	pthread_mutex_lock(&s_conn->ops.server.mutex);
	remove_list_element(&conn->list);
	pthread_mutex_unlock(&s_conn->ops.server.mutex);

    }

    free(conn);

}
void init_cb_dummy(struct fs_connection_s *conn, unsigned int fd)
{
}

void init_connection(struct fs_connection_s *c, unsigned char type, unsigned char role)
{

    switch (type) {

	case FS_CONNECTION_TYPE_LOCAL:
	case FS_CONNECTION_TYPE_TCP4:
	case FS_CONNECTION_TYPE_TCP6:
	case FS_CONNECTION_TYPE_UDP4:
	case FS_CONNECTION_TYPE_UDP6:
	case FS_CONNECTION_TYPE_FUSE:
	case FS_CONNECTION_TYPE_STD:

	    break;

	default:

	    logoutput("init_connection: type %i not supported", type);
	    return;

    }

    switch (role) {

	case FS_CONNECTION_ROLE_SERVER:
	case FS_CONNECTION_ROLE_CLIENT:

	    break;

	default:

	    logoutput("init_connection: role %i not supported", role);
	    return;

    }


    memset(c, 0, sizeof(struct fs_connection_s));

    c->type=type;
    c->role=role;
    c->status=FS_CONNECTION_FLAG_INIT;
    c->error=0;
    c->expire=0;
    c->data=NULL;
    init_list_element(&c->list, NULL);

    if (type == FS_CONNECTION_TYPE_FUSE) {

	c->io.fuse.bevent=NULL;
	set_io_fuse_ops_default(&c->io.fuse);

    } else if (type==FS_CONNECTION_TYPE_STD) {

	c->io.std.bevent=NULL;
	set_io_std_ops_default(&c->io.std);

    } else {

	c->io.socket.bevent=NULL;
	set_io_socket_ops_default(&c->io.socket);

    }

    if (role==FS_CONNECTION_ROLE_SERVER) {

	if (type==FS_CONNECTION_TYPE_LOCAL) {

	    c->ops.server.accept.local=accept_local_cb_dummy;

	} else if (type==FS_CONNECTION_TYPE_TCP4 || type==FS_CONNECTION_TYPE_TCP6 || type==FS_CONNECTION_TYPE_UDP4 || type==FS_CONNECTION_TYPE_UDP6) {

	    c->ops.server.accept.network=accept_network_cb_dummy;

	}

	init_list_header(&c->ops.server.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	pthread_mutex_init(&c->ops.server.mutex, NULL);

    } else if (role==FS_CONNECTION_ROLE_CLIENT) {

	if (type==FS_CONNECTION_TYPE_LOCAL) {

	    c->ops.client.id.local.uid=(uid_t) -1;
	    c->ops.client.id.local.pid=0; /* not possible value for client in userspace */

	} else if (type==FS_CONNECTION_TYPE_FUSE) {

	    c->ops.client.id.fuse.uid=getuid();
	    c->ops.client.id.fuse.gid=getgid();

	} else if (type==FS_CONNECTION_TYPE_TCP4 || type==FS_CONNECTION_TYPE_TCP6 || type==FS_CONNECTION_TYPE_UDP4 || type==FS_CONNECTION_TYPE_UDP6) {

	    init_host_address(&c->ops.client.id.host);

	}

	c->ops.client.event=event_cb_dummy;
	c->ops.client.disconnect=disconnect_cb_dummy;
	c->ops.client.init=init_cb_dummy;
	c->ops.client.server=NULL;

    }

}

void free_connection(struct fs_connection_s *c)
{

    if (c->role==FS_CONNECTION_ROLE_SERVER) {

	pthread_mutex_destroy(&c->ops.server.mutex);

    }

}

static void accept_local_connection(int sfd, void *ptr, struct event_s *event)
{
    struct fs_connection_s *s_conn=(struct fs_connection_s *) ptr;
    struct socket_ops_s *sops=s_conn->io.socket.sops;
    struct fs_connection_s *c_conn=NULL;
    struct bevent_s *bevent=s_conn->io.socket.bevent;
    struct sockaddr_un local;
    struct ucred cred;
    socklen_t s_len=0;
    int fd=-1;

    if (signal_is_error(event) || signal_is_close(event)) {

	logoutput("accept_local_connection: signal is error and/or close");
	goto disconnect;

    }

    s_len=sizeof(struct sockaddr_un);
    fd=(* sops->accept)(sfd, (struct sockaddr *) &local, &s_len);

    if (fd==-1) {

	logoutput("accept_local_connection: error %i accept (%s)", errno, strerror(errno));
	goto disconnect;

    }

    /* get credentials */

    s_len=sizeof(cred);
    if ((* sops->getsockopt)(fd, SOL_SOCKET, SO_PEERCRED, (char *)&cred, &s_len)==-1) {

	logoutput("accept_local_connection: error %i geting socket credentials (%s)", errno, strerror(errno));
	goto disconnect;

    }

    c_conn=(* s_conn->ops.server.accept.local)(cred.uid, cred.gid, cred.pid, s_conn);
    if (! c_conn) {

	logoutput("accept_local_connection: connection denied for user %i:%i pid %i", (unsigned int) cred.uid, cred.gid, cred.pid);
	goto disconnect;

    }

    memmove(&c_conn->io.socket.sockaddr.local, &local, sizeof(struct sockaddr_un));
    c_conn->ops.client.server=s_conn;
    c_conn->io.socket.sops=sops; /* use the same socket ops */

    pthread_mutex_lock(&s_conn->ops.server.mutex);
    add_list_element_first(&s_conn->ops.server.header, &c_conn->list);
    pthread_mutex_unlock(&s_conn->ops.server.mutex);

    (* c_conn->ops.client.init)(c_conn, fd);
    return;

    disconnect:

    if (fd>0) close(fd);
    if (c_conn) free(c_conn);
    return;

}

static void accept_network_connection(int sfd, void *ptr, struct event_s *event)
{
    struct fs_connection_s *s_conn=(struct fs_connection_s *) ptr;
    struct socket_ops_s *sops=s_conn->io.socket.sops;
    struct fs_connection_s *c_conn=NULL;
    struct sockaddr saddr, *sockptr=NULL;
    struct host_address_s host;
    socklen_t slen=0;
    int fd=-1;
    unsigned int domain=0;
    char *hostname=NULL;
    struct generic_error_s error=GENERIC_ERROR_INIT;

    if (signal_is_error(event) || signal_is_close(event)) {

	logoutput("accept_network_connection: signal is error and/or close");
	goto disconnect;

    }

    logoutput("accept_network_connection: new socket fd = %i", sfd);

    domain=(get_connection_info(s_conn, "ipv6")==0) ? AF_INET6 : AF_INET;
    slen=(domain==AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    if (s_conn->io.socket.bevent) {
	struct bevent_s *bevent=s_conn->io.socket.bevent;

	fd=(* sops->accept)(get_bevent_unix_fd(bevent), &saddr, &slen);

    }

    if (fd==-1) goto disconnect;

    init_host_address(&host);
    hostname=get_connection_hostname(s_conn, fd, 1, &error);

    if (hostname) {
	unsigned int len=strlen(hostname);

	if (len >= sizeof(host.hostname)) len=sizeof(host.hostname) - 1;

	memcpy(host.hostname, hostname, len);
	host.hostname[len]='\0';

	free(hostname);
	host.flags|=HOST_ADDRESS_FLAG_HOSTNAME;

    }

    if (domain==AF_INET6) {
	char *ip=get_connection_ipv6(s_conn, fd, 1, &error);

	if (ip) {
	    unsigned int len=strlen(ip);

	    if (len >= sizeof(host.ip.ip.v6)) len=sizeof(host.ip.ip.v6) - 1;

	    memcpy(host.ip.ip.v6, ip, len);
	    host.ip.ip.v6[len]='\0';

	    free(ip);
	    host.flags|=HOST_ADDRESS_FLAG_IP;

	}

    } else {
	char *ip=get_connection_ipv4(s_conn, fd, 1, &error);

	if (ip) {
	    unsigned int len=strlen(ip);

	    if (len >= sizeof(host.ip.ip.v4)) len=sizeof(host.ip.ip.v4) - 1;

	    memcpy(host.ip.ip.v4, ip, len);
	    host.ip.ip.v4[len]='\0';

	    free(ip);
	    host.flags|=HOST_ADDRESS_FLAG_IP;

	}

    }

    if ((host.flags & (HOST_ADDRESS_FLAG_IP | HOST_ADDRESS_FLAG_HOSTNAME))==0) goto disconnect; /* not enough info */

    c_conn=(* s_conn->ops.server.accept.network)(&host, s_conn);
    if (! c_conn) goto disconnect;

    sockptr= ((domain==AF_INET6) ? (struct sockaddr *)&c_conn->io.socket.sockaddr.inet6 : (struct sockaddr *)&c_conn->io.socket.sockaddr.inet);
    memmove(sockptr, &saddr, slen);

    c_conn->ops.client.server=s_conn;
    c_conn->io.socket.sops=sops; /* use the same server socket ops */

    pthread_mutex_lock(&s_conn->ops.server.mutex);
    add_list_element_first(&s_conn->ops.server.header, &c_conn->list);
    pthread_mutex_unlock(&s_conn->ops.server.mutex);

    (* c_conn->ops.client.init)(c_conn, fd);
    return;

    disconnect:

    if (fd>0) close(fd);
    if (c_conn) free(c_conn);
    return;

}

int connect_socket(struct fs_connection_s *conn, const struct sockaddr *addr, int *len)
{
    struct bevent_s *bevent=conn->io.socket.bevent;
    return (* conn->io.socket.sops->connect)(&conn->io.socket, (struct sockaddr *) addr, len);
}

int close_socket(struct fs_connection_s *conn)
{
    return (* conn->io.socket.sops->close)(&conn->io.socket);
}

int create_local_serversocket(char *path, struct fs_connection_s *conn, struct beventloop_s *loop, struct fs_connection_s *(* accept_cb)(uid_t uid, gid_t gid, pid_t pid, struct fs_connection_s *s_conn), struct generic_error_s *error)
{
    struct socket_ops_s *sops=conn->io.socket.sops;
    int result=-1;
    int fd=0;
    int len=0;

    if (!conn) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    } else if (get_connection_info(conn, "local")==-1 || get_connection_info(conn, "server")==-1) {

	logoutput("create_local_serversocket: not a local server");

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    }

    if (! loop) loop=get_mainloop();
    if (! accept_cb) accept_cb=accept_local_cb_dummy;

    /* add socket */

    fd=(* sops->socket)(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (fd==-1) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
        goto out;

    }

    /* bind path/familiy and socket */

    memset(&conn->io.socket.sockaddr.local, 0, sizeof(struct sockaddr_un));
    conn->io.socket.sockaddr.local.sun_family=AF_UNIX;
    snprintf(conn->io.socket.sockaddr.local.sun_path, sizeof(conn->io.socket.sockaddr.local.sun_path), path);
    len=sizeof(struct sockaddr_un);

    if ((* sops->bind)(fd, (struct sockaddr *) &conn->io.socket.sockaddr.local, &len, 0)==-1) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	close(fd);
        goto out;

    }

    /* listen */

    if ((* sops->listen)(fd, LISTEN_BACKLOG)==-1 ) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	close(fd);

    } else {
	struct bevent_s *bevent=create_fd_bevent(loop, accept_local_connection, (void *) conn);

	if (bevent==NULL) goto out;

	set_bevent_unix_fd(bevent, fd);
	set_bevent_watch(bevent, "i");

	if (add_bevent_beventloop(bevent)==0) {

    	    logoutput("create_server_socket: socket fd %i added to eventloop", fd);
	    result=fd;
	    conn->ops.server.accept.local=accept_cb;
	    conn->io.socket.bevent=bevent;

	} else {

    	    logoutput("create_server_socket: error adding socket fd %i to eventloop.", fd);
	    set_generic_error_system(error, EIO, __PRETTY_FUNCTION__);
	    close(fd);

	}

    }

    out:
    return result;

}

int create_network_serversocket(char *address, unsigned int port, struct fs_connection_s *conn, struct beventloop_s *loop, struct fs_connection_s *(* accept_cb)(struct host_address_s *h, struct fs_connection_s *s), struct generic_error_s *error)
{
    struct socket_ops_s *sops=conn->io.socket.sops;
    int result=-1;
    int fd=-1;
    int len=0;
    unsigned int domain=AF_INET;
    unsigned int type=SOCK_STREAM | SOCK_NONBLOCK;
    struct sockaddr *saddr=NULL;

    if (!conn) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    } else if (get_connection_info(conn, "network")==-1 || get_connection_info(conn, "server")==-1) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    }

    if (! loop) loop=get_mainloop();
    if (! accept_cb) accept_cb=accept_network_cb_dummy;

    /* add socket */

    domain=(get_connection_info(conn, "ipv6")==0) ? AF_INET6 : AF_INET;

    if (domain==AF_INET6) {

	if (check_family_ip_address(address, "ipv6")==0) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	}

    } else if (domain==AF_INET) {

	if (check_family_ip_address(address, "ipv4")==0) {

	    set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	    goto out;

	}

    } else {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	goto out;

    }

    if (get_connection_info(conn, "udp")==0) {

	fd=(* sops->socket)(domain, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);

    } else if (get_connection_info(conn, "tcp")==0) {

	fd=(* sops->socket)(domain, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_UDP);

    }

    if (fd==-1) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
        goto out;

    }

    /* bind addr/familiy and socket */

    if (domain==AF_INET6) {
	struct sockaddr_in6 *sin6=&conn->io.socket.sockaddr.inet6;

	len=sizeof(struct sockaddr_in6);
	memset(sin6, 0, len);
	sin6->sin6_family=AF_INET6;
	sin6->sin6_port = htons(port);
	inet_pton(AF_INET6, address, &sin6->sin6_addr);
	saddr=(struct sockaddr *)sin6;

    } else {
	struct sockaddr_in *sin=&conn->io.socket.sockaddr.inet;

	len=sizeof(struct sockaddr_in);
	memset(sin, 0, len);
	sin->sin_family=AF_INET;
	sin->sin_port = htons(port);
	inet_pton(AF_INET, address, &sin->sin_addr);
	saddr=(struct sockaddr *) sin;

    }

    if ((* sops->bind)(fd, saddr, &len, 0)==-1) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	close(fd);
        goto out;

    }

    /* listen */

    if ((* sops->listen)(fd, LISTEN_BACKLOG)==-1 ) {

        set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	close(fd);

    } else {
	struct bevent_s *bevent=create_fd_bevent(loop, accept_network_connection, (void *) conn);

	if (bevent==NULL) goto out;

	set_bevent_unix_fd(bevent, fd);
	set_bevent_watch(bevent, "i");

	if (add_bevent_beventloop(bevent)==0) {

    	    logoutput("create_network_serversocket: socket fd %i added to eventloop", fd);
	    result=fd;
	    conn->ops.server.accept.network=accept_cb;
	    conn->io.socket.bevent=bevent;

	} else {

    	    logoutput("create_network_serversocket: error adding socket fd %i to eventloop.", fd);
	    set_generic_error_system(error, EIO, __PRETTY_FUNCTION__);
	    close(fd);

	}

    }

    out:
    return result;
}

struct fs_connection_s *get_containing_connection(struct list_element_s *list)
{
    return (struct fs_connection_s *) ( ((char *) list) - offsetof(struct fs_connection_s, list));
}
struct fs_connection_s *get_next_connection(struct fs_connection_s *s_conn, struct fs_connection_s *c_conn)
{
    struct list_element_s *list=(c_conn) ? get_next_element(&c_conn->list) : get_list_head(&s_conn->ops.server.header, 0);
    return (list) ? get_containing_connection(list) : NULL;
}
int lock_connection_list(struct fs_connection_s *s_conn)
{
    return pthread_mutex_lock(&s_conn->ops.server.mutex);
}
int unlock_connection_list(struct fs_connection_s *s_conn)
{
    return pthread_mutex_unlock(&s_conn->ops.server.mutex);
}

int compare_network_address(struct fs_connection_s *conn, char *address, unsigned int port)
{
    struct addrinfo h;
    struct addrinfo *list, *w;
    int result=-1;
    struct sockaddr *saddr=NULL;

    if (conn==NULL || address==NULL) return -1;

    memset(&h, 0, sizeof(struct addrinfo));

    h.ai_protocol=0;
    h.ai_flags=AI_CANONNAME;
    h.ai_family=AF_UNSPEC;
    h.ai_socktype = 0;
    h.ai_canonname=NULL;
    h.ai_addrlen=0;
    h.ai_addr=(struct sockaddr *) &conn->io.socket;
    h.ai_next=NULL;

    if (get_connection_info(conn, "ipv4")==0) {

	h.ai_family=AF_INET;
	h.ai_addr = (struct sockaddr *) &conn->io.socket.sockaddr.inet;
	h.ai_addrlen = sizeof(struct sockaddr_in);

    } else if (get_connection_info(conn, "ipv6")==0) {

	h.ai_family=AF_INET6;
	h.ai_addr = (struct sockaddr *) &conn->io.socket.sockaddr.inet6;
    	h.ai_addrlen = sizeof(struct sockaddr_in6);

    }

    if (get_connection_info(conn, "tcp")==0) {

	h.ai_socktype = SOCK_STREAM;

    } else if (get_connection_info(conn, "udp")==0) {

	h.ai_socktype = SOCK_DGRAM;

    }

    result=getaddrinfo(address, NULL, &h, &list);

    if (result>0) {

	logoutput("compare_network_address: error %s", gai_strerror(result));
	return -1;

    }

    result=-1;
    for (w=list; w != NULL; w = w->ai_next) {
	char host[NI_MAXHOST+1];

	/* try first hostname */

	memset(host, '\0', NI_MAXHOST+1);

	if (getnameinfo((struct sockaddr *) w->ai_addr, w->ai_addrlen, host, NI_MAXHOST, NULL, 0, 0)==0) {

	    if (strcmp(address, host)==0) {

		result=0;
		goto out;

	    }

	}

	if (check_family_ip_address(address, "ipv4")==1 && w->ai_addr->sa_family==AF_INET) {
	    struct sockaddr_in *s=(struct sockaddr_in *) w->ai_addr;
	    char *tmp=inet_ntoa(s->sin_addr);

	    if (strcmp(tmp, address)==0) {

		free(tmp);
		result=0;
		goto out;

	    }

	    free(tmp);

	}

	/* what to do with IPv6 ?*/

    }

    out:

    free(list);
    return result;

}

static int _compare_network_conn_ipv4(struct fs_connection_s *a, struct fs_connection_s *b)
{
    char hosta[NI_MAXHOST+1];
    char hostb[NI_MAXHOST+1];
    struct sockaddr *sa, *sb;
    int result=-1;

    memset(hosta, '\0', NI_MAXHOST+1);
    memset(hostb, '\0', NI_MAXHOST+1);

    sa=(struct sockaddr *) &a->io.socket.sockaddr.inet;
    sb=(struct sockaddr *) &b->io.socket.sockaddr.inet;

    if (getnameinfo(sa, sizeof(struct sockaddr_in), hosta, NI_MAXHOST, NULL, 0, 0)==0 &&
	getnameinfo(sb, sizeof(struct sockaddr_in), hostb, NI_MAXHOST, NULL, 0, 0)==0) {

	if (strcmp(hosta, hostb)==0) result=0;

    }

    return result;

}
static int _compare_network_conn_ipv6(struct fs_connection_s *a, struct fs_connection_s *b)
{
    char hosta[NI_MAXHOST+1];
    char hostb[NI_MAXHOST+1];
    struct sockaddr *sa, *sb;
    int result=-1;

    memset(hosta, '\0', NI_MAXHOST+1);
    memset(hostb, '\0', NI_MAXHOST+1);

    sa=(struct sockaddr *) &a->io.socket.sockaddr.inet6;
    sb=(struct sockaddr *) &b->io.socket.sockaddr.inet6;

    if (getnameinfo(sa, sizeof(struct sockaddr_in6), hosta, NI_MAXHOST, NULL, 0, 0)==0 &&
	getnameinfo(sb, sizeof(struct sockaddr_in6), hostb, NI_MAXHOST, NULL, 0, 0)==0) {

	if (strcmp(hosta, hostb)==0) result=0;

    }

    return result;

}
static int _compare_network_conn_ipv4ipv6(struct fs_connection_s *a, struct fs_connection_s *b)
{
    char hosta[NI_MAXHOST+1];
    char hostb[NI_MAXHOST+1];
    struct sockaddr *sa, *sb;
    int result=-1;

    memset(hosta, '\0', NI_MAXHOST+1);
    memset(hostb, '\0', NI_MAXHOST+1);

    sa=(struct sockaddr *) &a->io.socket.sockaddr.inet;
    sb=(struct sockaddr *) &b->io.socket.sockaddr.inet6;

    if (getnameinfo(sa, sizeof(struct sockaddr_in), hosta, NI_MAXHOST, NULL, 0, 0)==0 &&
	getnameinfo(sb, sizeof(struct sockaddr_in6), hostb, NI_MAXHOST, NULL, 0, 0)==0) {

	if (strcmp(hosta, hostb)==0) result=0;

    }

    return result;

}
int compare_network_connection(struct fs_connection_s *a, struct fs_connection_s *b, unsigned int flags)
{

    if (flags & FS_CONNECTION_COMPARE_HOST) {

	if (get_connection_info(a, "ipv4")==0 && get_connection_info(b, "ipv4")==0) {

	    return _compare_network_conn_ipv4(a, b);

	} else if (get_connection_info(a, "ipv6")==0 && get_connection_info(b, "ipv6")==0) {

	    return _compare_network_conn_ipv6(a, b);

	} else if (get_connection_info(a, "ipv4")==0 && get_connection_info(b, "ipv6")==0) {

	    return _compare_network_conn_ipv4ipv6(a, b);

	} else if (get_connection_info(a, "ipv6")==0 && get_connection_info(b, "ipv4")==0) {

	    return _compare_network_conn_ipv4ipv6(b, a);

	}

    }

    return -1;

}

int get_connection_info(struct fs_connection_s *a, const char *what)
{

    if (strcmp(what, "ipv4")==0) {

	return (a->type==FS_CONNECTION_TYPE_TCP4 || a->type==FS_CONNECTION_TYPE_UDP4) ? 0 : -1;

    } else if (strcmp(what, "ipv6")==0) {

	return (a->type==FS_CONNECTION_TYPE_TCP6 || a->type==FS_CONNECTION_TYPE_UDP6) ? 0 : -1;

    } else if (strcmp(what, "network")==0) {

	return (a->type==FS_CONNECTION_TYPE_TCP4 || a->type==FS_CONNECTION_TYPE_UDP4 || a->type==FS_CONNECTION_TYPE_TCP6 || a->type==FS_CONNECTION_TYPE_UDP6) ? 0 : -1;

    } else if (strcmp(what, "tcp")==0) {

	return (a->type==FS_CONNECTION_TYPE_TCP4 || a->type==FS_CONNECTION_TYPE_TCP6) ? 0 : -1;

    } else if (strcmp(what, "udp")==0) {

	return (a->type==FS_CONNECTION_TYPE_UDP4 || a->type==FS_CONNECTION_TYPE_UDP6) ? 0 : -1;

    } else if (strcmp(what, "local")==0) {

	return (a->type==FS_CONNECTION_TYPE_LOCAL ) ? 0 : -1;

    } else if (strcmp(what, "client")==0) {

	return (a->role==FS_CONNECTION_ROLE_CLIENT ) ? 0 : -1;

    } else if (strcmp(what, "server")==0) {

	return (a->role==FS_CONNECTION_ROLE_SERVER ) ? 0 : -1;

    }

    return -1;

}

/* get ipv4 as char * of connection
    - what = 0 -> local
    - what = 1 -> remote */

char *get_connection_ipv4(struct fs_connection_s *conn, int fd, unsigned char what, struct generic_error_s *error)
{
    struct socket_ops_s *sops=conn->io.socket.sops;
    struct sockaddr_in addr;
    socklen_t len=sizeof(struct sockaddr_in);
    char *ipv4=NULL;
    char buffer[INET_ADDRSTRLEN + 1];

    if (fd<0) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	return NULL;

    }

    memset(buffer, '\0', INET_ADDRSTRLEN + 1);

    if (what==0) {

	if ((* sops->getsockname)(fd, (struct sockaddr *) &addr, &len)==-1) {

	    set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	    return NULL;

	}

    } else {

	if ((* sops->getpeername)(fd, (struct sockaddr *) &addr, &len)==-1) {

	    set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	    return NULL;

	}

    }

    if (inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer))) {

	ipv4=strdup(buffer);
	if (ipv4==NULL) set_generic_error_system(error, ENOMEM, __PRETTY_FUNCTION__);;

    }

    return ipv4;

}

/* get ipv6 as char * of connection
    - what = 0 -> local
    - what = 1 -> remote */

char *get_connection_ipv6(struct fs_connection_s *conn, int fd, unsigned char what, struct generic_error_s *error)
{
    struct socket_ops_s *sops=conn->io.socket.sops;
    struct sockaddr_in6 addr;
    socklen_t len=sizeof(struct sockaddr_in6);
    char *ipv6=NULL;
    char buffer[INET6_ADDRSTRLEN + 1];

    if (fd<0) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	return NULL;

    }

    memset(buffer, '\0', INET6_ADDRSTRLEN + 1);

    if (what==0) {

	if ((* sops->getsockname)(fd, (struct sockaddr *) &addr, &len)==-1) {

	    set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	    return NULL;

	}

    } else {

	if ((* sops->getpeername)(fd, (struct sockaddr *) &addr, &len)==-1) {

	    set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	    return NULL;

	}

    }

    if (inet_ntop(AF_INET6, &addr.sin6_addr, buffer, sizeof(buffer))) {

	ipv6=strdup(buffer);
	if (ipv6==NULL) set_generic_error_system(error, ENOMEM, __PRETTY_FUNCTION__);

    }

    return ipv6;

}

/* get hostname of connection
    - what = 0 -> local
    - what = 1 -> remote
*/

char *get_connection_hostname(struct fs_connection_s *conn, int fd, unsigned char what, struct generic_error_s *error)
{
    struct socket_ops_s *sops=conn->io.socket.sops;
    struct sockaddr addr;
    socklen_t len = sizeof(struct sockaddr);
    int result = 0;
    char tmp[NI_MAXHOST];
    unsigned int count = 0;

    if (fd<0) {

	set_generic_error_system(error, EINVAL, __PRETTY_FUNCTION__);
	return NULL;

    }

    if (what==0) {

	logoutput("get_connection_hostname: fd=%i local name", fd);

	if ((* sops->getsockname)(fd, &addr, &len)==-1) {

	    set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	    return NULL;

	}

    } else {

	logoutput("get_connection_hostname: fd=%i remote name", fd);

	if ((* sops->getpeername)(fd, &addr, &len)==-1) {

	    set_generic_error_system(error, errno, __PRETTY_FUNCTION__);
	    return NULL;

	}

    }

    return get_socket_addr_hostname(&addr, len, error);

}

unsigned int get_status_socket_connection(struct fs_connection_s *sc)
{
    int error=0;
    int fd=-1;

    if (sc->io.socket.bevent) fd=get_bevent_unix_fd(sc->io.socket.bevent);

    if (fd>=0) {
	socklen_t len=sizeof(error);
	struct socket_ops_s *sops=sc->io.socket.sops;

	if ((* sops->getsockopt)(fd, SOL_SOCKET, SO_ERROR, (void *) &error, &len)==0) {

	    logoutput("get_status_socket_connection: got error %i (%s)", error, strerror(error));

	} else {

	    error=errno;
	    logoutput("get_status_socket_connection: error %i (%s)", errno, strerror(errno));

	}

    } else {

	error=ENOTCONN;

    }

    return (unsigned int) abs(error);

}
