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

#ifndef _DISCOVER_DISCOVER_H
#define _DISCOVER_DISCOVER_H

#include "network.h"
#include "datatypes.h"

#define DISCOVER_NETWORK_HASHSIZE			64
#define DISCOVER_NETWORK_NETSOCKET_TEXTSIZE		128
#define DISCOVER_NETWORK_SHARED_SERVICE_NAMELEN		128

#define DISCOVER_METHOD_AVAHI				1
#define DISCOVER_METHOD_STATICFILE			2
/* other methods: nmbd for MS Windows hosts, .... */

#define DISCOVER_RESOURCE_TYPE_NETWORK_GROUP		1
#define DISCOVER_RESOURCE_TYPE_NETWORK_HOST		2
#define DISCOVER_RESOURCE_TYPE_NETWORK_SOCKET		3

#define DISCOVER_RESOURCE_FLAG_AVAHI			( 1 << 0 )
#define DISCOVER_RESOURCE_FLAG_STATICFILE		( 1 << 1 )
#define DISCOVER_RESOURCE_FLAG_NODOMAIN			( 1 << 2 )
#define DISCOVER_RESOURCE_FLAG_BUSY			( 1 << 3 )
#define DISCOVER_RESOURCE_FLAG_ALLOC			( 1 << 4 )
#define DISCOVER_RESOURCE_FLAG_PREFERREDDOMAIN		( 1 << 5 )

#define DISCOVER_RESOURCE_FLAG_SSH			( 1 << 11 )
#define DISCOVER_RESOURCE_FLAG_SFTP			( 1 << 12 )
#define DISCOVER_RESOURCE_FLAG_SMB			( 1 << 13 )
#define DISCOVER_RESOURCE_FLAG_WEBDAV			( 1 << 14 )
#define DISCOVER_RESOURCE_FLAG_NFS			( 1 << 15 )

#define DISCOVER_RESOURCE_FLAGS_FS			( DISCOVER_RESOURCE_FLAG_SSH | DISCOVER_RESOURCE_FLAG_SFTP | DISCOVER_RESOURCE_FLAG_SMB | DISCOVER_RESOURCE_FLAG_WEBDAV |  DISCOVER_RESOURCE_FLAG_NFS )

/*
    TODO:
    - add discovery of ssh servers by scanning sshd_config (system and personal)
    - add discovery of servers by scanning the network (nmap..)
*/

struct discover_resource_s;

struct discover_network_s {
    uint32_t						ctr;
    struct list_header_s				header;
    struct simple_locking_s				locking;
    struct timespec					changed;
    void 						(*process_new_service)(struct discover_resource_s *s, void *ptr);
    void						*ptr;
    struct list_header_s				hash[DISCOVER_NETWORK_HASHSIZE];
};

struct discover_netgroup_s {
    struct list_header_s				header;
    char						name[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
};

#define LOOKUP_NAME_TYPE_CANONNAME			1
#define LOOKUP_NAME_TYPE_DNSNAME			2
#define LOOKUP_NAME_TYPE_DISCOVERNAME			3

struct lookup_name_s {
    unsigned char					type;
    union _loop_name_u {
	char						*canonname;
	char						*dnsname;
	char						*discovername;
    } name;
};

#define DISCOVER_NAME_TYPE_AVAHI			1

struct discover_method_s {
    unsigned char					type;
    struct list_element_s				list;
    unsigned int 					size;
    char						buffer[];
};

struct discover_nethost_s {
    struct list_header_s				header;
    struct list_header_s 				methods;
    struct host_address_s				address;
    struct lookup_name_s				lookupname;
};

struct discover_netsocket_s {
    struct list_header_s				header;
    struct network_port_s				port;
    struct timespec					refresh;
    char						text[DISCOVER_NETWORK_NETSOCKET_TEXTSIZE + 1];
};

struct discover_resource_s {
    uint32_t						unique;
    unsigned char					type;
    unsigned int					flags;
    struct list_element_s				list;
    struct timespec					found;
    struct timespec					changed;
    union resource_type_s {
	struct discover_netgroup_s			group;
	struct discover_nethost_s			host;
	struct discover_netsocket_s			socket;
    } service;
};

/* Prototypes */

typedef void (*process_new_service)(struct discover_resource_s *s, void *ptr);

struct discover_resource_s *lookup_resource_id(uint32_t unique);
struct discover_resource_s *check_create_netgroup_resource(char *name);

struct discover_resource_s *get_discover_nethost(struct discover_resource_s *resource);
struct discover_resource_s *get_discover_netgroup(struct discover_resource_s *resource);

void add_net_service(unsigned int method, char *name, struct host_address_s *host, struct network_port_s *port, unsigned int type, char *text);
void get_net_services(struct timespec *since);

void free_discover_resource(struct discover_resource_s *r);
int init_discover_group(void (* cb)(struct discover_resource_s *s, void *ptr), void *ptr);
void free_discover_records();

int rlock_discover_network(struct simple_lock_s *lock);
int wlock_discover_network(struct simple_lock_s *lock);
int unlock_discover_network(struct simple_lock_s *lock);

struct discover_resource_s *get_next_hashed_discover_resource(void **p_ptr, unsigned int *p_hashvalue);

struct discover_resource_s *get_next_discover_resource(struct discover_resource_s *parent, struct discover_resource_s *r, unsigned char type);
struct timespec *get_discover_network_changed(unsigned char type, uint32_t unique);

void add_net_service_avahi(const char *name, const char *hostname, char *ipv4, const char *domain, unsigned int port, const char *type);
void synchronize_discover_resources();
void discover_services();

#endif
