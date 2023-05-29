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
#include "lookup.h"

/* functions to get local uid/gid when the user db is SHARED */

/* lookup the local uid/gid given the id */

void get_user_p2l_shared_byid(struct net_idmapping_s *mapping, struct net_entity_s *user)
{
    unsigned int error=0;

    user->localid=mapping->unknown_uid;
    (* mapping->lookup.lookup_user)(mapping, user, &error);
}

void get_group_p2l_shared_byid(struct net_idmapping_s *mapping, struct net_entity_s *group)
{
    unsigned int error=0;

    group->localid=mapping->unknown_gid;
    (* mapping->lookup.lookup_group)(mapping, group, &error);
}

/* lookup the local uid/gid given the (protocol) name */

void get_user_p2l_shared_byname(struct net_idmapping_s *mapping, struct net_entity_s *user)
{
    char *sep=NULL;
    unsigned int error=0;

    user->localid=mapping->unknown_uid;

    /* test there is a domain part */

    sep=memchr(user->net.name.ptr, user->net.name.len, '@');

    if (sep) {

	user->net.domain.ptr=sep+1;
	user->net.domain.len=(unsigned int)(user->net.name.ptr + user->net.name.len - sep - 1);
	*sep='\0';
	user->net.name.len=(unsigned int)(sep - user->net.name.ptr);

    }

    (* mapping->lookup.lookup_user)(mapping, user, &error);

}

void get_group_p2l_shared_byname(struct net_idmapping_s *mapping, struct net_entity_s *group)
{
    char *sep=NULL;
    unsigned int error=0;

    group->localid=mapping->unknown_gid;

    /* test there is a domain part */

    sep=memchr(group->net.name.ptr, group->net.name.len, '@');

    if (sep) {

	group->net.domain.ptr=sep+1;
	group->net.domain.len=(unsigned int)(group->net.name.ptr + group->net.name.len - sep - 1);
	*sep='\0';
	group->net.name.len=(unsigned int)(sep - group->net.name.ptr);

    }

    (* mapping->lookup.lookup_group)(mapping, group, &error);

}


/* functions to get local id when the id db is NON SHARED */

void get_user_p2l_nonshared_byid(struct net_idmapping_s *mapping, struct net_entity_s *user)
{

    user->localid=mapping->unknown_uid;

    if (user->net.id==mapping->su.type.user.uid) {

	user->localid=mapping->pwd->pw_uid;

    } else if (user->net.id==0) {

	user->localid=0;

    } else {
	unsigned int error=0;

	(* mapping->lookup.lookup_user)(mapping, user, &error);

    }

}

void get_group_p2l_nonshared_byid(struct net_idmapping_s *mapping, struct net_entity_s *group)
{

    group->localid=mapping->unknown_gid;

    if (group->net.id==mapping->sg.type.group.gid) {

	group->localid=mapping->pwd->pw_gid;

    } else if (group->net.id==0) {

	group->localid=0;

    } else {
	unsigned int error=0;

	(* mapping->lookup.lookup_group)(mapping, group, &error);

    }

}

/* lookup the local uid/gid given the (protocol) name */

void get_user_p2l_nonshared_byname(struct net_idmapping_s *mapping, struct net_entity_s *user)
{
    char *sep=NULL;
    unsigned int error=0;

    user->localid=mapping->unknown_uid;
    sep=memchr(user->net.name.ptr, user->net.name.len, '@');

    if (sep) {

	/* there is a domain specified: lookup in the cache */

	user->net.domain.ptr=sep+1;
	user->net.domain.len=(unsigned int)(user->net.name.ptr + user->net.name.len - sep - 1);
	*sep='\0';
	user->net.name.len=(unsigned int)(sep - user->net.name.ptr);

	(* mapping->lookup.lookup_user)(mapping, user, &error);

    } else {

	/* no domain: try the obvious ones first */

	if (user->net.name.len==mapping->su.len && memcmp(user->net.name.ptr, mapping->su.name, mapping->su.len)==0) {

	    user->localid=mapping->pwd->pw_uid;

	} else if (user->net.name.len==4 && memcmp(user->net.name.ptr, "root", 4)==0) {

	    user->localid=0;

	} else {

	    (* mapping->lookup.lookup_user)(mapping, user, &error);

	}

    }

}

void get_group_p2l_nonshared_byname(struct net_idmapping_s *mapping, struct net_entity_s *group)
{
    char *sep=NULL;
    unsigned int error=0;

    logoutput_debug("get_group_p2l_nonshared_byname: %.*s", group->net.name.len, group->net.name.ptr);

    group->localid=mapping->unknown_gid;
    sep=memchr(group->net.name.ptr, group->net.name.len, '@');

    if (sep) {

	/* there is a domain specified: lookup */

	group->net.domain.ptr=sep+1;
	group->net.domain.len=(unsigned int)(group->net.name.ptr + group->net.name.len - sep - 1);
	*sep='\0';
	group->net.name.len=(unsigned int)(sep - group->net.name.ptr);
	(* mapping->lookup.lookup_group)(mapping, group, &error);

    } else {

	/* no domain: try the obvious ones first */

	if (group->net.name.len==mapping->sg.len && memcmp(group->net.name.ptr, mapping->sg.name, mapping->sg.len)==0) {

	    group->localid=mapping->pwd->pw_gid;

	} else if (group->net.name.len==4 && memcmp(group->net.name.ptr, "root", 4)==0) {

	    group->localid=0;

	} else {

	    (* mapping->lookup.lookup_group)(mapping, group, &error);

	}

    }

}
