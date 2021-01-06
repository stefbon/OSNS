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
#include "utils.h"
#include "workspace-interface.h"
#include "workspaces.h"
#include "fuse.h"
#include "path-caching.h"

/*

    get the path relative to a "root" inode of a service

    the root of a service (SHH, NFS, WebDav and SMB) is connected at an inode in this fs
    for communication with the backend server most services use the path relative to this root
    this function determines the path relative to this "root"

    it does this by looking at the inode->fs-calls->get_type() value
    this is different for every set of fs-calls

*/

int get_service_path_default(struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct entry_s *entry=inode->alias;
    struct fuse_fs_s *fs=inode->fs;
    unsigned int pathlen=0;
    struct name_s *xname=NULL;
    struct inode_link_s *link=NULL;

    logoutput_info("get_service_path_default");

    appendname:

    xname=&entry->name;

    fpath->pathstart-=xname->len;
    memcpy(fpath->pathstart, xname->name, xname->len);
    fpath->pathstart--;
    *(fpath->pathstart)='/';
    pathlen+=xname->len+1;

    /* go one entry higher */

    entry=get_parent_entry(entry);
    inode=entry->inode;
    fs=inode->fs;

    fs_get_inode_link(inode, &link);
    if (link->type!=INODE_LINK_TYPE_CONTEXT) goto appendname;

    /* inode is the "root" of the service: data is holding the context */

    fpath->context=(struct service_context_s *) link->link.ptr;
    return pathlen;

}

unsigned int add_name_path(struct fuse_path_s *fpath, struct name_s *xname)
{
    fpath->pathstart-=xname->len;
    memcpy(fpath->pathstart, xname->name, xname->len);
    fpath->pathstart--;
    *fpath->pathstart='/';
    return xname->len+1;
}

void init_fuse_path(struct fuse_path_s *fpath, char *path, unsigned int len)
{
    fpath->context=NULL;
    fpath->path=path;
    fpath->len=len;
    fpath->pathstart=path+len;
    *(fpath->pathstart)='\0';
}

static int get_service_path_directory_default(struct directory_s *d, struct fuse_path_s *fpath)
{
    struct inode_s *inode=d->inode;
    return get_service_path_default(inode, fpath);
}

static int get_service_path_directory_root(struct directory_s *d, struct fuse_path_s *fpath)
{
    struct inode_link_s *link=NULL;
    fs_get_inode_link(d->inode, &link);
    fpath->context=(struct service_context_s *) link->link.ptr;
    return 0;
}

static int get_service_path_directory_cached(struct directory_s *d, struct fuse_path_s *fpath)
{
    struct pathcache_s *cache=d->pathcache;

    fpath->context=cache->context;
    fpath->pathstart-=cache->len;
    memcpy(fpath->pathstart, cache->path, cache->len);
    get_current_time(&cache->used);
    return cache->len;
}

static struct pathcache_s pathcache_default = {
    .type				= _PATHCACHE_TYPE_DEFAULT,
    .get_path				= get_service_path_directory_default,
    .context				= NULL,
    .len				= 0,
};

static struct pathcache_s pathcache_root = {
    .type				= _PATHCACHE_TYPE_ROOT,
    .get_path				= get_service_path_directory_root,
    .context				= NULL,
    .len				= 0,
};

void set_directory_pathcache(struct directory_s *directory, const char *what, char *path, struct service_context_s *c)
{

    if (strcmp(what, "root")==0) {

	directory->pathcache=&pathcache_root;

    } else if (strcmp(what, "default")==0) {

	directory->pathcache=&pathcache_default;

    } else if (strcmp(what, "cached")==0) {
	unsigned int len=strlen(path);
	struct pathcache_s *cache=directory->pathcache;

	if (cache && cache->type==_PATHCACHE_TYPE_CACHE) return;
	cache=malloc(sizeof(struct pathcache_s) + len);

	if (cache) {

	    cache->type=_PATHCACHE_TYPE_CACHE;
	    cache->context=c;
	    init_list_element(&cache->list, NULL);
	    cache->get_path=get_service_path_directory_cached;
	    cache->refcount=1;
	    get_current_time(&cache->used);
	    cache->len=len;
	    memcpy(cache->path, path, len);

	    directory->pathcache=cache;

	    pthread_mutex_lock(&c->mutex);
	    add_list_element_first(&c->service.filesystem.pathcaches, &cache->list);
	    pthread_mutex_unlock(&c->mutex);

	} else {

	    logoutput_warning("set_directory_pathcache: not able to allocate cache, falling back to default");

	}

    }

}

void release_directory_pathcache(struct directory_s *directory)
{
    struct pathcache_s *cache=directory->pathcache;

    if (cache->type==_PATHCACHE_TYPE_CACHE) {
	struct service_context_s *context=cache->context;

	if (context) pthread_mutex_lock(&context->mutex);
	if (cache->refcount>0) cache->refcount--; /* do not remove it */
	directory->pathcache=&pathcache_default; /* set to the default */
	if (context) pthread_mutex_unlock(&context->mutex);

    }

}

void remove_unused_pathcaches(struct service_context_s *c)
{
    struct timespec now;
    struct list_element_s *list=NULL;

    get_current_time(&now);
    now.tv_sec-=_PATHCACHE_TIMEOUT_DEFAULT;

    pthread_mutex_lock(&c->mutex);

    /* walk back */

    list=get_list_tail(&c->service.filesystem.pathcaches, 0);

    while (list) {
	struct list_element_s *prev=get_prev_element(list);
	struct pathcache_s *cache=(struct pathcache_s *)((char *)list - offsetof(struct pathcache_s, list));

	if ((cache->used.tv_sec <= now.tv_sec) || (cache->used.tv_sec == now.tv_sec || cache->used.tv_nsec <= now.tv_nsec)) {

	    if (cache->refcount <= 0) {

		/* not in use anymore and too old */

		remove_list_element(list);
		free(cache);

	    }

	} else {

	    /* list is desdending: earlier in time are at tail
		so when cache is added later all those to the head to
		the list will do also: safe to break */

	    break;


	}

	list=prev;

    }

    pthread_mutex_unlock(&c->mutex);
}

