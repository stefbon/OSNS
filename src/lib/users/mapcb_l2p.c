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

/* get protocol user by id 
    no domains, no names just uids and gids */

void get_user_l2p_byid(struct net_idmapping_s *m, struct net_entity_s *user)
{
    user->net.id=m->su.type.user.uid;
    if (user->local.uid==0) user->net.id=0;
}

void get_group_l2p_byid(struct net_idmapping_s *m, struct net_entity_s *group)
{
    group->net.id=m->sg.type.group.gid;
    if (group->local.gid==0) group->net.id=0;
}

/* get protocol user by name
    no domains */

void get_user_l2p_byname_client(struct net_idmapping_s *m, struct net_entity_s *user)
{

    if (user->local.uid==0) {

	user->net.name.len=4;
	if (user->net.name.ptr) memcpy(user->net.name.ptr, "root", 4);

    } else {

	user->net.name.len=m->su.len;
	if (user->net.name.ptr) memcpy(user->net.name.ptr, m->su.name, user->net.name.len);

    }

}

void get_group_l2p_byname_client(struct net_idmapping_s *m, struct net_entity_s *group)
{

    if (group->local.gid==0) {

	group->net.name.len=4;
	if (group->net.name.ptr) memcpy(group->net.name.ptr, "root", 4);

    } else {

	group->net.name.len=m->sg.len;
	if (group->net.name.ptr) memcpy(group->net.name.ptr, m->sg.name, group->net.name.len);

    }

}

struct _write_id_ssh_string_s {
    struct net_idmapping_s 	*m;
    struct net_entity_s		*ent;
};

void _cb_id_found(char *name, void *ptr)
{
    struct _write_id_ssh_string_s *wiss=(struct _write_id_ssh_string_s *) ptr;
    struct net_entity_s *ent=wiss->ent;
    unsigned int len=strlen(name);

    /* cb when name is found for the local id
	NOTE: the buffers for writing the name to have to be large enough ( to hold at least one user/group/other name) */

    ent->net.name.len=len;
    memcpy(ent->net.name.ptr, name, len);

}

void _cb_id_notfound(void *ptr)
{
    struct _write_id_ssh_string_s *wiss=(struct _write_id_ssh_string_s *) ptr;
    struct net_entity_s *ent=wiss->ent;

    ent->net.name.len=0;
}

void get_user_l2p_byname_server(struct net_idmapping_s *m, struct net_entity_s *user)
{
    struct _write_id_ssh_string_s wiss;

    wiss.m=m;
    wiss.ent=user;

    /* 20211104: just send the local user ... no domain ... no locking */

    get_local_user_byuid(user->local.uid, _cb_id_found, _cb_id_notfound, (void *) &wiss);

}

void get_group_l2p_byname_server(struct net_idmapping_s *m, struct net_entity_s *group)
{
    struct _write_id_ssh_string_s wiss;

    wiss.m=m;
    wiss.ent=group;

    /* 20211104: just send the local group ... no domain ... no locking */

    get_local_group_bygid(group->local.gid, _cb_id_found, _cb_id_notfound, (void *) &wiss);

}
