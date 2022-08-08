/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-connection.h"

#include "osns-protocol.h"
#include "receive.h"
#include "netcache.h"

unsigned int write_netcache_record_buffer(struct network_record_s *record, struct attr_buffer_s *ab, unsigned int *p_valid)
{
    unsigned int valid=*p_valid;
    unsigned int result=0;

    /* form of record over the connection is:
	    - uint32			offset
	    - uint32			valid
	    - ATTR			attr
    */

    (* ab->ops->rw.write.write_uint32)(ab, record->offset);
    (* ab->ops->rw.write.write_uint32)(ab, valid);

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV4) {

	if (record->ip.family==AF_INET) {
	    unsigned char len=strlen(record->ip.addr.v4);

	    (* ab->ops->rw.write.write_uchar)(ab, len);
	    (* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) record->ip.addr.v4, len);
	    result |= OSNS_NETCACHE_QUERY_ATTR_IPV4;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV6) {

	if (record->ip.family==AF_INET6) {
	    unsigned char len=strlen(record->ip.addr.v6);

	    (* ab->ops->rw.write.write_uchar)(ab, len);
	    (* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) record->ip.addr.v6, len);
	    result |= OSNS_NETCACHE_QUERY_ATTR_IPV6;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME) {

	if (record->hostname) {
	    unsigned char len=strlen(record->hostname);

	    (* ab->ops->rw.write.write_uchar)(ab, len);
	    (* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) record->hostname, len);
	    result |= OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN) {

	if (record->domain) {
	    unsigned char len=strlen(record->domain);

	    (* ab->ops->rw.write.write_uchar)(ab, len);
	    (* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) record->domain, len);
	    result |= OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_PORT) {

	(* ab->ops->rw.write.write_uint16)(ab, record->port.nr);
	result |= OSNS_NETCACHE_QUERY_ATTR_PORT;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE) {

	(* ab->ops->rw.write.write_uint16)(ab, record->port.type);
	result |= OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY) {

	(* ab->ops->rw.write.write_uint32)(ab, record->ip.family);
	result |= OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_SERVICE) {

	(* ab->ops->rw.write.write_uint16)(ab, record->service);
	result |= OSNS_NETCACHE_QUERY_ATTR_SERVICE;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_TRANSPORT) {

	(* ab->ops->rw.write.write_uint16)(ab, record->transport);
	result |= OSNS_NETCACHE_QUERY_ATTR_TRANSPORT;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_CREATETIME) {

	(* ab->ops->rw.write.write_int64)(ab, get_system_time_sec(&record->created));
	(* ab->ops->rw.write.write_uint32)(ab, get_system_time_nsec(&record->created));
	result |= OSNS_NETCACHE_QUERY_ATTR_CREATETIME;

    }

    *p_valid = result;
    return ab->pos;
}

