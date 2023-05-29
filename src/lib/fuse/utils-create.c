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

#include <sys/stat.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-sl.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"

#include "dentry.h"
#include "directory.h"
#include "fs-virtual.h"
#include "utils-create.h"

/* FUNCTIONS to CREATE an entry and inode */

static struct entry_s *_cb_create_entry(struct name_s *name)
{
    return create_entry(name);
}
static struct inode_s *_cb_create_inode()
{
    return create_inode();
}
static struct entry_s *_cb_insert_entry(struct directory_s *directory, struct name_s *name, unsigned int flags, unsigned int *error)
{
    return insert_entry(directory, name, error);
}
static struct entry_s *_cb_insert_entry_batch(struct directory_s *directory, struct name_s *name, unsigned int flags, unsigned int *error)
{
    return insert_entry_batch(directory, name, error);
}
static void _cb_adjust_pathmax_default(struct create_entry_s *ce)
{
    adjust_pathmax(ce->context, ce->pathlen);
}
static void _cb_adjust_pathmax_ignore(struct create_entry_s *ce)
{
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

static void _cb_cache_default(struct entry_s *entry, struct create_entry_s *ce)
{
    struct system_stat_s *a=&entry->inode->stat;
    struct system_stat_s *b=&ce->cache.stat;

    fill_inode_stat(entry->inode, b);
    set_type_system_stat(a, get_type_system_stat(b));
    set_mode_system_stat(a, get_mode_system_stat(b));
    set_size_system_stat(a, get_size_system_stat(b));
    set_nlink_system_stat(a, get_nlink_system_stat(b));
}

static void _cb_cache_ignore(struct entry_s *entry, struct create_entry_s *ce)
{}

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

	assign_directory_inode(inode);
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
    use_virtual_fs(NULL, inode);

}

static void _cb_found_default(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct system_stat_s *st=&ce->cache.stat;
    struct inode_s *inode=entry->inode;

    get_current_time_system_time(&inode->stime);

    /* when just created (for example by readdir) adjust the pathcache */

    if (inode->nlookup==0) (* ce->cb_adjust_pathmax)(ce); /* adjust the maximum path len */
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
    return get_directory(ce->context, inode, 0);
}

static struct directory_s *get_directory_02(struct create_entry_s *ce)
{
    return ce->tree.directory;
}

static struct directory_s *get_directory_03(struct create_entry_s *ce)
{
    struct inode_s *inode=ce->tree.opendir->header.inode;
    return get_directory(ce->context, inode, 0);
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
    ce->cb_cache_created=_cb_cache_default;
    ce->cb_cache_found=_cb_cache_default;
    ce->cb_check=_cb_check;

    ce->cb_adjust_pathmax=_cb_adjust_pathmax_default;
    ce->cb_context_created=_cb_context_created;
    ce->cb_context_found=_cb_context_found;

}

struct entry_s *create_entry_extended(struct create_entry_s *ce)
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

    inode=(* ce->cb_create_inode)();
    if (inode==NULL) {

	logoutput("create_entry_extended: unable to allocate inode");
	(* ce->cb_error) (parent, ce->name, ce, ENOMEM);
	return NULL;
    }

    result=(* ce->cb_insert_entry)(directory, ce->name, 0, &error);

    if (error==0) {

	/* new */

	entry=result;
	inode->alias=entry;
	entry->inode=inode;

	logoutput("create_entry_extended: entry %.*s added ino %li", ce->name->len, ce->name->name, get_ino_system_stat(&inode->stat));
	(* ce->cb_created)(entry, ce);

    } else if (error==EEXIST) {

	/* existing found */

	free_inode(inode);
	entry=result;
	inode=entry->inode;
	error=0;
	logoutput("create_entry_extended: existing entry %.*s found ino %li", ce->name->len, ce->name->name, get_ino_system_stat(&inode->stat));
	(* ce->cb_found)(entry, ce);

    } else {

	/* another error */

	logoutput("_create_entry_extended_common: error %i (%s)", error, strerror(error));
	free(inode);
	(* ce->cb_error) (parent, ce->name, ce, error);
	entry=NULL;

    }

    return entry;

}

void disable_ce_extended_adjust_pathmax(struct create_entry_s *ce)
{
    ce->cb_adjust_pathmax=_cb_adjust_pathmax_ignore;
}

void enable_ce_extended_adjust_pathmax(struct create_entry_s *ce)
{
    ce->cb_adjust_pathmax=_cb_adjust_pathmax_default;
}

void enable_ce_extended_batch(struct create_entry_s *ce)
{
    ce->cb_insert_entry=_cb_insert_entry_batch;
}

void disable_ce_extended_batch(struct create_entry_s *ce)
{
    ce->cb_insert_entry=_cb_insert_entry;
}

void disable_ce_extended_cache(struct create_entry_s *ce)
{
    ce->cb_cache_size=_cb_cache_size;
    ce->cb_cache_created=_cb_cache_ignore;
    ce->cb_cache_found=_cb_cache_ignore;
}
