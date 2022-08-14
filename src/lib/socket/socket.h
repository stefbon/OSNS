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

#ifndef LIB_SOCKET_SOCKET_H
#define LIB_SOCKET_SOCKET_H

#include "libosns-basic-system-headers.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/uio.h>

#include "libosns-network.h"
#include "lib/system/path.h"
#include "lib/system/stat.h"

/*

    There are different types of sockets:

    a. network sockets connected to an ip address and port. Clients can connect to it to get a connection via the accept/connect system calls.
	Well known sockets like a webserver (HTTP and HTTPS), a SSH server (SSH), mailserver (SMTP) are of this type. These are called endpoints.
	See man socket, man bind, man listen, man accept.
	The endpoint is required to setup the connection, is not the connection itself.
	The connection is socket/transport which will take care of the actual transport of data the server provides and the client wants.
	See man connect.

    b. local (or unix) sockets. Simular to a. network sockets, bu then attached to an file (a socket) in the filesystem in stead of a ip address. Usefull
	for local communication between processes.

    c. system sockets are sockets provided by the operating system like:
	- timerfd, signalfd, pidfd and fsnotify (under Linux)

    d. device sockets are character and block devices (mostly located in /dev under Linux)
	- character devices like /dev/fuse (under Linux)

    e. filesystem sockets used to read and/or write to files.
	See man open, man read, man write
	- special files like /proc/self/mountinfo (=> to monitor mounts) (under Linux)
*/

#define OSNS_SOCKET_TYPE_CONNECTION			1
#define OSNS_SOCKET_TYPE_DEVICE				2
#define OSNS_SOCKET_TYPE_FILESYSTEM			3
#define OSNS_SOCKET_TYPE_SYSTEM				4

/* flags info for CONNECTION */

#define OSNS_SOCKET_FLAG_LOCAL				(1 << 0)
#define OSNS_SOCKET_FLAG_UNIX				OSNS_SOCKET_TYPE_LOCAL
#define OSNS_SOCKET_FLAG_NET				(1 << 1)

/*additional info for DEVICE */

#define OSNS_SOCKET_FLAG_CHAR_DEVICE			(1 << 2)
#define OSNS_SOCKET_FLAG_BLOCK_DEVICE			(1 << 3)

/* additional info for FILESYSTEM */

#define OSNS_SOCKET_FLAG_FILE				(1 << 4)
#define OSNS_SOCKET_FLAG_DIR				(1 << 5)

/* additional flags for SYSTEM */

#define OSNS_SOCKET_FLAG_FSNOTIFY			(1 << 6)
#define OSNS_SOCKET_FLAG_TIMERFD			(1 << 7)
#define OSNS_SOCKET_FLAG_SIGNALFD			(1 << 8)

/* additional */

#define OSNS_SOCKET_FLAG_IPv4				(1 << 20)
#define OSNS_SOCKET_FLAG_IPv6				(1 << 21)
#define OSNS_SOCKET_FLAG_UDP				(1 << 22)
#define OSNS_SOCKET_FLAG_ENDPOINT			(1 << 23)
#define OSNS_SOCKET_FLAG_SERVER				(1 << 24)
#define OSNS_SOCKET_FLAG_RDWR				(1 << 25)
#define OSNS_SOCKET_FLAG_WRONLY				(1 << 26)
#define OSNS_SOCKET_FLAG_ALLOC				(1 << 27)

#define SOCKET_STATUS_INIT				1
#define SOCKET_STATUS_ACCEPT				2
#define SOCKET_STATUS_OPEN				2
#define SOCKET_STATUS_CONNECT				4
#define SOCKET_STATUS_BIND				8
#define SOCKET_STATUS_LISTEN				16

#define SOCKET_CHANGE_OP_OPEN				1
#define SOCKET_CHANGE_OP_CLOSE				2
#define SOCKET_CHANGE_OP_SET				3

#define OSNS_SOCKET_FSYNC_FLAG_DATA			1

#define OSNS_SOCKET_DIRECTORY_FLAG_EOD			1
#define OSNS_SOCKET_DIRECTORY_FLAG_ERROR		2

union osns_sockaddr_u {
#ifdef __linux__
    struct sockaddr_un					local;
    struct sockaddr_in					net4;
    struct sockaddr_in6					net6;
#endif
    char						buffer[132];
};

struct osns_sockaddr_s {
    union osns_sockaddr_u				data;
    struct sockaddr					*addr;
    socklen_t						len;
};

struct osns_socket_s;

struct local_peer_s {
    uid_t						uid;
    gid_t						gid;
    pid_t						pid;
};

struct network_peer_s {
    struct host_address_s 				host;
    struct network_port_s				port;
};

struct _connection_sops_s {
    int							(* open)(struct osns_socket_s *s);
    int							(* connect)(struct osns_socket_s *s);
    int							(* recv)(struct osns_socket_s *s, char *buffer, unsigned int size, unsigned int flags);
    int							(* send)(struct osns_socket_s *s, char *buffer, unsigned int size, unsigned int flags);
    int							(* writev)(struct osns_socket_s *s, struct iovec *iov, unsigned int count);
    int							(* readv)(struct osns_socket_s *s, struct iovec *iov, unsigned int count);
    int							(* sendmsg)(struct osns_socket_s *s, const struct msghdr *msg);
    int							(* recvmsg)(struct osns_socket_s *s, struct msghdr *msg);
};

struct _endpoint_sops_s {
    int							(* open)(struct osns_socket_s *s);
    struct osns_socket_s 				*(* accept)(struct osns_socket_s *server, struct osns_socket_s *client, int flags);
    int							(* bind)(struct osns_socket_s *s);
    int							(* listen)(struct osns_socket_s *s, int len);
};

struct _device_sops_s {
    int							(* open)(struct osns_socket_s *s, struct fs_location_path_s *path);
    int							(* read)(struct osns_socket_s *s, char *buffer, unsigned int size);
    int							(* write)(struct osns_socket_s *s, char *buffer, unsigned int size);
    int							(* writev)(struct osns_socket_s *s, struct iovec *iov, unsigned int count);
    int							(* readv)(struct osns_socket_s *s, struct iovec *iov, unsigned int count);
};

struct fs_init_s {
#ifdef __linux__
    mode_t						mode;
#endif
};

#define FS_DENTRY_FLAG_FILE				1
#define FS_DENTRY_FLAG_DIR				2
#define FS_DENTRY_FLAG_EOD				4

struct fs_dentry_s {
    uint16_t						flags;
    uint32_t						type;
    uint64_t						ino; /* ino on the server; some filesystems rely on this */
    uint16_t						len;
    char						*name;
};

#define FS_DENTRY_INIT					{0, 0, 0, 0, NULL}

union _filesystem_sops_u {
    struct file_sops_s {
	    int					(* open)(struct osns_socket_s *ref, struct fs_location_path_s *path, struct osns_socket_s *s, struct fs_init_s *init, unsigned int flags);
	    int					(* pread)(struct osns_socket_s *s, char *buffer, unsigned int size, off_t off);
	    int					(* pwrite)(struct osns_socket_s *s, char *buffer, unsigned int size, off_t off);
	    int					(* fsync)(struct osns_socket_s *s, unsigned int flags);
	    int					(* flush)(struct osns_socket_s *s, unsigned int flags);
	    off_t				(* lseek)(struct osns_socket_s *s, off_t off, int whence);
	    int					(* fgetstat)(struct osns_socket_s *s, unsigned int mask, struct system_stat_s *st);
	    int					(* fsetstat)(struct osns_socket_s *s, unsigned int mask, struct system_stat_s *st);
    } file;
    struct dir_sops_s {
	    int					(* open)(struct osns_socket_s *ref, struct fs_location_path_s *l, struct osns_socket_s *s, struct fs_init_s *init, unsigned int flags);
	    struct fs_dentry_s			*(* get_dentry)(struct osns_socket_s *s, unsigned char next);
	    int					(* fstatat)(struct osns_socket_s *s, char *name, unsigned int mask, struct system_stat_s *st, unsigned int flags);
	    int					(* unlinkat)(struct osns_socket_s *s, const char *name, unsigned int flags);
	    int					(* rmdirat)(struct osns_socket_s *s, const char *name, unsigned int flags);
	    int					(* fsyncdir)(struct osns_socket_s *s, unsigned int flags);
	    int					(* readlinkat)(struct osns_socket_s *s, const char *name, struct fs_location_path_s *target);
    } dir;
};

struct generic_socket_option_s {
    int						level;
    int						type;
    char					*value;
    unsigned int				len;
};

#define SOCKET_EVENT_TYPE_BEVENT		1

struct osns_socket_event_s {
    unsigned char				type;
    union _event_ctx_u {
	struct bevent_s				*bevent;
	void					*ptr;
    } link;
};

#ifdef __linux__

#define SOCKET_PROPERTY_FLAG_OWNER		1
#define SOCKET_PROPERTY_FLAG_GROUP		2
#define SOCKET_PROPERTY_FLAG_MODE		4

struct socket_properties_s {
    unsigned int				valid;
    char					*owner;
    char					*group;
    mode_t					mode;
};

#else

struct socket_properties_s {
};

#endif

struct osns_socket_s {
    unsigned int					type;
    unsigned int					flags;
    unsigned int					status;
#ifdef __linux__
    int							fd;
    unsigned int					pid;
#endif
    int							(* getsockopt)(struct osns_socket_s *sock, struct generic_socket_option_s *option);
    int							(* setsockopt)(struct osns_socket_s *sock, struct generic_socket_option_s *option);
    void						(* close)(struct osns_socket_s *sock);
    void						(* change)(struct osns_socket_s *sock, unsigned int what);
#ifdef __linux__
    int							(* get_unix_fd)(struct osns_socket_s *sock);
    void						(* set_unix_fd)(struct osns_socket_s *sock, int fd);
#endif
    union osns_socket_ops_u {
	struct _connection_sops_s			connection;
	struct _endpoint_sops_s				endpoint;
	struct _device_sops_s				device;
	union _filesystem_sops_u			filesystem;
    } sops;
    struct osns_socket_event_s 				event;
    union osns_socket_data_u {
	struct _socket_connection_data_s {
	    struct osns_sockaddr_s			sockaddr;
	} connection;
	struct _socket_directory_data_s {
	    unsigned int				flags;
	    char					*buffer;
	    unsigned int				size;
	    unsigned int				read;
	    unsigned int				pos;
	    struct fs_dentry_s				dentry;
	} directory;
	char						buffer[256];
    } data;
};

/* Prototypes */

void init_osns_socket(struct osns_socket_s *sock, unsigned int type, unsigned int flags);

#endif
