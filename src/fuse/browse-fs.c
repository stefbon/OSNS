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
#include "libosns-resources.h"

#include "browse/access.h"
#include "browse/getattr.h"
#include "browse/lookup.h"
#include "browse/opendir.h"
#include "browse/xattr.h"
#include "browse/statfs.h"

#include "client/workspaces.h"
#include "client/network.h"
#include "osns_client.h"

static unsigned int _fs_browse_get_name(struct service_context_s *ctx, char *buffer, unsigned int size)
{
    unsigned int len=0;

    if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	logoutput_debug("_fs_browse_get_name: type %u unique %u service %u", ctx->service.browse.type, ctx->service.browse.unique, ctx->service.browse.service);

	if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETWORK) {
	    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
	    struct client_session_s *session=get_client_session_workspace(w);

	    if (ctx->service.browse.service==NETWORK_SERVICE_TYPE_SFTP) {

		len=strlen(session->options.network.name);

		if (buffer && size) {

		    if (size < len) len=size;
		    memcpy(buffer, session->options.network.name, len);

		} else {

		    size=len;

		}

	    }

	} else if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETGROUP) {
	    uint32_t unique=ctx->service.browse.unique;
	    struct network_resource_s nr;
	    int result=0;

	    memset(&nr, 0, sizeof(struct network_resource_s));
	    nr.type=NETWORK_RESOURCE_TYPE_NETWORK_GROUP;
	    result=get_network_resource(unique, &nr);

	    if (result==1) {

		len=strlen(nr.data.domain);

		if (buffer && size) {

		    if (size < len) len=size;
		    memcpy(buffer, nr.data.domain, len);

		} else {

		    size=len;

		}

	    } else {

		logoutput_debug("_fs_browse_get_name: get resource data %i", result);

	    }

	} else if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST) {
	    uint32_t unique=ctx->service.browse.unique;
	    struct network_resource_s nr;
	    int result=0;

	    /* network host service ctx is pointing to network socket resource ... */

	    memset(&nr, 0, sizeof(struct network_resource_s));
	    nr.type=NETWORK_RESOURCE_TYPE_NETWORK_SOCKET;
	    result=get_network_resource(unique, &nr);

	    if (result==1) {

		unique=nr.parent_unique;

		memset(&nr, 0, sizeof(struct network_resource_s));
		nr.type=NETWORK_RESOURCE_TYPE_NETWORK_HOST;
		result=get_network_resource(unique, &nr);

		if (result==1) {

		    if (nr.data.address.flags & HOST_ADDRESS_FLAG_HOSTNAME) {
			char *sep=NULL;

			len=strlen(nr.data.address.hostname);
			sep=memchr(nr.data.address.hostname, '.', len);
			if (sep) len=(unsigned int)(sep - nr.data.address.hostname);

			if (buffer && size) {

			    if (size < len) len=size;
			    memcpy(buffer, nr.data.address.hostname, len);

			} else {

			    size=len;

			}

		    }

		}

	    } else {

		logoutput_debug("_fs_browse_get_name: get resource data %i", result);

	    }

	}

	if (buffer) {

	    if (len>0) {

		logoutput_debug("_fs_browse_get_name: found %s", buffer);

	    } else {

		logoutput_debug("_fs_browse_get_name: no name found");

	    }

	}

    } else {

	logoutput_debug("_fs_browse_get_name: context type %u not supported", ctx->type);

    }

    return size;

}

/* this fs is attached to the rootinode of the network fuse mountpoint */

static struct browse_service_fs_s browse_fs = {

    .get_name			= _fs_browse_get_name,

    .lookup_existing		= _fs_browse_lookup_existing,
    .lookup_new			= _fs_browse_lookup_new,

    .getattr			= _fs_browse_getattr,
    .setattr			= _fs_browse_setattr,
    .access			= _fs_browse_access,

    .opendir			= _fs_browse_opendir,

    .getxattr			= _fs_browse_getxattr,
    .setxattr			= _fs_browse_setxattr,
    .listxattr			= _fs_browse_listxattr,
    .removexattr		= _fs_browse_removexattr,

    .statfs			= _fs_browse_statfs,

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
