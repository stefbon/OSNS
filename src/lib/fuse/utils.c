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

/* FUNCTIONS to CREATE an entry and inode */

static struct entry_s *_cb_create_entry(struct name_s *name)
{
    return create_entry(name);
}
static struct inode_s *_cb_create_inode()
{
    return create_inode();
}

static struct entry_s *_cb_insert_entry(struct directory_s *directory, struct entry_s *entry, unsigned int flags, unsigned int *error)
{
    return insert_entry(directory, entry, error);
}
static struct entry_s *_cb_insert_entry_batch(struct directory_s *directory, struct entry_s *entry, unsigned int flags, unsigned int *error)
{
    return insert_entry_batch(directory, entry, error);
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
    fill_inode_stat(entry->inode, &ce->cache.stat);
    entry->inode->stat.sst_mode=ce->cache.stat.sst_mode;
    entry->inode->stat.sst_size=ce->cache.stat.sst_size;
    entry->inode->stat.sst_nlink=ce->cache.stat.sst_nlink;
}
static void _cb_cache_found(struct entry_s *entry, struct create_entry_s *ce)
{
    fill_inode_stat(entry->inode, &ce->cache.stat);
    entry->inode->stat.sst_mode=ce->cache.stat.sst_mode;
    entry->inode->stat.sst_size=ce->cache.stat.sst_size;
    entry->inode->stat.sst_nlink=ce->cache.stat.sst_nlink;
}

static void _cb_created_default(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct inode_s *inode=entry->inode;
    struct system_stat_s *stat=&inode->stat;

    inode->nlookup=0; 									/*20210401: changed from 1 to 0: setting of this should be done in the context */
    set_nlink_system_stat(stat, 1);
    get_current_time_system_time(&inode->stime); 					/* sync time */

    if (system_stat_test_ISDIR(stat)) {
	struct directory_s *directory=get_upper_directory_entry(entry);
	struct workspace_mount_s *w=get_workspace_mount_ctx(ce->context);

	assign_directory_inode(w, inode);
	increase_nlink_system_stat(stat, 1);

	/* directory has to exist ... check is not required */

	if (directory) {
	    struct inode_s *tmp=directory->inode;

	    set_ctime_system_stat(&tmp->stat, &inode->stime); 				/* change the ctime of parent directory since it's attr are changed */
	    set_mtime_system_stat(&tmp->stat, &inode->stime); 				/* change the mtime of parent directory since an entry is added */

	}

    }

    (* ce->cb_adjust_pathmax)(ce); 							/* adjust the maximum path len */
    (* ce->cb_cache_created)(entry, ce); 						/* create the inode stat and cache */
    (* ce->cb_context_created)(ce, entry); 						/* context depending cb, like a FUSE reply and adding inode to context, set fs etc */
    set_entry_ops(entry);
    use_virtual_fs(NULL, inode);

}

static void _cb_found_default(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct system_stat_s *st=&ce->cache.stat;
    struct inode_s *inode=entry->inode;

    get_current_time_system_time(&inode->stime);

    /* when just created (for example by readdir) adjust the pathcache */

    if (inode->nlookup==1) (* ce->cb_adjust_pathmax)(ce); /* adjust the maximum path len */
    (* ce->cb_cache_found)(entry, ce); /* get/set the inode stat cache */
    (* ce->cb_context_found)(ce, entry); /* context depending cb, like a FUSE reply and adding inode to context, set fs etc */

}

static void _cb_error_default(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int error)
{
    if (error==0) error=EIO;
    logoutput("_cb_error_default: error %i (%s)", error, strerror(error));
}

static struct directory_s *get_directory_01(struct create_entry_s *ce)
{
    struct inode_s *inode=ce->tree.parent->inode;
    struct workspace_mount_s *w=get_workspace_mount_ctx(ce->context);
    return get_directory(w, inode, 0);
}

static struct directory_s *get_directory_02(struct create_entry_s *ce)
{
    return ce->tree.directory;
}

static struct directory_s *get_directory_03(struct create_entry_s *ce)
{
    struct inode_s *inode=ce->tree.opendir->inode;
    struct workspace_mount_s *w=get_workspace_mount_ctx(ce->context);
    return get_directory(w, inode, 0);
}

void init_create_entry(struct create_entry_s *ce, struct name_s *n, struct entry_s *p, struct directory_s *d, struct fuse_opendir_s *fo, struct service_context_s *c, struct system_stat_s *stat, void *ptr)
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
    if (stat) memcpy(&ce->cache.stat, stat, sizeof(struct system_stat_s));
    ce->flags=0;
    ce->ptr=ptr;
    ce->error=0;

    ce->cache_size=0;
    ce->cache.abuff=NULL;

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

	    logoutput("_create_entry_extended_common: entry %.*s added ino %li", ce->name->len, ce->name->name, inode->stat.sst_ino);

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

void fill_fuse_attr_system_stat(struct fuse_attr *attr, struct system_stat_s *stat)
{

    attr->ino=stat->sst_ino;
    attr->size=stat->sst_size;
    attr->blksize=_DEFAULT_BLOCKSIZE;
    attr->blocks=calc_amount_blocks(attr->size, attr->blksize);

    attr->atime=(uint64_t) get_atime_sec_system_stat(stat);
    attr->atimensec=(uint64_t) get_atime_nsec_system_stat(stat);
    attr->mtime=(uint64_t) get_mtime_sec_system_stat(stat);
    attr->mtimensec=(uint64_t) get_mtime_nsec_system_stat(stat);
    attr->ctime=(uint64_t) get_ctime_sec_system_stat(stat);
    attr->ctimensec=(uint64_t) get_ctime_nsec_system_stat(stat);

    attr->mode=stat->sst_mode;
    attr->nlink=stat->sst_nlink;
    attr->uid=stat->sst_uid;
    attr->gid=stat->sst_gid;
    attr->rdev=0;

}
