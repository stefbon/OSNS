/*

  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "simple-list.h"
#include "simple-hash.h"

static inline struct hash_element_s *get_hash_element(struct list_element_s *list)
{
    return (struct hash_element_s *) ( ((char *) list) - offsetof(struct hash_element_s, list));
}

static struct hash_element_s *create_hash_element()
{
    struct hash_element_s *element=NULL;

    element=malloc(sizeof(struct hash_element_s));

    if (element) {

	element->data=NULL;
	init_list_element(&element->list, NULL);

    }

    return element;

}

static void insert_in_hash(struct simple_hash_s *group, struct hash_element_s *element)
{
    unsigned int i=((*group->hashfunction)(element->data) % group->len);
    add_list_element_last(&group->hash[i], &element->list);
}

static void move_from_hash(struct simple_hash_s *group, struct hash_element_s *element)
{
    unsigned int i=((*group->hashfunction) (element->data) % group->len);
    remove_list_element(&element->list);
}

struct hash_element_s *lookup_simple_hash(struct simple_hash_s *group, void *data)
{
    struct list_element_s *list=NULL;
    unsigned int i=0;
    struct hash_element_s *element=NULL;

    i=((*group->hashfunction) (data) % group->len);

    list=get_list_head(&group->hash[i]);

    while (list) {

	element=get_hash_element(list);
	if (element->data==data) break;
	list=get_next_element(list);
	element=NULL; /* element has an invalid value... forget it otherwise this will be returned */

    }

    return element;

}

int lock_hashtable(struct osns_lock_s *lock)
{
    return osns_lock(lock);
}

int unlock_hashtable(struct osns_lock_s *lock)
{
    return osns_unlock(lock);
}

void init_rlock_hashtable(struct simple_hash_s *group, struct osns_lock_s *lock)
{
    init_osns_readlock(&group->locking, lock);
}

void init_wlock_hashtable(struct simple_hash_s *group, struct osns_lock_s *lock)
{
    init_osns_writelock(&group->locking, lock);
}

int initialize_group(struct simple_hash_s *group, unsigned int (*hashfunction) (void *data), unsigned int len, unsigned int *error)
{
    int result=0;

    if (error) *error=ENOMEM;

    if (init_osns_locking(&group->locking, 0)==-1) goto error;

    group->hashfunction=hashfunction;
    group->len=len;
    group->hash=NULL;

    if (len>0) {

	group->hash=(struct list_header_s *) malloc(len * sizeof(struct list_header_s));
	if (! group->hash) goto error;
	if (error) *error=0;
	for (unsigned int i=0;i<len;i++) init_list_header(&group->hash[i], SIMPLE_LIST_TYPE_EMPTY, NULL);

    }

    out:
    return 0;

    error:
    return -1;

}

void free_group(struct simple_hash_s *group, void (*free_data) (void *data))
{
    struct osns_lock_s wlock;

    init_wlock_hashtable(group, &wlock);
    lock_hashtable(&wlock);

    if (group->hash) {
	struct list_element_s *list=NULL;
	struct hash_element_s *element=NULL;

	for (unsigned int i=0;i<group->len;i++) {

	    list=remove_list_head(&group->hash[i]);

	    while (list) {

		element=get_hash_element(list);
		if (free_data && element->data) free_data(element->data);
		free(element);
		element=NULL;

		list=remove_list_head(&group->hash[i]);

	    }

	}

	free(group->hash);
	group->hash=NULL;

    }

    unlock_hashtable(&wlock);
    clear_osns_locking(&group->locking);

}


void *get_next_hashed_value(struct simple_hash_s *group, void **index, unsigned int hashvalue)
{
    struct hash_element_s *element=NULL;
    struct list_element_s *list=NULL;

    if (*index) {

	element=(struct hash_element_s *) *index;
	list=get_next_element(&element->list);

    } else {

	hashvalue=hashvalue % group->len;
	list=get_list_head(&group->hash[hashvalue]);

    }

    element=(list) ? get_hash_element(list) : NULL;
    *index=(void *) element;
    return (element) ? element->data : NULL;
}

void add_data_to_hash(struct simple_hash_s *group, void *data)
{
    struct hash_element_s *element=create_hash_element();

    if (element) {

	element->data=data;
	init_list_element(&element->list, NULL);
	insert_in_hash(group, element);

    }

}

void remove_data_from_hash(struct simple_hash_s *group, void *data)
{
    struct hash_element_s *element=lookup_simple_hash(group, data);

    logoutput("remove_data_from_hash");

    if (element) {

	move_from_hash(group, element);
	element->data=NULL;
	free(element);

    }

}

void remove_data_from_hash_index(struct simple_hash_s *group, void **index)
{
    struct hash_element_s *element=(struct hash_element_s *) *index;

    if (element) {

	move_from_hash(group, element);
	free(element);
	*index=NULL;

    }

}

unsigned int get_hashvalue_index(void *index, struct simple_hash_s *group)
{
    struct hash_element_s *element=(struct hash_element_s *) index;
    return (element) ? (((*group->hashfunction) (element->data)) % group->len) : 0;
}

