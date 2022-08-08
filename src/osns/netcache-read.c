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
#include "receive.h"

static int ab_read_name_string(struct attr_buffer_s *ab, struct name_string_s *name)
{
    int result=-1;

    name->len=0;
    name->ptr=NULL;

    if (ab->left>=1) {
	unsigned int len=0;

	len=(* ab->ops->rw.read.read_uchar)(ab);
	result=1;

	if (len>0 && len <= ab->left) {

	    name->ptr=(char *) &ab->buffer[ab->pos];
	    ab->pos+=len;
	    ab->left-=len;
	    result+=len;
	    name->len=len;

	} else {

	    if (len>ab->left) result=1;

	}

    }

    return result;

}

int read_record_netcache(char *data, unsigned int size, unsigned int valid, struct query_netcache_attr_s *attr)
{
    struct attr_buffer_s ab;

    set_attr_buffer_read(&ab, data, size);

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV4) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_IPV4];

	if (ab_read_name_string(&ab, name)>0) {

	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_IPV4;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_IPV4;
	    logoutput_debug("read_record_netcache: found ipv4 %.*s", name->len, name->ptr);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV6) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_IPV6];

	if (ab_read_name_string(&ab, name)>0) {

	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_IPV6;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_IPV6;
	    logoutput_debug("read_record_netcache: found ipv6 %.*s", name->len, name->ptr);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSHOSTNAME];

	if (ab_read_name_string(&ab, name)>0) {

	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME;
	    logoutput_debug("read_record_netcache: found dnshostname %.*s", name->len, name->ptr);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN) {
	struct name_string_s *name=&attr->names[OSNS_NETCACHE_QUERY_INDEX_DNSDOMAIN];

	if (ab_read_name_string(&ab, name)>0) {

	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN;
	    logoutput_debug("read_record_netcache: found dnsdomain %.*s", name->len, name->ptr);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_PORT) {

	if (ab.left>=2) {

	    attr->port=(* ab.ops->rw.read.read_uint16)(&ab);
	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_PORT;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_PORT;
	    logoutput_debug("read_record_netcache: found port %u", attr->port);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE) {

	if (ab.left>=2) {

	    attr->comm_type=(* ab.ops->rw.read.read_uint16)(&ab);
	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE;
	    logoutput_debug("read_record_netcache: found type %u", attr->comm_type);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY) {

	if (ab.left>=2) {

	    attr->comm_family=(* ab.ops->rw.read.read_uint32)(&ab);
	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY;
	    logoutput_debug("read_record_netcache: found family %u", attr->comm_family);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_SERVICE) {

	if (ab.left>=2) {

	    attr->service=(* ab.ops->rw.read.read_uint16)(&ab);
	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_SERVICE;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_SERVICE;
	    logoutput_debug("read_record_netcache: found service %u", attr->service);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_TRANSPORT) {

	if (ab.left>=2) {

	    attr->transport=(* ab.ops->rw.read.read_uint16)(&ab);
	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_TRANSPORT;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_TRANSPORT;
	    logoutput_debug("read_record_netcache: found transport %u", attr->transport);

	} else {

	    goto errorout;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_CREATETIME) {

	if (ab.left >= 12) {
	    int64_t seconds=0;
	    uint32_t nseconds=0;

	    seconds=(* ab.ops->rw.read.read_int64)(&ab);
	    nseconds=(* ab.ops->rw.read.read_uint32)(&ab);

	    set_system_time(&attr->createtime, seconds, nseconds);
	    attr->valid |= OSNS_NETCACHE_QUERY_ATTR_CREATETIME;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_CREATETIME;

	} else {

	    goto errorout;

	}

    }

    if (valid>0) {

	logoutput_warning("read_record_netcache: valid %u", valid);

    }

    return (int) ab.pos;

    errorout:
    logoutput_debug("read_record_netcache: error processing data size %i valid %i", size, valid);
    return -1;

}
