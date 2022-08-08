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
#include "libosns-threads.h"
#include "libosns-interface.h"

#include "mapping.h"
#include "domain.h"
#include "entity.h"

int add_net2local_map(struct net_userscache_s *cache, struct net_entity_s *entity)
{
    struct net_domain_s *domain=NULL;
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) cache->buffer;
    unsigned int error=EIO;
    struct net_ent2local_s *ent2local=NULL;
    unsigned int flags=0;
    struct name_s name=INIT_NAME;

    if (entity->remote.domain.ptr==NULL || entity->remote.domain.len==0) {

	/* no domain: are users from the server localhost */

	flags=NET_DOMAIN_FLAG_LOCALHOST;
	if (cache->localhost) domain=cache->localhost;

    }

    if (domain==NULL) {
	struct name_s lookupname=INIT_NAME;

	if ((flags & NET_DOMAIN_FLAG_LOCALHOST)==0) set_name_from(&lookupname, 's', &entity->remote.domain);
	domain=find_domain_batch(sl, &lookupname, &error);

	if (domain) {

	    logoutput("add_net2local_map: domain %.*s found", entity->remote.domain.len, entity->remote.domain.ptr);

	}

    }

    if (domain==NULL) {

	domain=create_net_domain(&entity->remote.domain, flags, NET_NUMBER_REMOTE_USERS_DEFAULT, NET_NUMBER_REMOTE_GROUPS_DEFAULT);

	if (domain) {

	    if (insert_domain_batch(sl, domain, &error)==domain) {

		logoutput("add_net2local_map: added domain %.*s", entity->remote.domain.len, entity->remote.domain.ptr);

	    } else {

		logoutput("add_net2local_map: failed to add localhost domain, error %i (%s)", error, strerror(error));
		free_net_domain(&domain);
		goto error;

	    }

	} else {

	    error=ENOMEM;
	    logoutput("add_net2local_map: failed to create localhost domain, error %i (%s)", error, strerror(error));
	    goto error;

	}

	if (flags & NET_DOMAIN_FLAG_LOCALHOST) {

	    cache->localhost=domain;

	}

    }

    if (entity->flags & NET_ENTITY_FLAG_USER) {

	sl=domain->u_sl;

    } else if (entity->flags & NET_ENTITY_FLAG_GROUP) {

	sl=domain->g_sl;

    } else {

	error=EINVAL;
	goto error;

    }

    set_name_from(&name, 's', &entity->remote.name);
    ent2local=find_ent2local_batch(sl, &name, &error);

    if (ent2local==NULL) {

	ent2local=create_ent2local(entity);

	if (ent2local) {

	    if (ent2local==insert_ent2local_batch(sl, ent2local, &error)) {

		logoutput("add_net2local_map: added entity %.*s to domain %.*s", entity->remote.name.len, entity->remote.name.ptr, entity->remote.domain.len, entity->remote.domain.ptr);

	    } else {

		if (error==EEXIST) {

		    logoutput("add_net2local_map: entity %.*s domain %.*s already added", entity->remote.name.len, entity->remote.name.ptr, entity->remote.domain.len, entity->remote.domain.ptr);

		} else {

		    logoutput("add_net2local_map: error %i adding entity %.*s (%s)", error, entity->remote.name.len, entity->remote.name.ptr, strerror(error));

		}

		free_ent2local(&ent2local);

	    }

	} else {

	    error=ENOMEM;
	    logoutput("add_net2local_map: error %i adding entity %.*s (%s)", error, entity->remote.name.len, entity->remote.name.ptr, strerror(error));

	}

    }

    return 0;

    error:

    return -1;

}

static struct net_ent2local_s *find_ent2local(struct net_userscache_s *cache, struct net_entity_s *entity, size_t offset, unsigned int *error)
{
    struct net_domain_s *domain=NULL;
    struct net_ent2local_s *ent2local=NULL;
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) cache->buffer;
    unsigned int flags=0;

    *error=ENOENT;

    if (entity->remote.domain.ptr==NULL || entity->remote.domain.len==0) {

	/* no domain: are users from the server localhost */

	if (cache->localhost==NULL) goto notfound;
	domain=cache->localhost;
	flags=NET_DOMAIN_FLAG_LOCALHOST;

    } else {
	struct name_s lookupname=INIT_NAME;

	/* only here when there is a valid domain (not empty) */

	set_name_from(&lookupname, 's', &entity->remote.domain);
	domain=find_domain_batch(sl, &lookupname, error);

    }

    if (domain) {
	struct name_s lookupname=INIT_NAME;

	logoutput("find_ent2local: domain %.*s found", entity->remote.domain.len, entity->remote.domain.ptr);

	sl=(struct sl_skiplist_s *)((char *) domain + offset);
	set_name_from(&lookupname, 's', &entity->remote.name);

	return find_ent2local_batch(sl, &lookupname, error);

    }

    notfound:

    if (flags & NET_DOMAIN_FLAG_LOCALHOST) {

	logoutput_warning("find_ent2local: %s %.*s not found", ((entity->flags & NET_ENTITY_FLAG_USER) ? "user" : "group"), entity->remote.name.ptr, entity->remote.name.len);

    } else {

	logoutput_warning("find_ent2local: %s %.*s@%.*s not found", ((entity->flags & NET_ENTITY_FLAG_USER) ? "user" : "group"), entity->remote.name.ptr, entity->remote.name.len, entity->remote.domain.len, entity->remote.domain.ptr);

    }

    return NULL;

}

struct net_ent2local_s *find_user_ent2local(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *error)
{
    return find_ent2local(cache, entity, offsetof(struct net_domain_s, u_sl), error);
}

struct net_ent2local_s *find_group_ent2local(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *error)
{
    return find_ent2local(cache, entity, offsetof(struct net_domain_s, g_sl), error);
}

void clear_net_userscache(struct net_userscache_s *cache)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) cache->buffer;
    struct list_header_s *h=&sl->header;
    struct list_element_s *list=NULL;

    clear_sl_skiplist(sl);

    /* walk every domain*/

    list=get_list_head(h, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct net_domain_s *domain=(struct net_domain_s *) ((char *) list - offsetof(struct net_domain_s, list));

	free_net_domain(&domain);
	list=get_list_head(h, SIMPLE_LIST_FLAG_REMOVE);

    }

    free_sl_skiplist(sl);

}

struct net_userscache_s *create_net_userscache(unsigned int nrdomains)
{
    unsigned int size=0;
    unsigned char nrlanes=0;
    unsigned short prob=get_default_sl_prob();
    struct net_userscache_s *cache=NULL;
    struct sl_skiplist_s *sl=NULL;

    if (nrdomains==0) nrdomains=10; /* make this configurable */
    nrlanes=estimate_sl_lanes(nrdomains, prob);
    size = get_size_sl_skiplist(&nrlanes);

    cache=malloc(sizeof(struct net_userscache_s) + size);

    if (cache) {

	memset(cache, 0, sizeof(struct net_userscache_s) + size);

	cache->flags = (NET_USERSCACHE_FLAG_ALLOC | NET_USERSCACHE_FLAG_INIT);
	cache->localhost=NULL;
	cache->size=size;

	sl=(struct sl_skiplist_s *) cache->buffer;

	if (create_sl_skiplist(sl, prob, size, nrlanes)) {

	    logoutput("create_net_userscache: sl skiplist created (%i lanes)", nrlanes);

	} else {

	    logoutput("create_net_userscache: unable to create sl skiplist (%i lanes)", nrlanes);
	    goto error;

	}

	if (init_sl_skiplist(sl, compare_domains, NULL, NULL, get_list_element_domain, get_logname_domain)==0) {

	    logoutput("create_net_userscache: sl skiplist initialized");

	} else {

	    logoutput("create_net_userscache: unable to initialize");
	    goto error;

	}

    }

    return cache;

    error:

    if (sl) free_sl_skiplist(sl);
    if (cache) free(cache);
    return NULL;

}
