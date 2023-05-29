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

#ifndef LIB_SOCKET_COMMON_H
#define LIB_SOCKET_COMMON_H

/* Prototypes */

int socket_connect_dummy(struct osns_socket_s *sock);
int socket_readwrite_dummy(struct osns_socket_s *sock, char *buffer, unsigned int size);
int socket_recvsend_dummy(struct osns_socket_s *sock, char *buffer, unsigned int size, unsigned int flags);
int socket_writevreadv_dummy(struct osns_socket_s *sock, struct iovec *iov, unsigned int count);
int socket_recvmsg_dummy(struct osns_socket_s *sock, struct msghdr *msg);
int socket_sendmsg_dummy(struct osns_socket_s *sock, const struct msghdr *msg);

int socket_read_common(struct osns_socket_s *sock, char *buffer, unsigned int size);
int socket_write_common(struct osns_socket_s *sock, char *buffer, unsigned int size);

int socket_recv_common(struct osns_socket_s *s, char *buffer, unsigned int size, unsigned int flags);
int socket_send_common(struct osns_socket_s *s, char *buffer, unsigned int size, unsigned int flags);

int socket_writev_common(struct osns_socket_s *s, struct iovec *iov, unsigned int count);
int socket_readv_common(struct osns_socket_s *s, struct iovec *iov, unsigned int count);

int socket_sendmsg_common(struct osns_socket_s *sock, const struct msghdr *msg);
int socket_recvmsg_common(struct osns_socket_s *sock, struct msghdr *msg);


#endif
