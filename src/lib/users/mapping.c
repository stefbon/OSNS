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

#include <pwd.h>
#include <grp.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"

#include "mapping.h"
#include "mapcb.h"

static pthread_mutex_t mapping_mutex=PTHREAD_MUTEX_INITIALIZER;

void init_net_idmapping(struct net_idmapping_s *mapping, struct passwd *pwd)
{

    memset(mapping, 0, sizeof(struct net_idmapping_s));

    mapping->flags=NET_IDMAPPING_FLAG_INIT;
    mapping->mutex=&mapping_mutex;
    mapping->pwd=pwd;

    init_getent_fields(&mapping->su);
    init_ssh_string(&mapping->getent_su);

    init_getent_fields(&mapping->sg);
    init_ssh_string(&mapping->getent_sg);

    mapping->unknown_uid=(uid_t) -1;
    mapping->unknown_gid=(gid_t) -1;

    set_net_entity_map_func(mapping, NET_IDMAPPING_FLAG_MAPBYID);

    /* mapping->cache=NULL; */

}

void free_net_idmapping(struct net_idmapping_s *mapping)
{

    mapping->mutex=NULL;
    mapping->pwd=NULL;

    clear_ssh_string(&mapping->getent_su);
    clear_ssh_string(&mapping->getent_sg);
    init_getent_fields(&mapping->su);
    init_getent_fields(&mapping->sg);

    // if (mapping->cache) {

	// clear_net_userscache(mapping->cache);
	// free(mapping->cache);
	// mapping->cache=NULL;

    //}

}
