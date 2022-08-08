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

#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/uio.h>

#include "libosns-network.h"
#include "lib/system/path.h"
#include "lib/system/stat.h"
#include "lib/system/location.h"

#define SYSTEM_SOCKET_TYPE_CONNECTION			1
#define SYSTEM_SOCKET_TYPE_SYSTEM			2
#define SYSTEM_SOCKET_TYPE_FILESYSTEM			3

#define SYSTEM_SOCKET_TYPE_MASK				3

/* additional type info for CONNECTION */

#define SYSTEM_SOCKET_TYPE_LOCAL			(1 << 7)
#define SYSTEM_SOCKET_TYPE_UNIX				SYSTEM_SOCKET_TYPE_LOCAL
#define SYSTEM_SOCKET_TYPE_NET				(1 << 8)

/*additional type info for system */

#define SYSTEM_SOCKET_TYPE_CHAR_DEVICE			(1 << 15)
#define SYSTEM_SOCKET_TYPE_BLOCK_DEVICE			(1 << 16)
/* 20220408: flag for system devices like signalfd, timerfd and fsnotify */
#define SYSTEM_SOCKET_TYPE_SYSTEM_DEVICE		(1 << 17)
/* 20220315: flag for special files like under Linux /proc/self/mountinfo */
#define SYSTEM_SOCKET_TYPE_SYSTEM_FILE			(1 << 18)

/* additional type info for FILESYSTEM */

#define SYSTEM_SOCKET_TYPE_FILE				(1 << 26)
#define SYSTEM_SOCKET_TYPE_DIR				(1 << 27)

/* flags */

#define SYSTEM_SOCKET_FLAG_IPv4				(1 << 0)
#define SYSTEM_SOCKET_FLAG_IPv6				(1 << 1)
#define SYSTEM_SOCKET_FLAG_UDP				(1 << 2)

#define SYSTEM_SOCKET_FLAG_ENDPOINT			(1 << 6)
#define SYSTEM_SOCKET_FLAG_SERVER			(1 << 7)

#define SYSTEM_SOCKET_FLAG_POLLABLE			(1 << 10)
#define SYSTEM_SOCKET_FLAG_RDWR				(1 << 11)
#define SYSTEM_SOCKET_FLAG_WRONLY			(1 << 12)

#define SYSTEM_SOCKET_FLAG_NOOPEN			(1 << 20)

#define SOCKET_STATUS_INIT				1

#define SOCKET_STATUS_ACCEPT				2
#define SOCKET_STATUS_OPEN				2

#define SOCKET_STATUS_CONNECT				4

#define SOCKET_STATUS_BIND				8
#define SOCKET_STATUS_LISTEN				16

union system_sockaddr_u {
#ifdef __linux__
    struct sockaddr_un					local;
    struct sockaddr_in					net4;
    struct sockaddr_in6					net6;
#endif
    char						buffer[132];
};

struct system_sockaddr_s {
    union system_sockaddr_u				data;
    struct sockaddr					*addr;
    socklen_t						len;
};

struct system_socket_s;

struct local_peer_s {
    uid_t						uid;
    gid_t						gid;
    pid_t						pid;
};

struct network_peer_s {
    struct host_address_s 				host;
    struct network_port_s				port;
};

union _generic_socket_sops_u {
    struct _generic_connection_sops_s {
	    int						(* connect)(struct system_socket_s *s);
	    int						(* recv)(struct system_socket_s *s, char *buffer, unsigned int size, unsigned int flags);
	    int						(* send)(struct system_socket_s *s, char *buffer, unsigned int size, unsigned int flags);
	    int						(* writev)(struct system_socket_s *s, struct iovec *iov, unsigned int count);
	    int						(* readv)(struct system_socket_s *s, struct iovec *iov, unsigned int count);
	    int						(* sendmsg)(struct system_socket_s *s, const struct msghdr *msg);
	    int						(* recvmsg)(struct system_socket_s *s, struct msghdr *msg);
    } connection;
    struct _generic_endpoint_sops_s {
	    struct system_socket_s 			*(* accept)(struct system_socket_s *server, struct system_socket_s *client, int flags);
	    int						(* bind)(struct system_socket_s *s);
	    int						(* listen)(struct system_socket_s *s, int len);
    } endpoint;
};

struct _generic_system_sops_s {
    int							(* read)(struct system_socket_s *s, char *buffer, unsigned int size);
    int							(* write)(struct system_socket_s *s, char *buffer, unsigned int size);
    int							(* writev)(struct system_socket_s *s, struct iovec *iov, unsigned int count);
    int							(* readv)(struct system_socket_s *s, struct iovec *iov, unsigned int count);
};

// #define FS_DENTRY_FLAG_FILE				1
// #define FS_DENTRY_FLAG_DIR				2
// #define FS_DENTRY_FLAG_EOD				4

// struct fs_dentry_s {
//    uint16_t						flags;
//    uint32_t						type;
//    uint64_t						ino; /* ino on the server; some filesystems rely on this */
//    uint16_t						len;
//    char						*name;
//};

//#define FS_DENTRY_INIT					{0, 0, 0, 0, NULL}

/*union _generic_filesystem_sops_u {
    struct file_sops_s {
	    int					(* open)(struct system_socket_s *s, struct fs_location_path_s *path, unsigned int flags);
    	    int					(* create)(struct system_socket_s *s, struct fs_location_path_s *path, struct fs_init_s *init, unsigned int flags);
	    int					(* pread)(struct system_socket_s *s, char *buffer, unsigned int size, off_t off);
	    int					(* pwrite)(struct system_socket_s *s, char *buffer, unsigned int size, off_t off);
	    int					(* fsync)(struct system_socket_s *s);
	    int					(* fdatasync)(struct system_socket_s *s);
	    int					(* flush)(struct system_socket_s *s, unsigned int flags);
	    off_t				(* lseek)(struct system_socket_s *s, off_t off, int whence);
	    int					(* fgetstat)(struct system_socket_s *s, unsigned int mask, struct system_stat_s *st);
	    int					(* fsetstat)(struct system_socket_s *s, unsigned int mask, struct system_stat_s *st);
    } file;
    struct dir_sops_s {
	    int					(* open)(struct system_socket_s *s, struct fs_location_s *l, unsigned int flags);
	    struct fs_dentry_s			*(* get_dentry)(struct system_socket_s *s, unsigned char next);
	    int					(* fstatat)(struct system_socket_s *s, char *name, unsigned int mask, struct system_stat_s *st);
	    int					(* unlinkat)(struct system_socket_s *s, const char *name);
	    int					(* rmdirat)(struct system_socket_s *s, const char *name);
	    int					(* fsyncdir)(struct system_socket_s *s, unsigned int flags);
	    ssize_t				(* readlinkat)(struct system_socket_s *s, const char *name, struct fs_location_path_s *target);
    } dir;
};*/

struct generic_socket_option_s {
    int						level;
    int						type;
    char					*value;
    unsigned int				len;
};

struct system_socket_sops_s {
    int						(* getsockopt)(struct system_socket_s *sock, struct generic_socket_option_s *option);
    int						(* setsockopt)(struct system_socket_s *sock, struct generic_socket_option_s *option);
    void					(* close)(struct system_socket_s *sock);
    int						(* get_unix_fd)(struct system_socket_s *sock);
    void					(* set_unix_fd)(struct system_socket_s *sock, int fd);
    union _sops_role_u {
	struct _generic_system_sops_s		system;
	union _generic_socket_sops_u		socket;
//	union _generic_filesystem_sops_u	filesystem;
    } type;
};

#define SOCKET_BACKEND_ACTION_OPEN		1
#define SOCKET_BACKEND_ACTION_CLOSE		2

struct system_socket_backend_s {
#ifdef __linux__
    int						fd;
    pid_t					pid;
#endif
    void					(* cb_change)(struct system_socket_s *sock, unsigned char action);
    void					*ptr;
    /* union _socket_data_u {
	struct dir_data_s {
	    union _type_dir_data_u {
		struct _getdents_s {
		    char			*buffer;
		    unsigned int		size;
		    unsigned int		read;
		    unsigned int		pos;
		    struct fs_dentry_s		dentry;
		} getdents;
	    } data;
	} dir;
    } type; */
};

#define SOCKET_EVENT_TYPE_BEVENT		1

struct system_socket_event_s {
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

struct system_socket_s {
    unsigned int					type;
    unsigned int					flags;
    unsigned int					status;
    struct system_sockaddr_s				sockaddr;
    struct system_socket_event_s			event;
    struct system_socket_backend_s			backend;
    struct system_socket_sops_s				sops;
};

/* Prototypes */

void init_system_socket(struct system_socket_s *sock, unsigned int type, unsigned int flags, struct fs_location_path_s *path);
char *get_name_system_socket(struct system_socket_s *sock, const char *what);
unsigned int get_type_system_socket(char *type, char *subtype);

#endif
