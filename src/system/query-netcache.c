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

#include "netcache.h"
#include "receive.h"
#include "query.h"
#include "query-filter.h"
#include "query-netcache-filter.h"
#include "netcache-write.h"

static unsigned int allflags = (OSNS_NETCACHE_QUERY_FLAG_LOCALHOST | OSNS_NETCACHE_QUERY_FLAG_PRIVATE | OSNS_NETCACHE_QUERY_FLAG_DNSSD | OSNS_NETCACHE_QUERY_FLAG_DNS |
				OSNS_NETCACHE_QUERY_FLAG_CREATETIME_AFTER | OSNS_NETCACHE_QUERY_FLAG_CHANGETIME_AFTER);

static unsigned int allvalid = (OSNS_NETCACHE_QUERY_ATTR_IPV4 | OSNS_NETCACHE_QUERY_ATTR_IPV6 | OSNS_NETCACHE_QUERY_ATTR_DNSHOSTNAME | OSNS_NETCACHE_QUERY_ATTR_DNSDOMAIN | OSNS_NETCACHE_QUERY_ATTR_PORT |
				OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY | OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE | OSNS_NETCACHE_QUERY_ATTR_SERVICE | OSNS_NETCACHE_QUERY_ATTR_TRANSPORT | OSNS_NETCACHE_QUERY_ATTR_CREATETIME);

static unsigned int default_valid = (OSNS_NETCACHE_QUERY_ATTR_IPV4 | OSNS_NETCACHE_QUERY_ATTR_IPV6 | OSNS_NETCACHE_QUERY_ATTR_PORT | OSNS_NETCACHE_QUERY_ATTR_COMM_FAMILY | OSNS_NETCACHE_QUERY_ATTR_COMM_TYPE | OSNS_NETCACHE_QUERY_ATTR_SERVICE | OSNS_NETCACHE_QUERY_ATTR_TRANSPORT | OSNS_NETCACHE_QUERY_ATTR_CREATETIME);

static unsigned int cb_read_netcache(struct openquery_request_s *request, struct readquery_request_s *readquery)
{
    struct query_netcache_attr_s *filter=request->filter.netcache;
    struct network_record_s *record=find_network_record(readquery->offset, 0);
    struct attr_buffer_s ab=INIT_ATTR_BUFFER_NOWRITE;

    if (request->valid==0) {

	request->valid=default_valid;
	logoutput_debug("cb_read_netcache: taking default valid %i", request->valid);

    } else {

	logoutput_debug("cb_read_netcache: client specified valid %i", request->valid);

    }

    set_attr_buffer_write(&ab, readquery->buffer, readquery->size);

    while (record) {

	if ((* request->cb_filter)(request, (void *) record)==1) {
	    struct attr_buffer_s tmp=INIT_ATTR_BUFFER_NOWRITE;
	    unsigned int valid=request->valid;
	    unsigned int len=write_netcache_record_buffer(record, &tmp, &valid); /* get the required length using a "nowarite" attr buffer */

	    if ((len + 2) > ab.left) {

		logoutput_debug("cb_read_netcache: no space in buffer for next record");
		break;

	    }

	    (* ab.ops->rw.write.write_uint16)(&ab, (short) len);
	    len=write_netcache_record_buffer(record, &ab, &valid);
	    readquery->count++;

	}

	logoutput_debug("cb_read_netcache: next");
	record=get_next_network_record(record);
	if (record) readquery->offset=record->offset;

    }

    logoutput_debug("cb_read_netcache: pos %u", ab.pos);
    return ab.pos;

}

static void cb_close_netcache(struct openquery_request_s *request)
{

    if (request->filter.netcache) {

	free(request->filter.netcache);
	request->filter.netcache=NULL;

    }

}

/*
    data looks like:
    uint32				flags
    uint32				valid
    record string
	QUERYATTR			attr to filter the result
*/

int process_openquery_netcache(struct osns_receive_s *r, uint32_t id, char *data, unsigned int size, struct openquery_request_s *request)
{
    struct query_netcache_attr_s *filter=NULL;
    unsigned int flags=0;
    unsigned int len=0;
    unsigned int valid=0;
    unsigned int pos=0;

    logoutput_debug("process_openquery_netcache: id %u size %u", id, size);

    /* the filter should not be defined here ... */
    if (request->filter.netcache) return OSNS_STATUS_SYSTEMERROR;

    /* prepare a query in netcache
	filter is in data */

    if (size<10) return OSNS_STATUS_PROTOCOLERROR;
    flags=get_uint32(&data[pos]);
    pos+=4;
    valid=get_uint32(&data[pos]);
    pos+=4;
    len=get_uint16(&data[pos]); /* length of filter record */
    pos+=2;

    request->flags = (flags & allflags);
    request->valid = (valid & allvalid);

    if (request->valid != valid) {

	logoutput_warning("process_openquery_netcache: not all valid flags reckognized (%i, %i supported)", valid, request->valid);

    }

    logoutput_debug("process_openquery_netcache: len %u valid %u flags %u", len, valid, flags);

    if (len==0) {

	/* no filter specified */

	if (request->flags & (OSNS_NETCACHE_QUERY_FLAG_CREATETIME_AFTER | OSNS_NETCACHE_QUERY_FLAG_CHANGETIME_AFTER)) {

	    /* flags are invalid since no createtime/chamgetime is specified ... */

	    logoutput_warning("process_openquery_netcache: error flags createtime/changetime after specified but no filter data");
	    return OSNS_STATUS_INVALIDFLAGS;

	}

    } else {

	if (10 + len > size) return OSNS_STATUS_PROTOCOLERROR;
	if (read_netcache_query_filter(&data[pos], len, request)==-1) {

	    if (request->filter.netcache) {

		free(request->filter.netcache);
		request->filter.netcache=NULL;

	    }

	    logoutput_warning("process_openquery_netcache: error reading/processing data/filter");
	    return OSNS_STATUS_PROTOCOLERROR;

	}

    }

    request->cb_read=cb_read_netcache;
    request->cb_close=cb_close_netcache;
    return 0;

}

unsigned int get_default_netcache_valid()
{
    return default_valid;
}
