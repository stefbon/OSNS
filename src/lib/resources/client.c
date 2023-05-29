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

struct readrow_network_hlpr_s {
    void                                        (* cb)(struct network_resource_s *r, void *ptr);
    void                                        *ptr;
};

static int readrow_network_sql(void *stmt, struct db_query_result_s *result, void *ptr)
{
    struct readrow_network_hlpr_s *hlpr=(struct readrow_network_hlpr_s *) ptr;
    struct network_resource_s r;
    unsigned int len=0;

    if (stmt==NULL) return -1;
    memset(&r, 0, sizeof(struct network_resource_s));

    if (get_sql_query_result(stmt, result, 0, "int")==0) r.id=result->data.intval64;
    if (get_sql_query_result(stmt, result, 1, "int")==0) r.pid=result->data.intval64;
    if (get_sql_query_result(stmt, result, 2, "int")==0) r.type=result->data.intval64;
    if (get_sql_query_result(stmt, result, 3, "int")==0) r.flags=(uint32_t) result->data.intval64;
    if (get_sql_query_result(stmt, result, 4, "int")==0) r.createdate=(uint32_t) result->data.intval64;
    if (get_sql_query_result(stmt, result, 5, "int")==0) r.changedate=(uint32_t) result->data.intval64;
    if (get_sql_query_result(stmt, result, 6, "int")==0) len=(unsigned int) result->data.intval64;
    if (get_sql_query_result(stmt, result, 7, "txt")==0) {
        char *data=(char *) result->data.text;

        if ((r.type==NETWORK_RESOURCE_TYPE_GROUP) || (r.type==NETWORK_RESOURCE_TYPE_HOST)) {

            if (len>HOST_HOSTNAME_FQDN_MAX_LENGTH) {

                len=HOST_HOSTNAME_FQDN_MAX_LENGTH;
                result->errcode=ENAMETOOLONG;

            }

            memcpy(r.data.name, data, len);

        } else if (r.type==NETWORK_RESOURCE_TYPE_ADDRESS) {

            if (r.flags & NETWORK_RESOURCE_FLAG_IPv4) {

                if (len>INET_ADDRSTRLEN) {

                    len=INET_ADDRSTRLEN;
                    result->errcode=ENAMETOOLONG;

                }

                memcpy(r.data.ipv4, data, len);

            } else if (r.flags & NETWORK_RESOURCE_FLAG_IPv6) {

                if (len>INET6_ADDRSTRLEN) {

                    len=INET6_ADDRSTRLEN;
                    result->errcode=ENAMETOOLONG;

                }

                memcpy(r.data.ipv6, data, len);

            } else {

                logoutput_debug("readrow_network_sql: network record type address has data %.*s not reckognized (flags=%u)", len, data, r.flags);

            }

        } else if (r.type==NETWORK_RESOURCE_TYPE_SERVICE) {

            if (len>=NETWORK_SERVICE_BUFFER_LENGTH) {

                len=read_network_service(&r, data, len);

                if (r.flags & NETWORK_RESOURCE_FLAG_TCP) {

                    r.data.service.port.type=_NETWORK_PORT_TCP;

                } else if (r.flags & NETWORK_RESOURCE_FLAG_UDP) {

                    r.data.service.port.type=_NETWORK_PORT_UDP;

                }

            } else {

                result->errcode=ENODATA;

            }

        }

    }

    logoutput_debug("readrow_network_sql: found resource type %u", r.type);
    (* hlpr->cb)(&r, hlpr->ptr);
    return 0;


}

static int get_network_data_shared(struct dbconn_s *dbc, char *sqlquery, unsigned int len, void (* cb_readrecord)(struct network_resource_s *r, void *ptr), struct db_query_result_s *result, void *ptr)
{
    struct readrow_network_hlpr_s hlpr;

    memset(&hlpr, 0, sizeof(struct readrow_network_hlpr_s));
    hlpr.cb=cb_readrecord;
    hlpr.ptr=ptr;
    return sql_execute_simple_common(dbc, sqlquery, len, readrow_network_sql, &hlpr, result);
}

int browse_network_data(struct dbconn_s *dbc, uint64_t pid, unsigned int type, void (* cb_readrecord)(struct network_resource_s *r, void *ptr), struct db_query_result_s *result, void *ptr)
{
    char sqlquery[512];
    int len=snprintf(sqlquery, 512, "SELECT id,pid,type,flags,createdate,changedate,length(data),data FROM network WHERE pid=%li AND type=%u", pid, type);
    return get_network_data_shared(dbc, sqlquery, len, cb_readrecord, result, ptr);
}

int get_network_data(struct dbconn_s *dbc, uint64_t id, void (* cb_readrecord)(struct network_resource_s *r, void *ptr), struct db_query_result_s *result, void *ptr)
{
    char sqlquery[512];
    int len=snprintf(sqlquery, 512, "SELECT id,pid,type,flags,createdate,changedate,length(data),data FROM network WHERE id=%li", id);
    return get_network_data_shared(dbc, sqlquery, len, cb_readrecord, result, ptr);
}
