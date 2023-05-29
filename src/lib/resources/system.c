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

#include "resource.h"
#include "shared.h"

struct get_id_hlpr_s {
    uint64_t id;
    uint64_t changedate;
    uint64_t createdate;
};

static int readrow_sqlite(void *stmt, struct db_query_result_s *result, void *ptr)
{

    if (stmt) {
        struct get_id_hlpr_s *hlpr=(struct get_id_hlpr_s *) ptr;

        if (get_sql_query_result(stmt, result, 0, "int")==0) hlpr->id=result->data.intval64;
        if (get_sql_query_result(stmt, result, 1, "int")==0) hlpr->createdate=result->data.intval64;
        if (get_sql_query_result(stmt, result, 2, "int")==0) hlpr->changedate=result->data.intval64;
        return 0;

    }

    return -1;

}

static int insertrow_sqlite(void *stmt, struct db_query_result_s *result, void *ptr)
{

    if (stmt) {

        result->errcode=0;
        return 0;

    }

    return -1;

}

int save_network_data(struct dbconn_s *dbc, struct network_resource_s *resource, struct db_query_result_s *result)
{
    char sqlquery[512];
    unsigned int len=0;
    char *data=NULL;
    unsigned int size=0;
    unsigned int tmp=(resource->type==NETWORK_RESOURCE_TYPE_SERVICE) ? NETWORK_SERVICE_BUFFER_LENGTH : 0;
    char buffer[tmp];

    switch (resource->type) {

        case NETWORK_RESOURCE_TYPE_GROUP:
        case NETWORK_RESOURCE_TYPE_HOST:

            data=resource->data.name;
            size=strlen(data);
            break;

        case NETWORK_RESOURCE_TYPE_ADDRESS:

            data=(resource->flags & NETWORK_RESOURCE_FLAG_IPv6) ? resource->data.ipv6 : resource->data.ipv4;
            size=strlen(data);
            break;

        case NETWORK_RESOURCE_TYPE_SERVICE:

            /* convert the netservice type (udp, tcp, ... to a resource flag */

            if ((resource->data.service.port.type==_NETWORK_PORT_TCP) || (resource->data.service.port.type==_NETWORK_PORT_UDP)) {

                resource->flags &= ~(NETWORK_RESOURCE_FLAG_UDP | NETWORK_RESOURCE_FLAG_TCP);

                if (resource->data.service.port.type==_NETWORK_PORT_TCP) {

                    resource->flags |= NETWORK_RESOURCE_FLAG_TCP;

                } else if (resource->data.service.port.type==_NETWORK_PORT_UDP) {

                    resource->flags |= NETWORK_RESOURCE_FLAG_UDP;

                }

            }

            size=write_network_service(resource, buffer, tmp);
            data=buffer;
            break;

    }

    if (data==NULL) {

        logoutput_debug("save_network_data: resource %u not reckognized", resource->type);
        return -1;

    }


    resource->processdate=(uint64_t) time(NULL);
    resource->createdate=resource->processdate;
    resource->changedate=resource->createdate;

    len=snprintf(sqlquery, 512, "INSERT OR IGNORE INTO network (type,pid,flags,createdate,changedate,data) VALUES (%u,%lu,%u,%lu,%lu,'%.*s')", resource->type, resource->pid, resource->flags, resource->createdate, resource->changedate, size, data);

    if (sql_execute_simple_common(dbc, sqlquery, len, insertrow_sqlite, NULL, result)==0) {
        struct get_id_hlpr_s hlpr={0};

        len=snprintf(sqlquery, 512, "SELECT id,createdate,changedate FROM network WHERE type=%u AND pid=%lu AND data='%.*s'", resource->type, resource->pid, size, data);
        if (sql_execute_simple_common(dbc, sqlquery, len, readrow_sqlite, (void *) &hlpr, result)==0) {

            resource->id=hlpr.id;
            /* set date to */
            resource->createdate=hlpr.createdate;
            resource->changedate=hlpr.changedate;
            return 0;

        }

    }

    return -1;

}
