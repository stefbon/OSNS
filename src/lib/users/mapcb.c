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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pwd.h>
#include <grp.h>

#include "log.h"
#include "main.h"
#include "misc.h"

#include "threads.h"
#include "workspace-interface.h"
#include "mapping.h"
#include "lookup.h"
#include "mapcb_p2l.h"
#include "mapcb_l2p.h"


/* configure the mapping of users
    some of these settings depend on the host (shared or not shared)
    others depend on the system using this mapping like sftp subsystem
    earlier versions (<=3) use id's to export users, later versions use names
    */

void set_net_entity_map_func(struct net_idmapping_s *m, unsigned int flags)
{

    flags &= (NET_IDMAPPING_FLAG_MAPBYID | NET_IDMAPPING_FLAG_MAPBYNAME);

    /* defaults:
	- strict & nonshared */

    m->lookup.lookup_user=lookup_dummy;
    m->lookup.lookup_group=lookup_dummy;

    if (m->flags & NET_IDMAPPING_FLAG_SHARED) {

	if (flags & NET_IDMAPPING_FLAG_MAPBYID) {

	    logoutput("set_net_entity_map_func: shared and by id");

	    m->mapcb.get_user_p2l=get_user_p2l_shared_byid;
	    m->mapcb.get_group_p2l=get_group_p2l_shared_byid;
	    m->mapcb.get_user_l2p=get_user_l2p_byid;
	    m->mapcb.get_group_l2p=get_group_l2p_byid;

	    m->lookup.lookup_user=lookup_user_byid_map;
	    m->lookup.lookup_group=lookup_group_byid_map;

	    m->flags |= NET_IDMAPPING_FLAG_MAPBYID;

	} else if (flags & NET_IDMAPPING_FLAG_MAPBYNAME) {

	    logoutput("set_net_entity_map_func: shared and by name");

	    m->mapcb.get_user_p2l=get_user_p2l_shared_byname;
	    m->mapcb.get_group_p2l=get_group_p2l_shared_byname;

	    if (m->flags & NET_IDMAPPING_FLAG_CLIENT) {

		m->mapcb.get_user_l2p=get_user_l2p_byname_client;
	        m->mapcb.get_group_l2p=get_group_l2p_byname_client;

	    } else {

		m->mapcb.get_user_l2p=get_user_l2p_byname_server;
	        m->mapcb.get_group_l2p=get_group_l2p_byname_server;

	    }

	    m->lookup.lookup_user=lookup_user_byname_system;
	    m->lookup.lookup_group=lookup_group_byname_system;

	    m->flags |= NET_IDMAPPING_FLAG_MAPBYNAME;

	}

    } else {

	if (flags & NET_IDMAPPING_FLAG_MAPBYID) {

	    logoutput("set_net_entity_map_func: non shared and by id");

	    m->mapcb.get_user_p2l=get_user_p2l_nonshared_byid;
	    m->mapcb.get_group_p2l=get_group_p2l_nonshared_byid;
	    m->mapcb.get_user_l2p=get_user_l2p_byid;
	    m->mapcb.get_group_l2p=get_group_l2p_byid;

	    if (m->flags & NET_IDMAPPING_FLAG_NONSTRICT) {

		m->lookup.lookup_user=lookup_user_byid_map;
		m->lookup.lookup_group=lookup_group_byid_map;

	    } else {

		m->lookup.lookup_user=lookup_user_byid_system;
		m->lookup.lookup_group=lookup_group_byid_system;

	    }

	    m->flags |= NET_IDMAPPING_FLAG_MAPBYID;

	} else if (flags & NET_IDMAPPING_FLAG_MAPBYNAME) {

	    logoutput("set_net_entity_map_func: non shared and by name");

	    m->mapcb.get_user_p2l=get_user_p2l_nonshared_byname;
	    m->mapcb.get_group_p2l=get_group_p2l_nonshared_byname;

	    if (m->flags & NET_IDMAPPING_FLAG_CLIENT) {

		m->mapcb.get_user_l2p=get_user_l2p_byname_client;
	        m->mapcb.get_group_l2p=get_group_l2p_byname_client;

	    } else {

		m->mapcb.get_user_l2p=get_user_l2p_byname_server;
	        m->mapcb.get_group_l2p=get_group_l2p_byname_server;

	    }

	    m->lookup.lookup_user=lookup_user_byname_system;
	    m->lookup.lookup_group=lookup_group_byname_system;

	    m->flags |= NET_IDMAPPING_FLAG_MAPBYNAME;

	}

    }

}

void set_net_entity_lookup_func_cached(struct net_idmapping_s *m)
{

    /* cache can be used by both name and id lookups: how it has been setup is important (esp. the compare func)*/

    /*
    m->lookup.lookup_user = lookup_user_nonshared_cache;
    m->lookup.lookup_group = lookup_group_nonshared_cache;
    */

}
