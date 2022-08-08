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

#include "common.h"
#include "receive.h"
#include "send.h"

char *get_osns_service_name(uint16_t service)
{
    char *name="unknown";

    switch (service) {

	case OSNS_NETCACHE_SERVICE_TYPE_SSH:

	    name="ssh";
	    break;

	case OSNS_NETCACHE_SERVICE_TYPE_NFS:

	    name="nfs";
	    break;

	case OSNS_NETCACHE_SERVICE_TYPE_SFTP:

	    name="sftp";
	    break;

	case OSNS_NETCACHE_SERVICE_TYPE_SMB:

	    name="smb";
	    break;

	case OSNS_NETCACHE_SERVICE_TYPE_WEBDAV:

	    name="webdav";
	    break;


	default:

	    logoutput_debug("get_osns_service_name: service %u not found", service);

    }

    return name;

}

unsigned int create_osns_version(unsigned int major, unsigned int minor)
{

    if (((major & 0x00FF) != major) || ((minor & 0x00FF) != minor)) {

	logoutput_warning("create_osns_version: error ... major and/or minor out of range");
	return 0;

    }

    return (unsigned int) ((major << 16) + minor);
}

unsigned int get_osns_major(unsigned int version)
{
    return ((version >> 16) & 0x00FF);
}

unsigned int get_osns_minor(unsigned int version)
{
    return (version & 0x00FF);
}

unsigned int get_osns_protocol_flags(struct osns_connection_s *oc)
{
    unsigned int major=get_osns_major(oc->protocol.version);
    unsigned int flags=0;

    if (major==1) {

	flags=oc->protocol.level.one.flags;

    }

    return flags;
}
