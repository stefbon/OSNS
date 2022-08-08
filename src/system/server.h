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

#ifndef OSNS_SYSTEM_SERVER_H
#define OSNS_SYSTEM_SERVER_H

#include "libosns-connection.h"
#include "osns/receive.h"

/* prototypes */

int create_local_socket(struct connection_s *server, char *runpath, char *group);
void clear_local_connections();
void close_osns_server();
void clear_osns_server();
struct connection_s *get_client_connection(uint64_t unique);

#endif