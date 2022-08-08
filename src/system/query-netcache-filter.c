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
#include "osns/record.h"
#include "osns/netcache-read.h"

#include "netcache.h"
#include "receive.h"
#include "query.h"
#include "query-filter.h"

/* filter netcache records based on the filter read before */

int cb_filter_custom(struct openquery_request_s *request, void *ptr)
{
    struct query_netcache_attr_s *filter=request->filter.netcache;
    struct network_record_s *record=(struct network_record_s *) ptr;
    unsigned int valid=filter->valid;
    int (* cb_compare_names)(struct name_string_s *name, char *value);
    int (* cb_compare_uint32)(uint32_t param, uint32_t value);

    if (filter->flags & OSNS_NETCACHE_QUERY_FLAG_VALID_OR) {

	cb_compare_names=compare_filter_names_or;
	cb_compare_uint32=compare_filter_uint32_or;

    } else {

	cb_compare_names=compare_filter_names_and;
	cb_compare_uint32=compare_filter_uint32_and;

    }

    logoutput_debug("cb_filter_custom");

    /* if filter on createtime is specified alway do that the same */

    if (valid & OSNS_NETCACHE_QUERY_ATTR_CREATETIME) {

	if (request->flags & OSNS_NETCACHE_QUERY_FLAG_CREATETIME_AFTER) {

	    /* only those records allowed where createtime is after the time specified in the filter */
	    if (system_time_test_earlier(&filter->createtime, &record->created)<=0) return -1;

	} else {

	    if (system_time_test_earlier(&filter->createtime, &record->created)>0) return -1;

	}

	valid &= ~OSNS_NETCACHE_QUERY_ATTR_CREATETIME;

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV4) {

	if (record->ip.family==AF_INET) {
	    int tmp=(* cb_compare_names)(&filter->names[OSNS_NETCACHE_QUERY_INDEX_IPV4], record->ip.addr.v4);

	    if (tmp==-1 || tmp==1) return tmp;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_IPV4;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_IPV6) {

	if (record->ip.family==AF_INET6) {
	    int tmp=(* cb_compare_names)(&filter->names[OSNS_NETCACHE_QUERY_INDEX_IPV6], record->ip.addr.v6);

	    if (tmp==-1 || tmp==1) return tmp;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_IPV6;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME) {

	if (record->hostname) {
	    int tmp=(* cb_compare_names)(&filter->names[OSNS_NETCACHE_QUERY_INDEX_DNSHOSTNAME], record->hostname);

	    if (tmp==-1 || tmp==1) return tmp;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME;

	}

    }

    if (valid & OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN) {

	if (record->domain) {
	    int tmp=(* cb_compare_names)(&filter->names[OSNS_NETCACHE_QUERY_INDEX_DNSDOMAIN], record->domain);

	    if (tmp==-1 || tmp==1) return tmp;
	    valid &= ~OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN;

	}

    }

    if (filter->valid & OSNS_NETCACHE_QUERY_ATTR_PORT) {
	int tmp=(* cb_compare_uint32)(record->port.nr, filter->port);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_NETCACHE_QUERY_ATTR_PORT;

    }

    if (filter->valid & OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE) {
	int tmp=(* cb_compare_uint32)(record->port.type, filter->comm_type);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE;
    }

    if (filter->valid & OSNS_NETCACHE_QUERY_ATTR_SERVICE) {
	int tmp=(* cb_compare_uint32)(record->service, filter->service);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_NETCACHE_QUERY_ATTR_SERVICE;

    }

    if (filter->valid & OSNS_NETCACHE_QUERY_ATTR_TRANSPORT) {
	int tmp=(* cb_compare_uint32)(record->transport, filter->transport);

	if (tmp==-1 || tmp==1) return tmp;
	valid &= ~OSNS_NETCACHE_QUERY_ATTR_TRANSPORT;

    }

    if (valid>0) {

	logoutput_warning("cb_filter_custom: not all required filters applied (valid=%u)", valid);

    }

    /* when here depends on the flag VALID_OR:
	- OR: no test has been a match: no success
	- AND: no test has been a reason to exclude: success */

    return ((filter->flags & OSNS_NETCACHE_QUERY_FLAG_VALID_OR) ? -1 : 1);

}

/* read filter */

int read_netcache_query_filter(char *data, unsigned int size, struct openquery_request_s *request)
{
    struct query_netcache_attr_s tmp;
    unsigned int valid_namefields=0;
    unsigned int valid_fixedfields=0;
    unsigned int pos=0;
    unsigned int valid=0;
    unsigned int flags=0;
    int len=0;
    struct query_netcache_attr_s *filter=NULL;

    flags=get_uint32(&data[pos]);
    pos+=4;
    valid=get_uint32(&data[pos]);
    pos+=4;
    if (valid==0) return 0;

    memset(&tmp, 0, sizeof(struct query_netcache_attr_s));

    valid_namefields=(valid & (OSNS_NETCACHE_QUERY_ATTR_IPV4 | OSNS_NETCACHE_QUERY_ATTR_IPV6 | OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME | OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN));

    /* read the fields with a variable width first to determine the required size of the buffer to store them */
    if (valid_namefields) {

	len=read_record_netcache(&data[pos], (size - pos), valid_namefields, &tmp);

	if (len==-1) {

	    logoutput_debug("read_netcache_query_filter: unable to read namefields ... cannot continue");
	    goto error;

	}

	valid_fixedfields=(valid &= ~valid_namefields);

    } else {

	valid_fixedfields=valid;

    }

    filter=malloc(sizeof(struct query_netcache_attr_s) + len);
    if (filter==NULL) {

	logoutput_debug("read_netcache_query_filter: unable to allocate memory ... cannot continue");
	goto error;

    }

    memset(filter, 0, sizeof(struct query_netcache_attr_s) + len);

    filter->size = (unsigned int) len;
    filter->flags = (flags & OSNS_NETCACHE_QUERY_FLAG_VALID_OR);
    filter->valid = valid;

    if (len>0) {

	/* now the values found in data, have to go to the buffer in filter AND filter has to point to them */
	memcpy(filter->buffer, &data[pos], len);
	/* this should not cause an error */
	len=read_record_netcache(filter->buffer, len, valid_namefields, filter);
	pos+=len;

    }

    if (valid_fixedfields) {

	int tmp=read_record_netcache(&data[pos], (size - pos), valid_fixedfields, filter);

	if (tmp==-1) {

	    logoutput_debug("read_netcache_query_filter: unable to read fixed fields ... cannot continue");
	    goto error;

	}

	pos+=tmp;

    }

    request->filter.netcache=filter;
    request->cb_filter=cb_filter_custom;

    return (int) pos;
    error:

    if (filter) free(filter);
    return -1;

}

