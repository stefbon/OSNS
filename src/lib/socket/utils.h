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

#ifndef LIB_SOCKET_UTILS_H
#define LIB_SOCKET_UTILS_H

#include "socket.h"

/* Prototypes */

int translate_osns_socket_flags(unsigned int flags);
int check_status_hlpr(unsigned int status, unsigned int set, unsigned int notset);
unsigned int get_status_osns_socket(struct osns_socket_s *sock);

void set_osns_socket_nonblocking(struct osns_socket_s *sock);
void unset_osns_socket_nonblocking(struct osns_socket_s *sock);

unsigned char socket_connection_error(unsigned int error);
unsigned char socket_blocking_error(unsigned int error);

int set_socket_properties(struct osns_socket_s *sock, struct socket_properties_s *prop);

int get_local_peer_properties(struct osns_socket_s *sock, struct local_peer_s *peer);
int get_network_peer_properties(struct osns_socket_s *sock, struct network_peer_s *peer, const char *what);

int set_path_osns_sockaddr(struct osns_socket_s *sock, struct fs_location_path_s *path);
unsigned int get_path_osns_sockaddr(struct osns_socket_s *sock, struct fs_location_path_s *path);

int set_address_osns_sockaddr(struct osns_socket_s *sock, struct ip_address_s *address, unsigned int port);

#endif
