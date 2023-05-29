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

#ifndef LIB_DB_UTILS_H
#define LIB_DB_UTILS_H

struct db_query_result_s {
    unsigned int                    errcode;
    uint64_t                        rowid;
    union _query_data_u {
        int64_t                     intval64;
        const unsigned char         *text;
    } data;
};

#define DB_QUERY_RESULT_INIT          {0, 0}

/* prototypes */

int sql_execute_simple_common(struct dbconn_s *dbc, char *sqlquery, unsigned int len, int (* cb_success)(void *stmt, struct db_query_result_s *result, void *ptr), void *ptr, struct db_query_result_s *result);
void init_db_query_result(struct db_query_result_s *result);

int get_sql_query_result(void *ptr, struct db_query_result_s *query, unsigned int col, const char *what);

#endif
