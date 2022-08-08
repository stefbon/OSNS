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

#ifndef OSNS_SYSTEM_QUERY_H
#define OSNS_SYSTEM_QUERY_H

struct readquery_request_s {
    char						*buffer;
    unsigned int					size;
    unsigned int					offset;
    unsigned int					count;
};

struct openquery_request_s {
    struct osns_systemconnection_s			*sc;
    struct system_timespec_s				created;
    uint32_t						id;
    struct list_element_s				list;
    off_t						offset;
    unsigned int					status;
    unsigned int					(* cb_read)(struct openquery_request_s *request, struct readquery_request_s *rq);
    void						(* cb_close)(struct openquery_request_s *request);
    int							(* cb_filter)(struct openquery_request_s *request, void *ptr);
    unsigned int					flags;
    unsigned int					valid;
    union _query_attr_u {
	struct query_netcache_attr_s			*netcache;
	struct query_mountinfo_attr_s			*mountinfo;
    } filter;
};

/* prototypes */

void process_msg_openquery(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len);
void process_msg_readquery(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len);
void process_msg_closequery(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len);

void init_system_query();

#endif