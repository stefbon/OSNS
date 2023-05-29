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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "client/network.h"
#include "client/refresh.h"
#include "fuse/disconnected-fs.h"
#include "fuse/sftp-fs.h"


static unsigned int get_name_dummy(struct service_context_s *ctx, char *b, unsigned int l)
{
    return 0;
}

void _fs_browse_lookup(struct service_context_s *pctx, struct fuse_request_s *request, struct inode_s *pinode, struct name_s *xname)
{
    struct directory_s *pdirectory=get_directory(pctx, pinode, 0);
    unsigned int errcode=ENOENT; /* default */
    struct list_header_s *h=NULL;
    struct service_context_s *ctx=NULL;
    unsigned int tmp=0;
    struct entry_s *entry=find_entry(pdirectory, xname, &tmp);

    if (entry) {

        _fs_common_cached_lookup(pctx, request, entry->inode);

        if (system_stat_test_ISDIR(&entry->inode->stat)) {
            struct directory_s *directory=get_directory(pctx, entry->inode, 0);
            struct data_link_s *link=(directory) ? directory->ptr : NULL;

            if (link && link->type==DATA_LINK_TYPE_CONTEXT) {

                ctx=(struct service_context_s *)((char *) link - offsetof(struct service_context_s, link));
                errcode=0;
	        goto checkctx;

            }

        }

    }

    h=get_list_header_context(pctx);
    read_lock_list_header(h);
    ctx=get_next_service_context(pctx, NULL, "tree");

    while (ctx) {
	const char *what="";
	unsigned int (* get_name_cb)(struct service_context_s *ctx, char *b, unsigned int l)=get_name_dummy;
	unsigned int len=0;

	/* browse every ctx under pctx and compare it's name with the one to look for */

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
	    get_name_cb=ctx->service.filesystem.fs->get_name;

	}

	len=(* get_name_cb)(ctx, NULL, 0);

	if (len>0) {
	    char buffer[len+1];
	    struct name_s tmp;

	    memset(buffer, 0, len+1);
	    len=(* get_name_cb)(ctx, buffer, len);
	    set_name(&tmp, buffer, len);
	    calculate_nameindex(&tmp);

            logoutput_debug("_fs_browse_lookup: comparing %s (%u) with %.*s (%u)", buffer, len, xname->len, xname->name, xname->len);

	    if (compare_names(xname, &tmp)==0) {
		unsigned char action=0;
		struct entry_s *entry=install_virtualnetwork_map(ctx, pdirectory, buffer, what, &action);

		if (entry) {

		    if (action==FUSE_NETWORK_ACTION_FLAG_ADDED) {
			struct directory_s *directory=get_directory(pctx, entry->inode, 0);

			if (directory) directory->ptr=&ctx->link;

		    }

		    _fs_common_cached_lookup(ctx, request, entry->inode);
		    errcode=0;
		    break;

		}

	    }

	    nextctx:
	    ctx=get_next_service_context(pctx, ctx, "tree");

	}

    }

    read_unlock_list_header(h);

    out:
    if (errcode>0) reply_VFS_error(request, errcode);

    checkctx:

    if (ctx && (errcode==0)) {

        if ((ctx->type==SERVICE_CTX_TYPE_BROWSE) && (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST)) {

            refresh_network_host_lookup(ctx);

        } else if ((ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) && context_filesystem_is_disconnected(ctx)) {

            int result=refresh_network_service_lookup(pctx, ctx);

            if (result==0) {

                if (ctx->interface.type==_INTERFACE_TYPE_SFTP_CLIENT) {

                    set_context_filesystem_sftp(ctx);

                } else {

                    logoutput_warning("_fs_browse_lookup: cannot set ctx filesystem (type interface %u not reckognized)", ctx->interface.type);

                }

            }

        }

    }

}
