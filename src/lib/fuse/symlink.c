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
#include "symlink.h"

static struct fuse_symlink_s *create_fuse_cache_symlink(char *buffer, unsigned int len)
{
    struct fuse_symlink_s *cs=malloc(sizeof(struct fuse_symlink_s) + len); /* cached symlink */

    if (cs) {

	memset(cs, 0, sizeof(struct fuse_symlink_s) + len);

	cs->flags |= FUSE_SYMLINK_FLAG_ALLOC;
	init_data_link(&cs->link);
	init_list_element(&cs->list, NULL);
	cs->link.type=DATA_LINK_TYPE_SYMLINK;
	cs->len=len;
	memcpy(cs->path, buffer, len);

    }

    return cs;

}

struct fuse_symlink_s *get_inode_fuse_cache_symlink(struct inode_s *inode)
{
    struct fuse_symlink_s *cs=NULL;

    if (inode->ptr) {
	struct data_link_s *link=inode->ptr;

	if (link->type==DATA_LINK_TYPE_SYMLINK) cs=(struct fuse_symlink_s *)((char *) link - offsetof(struct fuse_symlink_s, link));

    }

    return cs;
}

unsigned int set_inode_fuse_cache_symlink(struct service_context_s *ctx, struct inode_s *inode, char *buffer)
{
    struct fuse_symlink_s *cs=get_inode_fuse_cache_symlink(inode);
    unsigned int len=strlen(buffer);
    unsigned int errcode=0;

    if (cs) {

	if (len==cs->len) {

	    /* only copy when differ */

	    if (memcmp(cs->path, buffer, len)) memcpy(cs->path, buffer, len);

	} else {
	    struct service_context_s *rootctx=get_root_context(ctx);
	    struct list_header_s *h=&rootctx->service.workspace.symlinks;
	    struct fuse_symlink_s *keep=cs;

	    write_lock_list_header(h);
	    remove_list_element(&cs->list);

	    cs=realloc(cs, (sizeof(struct fuse_symlink_s) + len));

	    if (cs) {

		memcpy(cs->path, buffer, len);
		cs->len=len;
		if (cs != keep) inode->ptr=&cs->link;
		add_list_element_first(h, &cs->list);

	    } else {

		errcode=ENOMEM;

	    }

	    write_unlock_list_header(h);

	}

    } else {

	cs=create_fuse_cache_symlink(buffer, len);

	if (cs) {
	    struct service_context_s *rootctx=get_root_context(ctx);
	    struct list_header_s *h=&rootctx->service.workspace.symlinks;

	    inode->ptr=&cs->link;

	    write_lock_list_header(h);
	    add_list_element_first(h, &cs->list);
	    write_unlock_list_header(h);

	} else {

	    errcode=ENOMEM;

	}

    }

    return errcode;

}

void free_fuse_symlink(struct fuse_symlink_s *cs)
{
    memset(cs, 0, sizeof(struct fuse_symlink_s) + cs->len);
    free(cs);
}
