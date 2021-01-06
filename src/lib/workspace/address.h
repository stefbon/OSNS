/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_WORKSPACE_ADDRESS_H
#define _LIB_WORKSPACE_ADDRESS_H

#include "workspaces.h"

/* prototypes */

void translate_context_network_port(struct service_address_s *service, unsigned int *port);
void translate_context_address_network(struct host_address_s *host, struct service_address_s *service, char **target, unsigned int *port, unsigned int *family);

#endif
