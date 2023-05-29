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
#include "open.h"

#ifdef HAVE_SQLITE3

static struct timespec busy_wait={0, 5000000};

static int custom_busy_handler(void *data, int times)
{
    nanosleep(&busy_wait, NULL);
    return (times>3) ? 0 : 1;
}

void lock_default(struct dbconn_s *dbc)
{
}

void unlock_default(struct dbconn_s *dbc)
{
}

static int handle_open_db_sqlite(char *path, char *name, struct dbconn_s *dbc, const char *role)
{
    int result=-1;
    unsigned int len01=((path) ? strlen(path) : 0);
    unsigned int len02=((name) ? strlen(name) : 0);

    if ((len01>0) && (len02>0)) {
        char buffer[len01 + len02 + 16]; /* enough bytes for path/name.sqlite ... 16 extra is more than enough for ".sqlite", the slash and the trailing zero */

        if (snprintf(buffer, len01 + len02 + 16, "%s/%s.sqlite", path, name)>0) {
	    unsigned int openflags=SQLITE_OPEN_FULLMUTEX; /* serialized */

            if (role && strcmp(role, "system")==0) {

	        openflags |= (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE); /* system has exclusive readwrite rights */

            } else {

	        openflags |= SQLITE_OPEN_READONLY; /* non-system can only read */

            }

	    if (sqlite3_open_v2(buffer, &dbc->sqlite, openflags, NULL)==SQLITE_OK) {

	        logoutput_debug("handle_open_db_sqlite: connection to db %s for %s", buffer, ((role) ? role : "unknown"));
	        sqlite3_busy_handler(dbc->sqlite, custom_busy_handler, NULL);
	        result=0;

                dbc->lock=lock_default;
                dbc->unlock=unlock_default;

	    } else {

	        logoutput_debug("handle_open_sb_sqlite: error opening db %s", buffer);

	    }

        }

    }

    return result;

}

void close_db_sql(struct dbconn_s *dbc)
{

    if (dbc->sqlite) {
        int result=sqlite3_close(dbc->sqlite);

        if (result!=SQLITE_OK) logoutput_debug("close_db_sqlite: error %i closing db %s", result);
        dbc->sqlite=NULL;

    }

}

int init_db_sql()
{
    int result=sqlite3_config(SQLITE_CONFIG_SERIALIZED); /* really make sure no threads are overwriting each others data ... wait for each other */

    if (result==SQLITE_OK) {

	logoutput("init_db_sqlite: set config options SERIALIZED");

    } else {

	if (result==SQLITE_ERROR) {

	    logoutput_error("init_db_sqlite: error setting config options SERIALIZED");

	} else {

	    logoutput_error("init_db_sqlite: setting config options SERIALIZED result in %i", result);

	}

    }

    return sqlite3_initialize();

}

void shutdown_db_sql()
{
    sqlite3_shutdown();
}

#else

static int handle_open_db_sql(char *path, char *name, struct dbconn_s *dbc, const char *role)
{
    return -1;
}

void close_db_sql(struct db_conn_s *dbc)
{
}

int init_db_sql()
{
    return -1;
}

void shutdown_db_sql()
{
}

#endif

int create_db_sql(char *path, char *name, struct dbconn_s *dbc)
{
    return handle_open_db_sqlite(path, name, dbc, "system");
}

int open_db_sql(char *path, char *name, struct dbconn_s *dbc)
{
    return handle_open_db_sqlite(path, name, dbc, "client");
}
