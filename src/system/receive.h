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

#ifndef OSNS_SYSTEM_RECEIVE_H
#define OSNS_SYSTEM_RECEIVE_H

#include "libosns-connection.h"
#include "osns/receive.h"

#define OSNS_SYSTEMCONNECTION_FLAG_WATCH_NETCACHE		1

struct osns_systemconnection_s {
    unsigned int			status;
    unsigned int			version;
    unsigned int			services;
    unsigned int			flags;
    struct list_header_s		mounts;
    struct connection_s			connection;
    struct osns_receive_s		receive;
    unsigned int			size;
    char 				buffer[];
};

/* prototypes */

struct connection_s *accept_connection_from_localsocket(struct connection_s *c_conn, struct connection_s *s_conn);
void clear_systemconnection(struct osns_systemconnection_s *sc);

#endif