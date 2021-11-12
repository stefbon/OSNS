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

#include "network.h"

#define SSH_SUBSYSTEM_CONNECTION_FLAG_STD			(1 << 0)
#define SSH_SUBSYSTEM_CONNECTION_FLAG_RECV_ERROR		(1 << 2)

#define SSH_SUBSYSTEM_CONNECTION_FLAG_TROUBLE			(1 << 28)
#define SSH_SUBSYSTEM_CONNECTION_FLAG_DISCONNECTING		(1 << 30)
#define SSH_SUBSYSTEM_CONNECTION_FLAG_DISCONNECTED		(1 << 31)

#define SSH_SUBSYSTEM_CONNECTION_FLAG_DISCONNECT		( SSH_SUBSYSTEM_CONNECTION_FLAG_DISCONNECTING | SSH_SUBSYSTEM_CONNECTION_FLAG_DISCONNECTED )

struct subsystem_std_connection_s {
    struct fs_connection_s				stdin;
    struct fs_connection_s				stdout;
    struct fs_connection_s				stderr;
};

#define SSH_SUBSYSTEM_CONNECTION_TYPE_STD		1

struct ssh_subsystem_connection_s {
    unsigned int					flags;
    unsigned int					error;
    union {
	struct subsystem_std_connection_s		std;
    } type;
    int							(* open)(struct ssh_subsystem_connection_s *c);
    int							(* read)(struct ssh_subsystem_connection_s *c, char *buffer, unsigned int size);
    int							(* write)(struct ssh_subsystem_connection_s *c, char *data, unsigned int size);
    int							(* close)(struct ssh_subsystem_connection_s *c);
};

/* prototypes */

int init_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection, unsigned char type);
int connect_ssh_subsystem_connection(struct ssh_subsystem_connection_s *c);

int add_ssh_subsystem_connection_eventloop(struct ssh_subsystem_connection_s *connection, void (* read_connection_signal)(int fd, void *ptr, struct event_s *event));
void remove_ssh_subsystem_connection_eventloop(struct ssh_subsystem_connection_s *connection);

void free_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection);
void disconnect_ssh_subsystem_connection(struct ssh_subsystem_connection_s *connection);

int start_thread_ssh_subsystem_connection_problem(struct ssh_subsystem_connection_s *connection);

#endif
