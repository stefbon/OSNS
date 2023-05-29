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

#ifndef SSH_SUBSYSTEM_CONNECTION_H
#define SSH_SUBSYSTEM_CONNECTION_H

#include "libosns-lock.h"
#include "libosns-socket.h"

#define SSH_SUBSYSTEM_CONNECTION_FLAG_STD			(1 << 0)
#define SSH_SUBSYSTEM_CONNECTION_FLAG_TROUBLE			(1 << 28)

#define SSH_SUBSYSTEM_SOCKET_TYPE_UNKNOWN                       0
#define SSH_SUBSYSTEM_SOCKET_TYPE_IN                            1
#define SSH_SUBSYSTEM_SOCKET_TYPE_OUT                           2
#define SSH_SUBSYSTEM_SOCKET_TYPE_ERROR                         3

#define SSH_SUBSYSTEM_SOCKET_FLAG_IN                            1
#define SSH_SUBSYSTEM_SOCKET_FLAG_OUT                           2
#define SSH_SUBSYSTEM_SOCKET_FLAG_ERROR                         4

struct subsystem_std_connection_s {
    struct osns_socket_s				stdin;
    struct osns_socket_s				stdout;
    struct osns_socket_s				stderr;
};

#define SSH_SUBSYSTEM_CONNECTION_TYPE_STD		1

struct ssh_subsystem_connection_s {
    unsigned int					flags;
    unsigned int                                        unique;
    unsigned int					errcode;
    struct shared_signal_s				*signal;
    union {
	struct subsystem_std_connection_s		std;
    } type;
    int							(* open)(struct ssh_subsystem_connection_s *c, int (* open_socket)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags));
    void						(* close)(struct ssh_subsystem_connection_s *c, unsigned int type);
    int                                                 (* write)(struct ssh_subsystem_connection_s *c, char *data, unsigned int size);
};

/* prototypes */

int init_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection, unsigned char type, struct shared_signal_s *signal, void (* init_socket)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags));
int open_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection, int (* open_socket)(struct ssh_subsystem_connection_s *connection, struct osns_socket_s *sock, unsigned int type, unsigned int flags));

#endif
