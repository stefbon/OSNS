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
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "libosns-basic-system-headers.h"
#include "libosns-log.h"

#include "address.h"

unsigned char check_family_ip_address(char *address, const char *what)
{
    if (strcmp(what, "ipv4")==0) {
	struct in_addr tmp;

	return inet_pton(AF_INET, address, (void *)&tmp);

    } else if (strcmp(what, "ipv6")==0) {
	struct in6_addr tmp;

	return inet_pton(AF_INET6, address, (void *)&tmp);

    }

    return 0;

}

int compare_ip_addresses(struct ip_address_s *a, struct ip_address_s *b)
{

    if (a->family==b->family) {

        if (a->family==IP_ADDRESS_FAMILY_IPv4) {

            return memcmp(a->addr.v4, b->addr.v4, INET_ADDRSTRLEN);

        } else if (a->family==IP_ADDRESS_FAMILY_IPv6) {

            return memcmp(a->addr.v6, b->addr.v6, INET6_ADDRSTRLEN);

        }

    }

    return -1;
}

int compare_ip_address(struct ip_address_s *a, const unsigned char type, void *ptr)
{
    int result=-1;

    switch (type) {

        case 'i' :
        {
            struct ip_address_s *b=(struct ip_address_s *) ptr;

            result=compare_ip_addresses(a, b);
            break;

        }

        case 'c' :
        {
            char *tmp=(char *) ptr;
            struct ip_address_s b;

            if (check_family_ip_address(tmp, "ipv4")) {

                b.family=IP_ADDRESS_FAMILY_IPv4;
                memcpy(b.addr.v4, tmp, strlen(tmp));

            } else if (check_family_ip_address(tmp, "ipv6")) {

                b.family=IP_ADDRESS_FAMILY_IPv6;
                memcpy(b.addr.v6, tmp, strlen(tmp));

            }

            result=compare_ip_addresses(a, &b);
            break;
        }

        default :

            logoutput_warning("compare_ip_address: type %u not supported", type);

    }

    return result;
}

int set_host_address(struct host_address_s *a, char *hostname, char *ipv4, char *ipv6)
{
    int result=-1;

    if (hostname && strlen(hostname)>0) {
	unsigned int len=strlen(hostname);

	memset(a->hostname, '\0', HOST_HOSTNAME_FQDN_MAX_LENGTH + 1);

	if (len>HOST_HOSTNAME_FQDN_MAX_LENGTH) len=HOST_HOSTNAME_FQDN_MAX_LENGTH;
	memcpy(a->hostname, hostname, len);
	a->flags|=HOST_ADDRESS_FLAG_HOSTNAME;
	result=0;
	logoutput_debug("set_host_address: set hostname %s", hostname);

    }

    if (ipv4 && strlen(ipv4)>0) {

	memset(a->ip.addr.v4, '\0', INET_ADDRSTRLEN + 1);

	if (strlen(ipv4) <=INET_ADDRSTRLEN) {

	    logoutput_debug("set_host_address: set ipv4 %s", ipv4);

	    strcpy(a->ip.addr.v4, ipv4);
	    a->ip.family=IP_ADDRESS_FAMILY_IPv4;
	    a->flags|=HOST_ADDRESS_FLAG_IP;
	    result=0;

	} else {

	    logoutput_warning("set_host_address: not set ipv4 %s (too long: len %i max %i)", ipv4, strlen(ipv4), INET_ADDRSTRLEN);

	}

    }

    if (ipv6 && (a->flags & HOST_ADDRESS_FLAG_IP)==0) {

	memset(a->ip.addr.v6, '\0', INET6_ADDRSTRLEN + 1);

	if (strlen(ipv6)<=INET6_ADDRSTRLEN) {

	    strcpy(a->ip.addr.v6, ipv6);
	    a->ip.family=IP_ADDRESS_FAMILY_IPv6;
	    a->flags|=HOST_ADDRESS_FLAG_IP;
	    result=0;

	}

    }

    return result;
}

void get_host_address(struct host_address_s *a, char **hostname, char **ipv4, char **ipv6)
{
    if ((a->flags & HOST_ADDRESS_FLAG_HOSTNAME) && hostname) *hostname=a->hostname;
    if ((a->flags & HOST_ADDRESS_FLAG_IP) && (a->ip.family==IP_ADDRESS_FAMILY_IPv4) && ipv4) *ipv4=a->ip.addr.v4;
    if ((a->flags & HOST_ADDRESS_FLAG_IP) && (a->ip.family==IP_ADDRESS_FAMILY_IPv6) && ipv6) *ipv6=a->ip.addr.v6;
}

void translate_context_host_address(struct host_address_s *host, char **target, unsigned int *family)
{

    // logoutput("translate_context_host_address");

    if (target) {

	if ((host->flags & HOST_ADDRESS_FLAG_HOSTNAME) && strlen(host->hostname)>0) {

	    *target=host->hostname;
	    if (family) *family=0;

	} else {

	    if (host->ip.family==IP_ADDRESS_FAMILY_IPv4) {

		*target=host->ip.addr.v4;
		if (family) *family=IP_ADDRESS_FAMILY_IPv4;

	    } else if (host->ip.family==IP_ADDRESS_FAMILY_IPv6) {

		*target=host->ip.addr.v6;
		if (family) *family=IP_ADDRESS_FAMILY_IPv6;

	    }

	}

    }

}

int compare_host_address(struct host_address_s *a, struct host_address_s *b)
{

    if ((a->flags & HOST_ADDRESS_FLAG_HOSTNAME) && (strlen(a->hostname)>0) && (b->flags & HOST_ADDRESS_FLAG_HOSTNAME) && (strlen(b->hostname)>0)) {

	if (strcmp(a->hostname, b->hostname)==0) return 0;

    }

    if (a->ip.family==IP_ADDRESS_FAMILY_IPv4 && b->ip.family==IP_ADDRESS_FAMILY_IPv4) {

	if (strcmp(a->ip.addr.v4, b->ip.addr.v4)==0) return 0;

    }

    if (a->ip.family==IP_ADDRESS_FAMILY_IPv6 && b->ip.family==IP_ADDRESS_FAMILY_IPv6) {

	if (strcmp(a->ip.addr.v6, b->ip.addr.v6)==0) return 0;

    }

    return -1;

}

void init_host_address(struct host_address_s *a)
{
    memset(a, 0, sizeof(struct host_address_s));
    a->flags=0;
    a->ip.family=0;
}

void init_network_service(struct network_service_s *s)
{
    s->port.nr=0;
    s->port.type=0;
    s->service=0;
    s->transport=0;
}

char *gethostnamefromspec(struct host_address_s *address, unsigned int flags)
{
    char *hostname=NULL;
    struct addrinfo hint;
    struct addrinfo *ais=NULL;
    int result=0;
    char *host=NULL;

    memset(&hint, 0, sizeof(struct addrinfo));

    hint.ai_family		= AF_UNSPEC;
    hint.ai_socktype		= 0;
    hint.ai_protocol		= 0;
    hint.ai_flags		= AI_PASSIVE | AI_CANONNAME;
    hint.ai_addrlen		= 0;
    hint.ai_addr		= NULL;
    hint.ai_canonname		= NULL;
    hint.ai_next		= NULL;

    if (strlen(address->hostname)>0) {

	host=address->hostname;

    } else {

	if (address->flags & HOST_ADDRESS_FLAG_IP) {

	    if (address->ip.family & IP_ADDRESS_FAMILY_IPv4) {

		hint.ai_family=AF_INET;
		host=address->ip.addr.v4;

	    } else if (address->ip.family & IP_ADDRESS_FAMILY_IPv6) {

		hint.ai_family=AF_INET6;
		host=address->ip.addr.v6;

	    }

	}

    }

    if (host==NULL) return NULL;
    result=getaddrinfo(host, NULL, &hint, &ais);

    if (result==0) {
	struct addrinfo *ai=ais;

	while (ai) {

	    if (ai->ai_canonname) {

		if (flags & GETHOSTNAME_FLAG_IGNORE_IPv4 && check_family_ip_address(ai->ai_canonname, "ipv4")==1) goto next;
		if (flags & GETHOSTNAME_FLAG_IGNORE_IPv6 && check_family_ip_address(ai->ai_canonname, "ipv6")==1) goto next;
		hostname=strdup(ai->ai_canonname);
		if (hostname) break;

	    }

	    next:
	    ai=ai->ai_next;

	}

    } else {

	logoutput_warning("gethostnamefromip: error %i while getting hostname (%s)", result, gai_strerror(result));

    }

    if (ais) freeaddrinfo(ais);
    return hostname;
}

char *get_hostname_addrinfo(struct ip_address_s *ip)
{
    struct host_address_s address;

    memset(&address, 0, sizeof(struct host_address_s));

    address.flags = HOST_ADDRESS_FLAG_IP;
    memcpy(&address.ip, ip, sizeof(struct ip_address_s));

    return gethostnamefromspec(&address, GETHOSTNAME_FLAG_IGNORE_IP);
}
