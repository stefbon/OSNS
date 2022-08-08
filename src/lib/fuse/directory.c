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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-sl.h"
#include "libosns-list.h"
#include "libosns-workspace.h"

#include "dentry.h"
#include "directory.h"

/* callbacks for the skiplist
    compare two elements to determine the right order */

static int compare_dentry(struct list_element_s *list, void *b)
{
    struct name_s *name=(struct name_s *) b;
    struct entry_s *entry=(struct entry_s *)((char *) list - offsetof(struct entry_s, list));
    return compare_names(&entry->name, name);
}

static struct list_element_s *get_list_element(void *b, struct sl_skiplist_s *sl)
{
    struct name_s *name=(struct name_s *) b;
    // struct entry_s *entry=(struct entry_s *)((char *) name - offsetof(struct entry_s, name));
    struct entry_s *entry=create_entry(name);
    return ((entry) ? &entry->list : NULL);
}

static char *get_logname(struct list_element_s *l)
{
    struct entry_s *entry=(struct entry_s *) ((char *) l - offsetof(struct entry_s, list));
    return entry->name.name;
}

struct directory_s *get_upper_directory_entry(struct entry_s *entry)
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
    struct directory_s *directory=get_upper_directory_entry(entry);
    struct inode_s *inode=(directory) ? directory->inode : NULL;
    return ((inode) ? inode->alias : NULL);
}

void init_directory_readlock(struct directory_s *directory, struct osns_lock_s *lock)
{
    init_osns_readlock(&directory->locking, lock);
}

void init_directory_writelock(struct directory_s *directory, struct osns_lock_s *lock)
{
    init_osns_writelock(&directory->locking, lock);
}

int lock_directory(struct directory_s *directory, struct osns_lock_s *lock)
{
    return osns_lock(lock);
}

int rlock_directory(struct directory_s *directory, struct osns_lock_s *lock)
{
    init_directory_readlock(directory, lock);
    return osns_lock(lock);
}

int wlock_directory(struct directory_s *directory, struct osns_lock_s *lock)
{
    init_directory_writelock(directory, lock);
    return osns_lock(lock);
}

int unlock_directory(struct directory_s *directory, struct osns_lock_s *lock)
{
    return osns_unlock(lock);
}

int upgradelock_directory(struct directory_s *directory, struct osns_lock_s *lock)
{
    return osns_upgradelock(lock);
}

int prelock_directory(struct directory_s *directory, struct osns_lock_s *lock)
{
    if (directory->flags & _DIRECTORY_FLAG_REMOVE) return -1;
    return osns_prelock(lock);
}

struct entry_s *get_next_entry(struct entry_s *entry)
{
    struct list_element_s *next=get_next_element(&entry->list);
    return (next) ? ((struct entry_s *)((char *)next - offsetof(struct entry_s, list))) : NULL;
}

struct entry_s *get_prev_entry(struct entry_s *entry)
{
    struct list_element_s *prev=get_prev_element(&entry->list);
    return (prev) ? ((struct entry_s *)((char *)prev - offsetof(struct entry_s, list))) : NULL;
}

/* callbacks for the skiplist */

int init_directory(struct directory_s *directory, unsigned char maxlanes)
{
    int result=0;

    set_system_time(&directory->synctime, 0, 0);
    init_list_element(&directory->list, NULL);
    result=init_osns_locking(&directory->locking, 0);

    if (result==-1) {

	logoutput_warning("init_directory: error initializing locking");
	goto out;

    }

    directory->link.type=DATA_LINK_TYPE_DIRECTORY;
    directory->link.refcount=0;

    if (directory->size>0) {
	struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;

	create_sl_skiplist(sl, 0, directory->size, 0);
	result=init_sl_skiplist(sl, compare_dentry, get_list_element, get_logname, NULL);
	if (result==-1) logoutput_warning("init_directory: error initializing skiplist");

    }

    out:
    return result;

}

struct directory_s *_create_directory(struct inode_s *inode)
{
    struct directory_s *directory=NULL;
    unsigned char maxlanes=0;
    unsigned int size=get_size_sl_skiplist(&maxlanes);

    logoutput("_create_directory: inode %li size %li", inode->stat.sst_ino, size);

    directory=malloc(sizeof(struct directory_s) + size);
    if (directory==NULL) goto failed;

    memset(directory, 0, sizeof(struct directory_s) + size);
    directory->flags=_DIRECTORY_FLAG_ALLOC;
    directory->size=size;

    if (init_directory(directory, maxlanes)==-1) goto failed;
    directory->inode=inode;
    return directory;

    failed:

    if (directory) free_directory(directory);
    return NULL;

}

void clear_directory(struct directory_s *directory)
{
    if (directory->size>0) {
	struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;

	free_sl_skiplist(sl);

    }

    clear_osns_locking(&directory->locking);
}

void free_directory(struct directory_s *directory)
{

    if (directory && (directory->flags & _DIRECTORY_FLAG_DUMMY)==0) {

	clear_directory(directory);
	if (directory->flags & _DIRECTORY_FLAG_ALLOC) free(directory);

    }

}

static struct dops_s dummy_dops;
static struct dops_s default_dops;
static struct dops_s removed_dops;

/* DUMMY DIRECTORY OPS */

static struct directory_s *get_directory_dummy(struct workspace_mount_s *w, struct inode_s *inode, unsigned int flags, struct osns_lock_s *lock)
{
    struct directory_s *directory=NULL;

    if (flags & GET_DIRECTORY_FLAG_NOCREATE) return &w->inodes.dummy_directory;

    if (osns_upgradelock(lock)==0) {

	directory=_create_directory(inode);

	if (directory) {

	    inode->ptr=&directory->link;
	    directory->link.refcount++;
	    directory->dops=&default_dops;

	    add_list_element_first(&w->inodes.directories, &directory->list);
	    w->inodes.dummy_directory.link.refcount--;

	} else {

	    /* fall back on dummy directory */

	    directory=&w->inodes.dummy_directory;

	}

	/* done with upgrade lock */
	osns_downgradelock(lock);

    }

    return directory;
}

static struct directory_s *remove_directory_dummy(struct workspace_mount_s *w, struct directory_s *d, struct osns_lock_s *lock)
{
    return NULL;
}

static unsigned int get_count_dummy(struct directory_s *d)
{
    return 0;
}

static struct entry_s *find_entry_dummy(struct directory_s *d, struct name_s *ln, unsigned int *error, unsigned int flags)
{
    *error=ENOENT;
    return NULL;
}

static void remove_entry_dummy(struct directory_s *d, struct entry_s *e, unsigned int *error, unsigned int flags)
{
    *error=ENOTDIR;
}

static struct entry_s *insert_entry_dummy(struct directory_s *d, struct name_s *n, unsigned int *error, unsigned int flags)
{
    *error=ENOTDIR;
    return NULL;
}

static struct dops_s dummy_dops = {
    .get_directory		= get_directory_dummy,
    .remove_directory		= remove_directory_dummy,
    .get_count			= get_count_dummy,
    .find_entry			= find_entry_dummy,
    .remove_entry		= remove_entry_dummy,
    .insert_entry		= insert_entry_dummy,
};

/* DEFAULT DIRECTORY OPS */

static struct directory_s *get_directory_default(struct workspace_mount_s *w, struct inode_s *inode, unsigned int flags, struct osns_lock_s *lock)
{
    struct data_link_s *link=inode->ptr;
    return ((struct directory_s *) ((char *) link - offsetof(struct directory_s, link)));
}

static struct directory_s *remove_directory_default(struct workspace_mount_s *w, struct directory_s *directory, struct osns_lock_s *lock)
{

    if (directory->flags & _DIRECTORY_FLAG_DUMMY) return NULL;

    if (osns_upgradelock(lock)==0) {
	struct inode_s *inode=NULL;

	inode=directory->inode;
	directory->inode=NULL;
	directory->link.refcount--;

	if (directory->link.refcount<=0) {

	    directory->flags |= _DIRECTORY_FLAG_REMOVE;
	    directory->dops=&removed_dops;
	    remove_list_element(&directory->list);

	}

	if (inode) inode->ptr=NULL;
	osns_downgradelock(lock);

    }

    return directory;
}

static unsigned int get_count_default(struct directory_s *d)
{
    if (d->size>0) {
	struct sl_skiplist_s *sl=(struct sl_skiplist_s *) d->buffer;

	return sl->header.count;

    }

    return 0;
}

static struct entry_s *find_entry_default(struct directory_s *d, struct name_s *ln, unsigned int *error, unsigned int flags)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) d->buffer;
    struct sl_searchresult_s result;
    struct entry_s *e=NULL;
    unsigned int slflags=((flags & DIRECTORY_OP_FLAG_LOCKED) ? SL_SEARCHRESULT_FLAG_EXCLUSIVE : 0);

    init_sl_searchresult(&result, (void *) ln, slflags);
    sl_find(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {

	e=(struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    } else {

	*error=(result.flags & SL_SEARCHRESULT_FLAG_ERROR) ? EIO : ENOENT;

    }

    return e;
}

static void remove_entry_default(struct directory_s *d, struct entry_s *e, unsigned int *error, unsigned int flags)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) d->buffer;
    struct sl_searchresult_s result;
    unsigned int slflags=((flags & DIRECTORY_OP_FLAG_LOCKED) ? SL_SEARCHRESULT_FLAG_EXCLUSIVE : 0);

    init_sl_searchresult(&result, (void *) &e->name, slflags);
    sl_delete(sl, &result);
    *error=(result.flags & SL_SEARCHRESULT_FLAG_EXACT) ? 0 : ENOENT;
}

static struct entry_s *insert_entry_default(struct directory_s *d, struct name_s *name, unsigned int *error, unsigned int flags)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) d->buffer;
    struct entry_s *entry=NULL;
    struct sl_searchresult_s result;
    unsigned int slflags=((flags & DIRECTORY_OP_FLAG_LOCKED) ? SL_SEARCHRESULT_FLAG_EXCLUSIVE : 0);

    init_sl_searchresult(&result, (void *) name, slflags);
    sl_insert(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_OK) {

	*error=0;
	entry=(struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    } else if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {

	*error=EEXIST;
	entry=(struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    } else {

	*error=EIO;

    }

    return entry;
}

static struct dops_s default_dops = {
    .get_directory		= get_directory_default,
    .remove_directory		= remove_directory_default,
    .get_count			= get_count_default,
    .find_entry			= find_entry_default,
    .remove_entry		= remove_entry_default,
    .insert_entry		= insert_entry_default,
};

/* REMOVED DIRECTORY OPS */

static struct directory_s *remove_directory_removed(struct workspace_mount_s *w, struct directory_s *directory, struct osns_lock_s *lock)
{
    return NULL;
}

static struct dops_s removed_dops = {
    .get_directory		= get_directory_default,
    .remove_directory		= remove_directory_removed,
    .get_count			= get_count_default,
    .find_entry			= find_entry_default,
    .remove_entry		= remove_entry_default,
    .insert_entry		= insert_entry_dummy,
};

/* functions which call the right function for the directory */

struct directory_s *get_directory(struct workspace_mount_s *w, struct inode_s *inode, unsigned int flags)
{
    struct osns_lock_s rlock;
    struct directory_s *dummy=&w->inodes.dummy_directory;
    struct directory_s *directory=NULL;

    init_osns_readlock(&dummy->locking, &rlock);

    if (osns_lock(&rlock)==0) {
	struct data_link_s *link=inode->ptr;
	struct directory_s *tmp=((link && link->type==DATA_LINK_TYPE_DIRECTORY) ? ((struct directory_s *) ((char *) link - offsetof(struct directory_s, link))) : dummy);

	directory=(* tmp->dops->get_directory)(w, inode, flags, &rlock);
	osns_unlock(&rlock);

    }

    return directory;

}

struct directory_s *remove_directory(struct workspace_mount_s *w, struct inode_s *inode)
{
    struct osns_lock_s rlock;
    struct directory_s *directory=&w->inodes.dummy_directory;

    init_osns_readlock(&directory->locking, &rlock);

    if (osns_lock(&rlock)==0) {
	struct data_link_s *link=inode->ptr;
	struct directory_s *tmp=((link && link->type==DATA_LINK_TYPE_DIRECTORY) ? ((struct directory_s *) ((char *) link - offsetof(struct directory_s, link))) : directory);

	directory=(* tmp->dops->remove_directory)(w, tmp, &rlock);
	osns_unlock(&rlock);

    }

    return directory;

}

unsigned int get_directory_count(struct directory_s *d)
{
    return (* d->dops->get_count)(d);
}

struct entry_s *find_entry(struct directory_s *d, struct name_s *ln, unsigned int *error)
{
    return (* d->dops->find_entry)(d, ln, error, 0);
}

struct entry_s *find_entry_batch(struct directory_s *d, struct name_s *ln, unsigned int *error)
{
    return (* d->dops->find_entry)(d, ln, error, DIRECTORY_OP_FLAG_LOCKED);
}

void remove_entry(struct directory_s *d, struct entry_s *e, unsigned int *error)
{
    (* d->dops->remove_entry)(d, e, error, 0);
}

void remove_entry_batch(struct directory_s *d, struct entry_s *e, unsigned int *error)
{
    (* d->dops->remove_entry)(d, e, error, DIRECTORY_OP_FLAG_LOCKED);
}

struct entry_s *insert_entry(struct directory_s *d, struct name_s *name, unsigned int *error)
{
    return (* d->dops->insert_entry)(d, name, error, 0);
}

struct entry_s *insert_entry_batch(struct directory_s *d, struct name_s *name, unsigned int *error)
{
    return (* d->dops->insert_entry)(d, name, error, DIRECTORY_OP_FLAG_LOCKED);
}

void assign_directory_inode(struct workspace_mount_s *w, struct inode_s *inode)
{
    struct directory_s *d=&w->inodes.dummy_directory;
    struct osns_lock_s wlock;

    init_osns_writelock(&d->locking, &wlock);

    if (osns_lock(&wlock)==0) {

	inode->ptr=&d->link;
	d->link.refcount++;

	osns_unlock(&wlock);

    }

}

void init_dummy_directory(struct directory_s *directory)
{
    memset(directory, 0, sizeof(struct directory_s));
    directory->flags=_DIRECTORY_FLAG_DUMMY;
    directory->size=0;

    if (init_directory(directory, 0)==-1) {

	logoutput_error("init_dummy_directory: error initializing dummy directory");

    } else {

	logoutput("init_dummy_directory: initialized dummy directory");

    }

    directory->dops=&dummy_dops;
}
