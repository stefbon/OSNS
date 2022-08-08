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
#include "osns_system.h"
#include "receive.h"
#include "osns/reply.h"
#include "netcache.h"
#include "netcache-write.h"

static void process_netcache_watch_event(uint64_t unique, uint32_t offset, uint32_t watchid)
{
    struct connection_s *c=get_client_connection(unique);

    if (c) {
	struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)c - offsetof(struct osns_systemconnection_s, connection));
	struct network_record_s *record=find_network_record(offset, 0);
	unsigned int valid=get_default_netcache_valid();

	while (record) {
	    struct attr_buffer_s ab=INIT_ATTR_BUFFER_NOWRITE;
	    unsigned int len=write_netcache_record_buffer(record, &ab, &valid);
	    char buffer[len + 2];

	    set_attr_buffer_write(&ab, buffer, len);

	    (* ab.ops->rw.write.write_uint16)(&ab, (short) len);
	    len=write_netcache_record_buffer(record, &ab, &valid);

	    if (osns_reply_records(&sc->receive, watchid, 1, buffer, len+2)==-1) {

		logoutput_debug("process_netcache_watch_event");

	    }

	    record=get_next_network_record(record);

	}

    }

}


/*
    receive a request to set a watch

    name string				what

*/

void process_msg_setwatch(struct osns_receive_s *r, uint32_t id, char *data, unsigned int len)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
    struct connection_s *c=&sc->connection;
    unsigned int status=OSNS_STATUS_PROTOCOLERROR;
    struct name_string_s what=NAME_STRING_INIT;
    unsigned int pos=0;
    unsigned char tmp=0;

    if (len<=9) goto out;
    pos=read_name_string(data, len, &what);
    if (pos<=1 || pos>len) goto out;

    if (compare_name_string(&what, 'c', "netcache")==0) {

	/* set a watch on the cached network records */

	if (sc->flags & OSNS_SYSTEMCONNECTION_FLAG_WATCH_NETCACHE) {

	    status=OSNS_STATUS_EXIST;
	    goto out;

	}

	if (add_netcache_watch(c->ops.client.unique, id, process_netcache_watch_event)==0) {

	    status=OSNS_STATUS_OK;

	}

    }

    out:
    osns_reply_status(r, id, status, NULL, 0);

}
