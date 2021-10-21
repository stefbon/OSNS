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

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "log.h"
#include "misc.h"
#include "sl.h"
#include "eventloop.h"

#include "dentry.h"
#include "directory.h"
#include "workspace-interface.h"

#include "fs.h"
#include "workspace.h"
#include "utils.h"

extern void fs_inode_forget(struct inode_s *inode);

static struct directory_s dummy_directory;
static struct dops_s dummy_dops;
static struct dops_s default_dops;
static struct dops_s removed_dops;

void set_inode_directory_dummy(struct inode_s *inode)
{
    if (inode && inode->link.type==0) {

	inode->link.type=DATA_LINK_TYPE_DIRECTORY;
	inode->link.link.ptr=&dummy_directory;

    }

}

/* DUMMY DIRECTORY OPS */

static void _init_directory(struct directory_s *d)
{
    d->dops=&default_dops;
    set_directory_pathcache_x(d);
}

static struct directory_s *get_directory_dummy(struct inode_s *inode)
{
    struct directory_s *directory=_create_directory(inode, _init_directory);

    if (directory) {

	inode->link.type=DATA_LINK_TYPE_DIRECTORY;
	inode->link.link.ptr=(void *) directory;

    } else {

	directory=&dummy_directory;

    }

    return directory;
}

struct directory_s *remove_directory_dummy(struct inode_s *inode, unsigned int *error)
{
    return NULL;
}

static struct dops_s dummy_dops = {
    .get_directory		= get_directory_dummy,
    .remove_directory		= remove_directory_dummy,
};

/* DEFAULT DIRECTORY OPS */

static struct directory_s *get_directory_common(struct inode_s *inode)
{
    return (struct directory_s *) inode->link.link.ptr;
}

static struct directory_s *remove_directory_common(struct inode_s *inode, unsigned int *error)
{
    struct directory_s *directory=(struct directory_s *) inode->link.link.ptr;
    directory->flags |= _DIRECTORY_FLAG_REMOVE;
    directory->dops=&removed_dops;
    directory->inode=NULL;
    set_directory_dump(inode, get_dummy_directory());
    return directory;
}

static struct dops_s default_dops = {
    .get_directory		= get_directory_common,
    .remove_directory		= remove_directory_common,
};

/* REMOVED DIRECTORY OPS */

static struct directory_s *get_directory_removed(struct inode_s *inode)
{
    return (struct directory_s *) inode->link.link.ptr;
}

static struct directory_s *remove_directory_removed(struct inode_s *inode, unsigned int *error)
{
    *error=ENOTDIR;
    return NULL;
}

static struct dops_s removed_dops = {
    .get_directory		= get_directory_removed,
    .remove_directory		= remove_directory_removed,
};

/* simple functions which call the right function for the directory */

struct directory_s *get_directory(struct inode_s *inode)
{
    struct simple_lock_s wlock;
    struct directory_s *directory=&dummy_directory;

    logoutput_debug("get_directory: ino %li", inode->st.st_ino);

    init_simple_writelock(&directory->locking, &wlock);

    if (simple_lock(&wlock)==0) {
	struct directory_s *tmp=(struct directory_s *) inode->link.link.ptr;

	directory=(* tmp->dops->get_directory)(inode);
	simple_unlock(&wlock);

    }

    return directory;

}

struct directory_s *remove_directory(struct inode_s *inode, unsigned int *error)
{
    struct simple_lock_s wlock;
    struct directory_s *directory=&dummy_directory;

    init_simple_writelock(&directory->locking, &wlock);

    if (simple_lock(&wlock)==0) {
	struct directory_s *tmp=(struct directory_s *) inode->link.link.ptr;

	directory=(* directory->dops->remove_directory)(inode, error);
	simple_unlock(&wlock);

    }

    return directory;

}

struct entry_s *find_entry(struct directory_s *directory, struct name_s *lookupname, unsigned int *error)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) lookupname, 0);
    sl_find(sl, &result);
    if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) return (struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));
    *error=(result.flags & SL_SEARCHRESULT_FLAG_ERROR) ? EIO : ENOENT;
    return NULL;
}

void remove_entry(struct directory_s *directory, struct entry_s *entry, unsigned int *error)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) &entry->name, 0);
    sl_delete(sl, &result);
    *error=(result.flags & SL_SEARCHRESULT_FLAG_EXACT) ? 0 : ENOENT;
}

struct entry_s *insert_entry(struct directory_s *directory, struct entry_s *entry, unsigned int *error, unsigned short flags)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) &entry->name, 0);
    sl_insert(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_OK) {

	*error=0;
	return (struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    } else if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {

	*error=EEXIST;
	return (struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    } else {

	*error=EIO;

    }

    return NULL;

}

struct entry_s *find_entry_batch(struct directory_s *directory, struct name_s *lookupname, unsigned int *error)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) lookupname, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_find(sl, &result);
    if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) return (struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    *error=(result.flags & SL_SEARCHRESULT_FLAG_ERROR) ? EIO : ENOENT;
    return NULL;
}

void remove_entry_batch(struct directory_s *directory, struct entry_s *entry, unsigned int *error)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) &entry->name, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_delete(sl, &result);

    *error=(result.flags & SL_SEARCHRESULT_FLAG_EXACT) ? 0 : ENOENT;
}

struct entry_s *insert_entry_batch(struct directory_s *directory, struct entry_s *entry, unsigned int *error, unsigned short flags)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
    struct sl_searchresult_s result;

    init_sl_searchresult(&result, (void *) &entry->name, SL_SEARCHRESULT_FLAG_EXCLUSIVE);
    sl_insert(sl, &result);

    if (result.flags & SL_SEARCHRESULT_FLAG_OK) {

	*error=0;
	return (struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    } else if (result.flags & SL_SEARCHRESULT_FLAG_EXACT) {

	*error=EEXIST;
	return (struct entry_s *)((char *) result.found - offsetof(struct entry_s, list));

    } else {

	*error=EIO;

    }

    return NULL;
}

uint64_t get_directory_count(struct directory_s *d)
{

    if (d->size>0) {
	struct sl_skiplist_s *sl=(struct sl_skiplist_s *) d->buffer;

	return sl->header.count;

    }

    return 0;

}

void init_directory_calls()
{
    struct directory_s *directory=&dummy_directory;

    memset(directory, 0, sizeof(struct directory_s));
    directory->flags=_DIRECTORY_FLAG_DUMMY;
    directory->size=0;

    if (init_directory(directory, 0)==-1) {

	logoutput_error("init_directory_calls: error initializing dummy directory");

    } else {

	logoutput("init_directory_calls: initialized dummy directory");

    }

    dummy_directory.dops=&dummy_dops;

    /* make data_link point to directory self */
    dummy_directory.link.type=DATA_LINK_TYPE_DIRECTORY;
    dummy_directory.link.link.ptr=&dummy_directory;
}

struct directory_s *get_dummy_directory()
{
    return &dummy_directory;
}

/* FUNCTIONS to CREATE an entry and inode */

static struct entry_s *_cb_create_entry(struct name_s *name)
{
    return create_entry(name);
}
static struct inode_s *_cb_create_inode()
{
    struct inode_s *inode=create_inode();

    set_inode_directory_dummy(inode);
    return inode;
}

static struct entry_s *_cb_insert_entry(struct directory_s *directory, struct entry_s *entry, unsigned int flags, unsigned int *error)
{
    return insert_entry(directory, entry, error, flags);
}
static struct entry_s *_cb_insert_entry_batch(struct directory_s *directory, struct entry_s *entry, unsigned int flags, unsigned int *error)
{
    return insert_entry_batch(directory, entry, error, flags);
}
static void _cb_adjust_pathmax_default(struct create_entry_s *ce)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(ce->context);
    adjust_pathmax(workspace, ce->pathlen);
}
static void _cb_context_created(struct create_entry_s *ce, struct entry_s *entry)
{
    add_inode_context(ce->context, entry->inode);
}
static void _cb_context_found(struct create_entry_s *ce, struct entry_s *entry)
{
}
static unsigned int _cb_cache_size(struct create_entry_s *ce)
{
    return 0;
}
static int _cb_check(struct create_entry_s *ce)
{
    return 0;
}
static void _cb_cache_created(struct entry_s *entry, struct create_entry_s *ce)
{
    fill_inode_stat(entry->inode, &ce->cache.st);
    entry->inode->st.st_mode=ce->cache.st.st_mode;
    entry->inode->st.st_size=ce->cache.st.st_size;
    entry->inode->st.st_nlink=ce->cache.st.st_nlink;
}
static void _cb_cache_found(struct entry_s *entry, struct create_entry_s *ce)
{
    fill_inode_stat(entry->inode, &ce->cache.st);
    entry->inode->st.st_mode=ce->cache.st.st_mode;
    entry->inode->st.st_size=ce->cache.st.st_size;
    entry->inode->st.st_nlink=ce->cache.st.st_nlink;
}

static void _cb_created_default(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct inode_s *inode=entry->inode;
    struct directory_s *directory=get_directory(inode);

    inode->nlookup=0; /*20210401: changed from 1 to 0: setting of this should be done in the context */
    inode->st.st_nlink=1;
    get_current_time(&inode->stim); 							/* sync time */

    if (directory && directory->inode) {

	memcpy(&directory->inode->st.st_ctim, &inode->stim, sizeof(struct timespec)); 		/* change the ctime of parent directory since it's attr are changed */
	memcpy(&directory->inode->st.st_mtim, &inode->stim, sizeof(struct timespec)); 		/* change the mtime of parent directory since an entry is added */

    }

    (* ce->cb_adjust_pathmax)(ce); 							/* adjust the maximum path len */
    (* ce->cb_cache_created)(entry, ce); 						/* create the inode stat and cache */
    (* ce->cb_context_created)(ce, entry); 						/* context depending cb, like a FUSE reply and adding inode to context, set fs etc */

    if (S_ISDIR(inode->st.st_mode)) {

	inode->st.st_nlink++;
	if (directory && directory->inode) directory->inode->st.st_nlink++;
	// set_directory_dump(inode, get_dummy_directory());

    }

    set_entry_ops(entry);
    use_virtual_fs(NULL, inode);

}

static void _cb_found_default(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct stat *st=&ce->cache.st;
    struct inode_s *inode=entry->inode;

    inode->nlookup++;
    get_current_time(&inode->stim);

    logoutput("_cb_found_default: A");

    /* when just created (for example by readdir) adjust the pathcache */

    if (inode->nlookup==1) (* ce->cb_adjust_pathmax)(ce); /* adjust the maximum path len */
    logoutput("_cb_found_default: B");
    (* ce->cb_cache_found)(entry, ce); /* get/set the inode stat cache */
    logoutput("_cb_found_default: C");
    (* ce->cb_context_found)(ce, entry); /* context depending cb, like a FUSE reply and adding inode to context, set fs etc */
    logoutput("_cb_found_default: D");

}

static void _cb_error_default(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int error)
{
    if (error==0) error=EIO;
    logoutput("_cb_error_default: error %i (%s)", error, strerror(error));
}

static struct directory_s *get_directory_01(struct create_entry_s *ce)
{
    struct inode_s *inode=ce->tree.parent->inode;
    return get_directory(inode);
}

static struct directory_s *get_directory_02(struct create_entry_s *ce)
{
    return ce->tree.directory;
}

static struct directory_s *get_directory_03(struct create_entry_s *ce)
{
    struct inode_s *inode=ce->tree.opendir->inode;
    return get_directory(inode);
}

void init_create_entry(struct create_entry_s *ce, struct name_s *n, struct entry_s *p, struct directory_s *d, struct fuse_opendir_s *fo, struct service_context_s *c, struct stat *st, void *ptr)
{
    memset(ce, 0, sizeof(struct create_entry_s));

    ce->name=n;

    if (p) {

	ce->tree.parent=p;
	ce->get_directory=get_directory_01;

    } else if (d) {

	ce->tree.directory=d;
	ce->get_directory=get_directory_02;

    } else if (fo) {

	ce->tree.opendir=fo;
	ce->get_directory=get_directory_03;

    }

    ce->context=c;
    if (st) memcpy(&ce->cache.st, st, sizeof(struct stat));
    ce->flags=0;
    ce->ptr=ptr;
    ce->error=0;

    ce->cache_size=0;
    ce->cache.buffer=NULL;

    ce->cb_create_entry=_cb_create_entry;
    ce->cb_create_inode=_cb_create_inode;
    ce->cb_insert_entry=_cb_insert_entry;

    ce->cb_created=_cb_created_default;
    ce->cb_found=_cb_found_default;
    ce->cb_error=_cb_error_default;

    ce->cb_cache_size=_cb_cache_size;
    ce->cb_cache_created=_cb_cache_created;
    ce->cb_cache_found=_cb_cache_found;
    ce->cb_check=_cb_check;

    ce->cb_adjust_pathmax=_cb_adjust_pathmax_default;
    ce->cb_context_created=_cb_context_created;
    ce->cb_context_found=_cb_context_found;

}

static struct entry_s *_create_entry_extended_common(struct create_entry_s *ce)
{
    struct entry_s *entry=NULL, *result=NULL;
    struct inode_s *inode=NULL;
    unsigned int error=0;
    struct entry_s *parent=NULL;
    struct directory_s *directory=NULL;

    ce->cache_size=(* ce->cb_cache_size)(ce);

    directory=(* ce->get_directory)(ce);
    parent=directory->inode->alias;

    if ((error=(* ce->cb_check)(ce))>0) {

	(* ce->cb_error) (parent, ce->name, ce, error);
	return NULL;

    }

    entry=(* ce->cb_create_entry)(ce->name);
    inode=(* ce->cb_create_inode)();

    if (entry && inode) {

	result=(* ce->cb_insert_entry)(directory, entry, 0, &error);

	if (error==0) {

	    /* new */

	    logoutput("_create_entry_extended_common: entry %.*s added ino %li", ce->name->len, ce->name->name, inode->st.st_ino);

	    inode->alias=entry;
	    entry->inode=inode;
	    (* ce->cb_created)(entry, ce);

	} else {

	    logoutput("_create_entry_extended_common: error %i (%s)", error, strerror(error));

	    if (error==EEXIST) {

		/* existing found */

		destroy_entry(entry);
		entry=result;
		free(inode);
		inode=entry->inode;

		error=0;

		(* ce->cb_found)(entry, ce);

	    } else {

		/* another error */

		destroy_entry(entry);
		free(inode);
		(* ce->cb_error) (parent, ce->name, ce, error);
		return NULL;

	    }

	}

    } else {

	/* unable to allocate entry and/or inode */

	if (entry) destroy_entry(entry);
	if (inode) free(inode);
	(* ce->cb_error) (parent, ce->name, ce, error);
	error=ENOMEM;
	return NULL;

    }

    return entry;

}

struct entry_s *create_entry_extended(struct create_entry_s *ce)
{
    logoutput("create_entry_extended: %.*s", ce->name->len, ce->name->name);
    return _create_entry_extended_common(ce);
}

/* use directory to add entry and inode, directory has to be locked */

struct entry_s *create_entry_extended_batch(struct create_entry_s *ce)
{
    ce->cb_insert_entry=_cb_insert_entry_batch;

    logoutput("create_entry_extended_batch: %.*s", ce->name->len, ce->name->name);
    return _create_entry_extended_common(ce);
}

static void _clear_directory(struct context_interface_s *i, struct directory_s *directory, char *path, unsigned int len, unsigned int level)
{
    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
    struct entry_s *entry=NULL, *next=NULL;
    struct inode_s *inode=NULL;
    struct list_element_s *list=NULL;

    logoutput("_clear_directory: level %i path %s", level, path);

    list=get_list_head(&sl->header, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {

	entry=(struct entry_s *)((char *) list - offsetof(struct entry_s, list));

	logoutput("_clear_directory: found %.*s", entry->name.len, entry->name.name);
	inode=entry->inode;

	if (inode) {

	    (* inode->fs->forget)(inode);

	    if (S_ISDIR(inode->st.st_mode)) {
		unsigned int error=0;
		struct directory_s *subdir=remove_directory(inode, &error);

		if (subdir) {
		    struct name_s *xname=&entry->name;
		    unsigned int keep=len;

		    path[len]='/';
		    len++;
		    memcpy(&path[len], xname->name, xname->len);
		    len+=xname->len;
		    path[len]='\0';
		    len++;

		    /* do directory recursive */

		    release_directory_pathcache(subdir);
		    _clear_directory(i, subdir, path, len, level+1);
		    free_directory(subdir);
		    len=keep;
		    memset(&path[len], 0, sizeof(path) - len);

		}

	    }

	    /* remove and free inode */

	    inode->alias=NULL;
	    free(inode);
	    entry->inode=NULL;

	}

	destroy_entry(entry);
	list=get_list_head(&sl->header, SIMPLE_LIST_FLAG_REMOVE);

    }

}

void clear_directory_recursive(struct context_interface_s *i, struct directory_s *directory)
{
    struct service_context_s *context=get_service_context(i);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    char path[workspace->pathmax];

    memset(path, 0, workspace->pathmax);
    _clear_directory(i, directory, path, 0, 0);
}
