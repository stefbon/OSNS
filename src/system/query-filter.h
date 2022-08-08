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

#ifndef OSNS_SYSTEM_QUERY_FILTER_H
#define OSNS_SYSTEM_QUERY_FILTER_H

/* prototypes */

int compare_filter_names_and(struct name_string_s *name, char *value);
int compare_filter_names_or(struct name_string_s *name, char *value);

int compare_filter_records_and(struct osns_record_s *record, char *value);
int compare_filter_records_or(struct osns_record_s *record, char *value);

int compare_filter_uint32_and(uint32_t param, uint32_t value);
int compare_filter_uint32_or(uint32_t param, uint32_t value);

#endif