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

void get_user_l2p_byname(struct net_idmapping_s *m, struct net_entity_s *user)
{

    if (user->local.uid==0) {

	user->net.name.len=4;
	if (user->net.name.ptr) memcpy(user->net.name.ptr, "root", 4);

    } else {

	user->net.name.len=m->su.len;
	if (user->net.name.ptr) memcpy(user->net.name.ptr, m->su.name, user->net.name.len);

    }

}

void get_group_l2p_byname(struct net_idmapping_s *m, struct net_entity_s *group)
{

    if (group->local.gid==0) {

	group->net.name.len=4;
	if (group->net.name.ptr) memcpy(group->net.name.ptr, "root", 4);

    } else {

	group->net.name.len=m->sg.len;
	if (group->net.name.ptr) memcpy(group->net.name.ptr, m->sg.name, group->net.name.len);

    }

}
