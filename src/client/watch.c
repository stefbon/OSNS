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
#include "osns/osns.h"

#include "osns_client.h"

#include "osns/send/reply.h"
#include "osns/utils.h"
#include "osns/netcache/read.h"

/* event like a new host, a new service */

static void process_osns_netcache_event(struct osns_connection_s *oc, struct osns_in_header_s *h, struct osns_setwatch_s *watch, char *data)
{

    if (h->len>8) {
        struct osns_buffer_s ob;
        unsigned int offset=0;
        unsigned int valid=0;

        init_osns_buffer_read(&ob, data, h->len);

        offset=(* ob.ops.r.read_uint32)(&ob);
        valid=(* ob.ops.r.read_uint32)(&ob);

        if (valid>0) {
            struct query_netcache_attr_s attr;

            memset(&attr, 0, sizeof(struct query_netcache_attr_s));

            if (read_record_netcache(&ob, valid, &attr)>=0) {

                process_netcache_attr(&attr, 0);

            } else {

                logoutput_debug("process_osns_netcache_event");

            }

        }

    }

}

void process_osns_watchevent_client(struct osns_connection_s *oc, struct osns_in_header_s *h, struct osns_setwatch_s *watch, char *data)
{

    switch (watch->what) {

        case OSNS_WATCH_TYPE_NETCACHE:

            process_osns_netcache_event(oc, h, watch, data);
            break;

        default:

            logoutput_debug("process_osns_watchevent_client: type event (%u) not reckognized", watch->what);

    }

}
