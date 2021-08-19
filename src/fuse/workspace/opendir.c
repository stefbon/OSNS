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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/statfs.h>

#include "log.h"
#include "main.h"
#include "misc.h"
#include "options.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "fuse/network.h"
#include "discover/discover.h"

extern struct fs_options_s fs_options;

void _fs_workspace_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, struct pathinfo_s *pathinfo, unsigned int flags)
{
    struct directory_s *directory=NULL;
    struct fuse_open_out open_out;
    struct service_context_s *parent=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(parent);
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;

    open_out.fh=(uint64_t) opendir;
    open_out.open_flags=0;
    reply_VFS_data(request, (char *) &open_out, sizeof(open_out));

    logoutput("_fs_workspace_opendir: ino %li", opendir->inode->st.st_ino);
    directory=get_directory(opendir->inode);

    /* here build the list of direntries */

    init_service_ctx_lock(&ctxlock, parent, NULL);
    if (lock_service_context(&ctxlock, "r", "p")==1) {
	struct inode_s *inode=opendir->inode;
	struct service_context_s *ctx=get_next_service_context(parent, NULL, "tree");

	while (ctx) {
	    struct service_fs_s *fs=NULL;
	    const char *what="";

	    if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {
		unsigned int type=ctx->service.browse.type;

		fs=ctx->service.browse.fs;

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

	    } else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

		fs=ctx->service.filesystem.fs;
		what="share";

	    }

	    if (fs) {
		unsigned int len=(* fs->get_name)(ctx, NULL, 0);
		char buffer[len+1];
		struct entry_s *entry=NULL;

		memset(buffer, 0, len+1);
		len=(* fs->get_name)(ctx, buffer, len);

		if (len>0) entry=install_virtualnetwork_map(ctx, inode->alias, buffer, what, NULL);

		if (entry) {
		    struct data_link_s *link=NULL;
		    struct directory_s *d=NULL;

		    queue_fuse_direntry(opendir, entry);

		    fs_get_data_link(entry->inode, &link);

		    if (link->type==0) {

			link->type=DATA_LINK_TYPE_CONTEXT;
			link->link.ptr=(void *) ctx;

		    }

		    d=get_directory(entry->inode);
		    if (d) set_directory_pathcache_zero(d);

		}

	    }

	    ctx=get_next_service_context(parent, ctx, "tree");

	}

	unlock_service_context(&ctxlock, "r", "p");

    }

    logoutput("_fs_workspace_opendir: ino %li finish direntry", opendir->inode->st.st_ino);

    finish_get_fuse_direntry(opendir);

    if (parent->type==SERVICE_CTX_TYPE_WORKSPACE) {

	logoutput("_fs_workspace_opendir: ino %li starting discover services thread", opendir->inode->st.st_ino);
	start_discover_service_context_connect(workspace);

    }

}

void _fs_workspace_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    return _fs_common_virtual_readdir(opendir, r, size, offset);
}

void _fs_workspace_readdirplus(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    reply_VFS_error(r, EOPNOTSUPP);
}

void _fs_workspace_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, unsigned char datasync)
{
    reply_VFS_error(r, 0);
}

void _fs_workspace_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *f_request)
{
    reply_VFS_error(f_request, 0);
}
