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
#include "workspace-interface.h"
#include "workspaces.h"
#include "context.h"
#include "fuse.h"
#include "path-caching.h"

struct getpath_buffer_s {
    struct service_context_s		*ctx;
    struct list_element_s		list;
    int					refcount;
    unsigned int			len;
    char				path[0];
};

/* get the path to the root (mountpoint) of this fuse fs */

int get_path_root(struct directory_s *directory, struct fuse_path_s *fpath)
{
    unsigned int pathlen=(unsigned int) (fpath->path + fpath->len - fpath->pathstart);
    struct entry_s *entry=directory->inode->alias;
    struct name_s *xname=NULL;

    appendname:

    xname=&entry->name;
    fpath->pathstart-=xname->len;
    memcpy(fpath->pathstart, xname->name, xname->len);
    fpath->pathstart--;
    *(fpath->pathstart)='/';
    pathlen+=xname->len+1;

    /* go one entry higher */

    entry=get_parent_entry(entry);
    if (get_ino_system_stat(&entry->inode->stat) > FUSE_ROOT_ID) goto appendname;

    return pathlen;

}

/*
    get the path relative to a "root" inode of a service

    the root of a service (SHH, NFS, WebDav and SMB) is connected at an inode in this fs
    for communication with the backend server most services use the path relative to this root
    this function determines the path relative to this "root"

    it does this by looking at the inode->fs-calls->get_type() value
    this is different for every set of fs-calls
*/

int get_service_path_default(struct directory_s *directory, struct fuse_path_s *fpath)
{
    struct inode_s *inode=directory->inode;
    struct entry_s *entry=inode->alias;
    struct fuse_fs_s *fs=inode->fs;
    unsigned int pathlen=0;
    struct name_s *xname=NULL;
    struct data_link_s *link=NULL;

    logoutput_debug("get_service_path_default");

    fs_get_data_link(inode, &link);

    /* walk back to the root of the context or the root of the mountpoint: whatever comes first */

    while (get_ino_system_stat(&inode->stat) > FUSE_ROOT_ID && link->type!=DATA_LINK_TYPE_CONTEXT) {

	xname=&entry->name;

	fpath->pathstart-=xname->len;
	memcpy(fpath->pathstart, xname->name, xname->len);
	fpath->pathstart--;
	*(fpath->pathstart)='/';
	pathlen+=xname->len+1;

	/* go one entry higher */

	entry=get_parent_entry(entry);
	inode=entry->inode;
	fs_get_data_link(inode, &link);

    }

    /* inode is the "root" of the service: data is holding the context */

    fpath->context=(struct service_context_s *) link->link.ptr;
    return pathlen;

}

void init_fuse_path(struct fuse_path_s *fpath, unsigned int len)
{

    /* init */
    fpath->len=len; /* size of buffer, not the path*/
    fpath->pathstart=&fpath->path[len - 1]; /* start at the end and append backwards */

    /* trailing zero */
    *fpath->pathstart='\0';
    // fpath->pathstart--;

}

void append_name_fpath(struct fuse_path_s *fpath, struct name_s *xname)
{

    /* entry */
    fpath->pathstart-=xname->len;
    memcpy(fpath->pathstart, xname->name, xname->len);

    /* slash */
    fpath->pathstart--;
    *fpath->pathstart='/';

}

/* functions when entry is root entry (zero depth) */

static unsigned int get_pathlen_0(struct service_context_s *ctx, struct directory_s *d)
{
    return 2;
}

static void append_path_0(struct service_context_s *ctx, struct directory_s *d, struct fuse_path_s *fpath)
{
    struct data_link_s *link=NULL;
    struct inode_s *inode=d->inode;

    /* only append the /. path part when path is empty */

    if (*(fpath->pathstart)=='\0') {

	/* root dot */
	fpath->pathstart--;
	*fpath->pathstart='.';

	/* slash */
	fpath->pathstart--;
	*fpath->pathstart='/';

    }

    /* service context */
    fs_get_data_link(inode, &link);
    fpath->context=(struct service_context_s *)(link->link.ptr);
    logoutput_debug("append_path_0: parent entry . context %s", ((fpath->context) ? fpath->context->name : "NULL"));

}

/* functions when depth to root is 1 (parent of entry is root entry) */

static unsigned int get_pathlen_1(struct service_context_s *ctx, struct directory_s *d)
{
    struct entry_s *entry=d->inode->alias;
    return 1 + entry->name.len;
}

static void append_path_1(struct service_context_s *ctx, struct directory_s *directory, struct fuse_path_s *fpath)
{
    struct entry_s *entry=directory->inode->alias;
    struct data_link_s *link=NULL;
    struct entry_s *parent=get_parent_entry(entry);

    /* entry */
    fpath->pathstart-=entry->name.len;
    memcpy(fpath->pathstart, entry->name.name, entry->name.len);

    /* (root) slash */
    fpath->pathstart--;
    *fpath->pathstart='/';

    /* service context */
    fs_get_data_link(parent->inode, &link);
    fpath->context=(link->type==DATA_LINK_TYPE_CONTEXT) ? (struct service_context_s *)(link->link.ptr) : NULL;
    entry=parent->inode->alias;
    logoutput_debug("append_path_1: parent entry %.*s context %s", entry->name.len, entry->name.name, ((fpath->context) ? fpath->context->name : "NULL"));

}

/* functions when depth (distance to root) is bigger than 1: x>1 */

static unsigned int get_pathlen_x(struct service_context_s *ctx, struct directory_s *d)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    return get_pathmax(w);
}

static void append_path_x(struct service_context_s *ctx, struct directory_s *directory, struct fuse_path_s *fpath)
{
    struct entry_s *entry=directory->inode->alias;
    struct data_link_s *link=NULL;
    struct entry_s *parent=get_parent_entry(entry);

    /* entry */
    fpath->pathstart-=entry->name.len;
    memcpy(fpath->pathstart, entry->name.name, entry->name.len);
    logoutput_debug("append_path_x: entry %.*s", entry->name.len, entry->name.name);

    /* (root) slash */
    fpath->pathstart--;
    *fpath->pathstart='/';

    /* service context */
    fs_get_data_link(parent->inode, &link);

    entry=parent->inode->alias;
    fpath->context=(link->type==DATA_LINK_TYPE_CONTEXT) ? (struct service_context_s *)(link->link.ptr) : NULL;
    logoutput_debug("append_path_x: parent entry %.*s context %s", entry->name.len, entry->name.name, ((fpath->context) ? fpath->context->name : "NULL"));

}

void get_service_context_path(struct service_context_s *ctx, struct directory_s *directory, struct fuse_path_s *fpath)
{
    struct data_link_s *link=NULL;

    logoutput_debug("get_service_context_path");

    /* get path :
	    append and walk back or get path from a cache */

    (* directory->getpath->append_path)(ctx, directory, fpath);
    fs_get_data_link(directory->inode, &link);
    fpath->context=(link->type==DATA_LINK_TYPE_CONTEXT) ? (struct service_context_s *)(link->link.ptr) : NULL;

    while (fpath->context==NULL) {
	struct entry_s *entry=directory->inode->alias;

	/* go one level higher */

	directory=get_upper_directory_entry(entry);
	entry=directory->inode->alias;

	logoutput_debug("get_service_context_path: entry %.*s", entry->name.len, entry->name.name);

	(* directory->getpath->append_path)(ctx, directory, fpath);
	fs_get_data_link(directory->inode, &link);
	fpath->context=(link->type==DATA_LINK_TYPE_CONTEXT) ? (struct service_context_s *)(link->link.ptr) : NULL;

    }

    logoutput_debug("get_service_context_path: path %s ctx %s", fpath->pathstart, fpath->context->name);

}

char *get_pathinfo_fpath(struct fuse_path_s *fpath, unsigned int *p_len)
{
    unsigned int len=(unsigned int)(fpath->path + fpath->len - fpath->pathstart - 1);

    *p_len=len;
    return fpath->pathstart;
}

static struct getpath_s getpath_0 = {
    .type				= GETPATH_TYPE_0,
    .get_pathlen			= get_pathlen_0,
    .append_path			= append_path_0,
};

static struct getpath_s getpath_1 = {
    .type				= GETPATH_TYPE_1,
    .get_pathlen			= get_pathlen_1,
    .append_path			= append_path_1,
};

static struct getpath_s getpath_x = {
    .type				= GETPATH_TYPE_X,
    .get_pathlen			= get_pathlen_x,
    .append_path			= append_path_x,
};

void set_directory_pathcache_zero(struct directory_s *d)
{
    d->getpath=&getpath_0;
}

void set_directory_pathcache_one(struct directory_s *d)
{
    d->getpath=&getpath_1;
}

void set_directory_pathcache_x(struct directory_s *d)
{
    d->getpath=&getpath_x;
}



/* functions when path is cached */

static unsigned int get_pathlen_custom(struct service_context_s *ctx, struct directory_s *d)
{
    struct getpath_s *getpath=d->getpath;
    struct getpath_buffer_s *gb=(struct getpath_buffer_s *) getpath->buffer;
    return gb->len;
}

static void append_path_custom(struct service_context_s *ctx, struct directory_s *d, struct fuse_path_s *fpath)
{
    struct getpath_s *getpath=d->getpath;
    struct getpath_buffer_s *gb=(struct getpath_buffer_s *) getpath->buffer;

    /* path */
    fpath->pathstart-=gb->len;
    memcpy(fpath->pathstart, gb->path, gb->len);

    /* service context */
    fpath->context=gb->ctx;

}

static struct getpath_s *create_cached_fuse_path(char *path, unsigned int len, struct service_context_s *ctx)
{
    struct getpath_s *getpath=malloc(sizeof(struct getpath_s) + sizeof(struct getpath_buffer_s) + len);

    if (getpath) {
	struct getpath_buffer_s *gb=(struct getpath_buffer_s *) getpath->buffer;

	getpath->type=GETPATH_TYPE_CUSTOM;
	getpath->get_pathlen=get_pathlen_custom;
	getpath->append_path=append_path_custom;

	gb->refcount=1;
	init_list_element(&gb->list, NULL);
	gb->ctx=ctx;
	gb->len=len;

	memcpy(gb->path, path, len); /* without trailing zero */
	add_list_element_first(&ctx->service.filesystem.pathcaches, &gb->list);

    }

    return getpath;

}

void set_directory_pathcache(struct service_context_s *ctx, struct directory_s *d, struct fuse_path_s *fpath)
{
    struct getpath_s *getpath=NULL;

    if (ctx && (ctx->type != SERVICE_CTX_TYPE_FILESYSTEM)) return;

    if ((getpath=d->getpath)) {

	if (getpath->type==GETPATH_TYPE_0 || getpath->type==GETPATH_TYPE_1) {

	    return;

	} else if (getpath->type==GETPATH_TYPE_CUSTOM) {
	    unsigned int lenfpath=0;
	    struct getpath_buffer_s *gb=NULL;

	    if (fpath==NULL) return;

	    lenfpath=strlen(fpath->pathstart);
	    gb=(struct getpath_buffer_s *) getpath->buffer;

	    /* only when exactly the same length reuse the cached path */

	    if (lenfpath==gb->len) {

		if (strncmp(fpath->pathstart, gb->path, gb->len)!=0) {

		    memcpy(fpath->pathstart, gb->path, gb->len);

		}

		gb->refcount++;
		return;

	    }

	    remove_list_element(&gb->list);
	    free(getpath);
	}

    }

    if (ctx) {

	if (d->inode == ctx->service.filesystem.inode) {

	    getpath=&getpath_0;

	} else {
	    struct directory_s *pdir=get_upper_directory_entry(d->inode->alias);

	    if (pdir) {

		if (pdir->inode == ctx->service.filesystem.inode) {

		    getpath=&getpath_1;
		}

	    }

	}

    }

    if (getpath==NULL) {

	if (fpath) {
	    int size=(fpath->path + fpath->len - fpath->pathstart);

	    getpath=create_cached_fuse_path(fpath->pathstart, size, ctx);

	} else {

	    getpath=&getpath_x;

	}

    }

    if (getpath) d->getpath=getpath;

}

void release_directory_pathcache(struct directory_s *d)
{
    struct getpath_s *getpath=d->getpath;

    if (getpath==NULL) {

	return;

    } else if (getpath->type==GETPATH_TYPE_CUSTOM) {
	struct getpath_buffer_s *gb=(struct getpath_buffer_s *) getpath->buffer;

	gb->refcount--;

	if (gb->refcount<=0) {

	    remove_list_element(&gb->list);
	    free(getpath);
	    d->getpath=NULL;
	    set_directory_getpath(d);

	}

    }

}

void set_directory_getpath(struct directory_s *d)
{
    struct inode_s *inode=NULL;
    struct entry_s *entry=NULL;

    if (d==NULL || d->getpath) {

	return;

    }

    inode=d->inode;

    if (inode) {
	struct entry_s *entry=inode->alias;
	struct entry_s *parent=NULL;

	if (get_ino_system_stat(&inode->stat)==FUSE_ROOT_ID) {

	    d->getpath=&getpath_0;
	    return;

	}

	parent=get_parent_entry(entry);
	inode=parent->inode;

	if (get_ino_system_stat(&inode->stat)==FUSE_ROOT_ID) {

	    d->getpath=&getpath_1;
	    return;

	}

    }

    d->getpath=&getpath_x;

}
