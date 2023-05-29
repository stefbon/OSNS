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

#ifndef LIB_DB_SHARED_H
#define LIB_DB_SHARED_H

#define NETWORK_SERVICE_BUFFER_LENGTH                   18

/* prototypes */

uint64_t get_dbid_network(struct dbconn_s *dbc, uint64_t pid, unsigned int type, const char *data, unsigned int size, struct db_query_result_s *result);
unsigned int write_network_service(struct network_resource_s *r, char *buffer, unsigned int size);
unsigned int read_network_service(struct network_resource_s *r, char *buffer, unsigned int size);

#endif
