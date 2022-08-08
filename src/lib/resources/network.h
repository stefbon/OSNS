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

#ifndef LIB_RESOURCE_NETWORK_H
#define LIB_RESOURCE_NETWORK_H

#include "libosns-network.h"
#include "libosns-datatypes.h"

#include "resource.h"

#define NETWORK_RESOURCE_TYPE_NETWORK				0
#define NETWORK_RESOURCE_TYPE_NETWORK_GROUP			1
#define NETWORK_RESOURCE_TYPE_NETWORK_HOST			2
#define NETWORK_RESOURCE_TYPE_NETWORK_SOCKET			3

#define NETWORK_RESOURCE_FLAG_NODOMAIN				1
#define NETWORK_RESOURCE_FLAG_SERVICE_GUESSED			2
#define NETWORK_RESOURCE_FLAG_DNSSD				4
#define NETWORK_RESOURCE_FLAG_LOCALHOST				8
#define NETWORK_RESOURCE_FLAG_PRIVATE				16

struct network_resource_s {
    unsigned char 						type;
    unsigned int						flags;
    uint32_t							parent_unique;
    union _network_resource_u {
	char 							domain[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
	struct host_address_s	 				address;
	struct network_service_s 				service;
    } data;
};

/* Prototypes */

int add_network_subsys(void (* cb)(struct resource_s *r, unsigned char what, void *ptr), void *ptr);
uint32_t add_network_service_resource(struct host_address_s *address, char *domainname, struct network_port_s *port, unsigned int service, unsigned int transport, unsigned int flags);

int get_network_resource(uint32_t unique, struct network_resource_s *nr);
void browse_network_resources(uint32_t unique, void (* cb)(uint32_t unique, const char *name, void *ptr), void *ptr);
uint32_t get_root_network_resources();
void browse_every_network_service_resource(unsigned int service, void (* cb)(uint32_t unique, struct network_resource_s *nr, void *ptr), void *ptr);

#endif
