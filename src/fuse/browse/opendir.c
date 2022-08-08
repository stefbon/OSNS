/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "client/network.h"

static unsigned int get_name_dummy(struct service_context_s *ctx, char *b, unsigned int l)
{
    return 0;
}

void populate_browse_entries(struct fuse_opendir_s *opendir)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(opendir->context);
    struct service_context_s *parent=opendir->context;
    struct directory_s *directory=get_directory(workspace, opendir->inode, 0);
    struct list_header_s *h=get_list_header_context(parent);
    struct inode_s *inode=opendir->inode;
    struct service_context_s *ctx=get_next_service_context(parent, NULL, "tree");

    /* here build the list of direntries */

    read_lock_list_header(h);
    ctx=get_next_service_context(parent, NULL, "tree");

    while (ctx) {
	const char *what="";
	unsigned int (* get_name_cb)(struct service_context_s *ctx, char *b, unsigned int l)=get_name_dummy;
	unsigned int len=0;

	if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {
	    unsigned int type=ctx->service.browse.type;

	    switch (type) {

		case SERVICE_BROWSE_TYPE_NETWORK:

		    what="network";
		    break;

		case SERVICE_BROWSE_TYPE_NETGROUP:

		    what="domain";
		    break;

		case SERVICE_BROWSE_TYPE_NETHOST:

		    what="server";
		    break;

	    }

	    get_name_cb=ctx->service.browse.fs->get_name;

	} else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    what="share";
	    if (ctx->service.filesystem.fs) get_name_cb=ctx->service.filesystem.fs->get_name;

	}

	len=(* get_name_cb)(ctx, NULL, 0);

	if (len>0) {
	    char buffer[len+1];
	    struct entry_s *entry=NULL;
	    unsigned char action=0;

	    memset(buffer, 0, len+1);
	    len=(* get_name_cb)(ctx, buffer, len);

	    entry=install_virtualnetwork_map(ctx, inode->alias, buffer, what, &action);

	    if (entry) {

		logoutput_debug("populate_browse_entries: name %s action %u ctx type %u", buffer, action, ctx->type);

		if (action==FUSE_NETWORK_ACTION_FLAG_ADDED) {
		    struct directory_s *directory=get_directory(workspace, entry->inode, 0);

		    if (directory) directory->ptr=&ctx->link;

		}

		queue_fuse_direntry(opendir, entry);

	    }

	}

	ctx=get_next_service_context(parent, ctx, "tree");

    }

    read_unlock_list_header(h);

    finish_get_fuse_direntry(opendir);

}

void _fs_browse_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags)
{
    struct fuse_open_out open_out;

    logoutput("_fs_browse_opendir: ino %li", get_ino_system_stat(&opendir->inode->stat));

    opendir->readdir=_fs_common_readdir;

    open_out.fh=(uint64_t) opendir;
    open_out.open_flags=0;
    reply_VFS_data(request, (char *) &open_out, sizeof(open_out));

    populate_browse_entries(opendir);

}
