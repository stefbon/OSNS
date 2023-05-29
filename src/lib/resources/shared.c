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

struct shared_lookup_hlpr_s {
    uint64_t id;
};

static int readrow_sqlite(void *stmt, struct db_query_result_s *result, void *ptr)
{

    if (stmt) {
        struct shared_lookup_hlpr_s *hlpr=(struct shared_lookup_hlpr_s *) ptr;

        if (get_sql_query_result(stmt, result, 0, "int")==0) hlpr->id=result->data.intval64;

    }

    return -1;

}

uint64_t get_dbid_network(struct dbconn_s *dbc, uint64_t pid, unsigned int type, const char *data, unsigned int size, struct db_query_result_s *result)
{
    char sqlquery[512];
    int len=snprintf(sqlquery, 512, "SELECT id FROM networkhost WHERE pid=%lu AND type=%u AND data='%.*s'", pid, type, size, data);
    struct shared_lookup_hlpr_s hlpr={0};
    return (sql_execute_simple_common(dbc, sqlquery, len, readrow_sqlite, (void *) &hlpr, result)==0) ? hlpr.id : 0;
}

/* convert the network netsrv struct to a string by using snprintf, using fixed size fields left alligned with zero's

    here a network_resource struct looks like:
    6                           portnr
    6                           service code used in OSNS (SSH, SFTP, etc)
    6                           transport
    ------
    18 bytes
*/

static unsigned int write_uint_to_buffer_hlpr(char *buffer, unsigned int len, unsigned int value)
{
    char tmp[len+1];

    if (snprintf(tmp, len+1, "%.*u", len, value)==len) {

        memcpy(buffer, tmp, len);
        return len;

    }

    return 0;

}

unsigned int write_network_service(struct network_resource_s *r, char *buffer, unsigned int size)
{
    struct network_service_s *netsrv=&r->data.service;
    unsigned int pos=0;

    if (buffer && (size>=18)) {

        pos+=write_uint_to_buffer_hlpr(buffer, 6, netsrv->port.nr);
        pos+=write_uint_to_buffer_hlpr(&buffer[pos], 6, netsrv->service);
        pos+=write_uint_to_buffer_hlpr(&buffer[pos], 6, netsrv->transport);

    } else {

        pos=18;

    }

    return pos;
}

static unsigned int read_uint_from_buffer_hlpr(char *buffer, unsigned int len)
{
    char tmp[len+1];

    memcpy(tmp, buffer, len);
    tmp[len]='\0';
    return atoi(tmp);
}

unsigned int read_network_service(struct network_resource_s *r, char *buffer, unsigned int size)
{
    unsigned int pos=0;

    logoutput_debug("read_network_service: type %u size %u", r->type, size);

    if (r->type==NETWORK_RESOURCE_TYPE_SERVICE) {
        struct network_service_s *netsrv=&r->data.service;

        if (buffer && (size>=NETWORK_SERVICE_BUFFER_LENGTH)) {

            netsrv->port.nr=read_uint_from_buffer_hlpr(&buffer[pos], 6);
            pos+=6;
            netsrv->service=read_uint_from_buffer_hlpr(&buffer[pos], 6);
            pos+=6;
            netsrv->transport=read_uint_from_buffer_hlpr(&buffer[pos], 6);
            pos+=6;

        }

    }

    return pos;

}
