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

#ifndef OSNS_SFTP_SUBSYSTEM_H
#define OSNS_SFTP_SUBSYSTEM_H

#include <pwd.h>
#include "network.h"
#include "users.h"
#include "sftp/attr-context.h"

#define SFTP_PROTOCOL_VERSION_DEFAULT		6

struct sftp_subsystem_s;
struct sftp_payload_s;

typedef void (* sftp_cb_t)(struct sftp_payload_s *p);

struct sftp_time_s {
    int64_t					sec;
    uint32_t					nsec;
};

#define SFTP_RECEIVE_STATUS_INIT			(1 << 0)
#define SFTP_RECEIVE_STATUS_PACKET			(1 << 10)
#define SFTP_RECEIVE_STATUS_WAITING1			(1 << 11)
#define SFTP_RECEIVE_STATUS_WAITING2			(1 << 12)
#define SFTP_RECEIVE_STATUS_WAIT			( SFTP_RECEIVE_STATUS_WAITING2 | SFTP_RECEIVE_STATUS_WAITING1 )
#define SFTP_RECEIVE_STATUS_ERROR			(1 << 30)
#define SFTP_RECEIVE_STATUS_DISCONNECT			(1 << 31)

#define SFTP_RECEIVE_BUFFER_SIZE_DEFAULT		16384

struct sftp_receive_s {
    unsigned char					flags;
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
    void						(* process_sftp_payload)(struct sftp_payload_s *payload);
    unsigned int					read;
    unsigned int					size;
    unsigned char					threads;
    char						*buffer;
};

struct sftp_payload_s {
    struct sftp_subsystem_s				*sftp;
    unsigned int					len;
    unsigned char					type;
    uint32_t						id;
    struct list_element_s				list;
    char						data[];
};

struct sftp_payload_queue_s {
    struct list_header_s				header;
    unsigned int					threads;
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
};

struct sftp_identity_s {
    struct passwd					pwd;
    unsigned int					size;
    unsigned int 					len_home;
    char						*buffer;
};

#define SFTP_SUBSYSTEM_FLAG_INIT			(1 << 0)
#define SFTP_SUBSYSTEM_FLAG_VERSION_RECEIVED		(1 << 1)
#define SFTP_SUBSYSTEM_FLAG_VERSION_SEND		(1 << 2)
#define SFTP_SUBSYSTEM_FLAG_SESSION			(1 << 3)

#define SFTP_SUBSYSTEM_FLAG_FINISH			(1 << 27)
#define SFTP_SUBSYSTEM_FLAG_TROUBLE			(1 << 28)
#define SFTP_SUBSYSTEM_FLAG_DISCONNECTING		(1 << 30)
#define SFTP_SUBSYSTEM_FLAG_DISCONNECTED		(1 << 31)

#define SFTP_CONNECTION_FLAG_STD			(1 << 0)
#define SFTP_CONNECTION_FLAG_RECV_EMPTY			(1 << 1)
#define SFTP_CONNECTION_FLAG_RECV_ERROR			(1 << 2)

struct sftp_std_connection_s {
    struct fs_connection_s				stdin;
    struct fs_connection_s				stdout;
    struct fs_connection_s				stderr;
};

struct sftp_connection_s {
    unsigned char					flags;
    unsigned int					error;
    union {
	struct sftp_std_connection_s			std;
    } type;
    int							(* open)(struct sftp_connection_s *c);
    int							(* read)(struct sftp_connection_s *c, char *buffer, unsigned int size);
    int							(* write)(struct sftp_connection_s *c, char *data, unsigned int size);
    int							(* close)(struct sftp_connection_s *c);

    /* TODO:
	read_extended and write_extended for different data like error messages and other data */
};

struct sftp_protocol_s {
    unsigned char					version;
};

struct sftp_subsystem_s {
    unsigned int					flags;
    struct net_idmapping_s				mapping;
    struct sftp_protocol_s				protocol;
    struct sftp_connection_s				connection;
    struct sftp_identity_s				identity;
    struct sftp_receive_s				receive;
    struct attr_context_s				attrctx;
    struct sftp_payload_queue_s				queue;
    sftp_cb_t						cb[256];
};

/* prototypes */

#endif
