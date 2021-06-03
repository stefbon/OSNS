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

void _fs_workspace_lookup_new(struct service_context_s *pctx, struct fuse_request_s *request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(pctx);
    unsigned int error=ENOENT; /* default */
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;

    init_service_ctx_lock(&ctxlock, pctx, NULL);

    if (lock_service_context(&ctxlock, "r", "p")==1) {
	struct service_context_s *ctx=get_next_service_context(pctx, NULL, "tree");

	while (ctx) {
	    struct service_fs_s *fs=NULL;
	    const char *what="";

	    if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {
		unsigned int type=ctx->service.browse.type;

		switch (type) {

		    case SERVICE_BROWSE_TYPE_NETWORK:

			fs=ctx->service.browse.fs;
			what="network";
			break;

		    case SERVICE_BROWSE_TYPE_NETGROUP:

			fs=ctx->service.browse.fs;
			what="domain";
			break;

		    case SERVICE_BROWSE_TYPE_NETHOST:

			fs=ctx->service.browse.fs;
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

		if (xname->len==len && strncmp(xname->name, buffer, len)==0) {

		    entry=install_virtualnetwork_map(ctx, inode->alias, buffer, what, NULL);

		    if (entry) {
			struct data_link_s *link=NULL;

			_fs_common_cached_lookup(ctx, request, entry->inode);
			error=0;

			fs_get_data_link(entry->inode, &link);

			if (link->type==0) {

			    link->type=DATA_LINK_TYPE_CONTEXT;
			    link->link.ptr=(void *) ctx;

			}

			break;

		    }

		}

	    }

	    ctx=get_next_service_context(pctx, ctx, "tree");

	}

	unlock_service_context(&ctxlock, "r", "p");

    }

    out:
    if (error>0) reply_VFS_error(request, error);

}

void _fs_workspace_lookup_existing(struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo)
{
    _fs_common_cached_lookup(context, request, entry->inode);
}
