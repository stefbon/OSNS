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

#include "osns-protocol.h"

#include "libosns-threads.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-fuse.h"
#include "libosns-context.h"
#include "libosns-log.h"

#include "next.h"

void free_service_context(struct service_context_s *ctx)
{
    free(ctx);
}

static void init_service_context(struct service_context_s *context, unsigned char ctype, unsigned int size, unsigned int itype)
{
    struct context_interface_s *interface=&context->interface;

    /* context */

    context->type=ctype; /* like SERVICE_CTX_TYPE_WORKSPACE */
    init_list_element(&context->wlist, NULL);
    context->link.type=DATA_LINK_TYPE_CONTEXT;
    context->link.refcount=0;

    if (ctype==SERVICE_CTX_TYPE_WORKSPACE) {

	init_list_header(&context->service.workspace.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	context->service.workspace.signal=NULL;
	context->service.workspace.fs=NULL;

    } else if (ctype==SERVICE_CTX_TYPE_BROWSE) {

	context->service.browse.type=0;
	context->service.browse.fs=NULL;
	init_list_element(&context->service.browse.clist, NULL);
	set_system_time(&context->service.browse.refresh, 0, 0);
	init_list_header(&context->service.browse.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	context->service.browse.threadid=0;
	context->service.browse.unique=0;
	context->service.browse.service=0;

    } else if (ctype==SERVICE_CTX_TYPE_FILESYSTEM) {

	context->service.filesystem.inode=NULL;
	context->service.filesystem.fs=NULL;
	init_list_element(&context->service.filesystem.clist, NULL);
	init_list_header(&context->service.filesystem.pathcaches, SIMPLE_LIST_TYPE_EMPTY, NULL);
	context->service.filesystem.service=0;

    } else if (ctype==SERVICE_CTX_TYPE_SHARED) {

	set_system_time(&context->service.shared.refresh, 0, 0);
	context->service.shared.unique=0;
	context->service.shared.service=0;
	context->service.shared.transport=0;

    }

    /* interface */

    interface->type=itype; /* like _INTERFACE_TYPE_SSH_SESSION, _INTERFACE_TYPE_FUSE */
    interface->flags=0;
    interface->size=size;

}

void set_name_service_context(struct service_context_s *ctx)
{

    if (strlen(ctx->name)==0) {
	struct workspace_mount_s *workspace=get_workspace_mount_ctx(ctx);
	char *basename="UNKNOWN";
	char *name="unknown";

	if (workspace) {

	    if (workspace->type==OSNS_MOUNT_TYPE_NETWORK) {

		basename="FUSE:NETWORK";

	    }

	}

	if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    name="filesystem";

	} else if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	    if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETWORK) {

		name="network";

	    } else if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETGROUP) {

		name="netgroup";

	    } else if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST) {

		name="nethost";

	    } else if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETSOCKET) {

		name="netsocket";

	    }

	} else if (ctx->type==SERVICE_CTX_TYPE_WORKSPACE) {

	    name="fuse";

	} else if (ctx->type==SERVICE_CTX_TYPE_SHARED) {

	    name="shared";

	}

	snprintf(ctx->name, sizeof(ctx->name) - 1, "%s:%s", basename, name);

    }

}

struct service_context_s *create_service_context(struct workspace_mount_s *workspace, struct service_context_s *parent, struct interface_list_s *ilist, unsigned char type, struct service_context_s *primary)
{
    struct service_context_s *context=NULL;
    unsigned int size=0;

    if (ilist) {
	struct interface_ops_s *ops=ilist->ops;

	if ((primary==NULL) || (primary->interface.type!=ilist->type)) {

	    size=(* ops->get_buffer_size)(ilist);

	}

    }

    logoutput("create_service_context: type %u size %u", type, size);

    context=malloc(sizeof(struct service_context_s) + size);
    if (context==NULL) return NULL;
    memset(context, 0, sizeof(struct service_context_s) + size);
    context->flags=SERVICE_CTX_FLAG_ALLOC;
    init_context_interface(&context->interface, NULL, NULL);
    init_service_context(context, type, size, (ilist) ? ilist->type : 0);

    if (workspace) add_service_context_workspace(workspace, context);
    if (parent) set_parent_service_context(parent, context);

    /* initialize the black box buffer for the interface specific library */

    if (ilist) {
	struct interface_ops_s *ops=ilist->ops;

	logoutput("create_service_context: init buffer for %s:%s", ops->name, ilist->name);

	if ((* ops->init_buffer)(&context->interface, ilist, ((primary) ? &primary->interface : NULL))==0) {

	    if (ops->name && ilist->name) {

		snprintf(context->name, sizeof(context->name) - 1, "%s:%s", ops->name, ilist->name);
		logoutput("create_service_context: set name %s", context->name);

	    }

	} else {

	    remove_list_element(&context->wlist);
	    if (parent) unset_parent_service_context(parent, context);
	    free(context);
	    context=NULL;

	}

    }

    out:
    return context;

}
