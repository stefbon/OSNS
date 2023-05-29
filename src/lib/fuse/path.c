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

void init_fusepath(struct fuse_path_s *fusepath, unsigned int len)
{
    fusepath->len=len; 					/* size of buffer, not the path */
    fusepath->context=NULL;
    memset(&fusepath->buffer, 0, len);
    fs_path_assign_buffer(&fusepath->path, fusepath->buffer, len);
}

void prepend_name_fusepath(struct fuse_path_s *fusepath, struct name_s *xname)
{
    /* entry */
    unsigned int tmp=fs_path_prepend_raw(&fusepath->path, xname->name, xname->len);

    /* slash */
    tmp+=fs_path_prepend_raw(&fusepath->path, "/", 1);
}

void start_directory_fusepath(struct fuse_path_s *fusepath)
{
    unsigned int tmp=fs_path_prepend_raw(&fusepath->path, "/.", 2);
}

/* get the path to the root (mountpoint) of this fuse fs */

int get_path_root_workspace(struct directory_s *directory, struct fuse_path_s *fusepath)
{
    struct entry_s *entry=directory->inode->alias;
    struct name_s *xname=NULL;

    appendname:

    xname=&entry->name;
    prepend_name_fusepath(fusepath, xname);

    /* go one entry higher */
    entry=get_parent_entry(entry);
    if (get_ino_system_stat(&entry->inode->stat) > FUSE_ROOT_ID) goto appendname;
    return fusepath->path.len;

}

/*
    get the path relative to a "root" inode of a service

    the root of a service (SHH, NFS, WebDav and SMB) is connected at an inode in this fs
    for communication with the backend server most services use the path relative to this root
    this function determines the path relative to this "root"

    it does this by looking at the inode->fs-calls->get_type() value
    this is different for every set of fs-calls
*/

void get_path_root_context(struct directory_s *directory, struct fuse_path_s *fusepath)
{
    struct inode_s *inode=directory->inode;
    struct entry_s *entry=inode->alias;
    struct name_s *xname=NULL;
    struct data_link_s *link=directory->ptr;

    /* walk back to the root of the context or
	the first entry which inode points to a context (via a directory ...) : whatever comes first */

    while ((get_ino_system_stat(&inode->stat) > FUSE_ROOT_ID) && (fusepath->context==NULL)) {

	if (link) {

	    if (link->type==DATA_LINK_TYPE_CONTEXT) {

		fusepath->context=(struct service_context_s *)((char *) link - offsetof(struct service_context_s, link));
		break;

	    } else if (link->type==DATA_LINK_TYPE_PATH) {
		struct cached_path_s *cp=(struct cached_path_s *)((char *) link - offsetof(struct cached_path_s, link));

		if (cp->list.h) fusepath->context=(struct service_context_s *)((char *) cp->list.h - offsetof(struct service_context_s, service.filesystem.pathcaches));
		unsigned int tmp=fs_path_prepend_raw(&fusepath->path, cp->path, cp->len);
		break;

	    }

	}

	xname=&entry->name;
	prepend_name_fusepath(fusepath, xname);

	/* go one entry higher */

	directory=get_upper_directory_entry(entry);
	inode=directory->inode;
	link=directory->ptr;
	entry=inode->alias;

    }

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
