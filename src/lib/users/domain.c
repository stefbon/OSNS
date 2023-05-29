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

/* callbacks for domains */

int compare_domains(struct list_element_s *list, void *b)
{
    struct name_s *name=(struct name_s *) b;
    struct net_domain_s *domain=(struct net_domain_s *) ((char *) list - offsetof(struct net_domain_s, list));

    /* names can have zero length -> domain is localhost */

    if (domain->name.len==0) return ((name->len==0) ? 0 : 1);
    return compare_names(&domain->name, name);
}

struct list_element_s *get_list_element_domain(void *b, struct sl_skiplist_s *sl)
{
    struct name_s *name=(struct name_s *) b;
    struct net_domain_s *domain=(struct net_domain_s *)((char *) name - offsetof(struct net_domain_s, name));
    return &domain->list;
}

char *get_logname_domain(struct list_element_s *l)
{
    struct net_domain_s *domain=(struct net_domain_s *) ((char *) l - offsetof(struct net_domain_s, list));
    return domain->name.name;
}

struct net_domain_s *get_next_domain(struct net_domain_s *domain)
{
    struct list_element_s *next=get_next_element(&domain->list);
    return (next) ? ((struct net_domain_s *)((char *)next - offsetof(struct net_domain_s, list))) : NULL;
}

struct net_domain_s *get_prev_domain(struct net_domain_s *domain)
{
    struct list_element_s *prev=get_prev_element(&domain->list);
    return (prev) ? ((struct net_domain_s *)((char *)prev - offsetof(struct net_domain_s, list))) : NULL;
}

int init_net_domain(struct net_domain_s *domain, unsigned int u_size, unsigned int g_size, struct ssh_string_s *tmpname, unsigned int flags)
{
    int result=0;

    if (domain->size > 0 && domain->size < u_size + g_size) {

	logoutput_warning("init_net_domain: memory required for sl u size %i g size %i too small (%i)", u_size, g_size, domain->size);
	return -1;

    }

    logoutput_debug("init_net_domain: sl u size %u g size %u", u_size, g_size);

    init_list_element(&domain->list, NULL);
    init_name(&domain->name);

    if (tmpname) {

	/* if tmpname is defined take over the data */
	if (tmpname->flags & SSH_STRING_FLAG_ALLOC) domain->flags |= NET_DOMAIN_FLAG_NAME_ALLOC;
	set_name_from(&domain->name, 's', tmpname);
	init_ssh_string(tmpname);
	calculate_nameindex(&domain->name);

    }

    if (domain->size>0) {

	/* skiplist for users */

	result=-1;
	domain->u_sl=(struct sl_skiplist_s *) domain->buffer;
	create_sl_skiplist(domain->u_sl, 0, u_size, 0);

	if (flags & NET_IDMAPPING_FLAG_MAPBYNAME) {

	    result=init_sl_skiplist(domain->u_sl, compare_ent2local_byname, get_list_element_ent2local_byname, get_logname_ent2local, NULL);

	} else if (flags & NET_IDMAPPING_FLAG_MAPBYID) {

	    result=init_sl_skiplist(domain->u_sl, compare_ent2local_byid, get_list_element_ent2local_byid, get_logname_ent2local, NULL);

	}

	if (result==-1) {

	    logoutput_warning("init_net_domain: error initializing users skiplist");
	    goto out;

	}

	/* skiplist for groups */

	result=-1;
	domain->g_sl=(struct sl_skiplist_s *) (domain->buffer + u_size);
	create_sl_skiplist(domain->g_sl, 0, g_size, 0);

	if (flags & NET_IDMAPPING_FLAG_MAPBYNAME) {

	    result=init_sl_skiplist(domain->g_sl, compare_ent2local_byname, get_list_element_ent2local_byname, get_logname_ent2local, NULL);

	} else if (flags & NET_IDMAPPING_FLAG_MAPBYID) {

	    result=init_sl_skiplist(domain->g_sl, compare_ent2local_byid, get_list_element_ent2local_byid, get_logname_ent2local, NULL);

	}

	if (result==-1) {

	    logoutput_warning("init_net_domain: error initializing group skiplist");
	    goto out;

	}

    }

    out:
    return result;

}

    /* calculate the required sizes:
	- first the nr of lanes given the expected amount of elements
	- second determine the size given the amount of lanes */

unsigned int get_size_net_domain(uint64_t u_count, uint64_t g_count, unsigned int *p_u_size, unsigned int *p_g_size)
{
    unsigned int size=0;
    unsigned short prob=get_default_sl_prob();
    unsigned char u_nrlanes=0;
    unsigned char g_nrlanes=0;
    unsigned int u_size=0;
    unsigned int g_size=0;

    u_nrlanes=estimate_sl_lanes(u_count, prob);
    g_nrlanes=estimate_sl_lanes(g_count, prob);

    u_size=get_size_sl_skiplist(&u_nrlanes);
    g_size=get_size_sl_skiplist(&g_nrlanes);

    *p_u_size=u_size;
    *p_g_size=g_size;

    return (sizeof(struct net_domain_s) + u_size + g_size);
}

struct net_domain_s *create_net_domain(struct ssh_string_s *name, unsigned int flags, uint64_t u_count, uint64_t g_count)
{
    unsigned int size=0;
    unsigned int u_size=0;
    unsigned int g_size=0;
    struct net_domain_s *domain=NULL;
    struct ssh_string_s tmpname=SSH_STRING_INIT;
    struct ssh_string_s *dummy=NULL;

    if ((flags & NET_DOMAIN_FLAG_LOCALHOST)==0) {
	struct ssh_string_s *tmp=&tmpname;

	if (ssh_string_isempty(name)) return NULL;
	logoutput("create_net_domain: name %.*s", name->len, name->ptr);

	if (create_copy_ssh_string(&tmp, name)==-1) {

	    logoutput("create_net_domain: error create copy string name %.*s", name->len, name->ptr);
	    goto error;

	}

	dummy=&tmpname;

    } else {

	logoutput("create_net_domain: for localhost");

    }

    size=get_size_net_domain(u_count, g_count, &u_size, &g_size);
    domain=malloc(size);
    if (domain==NULL) goto error;

    memset(domain, 0, size);
    domain->flags=NET_DOMAIN_FLAG_ALLOC;
    domain->size=u_size + g_size;
    if (init_net_domain(domain, u_size, g_size, dummy, NET_IDMAPPING_FLAG_MAPBYNAME)==-1) goto error;
    return domain;

    error:

    if (domain) free_net_domain(&domain);
    clear_ssh_string(&tmpname);
    return NULL;

}

struct net_domain_s *find_domain_batch(struct sl_skiplist_s *sl, struct name_s *lookupname, unsigned int *error)
{
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) lookupname, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_find(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) return (struct net_domain_s *)((char *) result.found - offsetof(struct net_domain_s, list));
    *error=(result.flags & SL_SEARCHRESULT_FLAG_ERROR) ? EIO : ENOENT;

    return NULL;
}

struct net_domain_s *insert_domain_batch(struct sl_skiplist_s *sl , struct net_domain_s *domain, unsigned int *error)
{
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) &domain->name, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_insert(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_OK) {

	*error=0;
	return (struct net_domain_s *)((char *) result.found - offsetof(struct net_domain_s, list));

    } else if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {

	*error=EEXIST;
	return (struct net_domain_s *)((char *) result.found - offsetof(struct net_domain_s, list));

    } else {

	*error=EIO;

    }

    return NULL;
}

static void clear_net_entity_sl(struct sl_skiplist_s *sl)
{
    struct list_header_s *h=&sl->header;
    struct list_element_s *list=NULL;

    clear_sl_skiplist(sl);

    list=remove_list_head(h);

    while (list) {
	struct net_ent2local_s *ent2local=(struct net_ent2local_s *) ((char *) list - offsetof(struct net_ent2local_s, list));

	free_ent2local(&ent2local);
	list=remove_list_head(h);

    }

    free_sl_skiplist(sl);

}

void free_net_domain(struct net_domain_s **p_domain)
{
    struct net_domain_s *domain=(p_domain) ? *p_domain : NULL;

    if (domain==NULL) return;

    if (domain->g_sl) clear_net_entity_sl(domain->g_sl);
    if (domain->u_sl) clear_net_entity_sl(domain->u_sl);

    if (domain->flags & NET_DOMAIN_FLAG_NAME_ALLOC) {

	if (domain->name.name) {

	    free(domain->name.name);
	    domain->name.name=NULL;

	}

	init_name(&domain->name);
	domain->flags &= ~NET_DOMAIN_FLAG_NAME_ALLOC;

    }

    if (domain->flags & NET_DOMAIN_FLAG_ALLOC) {

	free(domain);
	*p_domain=NULL;

    }

}
