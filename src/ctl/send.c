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
#include "osns/send.h"

#include "options.h"
#include "receive.h"
#include "send-netcache.h"
#include "send-mountcmd.h"

void process_ctl_arguments(struct osns_connection_s *oc, struct ctl_arguments_s *arg)
{

    logoutput_debug("process_ctl_arguments: type=%u", arg->type);

    if (arg->type==OSNS_COMMAND_TYPE_LIST) {

	if (arg->cmd.list.service==OSNS_LIST_TYPE_NETCACHE) {

	    process_netcache_query(oc, arg);

	}

    } else if (arg->type==OSNS_COMMAND_TYPE_MOUNT) {

	process_mountcmd(oc, arg);

    }

}
