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

#ifndef LIB_DB_DB_H
#define LIB_DB_DB_H

#define DB_BACKEND_SQLITE				        1

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

struct dbconn_s {
#ifdef HAVE_SQLITE3
    sqlite3				*sqlite;
#endif
    pthread_t				threadid;
    unsigned int                        lockstatus;
    void                                (* lock)(struct dbconn_s *dbc);
    void                                (* unlock)(struct dbconn_s *dbc);
    void                                *ptr;
};

extern void lock_default(struct dbconn_s *dbc);
extern void unlock_default(struct dbconn_s *dbc);

#define DB_DBCONN_INIT                  {NULL, 0, 0, lock_default, unlock_default, NULL}

struct db_lookup_result_s {
    int                                 code;
    unsigned int                        rowid;
    unsigned int                        errcode;
};

#define DB_LOOKUP_RESULT_INIT          {0, 0, 0}

#endif
