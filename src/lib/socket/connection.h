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

#ifndef LIB_SOCKET_CONNECTION_H
#define LIB_SOCKET_CONNECTION_H

/* Prototypes */

void init_system_socket_connection(struct system_socket_s *sock);

int socket_bind(struct system_socket_s *sock);
int socket_listen(struct system_socket_s *sock, int length);
struct system_socket_s *socket_accept(struct system_socket_s *server, struct system_socket_s *client, int flags);

int socket_connect(struct system_socket_s *sock);

int socket_recv(struct system_socket_s *s, void *buffer, unsigned int size, unsigned int flags);
int socket_send(struct system_socket_s *s, void *buffer, unsigned int size, unsigned int flags);

int socket_writev(struct system_socket_s *s, struct iovec *iov, unsigned int count);
int socket_readv(struct system_socket_s *s, struct iovec *iov, unsigned int count);

int socket_sendmsg(struct system_socket_s *sock, const struct msghdr *msg);
int socket_recvmsg(struct system_socket_s *sock, struct msghdr *msg);

#endif
