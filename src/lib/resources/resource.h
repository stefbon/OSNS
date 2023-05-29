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

#ifndef LIB_RESOURCES_RESOURCE_H
#define LIB_RESOURCES_RESOURCE_H

#include "libosns-network.h"
#include "libosns-datatypes.h"

#define NETWORK_RESOURCE_FLAG_NODOMAIN                          (1 << 0)
#define NETWORK_RESOURCE_FLAG_SERVICE_GUESSED                   (1 << 1)
#define NETWORK_RESOURCE_FLAG_DNSSD                             (1 << 2)
#define NETWORK_RESOURCE_FLAG_LOCALHOST                         (1 << 3)
#define NETWORK_RESOURCE_FLAG_PRIVATE                           (1 << 4)
#define NETWORK_RESOURCE_FLAG_HOSTNAME_DNS                      (1 << 5)
#define NETWORK_RESOURCE_FLAG_HOSTNAME_ADDRINFO                 (1 << 6)
#define NETWORK_RESOURCE_FLAG_IPv4                              (1 << 7)
#define NETWORK_RESOURCE_FLAG_IPv6                              (1 << 8)
#define NETWORK_RESOURCE_FLAG_UDP                               (1 << 9)
#define NETWORK_RESOURCE_FLAG_TCP                               (1 << 10)
#define NETWORK_RESOURCE_FLAG_DOMAIN_FQDN                       (1 << 11)
#define NETWORK_RESOURCE_FLAG_REMOVED                           (1 << 12)

#define NETWORK_GROUP_FLAGS_ALL                                 ( NETWORK_RESOURCE_FLAG_NODOMAIN | NETWORK_RESOURCE_FLAG_DNSSD | NETWORK_RESOURCE_FLAG_DOMAIN_FQDN | NETWORK_RESOURCE_FLAG_REMOVED)
#define NETWORK_HOST_FLAGS_ALL                                  ( NETWORK_RESOURCE_FLAG_DNSSD | NETWORK_RESOURCE_FLAG_LOCALHOST | NETWORK_RESOURCE_FLAG_PRIVATE | NETWORK_RESOURCE_FLAG_HOSTNAME_DNS | NETWORK_RESOURCE_FLAG_HOSTNAME_ADDRINFO | NETWORK_RESOURCE_FLAG_REMOVED)
#define NETWORK_ADDRESS_FLAGS_ALL                               ( NETWORK_RESOURCE_FLAG_DNSSD | NETWORK_RESOURCE_FLAG_IPv4 | NETWORK_RESOURCE_FLAG_IPv6 | NETWORK_RESOURCE_FLAG_REMOVED)
#define NETWORK_SERVICE_FLAGS_ALL                               ( NETWORK_RESOURCE_FLAG_DNSSD | NETWORK_RESOURCE_FLAG_UDP | NETWORK_RESOURCE_FLAG_TCP | NETWORK_RESOURCE_FLAG_REMOVED)

#define NETWORK_RESOURCE_TYPE_GROUP                             1
#define NETWORK_RESOURCE_TYPE_HOST                              2
#define NETWORK_RESOURCE_TYPE_ADDRESS                           3
#define NETWORK_RESOURCE_TYPE_SERVICE                           4

struct network_resource_s {
    uint64_t                                                    id;
    uint64_t                                                    pid;
    uint32_t                                                    type;
    uint32_t                                                    flags;
    uint64_t                                                    createdate;
    uint64_t                                                    changedate;
    uint64_t                                                    processdate;
    union _network_type_u {
        char							name[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
	char					                ipv4[INET_ADDRSTRLEN + 1];
	char					                ipv6[INET6_ADDRSTRLEN + 1];
        struct network_service_s			        service;
    } data;
};

/* Prototypes */

#endif
