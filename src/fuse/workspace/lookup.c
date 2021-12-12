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

	    /* browse every ctx under pctx and compare it's name with the one to look for */

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

	    /* get name from ctx through the fs it's using and compare*/

	    if (fs) {
		unsigned int len=(* fs->get_name)(ctx, NULL, 0);
		char buffer[len+1];
		struct name_s tmp;

		memset(buffer, 0, len+1);
		len=(* fs->get_name)(ctx, buffer, len);
		set_name(&tmp, buffer, len);
		calculate_nameindex(&tmp);

		if (compare_names(xname, &tmp)==0) {
		    struct entry_s *entry=NULL;

		    entry=install_virtualnetwork_map(ctx, inode->alias, buffer, what, NULL);

		    if (entry) {
			struct directory_s *directory=NULL;

			_fs_common_cached_lookup(ctx, request, entry->inode);
			error=0;

			directory=get_directory(workspace, entry->inode, 0);
			if (directory) {

			    /* make directory point to context
				justy check here*/

			    pthread_mutex_lock(&workspace->mutex);

			    if (directory->ptr==NULL) {

				directory->ptr=&ctx->link;
				ctx->link.refcount++;

			    }

			    pthread_mutex_unlock(&workspace->mutex);

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
