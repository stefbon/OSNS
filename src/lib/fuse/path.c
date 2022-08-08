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
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse.h"

#include "path.h"

/* get the path to the root (mountpoint) of this fuse fs */

int get_path_root_workspace(struct directory_s *directory, struct fuse_path_s *fpath)
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

int get_path_root_context(struct directory_s *directory, struct fuse_path_s *fpath)
{
    struct inode_s *inode=directory->inode;
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=0;
    struct name_s *xname=NULL;
    struct data_link_s *link=NULL;

    logoutput_debug("get_service_path_default");

    link=directory->ptr;

    /* walk back to the root of the context or
	the first entry which inode points to a context (via a directory ...) : whatever comes first */

    while ((get_ino_system_stat(&inode->stat) > FUSE_ROOT_ID) && (fpath->context==NULL)) {

	if (link) {

	    if (link->type==DATA_LINK_TYPE_CONTEXT) {

		fpath->context=(struct service_context_s *)((char *) link - offsetof(struct service_context_s, link));
		break;

	    } else if (link->type==DATA_LINK_TYPE_PATH) {

		struct cached_path_s *cp=(struct cached_path_s *)((char *) link - offsetof(struct cached_path_s, link));

		if (cp->list.h) {
		    struct list_header_s *h=cp->list.h;

		    fpath->context=(struct service_context_s *)((char *) h - offsetof(struct service_context_s, service.filesystem.pathcaches));

		}

		fpath->pathstart-=cp->len;
		memcpy(fpath->pathstart, cp->path, cp->len);
		pathlen+=cp->len;
		break;

	    }

	}

	xname=&entry->name;

	fpath->pathstart-=xname->len;
	memcpy(fpath->pathstart, xname->name, xname->len);
	fpath->pathstart--;
	*(fpath->pathstart)='/';
	pathlen+=xname->len+1;

	/* go one entry higher */

	directory=get_upper_directory_entry(entry);
	inode=directory->inode;
	link=directory->ptr;
	entry=inode->alias;

    }

    return pathlen;

}

void init_fuse_path(struct fuse_path_s *fpath, unsigned int len)
{

    /* init */
    fpath->len=len; 					/* size of buffer, not the path*/
    fpath->pathstart=&fpath->path[len - 1]; 		/* start at the end and append backwards */

    /* trailing zero */
    *fpath->pathstart='\0';
    fpath->context=NULL;

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

void start_directory_fpath(struct fuse_path_s *fpath)
{
    fpath->pathstart-=2;
    memcpy(fpath->pathstart, "/.", 2);
}

void cache_fuse_path(struct directory_s *directory, struct fuse_path_s *fpath)
{
    struct data_link_s *link=directory->ptr;
    struct service_context_s *ctx=fpath->context;

    if (ctx && link==NULL) {
	unsigned int len=(unsigned int)(&fpath->path[fpath->len - 1] - fpath->pathstart);

	if (len>0) {
	    struct cached_path_s *cp=malloc(sizeof(struct cached_path_s) + len);

	    if (cp) {

		memset(cp, 0, sizeof(struct cached_path_s) + len);
		init_list_element(&cp->list, NULL);
		cp->len=len;
		memcpy(cp->path, fpath->pathstart, len);

		write_lock_list_header(&ctx->service.filesystem.pathcaches);
		add_list_element_first(&ctx->service.filesystem.pathcaches, &cp->list);
		write_unlock_list_header(&ctx->service.filesystem.pathcaches);

		directory->ptr=&cp->link;
		cp->link.type=DATA_LINK_TYPE_PATH;
		cp->link.refcount=1;

	    }

	}

    }

}

void release_cached_path(struct directory_s *directory)
{
    struct data_link_s *link=directory->ptr;

    if (link) {

	if (link->type==DATA_LINK_TYPE_PATH) {
	    struct cached_path_s *cp=(struct cached_path_s *)((char *) link - offsetof(struct cached_path_s, link));

	    cp->refcount-=((cp->refcount>=1) ? 1 : 0);

	    if (cp->refcount==0) {
		 struct list_header_s *h=cp->list.h;
		struct service_context_s *ctx=(struct service_context_s *)((char *) h - offsetof(struct service_context_s, service.filesystem.pathcaches));

		write_lock_list_header(&ctx->service.filesystem.pathcaches);
		remove_list_element(&cp->list);
		write_unlock_list_header(&ctx->service.filesystem.pathcaches);

		free(cp);
		directory->ptr=NULL;

	    }

	}

    }

}

void free_cached_path(struct list_element_s *list)
{
    struct cached_path_s *cp=(struct cached_path_s *)((char *) list - offsetof(struct cached_path_s, list));
    free(cp);
}
