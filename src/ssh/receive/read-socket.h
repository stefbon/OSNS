/*
  2018 Stef Bon <stefbon@gmail.com>

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

#ifndef _SSH_RECEIVE_READ_SOCKET_H
#define _SSH_RECEIVE_READ_SOCKET_H

#define SSH_RAWDATA_FLAG_WATCHED                1
#define SSH_RAWDATA_FLAG_COMPLETE               2

struct ssh_rawdata_s {
    struct list_element_s                       list;
    unsigned int                                flags;
    unsigned int                                msgsize;
    unsigned int                                pos;
    unsigned int                                size;
    char                                        *buffer;
};

/* prototypes */

void set_ssh_socket_behaviour(struct osns_socket_s *sock, const char *phase);
void disable_ssh_socket_read_data(struct osns_socket_s *sock);
void enable_ssh_socket_read_data(struct osns_socket_s *sock);

void init_ssh_socket_behaviour(struct osns_socket_s *sock);

#endif
