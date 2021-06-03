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

#ifndef _LOCALSOCKET_SEND_H
#define _LOCALSOCKET_SEND_H

#include "osns_socket.h"

/* prototypes */

int send_osns_packet(struct osns_localsocket_s *localsocket, char *b, unsigned int size, unsigned int *error);
int send_osns_msg_init(struct osns_localsocket_s *localsocket, unsigned int major, unsigned int minor);
int send_osns_msg_notsupported(struct osns_localsocket_s *localsocket, uint32_t id);

#endif
