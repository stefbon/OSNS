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

#include "local.h"
#include "mapping.h"

/* lookup the local uid/gid given the (remote) id by just accepting it
    these "lookups" are used in the case (SHARED | NSS USERDB | MAPBYID) and (NONSHARED | NONSTRICT | MAPBYID) */

void lookup_user_byid_map(struct net_idmapping_s *mapping, struct net_entity_s *user, unsigned int *error)
{
    *error=0;
    user->local.uid=user->net.id;
}

void lookup_group_byid_map(struct net_idmapping_s *mapping, struct net_entity_s *group, unsigned int *error)
{
    *error=0;
    group->local.gid=group->net.id;
}

/* lookup the local uid/gid given the (remote) id by testing they exist
    these lookups are used in the case (NONSHARED | STRICT | MAPBYID) */

void lookup_user_byid_system(struct net_idmapping_s *mapping, struct net_entity_s *user, unsigned int *error)
{
    *error=0;
    get_local_uid_byid(user->net.id, &user->local.uid, error);
}

void lookup_group_byid_system(struct net_idmapping_s *mapping, struct net_entity_s *group, unsigned int *error)
{
    *error=0;
    get_local_gid_byid(group->net.id, &group->local.gid, error);
}

/* lookup the local uid/gid given the (remote) name in the local user/group system
    these lookups are used in the cases (SHARED | NSS USERDB | MAPBYNAME) and (NONSHARED | NONSTRICT | MAPBYNAME) */

void lookup_user_byname_system(struct net_idmapping_s *mapping, struct net_entity_s *user, unsigned int *error)
{
    unsigned int len=user->net.name.len;
    char tmp[len + 1];

    memcpy(tmp, user->net.name.ptr, len);
    tmp[len]='\0';

    lock_local_userbase();
    get_local_uid_byname(tmp, &user->local.uid, error);
    unlock_local_userbase();

}

void lookup_group_byname_system(struct net_idmapping_s *mapping, struct net_entity_s *group, unsigned int *error)
{
    unsigned int len=group->net.name.len;
    char tmp[len + 1];

    memcpy(tmp, group->net.name.ptr, len);
    tmp[len]='\0';

    lock_local_groupbase();
    get_local_gid_byname(tmp, &group->local.gid, error);
    unlock_local_groupbase();

}

/* lookup the local uid/gid given the (remote) name in a cache
    these lookups are used in the cases (SHARED | DOMAINDB | MAPBYNAME) */

/* void lookup_user_byname_cache(struct net_idmapping_s *mapping, struct net_entity_s *entity, unsigned int *error)
{
    struct net_userscache_s *cache=mapping->cache;
    struct net_ent2local_s *ent2local=NULL; // =find_user_ent2local(cache, entity, error);

    if (ent2local) {

*/

	/* per protocol the ent2local is a user related struct */
/*
	entity->local.uid=ent2local->localid.uid;

    }

}*/

/*void lookup_group_byname_cache(struct net_idmapping_s *mapping, struct net_entity_s *entity, unsigned int *error)
{
    struct net_userscache_s *cache=mapping->cache;
    struct net_ent2local_s *ent2local=NULL; // find_group_ent2local(cache, entity, error);

    if (ent2local) { */

	/* per protocol the ent2local is a group related struct */

/*	entity->local.gid=ent2local->localid.gid;

    }

}*/

/* ignore lookup the local uid/gid
    these lookups are used in the cases (SHARED | DOMAINDB) and (NONSHARED | STRICT) */

void lookup_dummy(struct net_idmapping_s *m, struct net_entity_s *e, unsigned int *error)
{}

