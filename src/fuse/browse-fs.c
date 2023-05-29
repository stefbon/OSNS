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

#include "browse/getattr.h"
#include "browse/lookup.h"
#include "browse/opendir.h"

#include "shared/access.h"
#include "shared/xattr.h"
#include "shared/statfs.h"

#include "client/workspaces.h"
#include "client/network.h"
#include "client/resources.h"
#include "osns_client.h"

static unsigned int copy_name2buffer_hlpr(char *buffer, unsigned int size, char *name, unsigned int len)
{

    if (buffer) {

        if (size < len) len=size;
        memcpy(buffer, name, len);

    }

    return len;
}

struct read_network_resource_name_hlpr_s {
    char *buffer;
    unsigned int size;
};

static void read_network_resource_name_hlpr(struct network_resource_s *r, void *ptr)
{
    struct read_network_resource_name_hlpr_s *hlpr=(struct read_network_resource_name_hlpr_s *) ptr;

    if ((r->type==NETWORK_RESOURCE_TYPE_GROUP) || (r->type==NETWORK_RESOURCE_TYPE_HOST)) {

        hlpr->size=copy_name2buffer_hlpr(hlpr->buffer, hlpr->size, r->data.name, strlen(r->data.name));

    } else {

        hlpr->size=0;

    }

}

static unsigned int _fs_browse_get_name(struct service_context_s *ctx, char *buffer, unsigned int size)
{
    unsigned int len=0;

    if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	logoutput_debug("_fs_browse_get_name: type %u unique %u service %u", ctx->service.browse.type, ctx->service.browse.unique, ctx->service.browse.service);

	if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETWORK) {
	    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
	    struct client_session_s *session=get_client_session_workspace(w);

	    if (ctx->service.browse.service==NETWORK_SERVICE_TYPE_SSH) {

                len=copy_name2buffer_hlpr(buffer, size, session->options.network.name, strlen(session->options.network.name));

	    }

	} else if ((ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETGROUP) || (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST)) {
	    struct read_network_resource_name_hlpr_s hlpr;
	    struct db_query_result_s result=DB_QUERY_RESULT_INIT;

            hlpr.buffer=buffer;
            hlpr.size=size;

            if (get_client_network_data(ctx->service.browse.unique, read_network_resource_name_hlpr, &result, (void *) &hlpr)==0) {

                if (buffer) logoutput_debug("_fs_browse_get_name: found name %s for unique %u", buffer, ctx->service.browse.unique);
                len=hlpr.size;

            } else {

                logoutput_debug("_fs_browse_get_name: unable to get name for unique %u", ctx->service.browse.unique);

            }

	}

    } else {

	logoutput_debug("_fs_browse_get_name: context type %u not supported", ctx->type);

    }

    return len;

}

static void _fs_browse_setxattr(struct service_context_s *context, struct fuse_request_s *freq, struct inode_s *inode, const char *name, const char *value, size_t size, int flags)
{
    reply_VFS_error(freq, ENODATA);
}

static void _fs_browse_getxattr(struct service_context_s *context, struct fuse_request_s *freq, struct inode_s *inode, const char *name, size_t size)
{
    reply_VFS_error(freq, ENODATA);
}

static void _fs_browse_listxattr(struct service_context_s *context, struct fuse_request_s *freq, struct inode_s *inode, size_t size)
{
    reply_VFS_error(freq, ENODATA);
}

static void _fs_browse_removexattr(struct service_context_s *context, struct fuse_request_s *freq, struct inode_s *inode, const char *name)
{
    reply_VFS_error(freq, ENODATA);
}

/* this fs is attached to the rootinode of the network fuse mountpoint */

static struct browse_service_fs_s browse_fs = {

    .get_name			= _fs_browse_get_name,

    .lookup			= _fs_browse_lookup,
    .getattr			= _fs_browse_getattr,
    .setattr			= _fs_browse_setattr,

    .access			= _fs_common_access,

    .opendir			= _fs_browse_opendir,

    .getxattr			= _fs_browse_getxattr,
    .setxattr			= _fs_browse_setxattr,
    .listxattr			= _fs_browse_listxattr,
    .removexattr		= _fs_browse_removexattr,

    .statfs			= _fs_common_statfs,

};

void set_browse_context_fs(struct service_context_s *context)
{

    if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	context->service.browse.fs=&browse_fs;

    } else if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	context->service.workspace.fs=&browse_fs;

    } else {

	logoutput_warning("set_browse_context_fs: context type %i not reckognized", context->type);

    }

}
