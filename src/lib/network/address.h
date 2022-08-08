/*
  2017 Stef Bon <stefbon@gmail.com>

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

*/

#ifndef _LIB_NETWORK_ADDRESS_H
#define _LIB_NETWORK_ADDRESS_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define _NETWORK_PORT_TCP			SOCK_STREAM
#define _NETWORK_PORT_UDP			SOCK_DGRAM

struct network_port_s {
    unsigned int				nr;
    unsigned int				type;
};

struct network_service_s {
    struct network_port_s			port;
    unsigned int				service;
    unsigned int				transport;
};

#define HOST_HOSTNAME_FQDN_MAX_LENGTH		253
#define HOST_HOSTNAME_MAX_LENGTH		63

#define IP_ADDRESS_FAMILY_IPv4			AF_INET
#define IP_ADDRESS_FAMILY_IPv6			AF_INET6

struct ip_address_s {
    unsigned int				family;
    union {
	char					v4[INET_ADDRSTRLEN + 1];
	char					v6[INET6_ADDRSTRLEN + 1];
    } addr;
};

#define HOST_ADDRESS_FLAG_HOSTNAME		1
#define HOST_ADDRESS_FLAG_IP			2
#define HOST_ADDRESS_FLAG_CANONNAME		4
#define HOST_ADDRESS_FLAG_DNSNAME		8

struct host_address_s {
    unsigned int				flags;
    char					hostname[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
    struct ip_address_s				ip;
};

/* prototypes */

unsigned char check_family_ip_address(char *address, const char *what);

int compare_host_address(struct host_address_s *a, struct host_address_s *b);
int set_host_address(struct host_address_s *a, char *hostname, char *ipv4, char *ipv6);
void translate_context_host_address(struct host_address_s *host, char **target, unsigned int *family);
void get_host_address(struct host_address_s *a, char **hostname, char **ipv4, char **ipv6);

void init_host_address(struct host_address_s *a);
void init_network_service(struct network_service_s *s);

#define GETHOSTNAME_FLAG_IGNORE_IPv4			1
#define GETHOSTNAME_FLAG_IGNORE_IPv6			2
#define GETHOSTNAME_FLAG_IGNORE_IP			( GETHOSTNAME_FLAG_IGNORE_IPv4 | GETHOSTNAME_FLAG_IGNORE_IPv6 )

char *gethostnamefromspec(struct host_address_s *address, unsigned int flags);
char *get_hostname_addrinfo(struct ip_address_s *ip);

#endif
