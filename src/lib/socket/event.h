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

#ifndef LIB_SOCKET_READ_H
#define LIB_SOCKET_READ_H

struct beventloop_s;

/* prototypes */

void set_socket_context_defaults(struct osns_socket_s *sock);
void process_socket_error_default(struct osns_socket_s *sock, unsigned int level, unsigned int errcode, void *ptr);
void process_socket_close_default(struct osns_socket_s *sock, unsigned int level, void *ptr);
struct socket_rawdata_s *get_socket_rawdata_from_list(struct osns_socket_s *sock, uint64_t ctr);

void set_bevent_process_data_custom(struct bevent_s *bevent);
void set_bevent_process_data_default(struct bevent_s *bevent);

int add_osns_socket_eventloop(struct osns_socket_s *sock, struct beventloop_s *loop, void *ptr, unsigned int flags);
void remove_osns_socket_eventloop(struct osns_socket_s *sock);

struct beventloop_s *osns_socket_get_eventloop(struct osns_socket_s *sock);

void set_osns_socket_buffer(struct osns_socket_s *sock, char *buffer, unsigned int size);
void set_osns_socket_control_data_buffer(struct osns_socket_s *sock, char *buffer, unsigned int size);
void set_read_osns_socket_cmsg(struct osns_socket_s *sock, unsigned char enable);

#endif
