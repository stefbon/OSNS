/*
  2010, 2011, 2012, 2013, 2014 Stef Bon <stefbon@gmail.com>

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


#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-fuse-public.h"
#include "libosns-network.h"

#include "libosns-db.h"

/*
    all puprose table for network group/domain, host, address and servuce records

    - uint64 id                                unique id
    - uint64 pid                               parent id
    - uint32 type                              type like group, host or address
    - uint32 flags                             flags like DNS_DOMAIN or not and detected by DNSSD
    - uint64 seconds since epoch               create date
    - uint64 seconds since epoch               change date
    - bytes [] data                            name of group or domain, or hostname, or address, or service

*/

int check_network_table(struct dbconn_s *dbc)
{
    char sqlquery[512];
    struct db_query_result_s result = DB_QUERY_RESULT_INIT;
    int len=snprintf(sqlquery, 512, "CREATE TABLE IF NOT EXISTS network (id INTEGER PRIMARY KEY, pid INTEGER DEFAULT 0, type UNSIGNED INTEGER DEFAULT 0, flags UNSIGNED INT DEFAULT 0, createdate UNSIGNED INTEGER DEFAULT 0, changedate UNSIGNED INTEGER DEFAULT 0, data TEXT NOT NULL, UNIQUE (type,pid,data))");
    return sql_execute_simple_common(dbc, sqlquery, len, NULL, NULL, &result);
}

int create_network_indices(struct dbconn_s *dbc)
{
    char sqlquery[512];
    struct db_query_result_s result = DB_QUERY_RESULT_INIT;
    int len=snprintf(sqlquery, 512, "CREATE INDEX IF NOT EXISTS changedate ON network(changedate)");
    return sql_execute_simple_common(dbc, sqlquery, len, NULL, NULL, &result);
}
