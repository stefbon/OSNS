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

#ifndef CLIENT_RESOURCES_H
#define CLIENT_RESOURCES_H

#include "libosns-db.h"
#include "libosns-resource.h"

/* prototypes */

int open_network_db(char *path);
void close_network_db();

int browse_client_network_data(uint64_t pid, unsigned int type, void (* cb_readrecord)(struct network_resource_s *r, void *ptr), struct db_query_result_s *result, void *ptr);
int get_client_network_data(uint64_t id, void (* cb_readrecord)(struct network_resource_s *r, void *ptr), struct db_query_result_s *result, void *ptr);

uint64_t get_parent_id_network_resource(uint64_t dbid);

#endif
