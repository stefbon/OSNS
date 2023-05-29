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

#include "cache.h"

static int add_net2local_map_byname(struct net_userscache_s *cache, struct net_entity_s *entity)
{
    struct net_domain_s *domain=NULL;
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) cache->buffer;
    unsigned int error=EIO;
    struct net_ent2local_s *ent2local=NULL;
    unsigned int flags=0;

    if (entity->net.domain.ptr==NULL || entity->net.domain.len==0) {

	/* no domain: are users from the server localhost */
	flags=NET_DOMAIN_FLAG_LOCALHOST;
	if (cache->localhost) domain=cache->localhost;

    }

    if (domain==NULL) {
	struct name_s lookupname=INIT_NAME;

	if ((flags & NET_DOMAIN_FLAG_LOCALHOST)==0) set_name_from(&lookupname, 's', &entity->net.domain);
	domain=find_domain_batch(sl, &lookupname, &error);
	if (domain) logoutput_debug("add_net2local_map_byname: domain %.*s found", entity->net.domain.len, entity->net.domain.ptr);

    }

    if (domain==NULL) {

	domain=create_net_domain(&entity->net.domain, flags, NET_NUMBER_REMOTE_USERS_DEFAULT, NET_NUMBER_REMOTE_GROUPS_DEFAULT);

	if (domain) {

	    if (insert_domain_batch(sl, domain, &error)==domain) {

		logoutput_debug("add_net2local_map_byname: added domain %.*s", entity->net.domain.len, entity->net.domain.ptr);

	    } else {

		logoutput_debug("add_net2local_map_byname: unable to add localhost domain, error %i (%s)", error, strerror(error));
		free_net_domain(&domain);
		return -1;

	    }

	} else {

	    logoutput_debug("add_net2local_map_byname: unable to create localhost domain, error %i (%s)", error, strerror(error));
	    return -1;

	}

	if (flags & NET_DOMAIN_FLAG_LOCALHOST) cache->localhost=domain;

    }

    if (entity->flags & NET_ENTITY_FLAG_USER) {

	sl=domain->u_sl;

    } else if (entity->flags & NET_ENTITY_FLAG_GROUP) {

	sl=domain->g_sl;

    } else {

	return -1;

    }

    ent2local=create_ent2local(entity);

    if (ent2local) {

	if (ent2local==insert_ent2local_batch_byname(sl, ent2local, &error)) {

	    logoutput_debug("add_net2local_map_byname: added entity %.*s to domain %.*s", entity->net.name.len, entity->net.name.ptr, entity->net.domain.len, entity->net.domain.ptr);

	} else {

	    logoutput_debug("add_net2local_map_byname: error %i adding entity %.*s (%s)", error, entity->net.name.len, entity->net.name.ptr, strerror(error));
	    free_ent2local(&ent2local);
	    return -1;

	}

    } else {

	logoutput_debug("add_net2local_map_byname: error %i adding entity %.*s (%s)", error, entity->net.name.len, entity->net.name.ptr, strerror(error));
	return -1;

    }

    return 0;
}

static void find_ent2local_byname(struct net_userscache_s *cache, struct net_entity_s *entity, size_t offset, unsigned int *error)
{
    struct net_domain_s *domain=NULL;
    struct net_ent2local_s *ent2local=NULL;
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) cache->buffer;
    unsigned int flags=0;

    *error=ENOENT;

    if (entity->net.domain.ptr==NULL || entity->net.domain.len==0) {

	/* no domain: are users from the server localhost */

	if (cache->localhost==NULL) goto notfound;
	domain=cache->localhost;
	flags=NET_DOMAIN_FLAG_LOCALHOST;

    } else {
	struct name_s lookupname=INIT_NAME;

	/* only here when there is a valid domain (not empty) */

	set_name_from(&lookupname, 's', &entity->net.domain);
	domain=find_domain_batch(sl, &lookupname, error);

    }

    if (domain) {
	struct name_s lookupname=INIT_NAME;

	logoutput_debug("find_ent2local_byname: domain %.*s found", entity->net.domain.len, entity->net.domain.ptr);

	sl=(struct sl_skiplist_s *)((char *) domain + offset);
	set_name_from(&lookupname, 's', &entity->net.name);
	find_ent2local_batch_shared(sl, (void *) &lookupname, entity, error);
	return;

    }

    notfound:

    if (flags & NET_DOMAIN_FLAG_LOCALHOST) {

	logoutput_warning("find_ent2local_byname: %s %.*s not found", ((entity->flags & NET_ENTITY_FLAG_USER) ? "user" : "group"), entity->net.name.ptr, entity->net.name.len);

    } else {

	logoutput_warning("find_ent2local_byname: %s %.*s@%.*s not found", ((entity->flags & NET_ENTITY_FLAG_USER) ? "user" : "group"), entity->net.name.ptr, entity->net.name.len, entity->net.domain.len, entity->net.domain.ptr);

    }

    return NULL;

}

static void find_user_ent2local_byname(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *error)
{
    find_ent2local_byname(cache, entity, offsetof(struct net_domain_s, u_sl), error);
}

static void find_group_ent2local_byname(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *error)
{
    find_ent2local_byname(cache, entity, offsetof(struct net_domain_s, g_sl), error);
}

static void clear_net_userscache_byname(struct net_userscache_s *cache)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) cache->buffer;
    struct list_header_s *h=&sl->header;
    struct list_element_s *list=NULL;

    clear_sl_skiplist(sl);

    /* walk every domain*/

    list=remove_list_head(h);

    while (list) {
	struct net_domain_s *domain=(struct net_domain_s *) ((char *) list - offsetof(struct net_domain_s, list));

	free_net_domain(&domain);
	list=remove_list_head(h);

    }

    free_sl_skiplist(sl);

}

/*
    with name resolution domains are part of the full name
    like joe@example.org
    first step is to lookup the example.org domain and then the user joe (which is user of the example.org domain)

    the lookup method like first looking up the directory, and in that directory the file
*/

static struct net_userscache_s *create_net_userscache_name(unsigned int guessed_count)
{
    struct net_userscache_s *cache=NULL;
    unsigned char nrlanes=0;
    unsigned short prob=get_default_sl_prob();
    unsigned int size=0;

    nrlanes=estimate_sl_lanes(guessed_count, prob);
    size = get_size_sl_skiplist(&nrlanes);

    cache=malloc(sizeof(struct net_userscache_s) + size);

    if (cache) {
	struct sl_skiplist_s *sl=NULL;

	memset(cache, 0, sizeof(struct net_userscache_s) + size);

	cache->flags=(NET_USERSCACHE_FLAG_ALLOC | NET_USERSCACHE_FLAG_INIT);
	cache->localhost=NULL;
	cache->size=size;

	sl=(struct sl_skiplist_s *) cache->buffer;

	if (create_sl_skiplist(sl, prob, size, nrlanes)) {

	    logoutput_debug("create_net_userscache_common: sl skiplist created (%u lanes %u count)", nrlanes, guessed_count);

	} else {

	    logoutput_debug("create_net_userscache_common: unable to create sl skiplist (%i lanes %u count)", nrlanes, guessed_count);
	    free(cache);
	    cache=NULL;

	}

    }

    return cache;

}

/* search through cache using ids */

static int add_net2local_map_byid(struct net_userscache_s *cache, struct net_entity_s *entity)
{
    struct net_domain_s *domain=(struct net_domain_s *) cache->buffer;
    unsigned int errcode=0;
    struct sl_skiplist_s *sl=NULL;
    struct net_ent2local_s *ent2local=NULL;

    if (entity->flags & NET_ENTITY_FLAG_USER) {

	sl=domain->u_sl;

    } else if (entity->flags & NET_ENTITY_FLAG_GROUP) {

	sl=domain->g_sl;

    } else {

	return -1;

    }

    ent2local=create_ent2local(entity);

    if (ent2local) {

	if (ent2local==insert_ent2local_batch_byid(sl, ent2local, &errcode)) {

	    logoutput_debug("add_net2local_map_byname: added entity %.*s to domain %.*s", entity->net.name.len, entity->net.name.ptr, entity->net.domain.len, entity->net.domain.ptr);

	} else {

	    logoutput_debug("add_net2local_map_byname: error %i adding entity %.*s (%s)", errcode, entity->net.name.len, entity->net.name.ptr, strerror(errcode));
	    free_ent2local(&ent2local);
	    return -1;

	}

    } else {

	errcode=ENOMEM;
	logoutput_debug("add_net2local_map_byname: error %i adding entity %.*s (%s)", errcode, entity->net.name.len, entity->net.name.ptr, strerror(errcode));
	return -1;

    }

    return 0;

}

static void find_user_ent2local_byid(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *err)
{
    struct net_domain_s *domain=(struct net_domain_s *) cache->buffer;
    struct sl_skiplist_s *sl=domain->u_sl;
    find_ent2local_batch_shared(sl, &entity->net.id, entity, err);
}

static void find_group_ent2local_byid(struct net_userscache_s *cache, struct net_entity_s *entity, unsigned int *err)
{
    struct net_domain_s *domain=(struct net_domain_s *) cache->buffer;
    struct sl_skiplist_s *sl=domain->g_sl;
    find_ent2local_batch_shared(sl, &entity->net.id, entity, err);
}

static struct net_userscache_s *create_net_userscache_byid()
{
    struct net_userscache_s *cache=NULL;
    unsigned int size=0;
    unsigned int u_size=0;
    unsigned int g_size=0;

    size=get_size_net_domain(NET_NUMBER_REMOTE_USERS_DEFAULT, NET_NUMBER_REMOTE_GROUPS_DEFAULT, &u_size, &g_size);
    cache=malloc(sizeof(struct net_userscache_s) + size);

    if (cache) {
	struct net_domain_s *domain=NULL;

	memset(cache, 0, sizeof(struct net_userscache_s) + size);

	cache->flags=(NET_USERSCACHE_FLAG_ALLOC | NET_USERSCACHE_FLAG_INIT);
	cache->localhost=NULL;
	cache->size=size;

	domain=(struct net_domain_s *) cache->buffer;
	domain->flags = NET_DOMAIN_FLAG_ALLOC;
	domain->size=u_size + g_size;

	if (init_net_domain(domain, u_size, g_size, NULL, NET_IDMAPPING_FLAG_MAPBYID)==-1) {

	    free(cache);
	    return NULL;

	}

	cache->localhost=domain;

    }

    return cache;
}

static void clear_net_userscache_byid(struct net_userscache_s *cache)
{
    struct net_domain_s *domain=(struct net_domain_s *) cache->buffer;
    free_net_domain(&domain);
}

struct net_userscache_s *create_net_userscache(unsigned int flags)
{
    struct net_userscache_s *cache=NULL;

    if (flags & NET_IDMAPPING_FLAG_MAPBYNAME) {

	cache=create_net_userscache_name(10);

	if (cache) {

	    cache->add_net2local_map=add_net2local_map_byname;
	    cache->clear=clear_net_userscache_byname;
	    cache->find_user_ent2local=find_user_ent2local_byname;
	    cache->find_group_ent2local=find_group_ent2local_byname;

	}

    } else if (flags & NET_IDMAPPING_FLAG_MAPBYID) {

	cache=create_net_userscache_byid();

	if (cache) {

	    cache->add_net2local_map=add_net2local_map_byid;
	    cache->clear=clear_net_userscache_byid;
	    cache->find_user_ent2local=find_user_ent2local_byid;
	    cache->find_group_ent2local=find_group_ent2local_byid;

	}

    }

    return cache;

}
