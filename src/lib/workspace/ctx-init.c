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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fsuid.h>

#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "threads.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspaces.h"
#include "fuse.h"
#include "context.h"
#include "log.h"

void free_service_context(struct service_context_s *ctx)
{
    logoutput_debug("free_service_context");

    if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	pthread_mutex_destroy(&ctx->service.filesystem.mutex);

    }

    free(ctx);
}

static int connect_default(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    return -1;
}
static int start_default(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    return -1;
}
static int signal_nothing(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    return -1;
}
static char *get_interface_buffer(struct context_interface_s *interface)
{
    return interface->buffer;
}

static void init_service_context(struct service_context_s *context, unsigned char ctype, unsigned int size, unsigned int itype)
{
    struct context_interface_s *interface=&context->interface;

    /* context */

    context->type=ctype; /* like SERVICE_CTX_TYPE_WORKSPACE */
    init_list_element(&context->wlist, NULL);

    if (ctype==SERVICE_CTX_TYPE_WORKSPACE) {

	context->service.workspace.fs=NULL;
	init_list_header(&context->service.workspace.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	context->service.workspace.signal=NULL;

    } else if (ctype==SERVICE_CTX_TYPE_NETWORK) {

	context->service.network.fs=NULL;
	init_list_element(&context->service.network.clist, NULL);
	init_list_header(&context->service.network.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	context->service.network.refresh.tv_sec=0;
	context->service.network.refresh.tv_nsec=0;
	context->service.network.threadid =0;

    } else if (ctype==SERVICE_CTX_TYPE_BROWSE) {

	context->service.browse.type=0;
	context->service.browse.fs=NULL;
	context->service.browse.unique=0;
	init_list_element(&context->service.browse.clist, NULL);
	context->service.browse.refresh.tv_sec=0;
	context->service.browse.refresh.tv_nsec=0;
	init_list_header(&context->service.browse.header, SIMPLE_LIST_TYPE_EMPTY, NULL);
	context->service.browse.threadid =0;

    } else if (ctype==SERVICE_CTX_TYPE_FILESYSTEM) {

	context->service.filesystem.inode=NULL;
	context->service.filesystem.fs=NULL;
	init_list_element(&context->service.filesystem.clist, NULL);
	init_list_header(&context->service.filesystem.pathcaches, SIMPLE_LIST_TYPE_EMPTY, NULL);
	pthread_mutex_init(&context->service.filesystem.mutex, NULL);

    }

    /* interface */

    interface->type=itype; /* like _INTERFACE_TYPE_SSH_SESSION, _INTERFACE_TYPE_FUSE */
    interface->flags=0;
    interface->connect=connect_default;
    interface->start=start_default;
    interface->signal_context=signal_nothing;
    interface->signal_interface=signal_nothing;
    interface->get_interface_buffer=get_interface_buffer;
    interface->size=size;
    memset(interface->buffer, 0, size);

}

void set_name_service_context(struct service_context_s *ctx)
{

    if (strlen(ctx->name)==0) {
	struct workspace_mount_s *workspace=get_workspace_mount_ctx(ctx);
	char *basename="UNKNOWN";
	char *name="unknown";

	if (workspace) {

	    if (workspace->type==WORKSPACE_TYPE_NETWORK) {

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

	}

	snprintf(ctx->name, sizeof(ctx->name) - 1, "%s:%s", basename, name);

    }

}

struct service_context_s *create_service_context(struct workspace_mount_s *workspace, struct service_context_s *parent, struct interface_list_s *ilist, unsigned char type, struct service_context_s *primary)
{
    struct service_context_s *context=NULL;
    unsigned int size=0;

    logoutput("create_service_context");

    if (ilist) {
	struct interface_ops_s *ops=ilist->ops;

        size=(primary==NULL) ? (* ops->get_buffer_size)(ilist) : 0;

    }

    context=malloc(sizeof(struct service_context_s) + size);
    if (context==NULL) return NULL;
    memset(context, 0, sizeof(struct service_context_s) + size);
    context->flags=SERVICE_CTX_FLAG_ALLOC;
    init_service_context(context, type, size, (ilist) ? ilist->type : 0);

    if (workspace) add_list_element_last(&workspace->contexes, &context->wlist);
    if (parent) set_parent_service_context(parent, context);

    /* initialize the black box buffer for the interface specific library */

    if (ilist) {
	struct interface_ops_s *ops=ilist->ops;

	logoutput("create_service_context: init buffer for %s:%s", ops->name, ilist->name);

	if ((* ops->init_buffer)(&context->interface, ilist, ((primary) ? &primary->interface : NULL))==0) {

	    if (ops->name && ilist->name) snprintf(context->name, sizeof(context->name) - 1, "%s:%s", ops->name, ilist->name);

	    if (context->type==SERVICE_CTX_TYPE_WORKSPACE && ilist->type==_INTERFACE_TYPE_FUSE) {

		context->service.workspace.signal=get_fusesocket_signal(&context->interface);

	    }

	} else {

	    if (workspace) (* workspace->remove_context)(&context->wlist);
	    if (parent) unset_parent_service_context(parent, context);
	    free(context);
	    context=NULL;
	    goto out;

	}

    }

    out:
    return context;

}
