/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include <arpa/inet.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-lock.h"

#include "resource.h"
#include "network.h"
#include "create.h"
#include "localhost.h"

#define RESOURCE_HASHSIZE				64
#define RESOURCE_STATUS_LOCK_UNIQUE			1

struct localhost_resources_s {
    uint32_t							ctr;
    unsigned int						status;
    struct list_header_s					header;
    struct shared_signal_s					*signal;
    struct list_header_s					hash[RESOURCE_HASHSIZE];
};

static struct localhost_resources_s localhost;			/* "root" of all groups of resources */

struct resource_s *lookup_resource_id(uint32_t unique)
{
    unsigned int hashvalue=(unique % RESOURCE_HASHSIZE);
    void *index=NULL;
    struct resource_s *resource=NULL;
    struct list_header_s *h=NULL;;
    struct list_element_s *list=NULL;

    searchheader:

    h=&localhost.hash[hashvalue];
    read_lock_list_header(h);

    list=get_list_head(h, 0);

    while (list) {

	resource=(struct resource_s *)((char *)list - offsetof(struct resource_s, list));
	if (resource->unique==unique) break;
	list=get_next_element(list);
	resource=NULL;

    }

    read_unlock_list_header(h);
    return resource;

}

void add_resource_hash(struct resource_s *r)
{
    unsigned int hashvalue=(r->unique % RESOURCE_HASHSIZE);
    struct list_header_s *h=&localhost.hash[hashvalue];

    logoutput_debug("add_resource_hash: r type %s unique %u", r->name, r->unique);

    write_lock_list_header(h);
    add_list_element_first(h, &r->list);
    write_unlock_list_header(h);
}

uint32_t get_localhost_unique_ctr()
{
    uint32_t unique=0;

    signal_lock_flag(localhost.signal, &localhost.status, RESOURCE_STATUS_LOCK_UNIQUE);
    unique=localhost.ctr;
    localhost.ctr++;
    signal_unlock_flag(localhost.signal, &localhost.status, RESOURCE_STATUS_LOCK_UNIQUE);

    return unique;
}

void remove_resource_hash(struct resource_s *resource)
{
    unsigned int hashvalue=(resource->unique % RESOURCE_HASHSIZE);
    struct list_header_s *h=&localhost.hash[hashvalue];

    write_lock_list_header(h);
    remove_list_element(&resource->list);
    write_unlock_list_header(h);
}

void increase_refcount_resource(struct resource_s *r)
{
    struct list_element_s *list=&r->list;

    write_lock_list_element(list);
    r->refcount++;
    write_unlock_list_element(list);
}

void decrease_refcount_resource(struct resource_s *r)
{
    struct list_element_s *list=&r->list;

    write_lock_list_element(list);
    if (r->refcount>0) r->refcount--;
    write_unlock_list_element(list);
}

static inline struct resource_s *get_resource_from_list(struct list_element_s *list)
{
    return (struct resource_s *)((char *)list - offsetof(struct resource_s, list));
}

/* get nect resource in the hash table */

struct resource_s *get_next_hashed_resource(struct resource_s *resource, unsigned int flags)
{
    unsigned int hashvalue=0;
    struct list_element_s *list=NULL;
    struct list_header_s *h=NULL;

    if (resource) {

	hashvalue=(resource->unique % RESOURCE_HASHSIZE);
	h=&localhost.hash[hashvalue];
	list=&resource->list;

	read_lock_list_header(h);

	if (list->h==h) {

	    if (flags & GET_RESOURCE_FLAG_UPDATE_USE) decrease_refcount_resource(resource);
	    list=get_next_element(list);
	    resource=NULL;

	    if (list) {

		resource=get_resource_from_list(list);
		if (flags & GET_RESOURCE_FLAG_UPDATE_USE) increase_refcount_resource(resource);
		read_unlock_list_header(h);
		return resource;

	    }

	} else {

	    read_unlock_list_header(h);
	    return NULL;

	}

	read_unlock_list_header(h);
	hashvalue++;

    }

    while (hashvalue < RESOURCE_HASHSIZE) {

	h=&localhost.hash[hashvalue];
	read_lock_list_header(h);
	list=get_list_head(h, 0);

	if (list) {

	    resource=get_resource_from_list(list);
	    if (flags & GET_RESOURCE_FLAG_UPDATE_USE) increase_refcount_resource(resource);
	    read_unlock_list_header(h);
	    return resource;

	}

	read_unlock_list_header(h);
	hashvalue++;

    }

    return NULL;


}

void init_localhost_resources(struct shared_signal_s *signal)
{

    /* the root for all resouce groups */

    memset(&localhost, 0, sizeof(struct localhost_resources_s));

    localhost.ctr=1;
    localhost.status=0;
    init_list_header(&localhost.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    localhost.signal=(signal ? signal : get_default_shared_signal());
    for (unsigned int i=0; i<RESOURCE_HASHSIZE; i++) init_list_header(&localhost.hash[i], SIMPLE_LIST_TYPE_EMPTY, NULL);

}

void block_delete_resources()
{
    signal_lock_flag(localhost.signal, &localhost.status, RESOURCE_STATUS_BLOCK_DELETE);
}

void unblock_delete_resources()
{
    signal_unlock_flag(localhost.signal, &localhost.status, RESOURCE_STATUS_BLOCK_DELETE);
}

static void update_single_system_timespec(struct system_timespec_s *changed, struct system_timespec_s *t)
{
    if (system_time_test_earlier(t, changed)==1) copy_system_time(t, changed);
}

void set_changed(struct resource_s *r, struct system_timespec_s *c)
{
    unsigned int *p_status=NULL;

    p_status=&r->status;

    signal_lock_flag(localhost.signal, p_status, RESOURCE_STATUS_CHANGE_TIME);
    update_single_system_timespec(c, &r->changed);
    signal_unlock_flag(localhost.signal, p_status, RESOURCE_STATUS_CHANGE_TIME);
}

void free_resource_records()
{

    for (unsigned int i=0; i<RESOURCE_HASHSIZE; i++) {
	struct list_element_s *list=get_list_head(&localhost.hash[i], SIMPLE_LIST_FLAG_REMOVE);

	while (list) {
	    struct resource_s *r=(struct resource_s *)((char *) list - offsetof(struct resource_s, list));
	    struct resource_subsys_s *subsys=r->subsys;

	    (* subsys->free)(r);
	    list=get_list_head(&localhost.hash[i], SIMPLE_LIST_FLAG_REMOVE);

	}

    }

}
