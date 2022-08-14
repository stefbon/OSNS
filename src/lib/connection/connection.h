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

#ifndef _LIB_CONNECTION_CONNECTION_H
#define _LIB_CONNECTION_CONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "libosns-eventloop.h"
#include "libosns-list.h"
#include "libosns-misc.h"
#include "libosns-error.h"
#include "lib/datatypes/ssh-string.h"

#include "libosns-socket.h"

#define LISTEN_BACKLOG							50

#define CONNECTION_TYPE_LOCAL						1
#define CONNECTION_TYPE_NETWORK						2
#define CONNECTION_TYPE_SYSTEM						3

#define CONNECTION_ROLE_CLIENT						1
#define CONNECTION_ROLE_SERVER						2
#define CONNECTION_ROLE_SOCKET						3

#define CONNECTION_FLAG_IPv4						1
#define CONNECTION_FLAG_IPv6						2
#define CONNECTION_FLAG_UDP						4

#define CONNECTION_STATUS_INIT						(1 << 0)
#define CONNECTION_STATUS_CONNECTING					(1 << 1)
#define CONNECTION_STATUS_CONNECTED					(1 << 2)
#define CONNECTION_STATUS_EVENTLOOP					(1 << 3)
#define CONNECTION_STATUS_SERVERLIST					(1 << 4)
#define CONNECTION_STATUS_DISCONNECTING					(1 << 5)
#define CONNECTION_STATUS_DISCONNECTED					(1 << 6)
#define CONNECTION_STATUS_DISCONNECT					( CONNECTION_STATUS_DISCONNECTING | CONNECTION_STATUS_DISCONNECTED )

#define CONNECTION_COMPARE_HOST					1

struct connection_s;

union connection_peer_u {
    struct local_peer_s					local;
    struct network_peer_s 				network;
};

struct connection_address_s {
    union _conn_address_u {
	struct fs_location_path_s		*path;
	struct network_peer_s			*peer;
    } target;
};

struct connection_s {
    unsigned int				status;
    unsigned int				error;
    unsigned int				expire;
    struct shared_signal_s			*signal;
    void 					*data;
    struct list_element_s			list;
    struct osns_socket_s			sock;
    union {
	struct server_ops_s {
	    struct connection_s			*(* accept_peer)(struct connection_s *c_conn, struct connection_s *s_conn);
	    struct list_header_s		header;
	    uint64_t				unique;
	} server;
	struct client_ops_s {
	    union connection_peer_u		peer;
	    void 				(* disconnect)(struct connection_s *conn, unsigned char remote);
	    void				(* error)(struct connection_s *conn, struct generic_error_s *e);
	    void				(* dataavail)(struct connection_s *conn);
	    struct connection_s			*server;
	    uint64_t				unique;
	} client;
    } ops;
};

/* Prototypes */

void disconnect_cb_default(struct connection_s *conn, unsigned char remote);

void init_connection(struct connection_s *c, unsigned char type, unsigned char role, unsigned int flags);
void clear_connection(struct connection_s *c);
void set_connection_shared_signal(struct connection_s *c, struct shared_signal_s *signal);

int create_serversocket(struct connection_s *server, struct beventloop_s *loop, struct connection_s *(* accept_cb)(struct connection_s *c_conn, struct connection_s *s_conn), struct connection_address_s *address, struct generic_error_s *error);
int create_connection(struct connection_s *client, struct connection_address_s *address, struct beventloop_s *loop);
int create_local_connection(struct connection_s *client, char *runpath, struct beventloop_s *loop);

struct connection_s *get_next_connection(struct connection_s *s_conn, struct connection_s *c_conn);

#endif
