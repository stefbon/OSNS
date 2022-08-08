/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-network.h"

#include "osns-protocol.h"
#include "osnsctl.h"
#include "osns/utils.h"

#include "receive.h"

void print_record_netcache(struct query_netcache_attr_s *attr)
{
    unsigned int valid=attr->valid;
    struct name_string_s *host=NULL;
    struct name_string_s *address=NULL;
    char tmpname[74];
    char tmpfamily[8];
    char tmpservice[12];

    memset(tmpname, ' ', 74);
    memset(tmpfamily, ' ', 8);
    memset(tmpservice, ' ', 12);

    /* DOMAIN */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN) {
	struct name_string_s *tmp=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSDOMAIN];

	fprintf(stdout, "%.*s\n", tmp->len, tmp->ptr);

    } else {

	fprintf(stdout, "\n");

    }

    fflush(stdout);

    /* HOSTNAME, try the hostname first, then ipv4, then ipv6 */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME) {

	host=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSHOSTNAME];
	if (host->len==0) host=NULL;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV4) {

	address=&attr->names[OSNS_NETCACHE_QUERY_INDEX_IPV4];
	if (address->len==0) address=NULL;

    }

    if (address==NULL && (valid & OSNS_NETCACHE_QUERY_ATTR_IPV6)) {

	address=&attr->names[OSNS_NETCACHE_QUERY_INDEX_IPV6];
	if (address->len==0) address=NULL;

    }

    if (host==NULL) {

	host=address;
	address=NULL;

    }

    if (host) {

	if (host->len>40) {

	    tmpname[0]='~';
	    memcpy(&tmpname[1], host->ptr, 40);

	} else {

	    memcpy(&tmpname[1], host->ptr, host->len);

	}

	if (address) memcpy(&tmpname[44], address->ptr, address->len);

    }

    fprintf(stdout, "%.*s", 74, tmpname);

    /* PORT */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_PORT) {

	fprintf(stdout, "%.*u", 6, attr->port);

    } else {

	fprintf(stdout, "      ");

    }

    fprintf(stdout, "  ");

    /* TYPE */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE) {

	if (attr->comm_type==SOCK_STREAM) {

	    memcpy(&tmpfamily[0], "stream", 6);

	} else if (attr->comm_type==SOCK_DGRAM) {

	    memcpy(&tmpfamily[0], "dgram", 5);

	}

    }

    fprintf(stdout, "%.*s ", 8, tmpfamily);

    /* SERVICE */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_SERVICE) {
	char *name=get_osns_service_name(attr->service);

	if (name) {
	    unsigned int len=strlen(name);

	    memcpy(tmpservice, name, ((len>12) ? 12 : len));

	} else {

	    memcpy(tmpservice, "unknown", 7);

	}

    }

    fprintf(stdout, "%.*s ", 12, tmpservice);

    /* TRANSPORT (if any) */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_TRANSPORT) {

	if (attr->transport==OSNS_NETCACHE_TRANSPORT_TYPE_SSH) {

	    fprintf(stdout, "ssh");

	}

    }

    fprintf(stdout, "\n");
    fflush(stdout);

}
