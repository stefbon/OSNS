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

#include "db.h"
#include "utils.h"

#ifdef HAVE_SQLITE3
#include <sqlite3.h>

int sql_execute_simple_common(struct dbconn_s *dbc, char *sqlquery, unsigned int len, int (* cb_success)(void *stmt, struct db_query_result_s *result, void *ptr), void *ptr, struct db_query_result_s *result)
{
    sqlite3_stmt *stmt=NULL;
    int tmp=-1;
    int code=0;

    (* dbc->lock)(dbc);

    code=sqlite3_prepare_v2(dbc->sqlite, sqlquery, len, &stmt, NULL);

    if (code==SQLITE_OK) {
        unsigned int count=0;

	dostep:

	code=sqlite3_step(stmt);
	count++;

        result->rowid=sqlite3_last_insert_rowid(dbc->sqlite); /* keep that for various cases ... after a (real) insert */
        sqlite3_set_last_insert_rowid(dbc->sqlite, 0);

	if (code==SQLITE_DONE) {

            /* success after insert or delete */

            tmp=0;
            if (count>1) logoutput_debug("sql_execute_simple_common: done after %u steps", (count-1));

	} else if (code==SQLITE_ROW) {

            /* success after select for example ... a row is available */

            while (code==SQLITE_ROW) {

                if (cb_success) {

	            tmp=(* cb_success)((void *) stmt, result, ptr);
	            if (tmp==-1) break;

                } else {

                    tmp=0;
                    break;

                }

	        code=sqlite3_step(stmt);
	        count++;

            }

            if (count>1) logoutput_debug("sql_execute_simple_common: done after %u steps", (count-1));

	} else {

	    if (code==SQLITE_ERROR) {

		logoutput("sql_execute_simple_common: error executing sql statement %s", sqlquery);

	    } else {

		logoutput("sql_execute_simple_common: unknown return code (%i) executing sql statement %s", code, sqlquery);

	    }

	    tmp=-1;
	    result->errcode=EIO;

	}

	sqlite3_finalize(stmt);

    } else {

	logoutput("sql_execute_simple_common: error (%i) preparing sql statement %s", result, sqlquery);
	tmp=-1;
	result->errcode=EIO;

    }

    (* dbc->unlock)(dbc);

    return tmp;

}

int get_sql_query_result(void *ptr, struct db_query_result_s *result, unsigned int col, const char *what)
{
    int tmp=-1;

    if (ptr) {
        sqlite3_stmt *stmt=(sqlite3_stmt *) ptr;

        if (col < sqlite3_column_count(stmt)) {

            if ((strcmp(what, "int64")==0) || (strcmp(what, "int")==0)) {

                if (sqlite3_column_type(stmt, col)==SQLITE_INTEGER) {

                    result->data.intval64=sqlite3_column_int64(stmt, col);
                    tmp=0;

                }

            } else if (strcmp(what, "txt")==0) {

                if (sqlite3_column_type(stmt, col)==SQLITE_TEXT) {

                    result->data.text=sqlite3_column_text(stmt, col);
                    tmp=0;

                }

            }

        }

    }

    return tmp;
}

#else

int sql_execute_simple_common(struct dbconn_s *dbc, char *sqlquery, unsigned int len, int (* cb_success)(void *stmt, struct db_query_result_s *result, void *ptr), void *ptr, struct db_query_result_s *result)
{
    return -1;
}

int get_sql_query_result(void *ptr, struct db_query_result_s *result, unsigned int col, const char *what)
{
    return -1;
}

#endif

void init_db_query_result(struct db_query_result_s *result)
{
    result->rowid=0;
    result->errcode=0;
}

