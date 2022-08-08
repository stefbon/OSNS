/*
  2017 Stef Bon <stefbon@gmail.com>

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

*/

#ifndef _LIB_NETWORK_UTILS_H
#define _LIB_NETWORK_UTILS_H

#include <sys/socket.h>

/* prototypes */

unsigned int get_msghdr_controllen(struct msghdr *message, const char *what);
void add_fd_msghdr(struct msghdr *message, char *buffer, unsigned int size, int fd);
int read_fd_msghdr(struct msghdr *message);

#endif
