/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include <arpa/inet.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-db.h"
#include "libosns-resource-client.h"

static struct dbconn_s dbc=DB_DBCONN_INIT;
static uint64_t dbid_nodomain=0;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;

struct shared_lookup_hlpr_s {
    uint64_t id;
};

static void lock_custom(struct dbconn_s *dbc)
{
    pthread_mutex_lock(&mutex);
}

static void unlock_custom(struct dbconn_s *dbc)
{
    pthread_mutex_unlock(&mutex);
}

static int read_row_shared_sqlite(void *stmt, struct db_query_result_s *result, void *ptr)
{

    if (stmt) {
        struct shared_lookup_hlpr_s *hlpr=(struct shared_lookup_hlpr_s *) ptr;

        if (get_sql_query_result(stmt, result, 0, "int")==0) hlpr->id=result->data.intval64;

    }

    return -1; /* always stop at first occurence */

}

int open_network_db(char *path)
{

    memset(&dbc, 0, sizeof(struct dbconn_s));

    if (open_db_sql(path, "network", &dbc)==0) {
        struct db_query_result_s result=DB_QUERY_RESULT_INIT;
        char sqlquery[512];
        int len=0;
        struct shared_lookup_hlpr_s hlpr={0};

        logoutput_debug("open_network_db: open network db success");

        dbc.lock=lock_custom;
        dbc.unlock=unlock_custom;

        /* look for the nodomain network group
            == a network group with an empty name (not NULL but one with zero length) and the flag NODOMAIN set */

        len=snprintf(sqlquery, 512, "SELECT id FROM network WHERE type=%u AND flags=%u", NETWORK_RESOURCE_TYPE_GROUP, NETWORK_RESOURCE_FLAG_NODOMAIN);
        if (sql_execute_simple_common(&dbc, sqlquery, len, read_row_shared_sqlite, (void *) &hlpr, &result)==0) {

            dbid_nodomain=hlpr.id;
            logoutput_debug("open_network_db: found nodomain with dbid %lu", hlpr.id);

        } else {

            /* fatal? */
            logoutput_debug("open_network_db: unable to find nodomain");

        }

    }

    return 0;
    errorout:
    close_db_sql(&dbc);
    return -1;
}

void close_network_db()
{
    close_db_sql(&dbc);
}

int browse_client_network_data(uint64_t pid, unsigned int type, void (* cb_readrecord)(struct network_resource_s *r, void *ptr), struct db_query_result_s *result, void *ptr)
{
    return browse_network_data(&dbc, pid, type, cb_readrecord, result, ptr);
}

int get_client_network_data(uint64_t id, void (* cb_readrecord)(struct network_resource_s *r, void *ptr), struct db_query_result_s *result, void *ptr)
{
    return get_network_data(&dbc, id, cb_readrecord, result, ptr);
}

/* get parent id (pid) of network resource record if any

    return 0 if not found */

struct read_network_pid_hlpr_s {
    uint64_t dbid;
};

static void read_network_pid_hlpr(struct network_resource_s *r, void *ptr)
{
    struct read_network_pid_hlpr_s *hlpr=(struct read_network_pid_hlpr_s *) ptr;
    hlpr->dbid=r->pid;
}

uint64_t get_parent_id_network_resource(uint64_t dbid)
{
    struct db_query_result_s result=DB_QUERY_RESULT_INIT;
    struct read_network_pid_hlpr_s hlpr={0};
    uint64_t pid=0;

    if (get_client_network_data(dbid, read_network_pid_hlpr, &result, (void *) &hlpr)==0) pid=hlpr.dbid;
    logoutput_debug("get_parent_id_network_resource: dbid %lu pid %lu", dbid, pid);
    return pid;
}
