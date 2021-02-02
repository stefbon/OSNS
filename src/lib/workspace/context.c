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

struct service_context_s *get_service_context(struct context_interface_s *interface)
{
    return (struct service_context_s *) ( ((char *) interface) - offsetof(struct service_context_s, interface));
}

void free_service_context(struct service_context_s *context)
{
    logoutput("free_service_context");
    free(context);
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
static struct context_interface_s *get_parent_interface(struct context_interface_s *interface)
{
    if (interface) {
	struct service_context_s *context=get_service_context(interface);
	if (context->parent) return &context->parent->interface;
    }
    return NULL;
}

static void init_service_context(struct service_context_s *context, unsigned char ctype, unsigned int size, unsigned int itype)
{
    struct context_interface_s *interface=&context->interface;

    /* context */

    context->type=ctype; /* like SERVICE_CTX_TYPE_WORKSPACE, SERVICE_CTX_TYPE_CONNECTION */
    context->fscount=0;
    context->serviceid=0;
    context->workspace=NULL;
    init_list_element(&context->list, NULL);
    context->parent=NULL;
    pthread_mutex_init(&context->mutex, NULL);

    if (ctype==SERVICE_CTX_TYPE_FILESYSTEM) {

	context->service.filesystem.inode=NULL;
	context->service.filesystem.fs=NULL;
	init_list_header(&context->service.filesystem.pathcaches, SIMPLE_LIST_TYPE_EMPTY, NULL);

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

struct service_context_s *create_service_context(struct workspace_mount_s *workspace, struct service_context_s *parent, struct interface_list_s *ilist, unsigned char type, struct service_context_s *primary)
{
    struct service_context_s *context=NULL;
    struct interface_ops_s *ops=ilist->ops;
    unsigned int size=0;

    logoutput("create_service_context");

    size=(primary==NULL) ? (* ops->get_buffer_size)(ilist) : 0;

    context=malloc(sizeof(struct service_context_s) + size);
    if (context==NULL) return NULL;
    memset(context, 0, sizeof(struct service_context_s) + size);
    context->flags=SERVICE_CTX_FLAG_ALLOC;
    init_service_context(context, type, size, ilist->type);

    if (workspace) {

	context->workspace=workspace;
	(* workspace->add_context)(workspace, &context->list);

    }

    context->parent=parent;

    /* initialize the black box buffer for the interface specific library */

    logoutput("create_service_context: init");

    if ((* ops->init_buffer)(&context->interface, ilist, ((primary) ? &primary->interface : NULL))==0) {

	snprintf(context->name, sizeof(context->name), "%s:%s", ops->name, ilist->name);

    } else {

	if (workspace) (* workspace->remove_context)(&context->list);
	free(context);
	context=NULL;

    }

    logoutput("create_service_context: ready");

    return context;

}

struct service_context_s *get_next_service_context(struct workspace_mount_s *workspace, struct service_context_s *context)
{
    struct list_element_s *list=((context) ? get_next_element(&context->list) : get_list_head(&workspace->contexes, 0));
    return (list) ? ((struct service_context_s *)(((char *) list) - offsetof(struct service_context_s, list))) : NULL;
}

struct context_interface_s *get_next_context_interface(struct context_interface_s *reference, struct context_interface_s *interface)
{
    struct service_context_s *context=NULL;
    struct workspace_mount_s *workspace=NULL;

    if (interface) {

	context=(struct service_context_s *) (((char *) interface) - offsetof(struct service_context_s, interface));
	workspace=context->workspace;
	context=get_next_service_context(workspace, context);

    } else if (reference) {

	context=(struct service_context_s *) (((char *) reference) - offsetof(struct service_context_s, interface));
	workspace=context->workspace;
	context=get_next_service_context(workspace, NULL);

    }

    return ((context) ? (&context->interface) : NULL);

}

struct service_context_s *get_root_context(struct service_context_s *context)
{
    struct workspace_mount_s *workspace=context->workspace;
    return get_workspace_context(workspace);
}

struct osns_user_s *get_user_context(struct service_context_s *context)
{
    struct workspace_mount_s *workspace=context->workspace;
    return workspace->user;
}

struct service_context_s *get_container_context(struct list_element_s *list)
{
    return (list) ? (struct service_context_s *) (((char *)list) - offsetof(struct service_context_s, list)) : NULL;
}

struct service_context_s *get_workspace_context(struct workspace_mount_s *workspace)
{
    struct list_element_s *list=get_list_head(&workspace->contexes, 0);
    return get_container_context(list);
}
