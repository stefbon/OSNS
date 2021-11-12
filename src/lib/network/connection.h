/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_NETWORK_CONNECTION_H
#define _LIB_NETWORK_CONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "eventloop.h"
#include "list.h"
#include "misc.h"
#include "network.h"
#include "error.h"

#define FS_CONNECTION_ROLE_SERVER					1
#define FS_CONNECTION_ROLE_CLIENT					2

#define LISTEN_BACKLOG 50

#define FS_CONNECTION_TYPE_LOCAL					1
#define FS_CONNECTION_TYPE_TCP4						2
#define FS_CONNECTION_TYPE_TCP6						3
#define FS_CONNECTION_TYPE_UDP4						4
#define FS_CONNECTION_TYPE_UDP6						5
#define FS_CONNECTION_TYPE_FUSE						6
#define FS_CONNECTION_TYPE_STD						7

#define SOCKET_OPS_TYPE_ZERO						0
#define SOCKET_OPS_TYPE_DEFAULT						1

#define FUSE_OPS_TYPE_ZERO						0
#define FUSE_OPS_TYPE_DEFAULT						1

#define STD_OPS_TYPE_ZERO						0
#define STD_OPS_TYPE_DEFAULT						1

#define STD_SOCKET_TYPE_STDIN						1
#define STD_SOCKET_TYPE_STDOUT						2
#define STD_SOCKET_TYPE_STDERR						3

#define FS_CONNECTION_FLAG_INIT						(1 << 0)
#define FS_CONNECTION_FLAG_CONNECTING					(1 << 1)
#define FS_CONNECTION_FLAG_CONNECTED					(1 << 2)
#define FS_CONNECTION_FLAG_EVENTLOOP					(1 << 3)
#define FS_CONNECTION_FLAG_DISCONNECTING				(1 << 4)
#define FS_CONNECTION_FLAG_DISCONNECTED					(1 << 5)
#define FS_CONNECTION_FLAG_DISCONNECT					( FS_CONNECTION_FLAG_DISCONNECTING | FS_CONNECTION_FLAG_DISCONNECTED )

#define FS_CONNECTION_FLAG_WRITE					(1 << 6)
#define FS_CONNECTION_FLAG_WAITING					(1 << 7)
#define FS_CONNECTION_FLAG_WRITABLE					(1 << 8)

#define FS_CONNECTION_COMPARE_HOST					1

struct fs_connection_s;

typedef void (* event_cb)(struct fs_connection_s *conn, void *data, uint32_t events);
typedef struct fs_connection_s *(* accept_local_cb)(uid_t uid, gid_t gid, pid_t pid, struct fs_connection_s *conn);
typedef struct fs_connection_s *(* accept_network_cb)(struct host_address_s *host, struct fs_connection_s *conn);
typedef void (* disconnect_cb)(struct fs_connection_s *conn, unsigned char remote);
typedef void (* init_cb)(struct fs_connection_s *conn, unsigned int fd);

struct io_socket_s {
    unsigned char				type;
    struct socket_ops_s				*sops;
    struct bevent_s				*bevent;
    union {
	struct sockaddr_un 			local;
	struct sockaddr_in			inet;
	struct sockaddr_in6			inet6;
    } sockaddr;
};

struct socket_ops_s {
    unsigned char				type;
    int						(* accept)(int fd, struct sockaddr *addr, unsigned int *len);
    int						(* bind)(int fd, struct sockaddr *addr, int *len, int sock);
    int						(* close)(struct io_socket_s *s);
    int						(* connect)(struct io_socket_s *s, struct sockaddr *addr, int *len);
    int						(* getpeername)(int fd, struct sockaddr *addr, unsigned int *len);
    int						(* getsockname)(int fd, struct sockaddr *addr, unsigned int *len);
    int						(* getsockopt)(int fd, int level, int optname, char *optval, unsigned int *optlen);
    int						(* setsockopt)(int fd, int level, int optname, char *optval, unsigned int optlen);
    int						(* listen)(int fd, int len);
    int						(* socket)(int af, int type, int protocol);
    int						(* recv)(struct io_socket_s *s, char *buffer, unsigned int size, unsigned int flags);
    int						(* send)(struct io_socket_s *s, char *buffer, unsigned int size, unsigned int flags);
    int						(* start)();
    int						(* finish)();
};

struct io_fuse_s {
    struct fuse_ops_s				*fops;
    struct bevent_s				*bevent;
};

struct fuse_ops_s {
    unsigned char				type;
    int						(* open)(char *path, unsigned int flags);
    int						(* close)(struct io_fuse_s *s);
    ssize_t					(* writev)(struct io_fuse_s *s, struct iovec *iov, int count);
    int						(* read)(struct io_fuse_s *s, void *buffer, size_t size);
};

struct io_std_s {
    unsigned char				type;
    struct std_ops_s				*sops;
    struct bevent_s				*bevent;
};

struct std_ops_s {
    unsigned char				type;
    int						(* open)(struct io_std_s *s, unsigned int flags);
    int						(* close)(struct io_std_s *s);
    int						(* read)(struct io_std_s *s, char *buffer, unsigned int size);
    int						(* write)(struct io_std_s *s, char *data, unsigned int size);
};

struct fs_connection_s {
    unsigned char 				type;
    unsigned char				role;
    unsigned char				status;
    unsigned int				error;
    unsigned int				expire;
    void 					*data;
    struct list_element_s			list;
    union io_target_s {
	struct io_socket_s			socket;
	struct io_fuse_s			fuse;
	struct io_std_s				std;
    } io;
    union {
	struct server_ops_s {
	    union {
		accept_local_cb			local;
		accept_network_cb		network;
	    } accept;
	    struct list_header_s		header;
	    pthread_mutex_t			mutex;
	} server;
	struct client_ops_s {
	    union {
		struct local_client_s {
		    uid_t			uid;
		    pid_t			pid;
		} local;
		struct fuse_client_s {
		    uid_t			uid;
		    gid_t			gid;
		} fuse;
		struct host_address_s 		host;
	    } id;
	    disconnect_cb			disconnect;
	    event_cb 				event;
	    init_cb				init;
	    struct fs_connection_s		*server;
	} client;
    } ops;
};

/* Prototypes */

void set_io_socket_ops_zero(struct io_socket_s *s);
void set_io_socket_ops_default(struct io_socket_s *s);
void set_io_std_ops_zero(struct io_std_s *s);
void set_io_std_ops_default(struct io_std_s *s);
void set_io_fuse_ops_zero(struct io_fuse_s *s);
void set_io_fuse_ops_default(struct io_fuse_s *s);

void set_io_std_type(struct fs_connection_s *c, const char *what);

void init_connection(struct fs_connection_s *connection, unsigned char type, unsigned char role);
void free_connection(struct fs_connection_s *c);
int create_local_serversocket(char *path, struct fs_connection_s *conn, struct beventloop_s *loop, struct fs_connection_s *(* accept_cb)(uid_t uid, gid_t gid, pid_t pid, struct fs_connection_s *s_conn), struct generic_error_s *error);
int create_network_serversocket(char *address, unsigned int port, struct fs_connection_s *conn, struct beventloop_s *loop, struct fs_connection_s *(* accept_cb)(struct host_address_s *h, struct fs_connection_s *s), struct generic_error_s *error);

int connect_socket(struct fs_connection_s *conn, const struct sockaddr *addr, int *len);
int close_socket(struct fs_connection_s *conn);

struct fs_connection_s *get_containing_connection(struct list_element_s	*list);
struct fs_connection_s *get_next_connection(struct fs_connection_s *s_conn, struct fs_connection_s *c_conn);

int lock_connection_list(struct fs_connection_s *s_conn);
int unlock_connection_list(struct fs_connection_s *s_conn);

int compare_network_address(struct fs_connection_s *conn, char *address, unsigned int port);
int compare_network_connection(struct fs_connection_s *a, struct fs_connection_s *b, unsigned int flags);

int get_connection_info(struct fs_connection_s *a, const char *what);

char *get_connection_ipv4(struct fs_connection_s *a, int fd, unsigned char what, struct generic_error_s *error);
char *get_connection_ipv6(struct fs_connection_s *a, int fd, unsigned char what, struct generic_error_s *error);
char *get_connection_hostname(struct fs_connection_s *a, int fd, unsigned char what, struct generic_error_s *error);

unsigned int get_status_socket_connection(struct fs_connection_s *sc);

#endif
