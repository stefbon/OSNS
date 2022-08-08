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
#include "libosns-network.h"
#include "libosns-connection.h"
#include "lib/system/time.h"
#include "libosns-socket.h"
#include "libosns-threads.h"

#include "osns-protocol.h"
#include "common.h"
#include "send.h"
#include "utils.h"

void process_osns_initialization(struct osns_connection_s *oc, unsigned int flags)
{
    struct osns_receive_s *r=&oc->receive;

    logoutput_debug("process_osns_initialization: flags %u", flags);

    flags &= OSNS_INIT_ALL_FLAGS;

    if (flags & OSNS_INIT_FLAG_NETCACHE) {

	/* when getting info about network services and hosts via DNSSD for example, desired is translation into dns fqdn */
	flags |= OSNS_INIT_FLAG_DNSLOOKUP;

    }

    if (send_osns_msg_init(r, oc->protocol.version, flags)>0) {
	struct system_timespec_s expire=SYSTEM_TIME_INIT;

	get_current_time_system_time(&expire);
	system_time_add(&expire, SYSTEM_TIME_ADD_ZERO, 1); /* 1 second should be more than enough ro receive a reply from a local service ... make this configurable ?? */

	if (signal_wait_flag_set(r->signal, &oc->status, OSNS_CONNECTION_STATUS_VERSION, &expire)==0) {
	    unsigned int versionmajor=get_osns_major(oc->protocol.version);
	    unsigned int versionminor=get_osns_minor(oc->protocol.version);

	    logoutput("process_osns_initialization: received version %u:%u", versionmajor, versionminor);

	    if (versionmajor==1) {

		logoutput("process_osns_initialization: received services %u: requested %u suported", oc->protocol.level.one.flags, flags);

	    }

	}

    } else {

	logoutput_warning("process_osns_initialization: unable to send init msg");

    }

}
