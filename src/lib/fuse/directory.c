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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#include "log.h"
#include "misc.h"

#include "sl.h"
#include "list.h"
#include "dentry.h"
#include "directory.h"

extern struct directory_s *get_dummy_directory();
extern void fs_get_inode_link(struct inode_s *inode, struct inode_link_s **link);

/* callbacks for the skiplist
    compare two elements to determine the right order */

static int compare_dentry(struct list_element_s *list, void *b)
{
    int result=0;
    struct name_s *name=(struct name_s *) b;
    struct entry_s *entry=(struct entry_s *)((char *) list - offsetof(struct entry_s, list));

    logoutput_debug("compare_dentry: name %s - entry %s", name->name, entry->name.name);

    if (entry->name.index==name->index) {

	if (name->len > 6) {

	    result=(entry->name.len > 6) ? strcmp(entry->name.name + 6, name->name + 6) : -1;

	} else if (name->len==6) {

	    result=(entry->name.len>6) ? 1 : 0;

	}

    } else {

	result=(entry->name.index > name->index) ? 1 : -1;

    }

    return result;

}

static struct list_element_s *get_list_element(void *b, struct sl_skiplist_s *sl)
{
    struct name_s *name=(struct name_s *) b;
    struct entry_s *entry=(struct entry_s *)((char *) name - offsetof(struct entry_s, name));
    return &entry->list;
}

static void _delete_dentry_cb(struct list_element_s *list)
{
    struct entry_s *entry=(struct entry_s *) ((char *) list - offsetof(struct entry_s, list));

    /* set a flag entry is removed from list */
}

static void _insert_dentry_cb(struct list_element_s *list)
{
    struct entry_s *entry=(struct entry_s *) ((char *) list - offsetof(struct entry_s, list));

    /* set a flag entry is inserted in list */
}

static char *get_logname(struct list_element_s *l)
{
    struct entry_s *entry=(struct entry_s *) ((char *) l - offsetof(struct entry_s, list));

    return entry->name.name;
}

struct directory_s *get_directory_entry(struct entry_s *entry)
{
    struct list_header_s *h=entry->list.h;

    if (h) {
	char *buffer=(char *)((char *) h - offsetof(struct sl_skiplist_s, header));
	return (struct directory_s *)((char *) buffer - offsetof(struct directory_s, buffer));

    }

    return NULL;
}

struct entry_s *get_parent_entry(struct entry_s *entry)
{
    struct directory_s *directory=get_directory_entry(entry);
    struct inode_s *inode=(directory) ? directory->inode : NULL;
    return ((inode) ? inode->alias : NULL);
}

void init_directory_readlock(struct directory_s *directory, struct simple_lock_s *lock)
{
    init_simple_readlock(&directory->locking, lock);
}

void init_directory_writelock(struct directory_s *directory, struct simple_lock_s *lock)
{
    init_simple_writelock(&directory->locking, lock);
}

int lock_directory(struct directory_s *directory, struct simple_lock_s *lock)
{
    return simple_lock(lock);
}

int rlock_directory(struct directory_s *directory, struct simple_lock_s *lock)
{
    init_directory_readlock(directory, lock);
    return simple_lock(lock);
}

int wlock_directory(struct directory_s *directory, struct simple_lock_s *lock)
{
    init_directory_writelock(directory, lock);
    return simple_lock(lock);
}

int unlock_directory(struct directory_s *directory, struct simple_lock_s *lock)
{
    return simple_unlock(lock);
}

int upgradelock_directory(struct directory_s *directory, struct simple_lock_s *lock)
{
    return simple_upgradelock(lock);
}

int prelock_directory(struct directory_s *directory, struct simple_lock_s *lock)
{
    if (directory->flags & _DIRECTORY_FLAG_REMOVE) return -1;
    return simple_prelock(lock);
}

struct entry_s *get_next_entry(struct entry_s *entry)
{
    struct list_element_s *next=entry->list.n;
    return (next) ? ((struct entry_s *)((char *)next - offsetof(struct entry_s, list))) : NULL;
}

struct entry_s *get_prev_entry(struct entry_s *entry)
{
    struct list_element_s *prev=entry->list.p;
    return (prev) ? ((struct entry_s *)((char *)prev - offsetof(struct entry_s, list))) : NULL;
}

/* callbacks for the skiplist */

int init_directory(struct directory_s *directory, unsigned char maxlanes)
{
    int result=0;

    logoutput("init_directory: sl maxlanes %i", maxlanes);

    directory->synctime.tv_sec=0;
    directory->synctime.tv_nsec=0;
    directory->inode=NULL;

    result=init_simple_locking(&directory->locking);

    if (result==-1) {

	logoutput_warning("init_directory: error initializing locking");
	goto out;

    }

    directory->dops=NULL;
    directory->link.type=0;
    directory->link.link.ptr=NULL;
    set_directory_pathcache(directory, "default", NULL, NULL);

    if (directory->size>0) {
	struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;

	create_sl_skiplist(sl, 0, directory->size, 0);
	result=init_sl_skiplist(sl, compare_dentry, _insert_dentry_cb, _delete_dentry_cb, get_list_element, get_logname);
	if (result==-1) logoutput_warning("init_directory: error initializing skiplist");

    }

    out:
    return result;

}

struct directory_s *get_directory_dump(struct inode_s *inode)
{
    return (struct directory_s *) inode->link.link.ptr;
}

void set_directory_dump(struct inode_s *inode, struct directory_s *d)
{
    inode->link.type=INODE_LINK_TYPE_DIRECTORY;
    inode->link.link.ptr=(void *) d;
}

struct directory_s *_create_directory(struct inode_s *inode, void (* init_cb)(struct directory_s *directory))
{
    struct directory_s *directory=NULL;
    unsigned char maxlanes=0;
    unsigned int size=get_size_sl_skiplist(&maxlanes);

    logoutput("_create_directory: inode %li size %li", inode->st.st_ino, size);

    directory=malloc(sizeof(struct directory_s) + size);
    if (directory==NULL) return NULL;

    memset(directory, 0, sizeof(struct directory_s) + size);
    directory->flags=_DIRECTORY_FLAG_ALLOC;
    directory->size=size;

    if (init_directory(directory, maxlanes)==-1) {

	free_directory(directory);
	return NULL;

    }

    directory->inode=inode;
    if (init_cb) (* init_cb)(directory);
    return directory;

}

void clear_directory(struct directory_s *directory)
{
    if (directory->size>0) {
	struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;

	free_sl_skiplist(sl);

    }

    release_directory_pathcache(directory);
    clear_simple_locking(&directory->locking);
}

void free_directory(struct directory_s *directory)
{
    clear_directory(directory);
    if (directory->flags & _DIRECTORY_FLAG_ALLOC) free(directory);
}
