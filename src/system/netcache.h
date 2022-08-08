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

#ifndef OSNS_SYSTEM_NETCACHE_H
#define OSNS_SYSTEM_NETCACHE_H

struct network_record_s {
    unsigned int			flags;
    struct system_timespec_s		created;
    struct system_timespec_s		removed;
    char				*hostname;
    char				*domain;
    struct ip_address_s			ip;
    struct network_port_s		port;
    uint16_t				service;
    uint16_t				transport;
    uint32_t				offset;
    struct list_element_s		list; /* list hashtable */
};

/* prototypes */

void add_detected_network_service(const char *hostname, const char *domain, char *ip, unsigned int ip_family, uint16_t port, unsigned int flags, const char *type);

void init_network_cache();
void start_network_cache();
void stop_network_cache();
void remove_cached_network_records();

#define FIND_NETWORK_RECORD_FLAG_EXACT				1

struct network_record_s *find_network_record(uint32_t offset, unsigned char flags);
struct network_record_s *get_next_network_record(struct network_record_s *record);

int add_netcache_watch(uint64_t unique, uint32_t watchid, void (* cb)(uint64_t unique, uint32_t offset, uint32_t watchid));

#endif
