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
#include "session.h"
#include "log.h"

struct service_context_s *get_parent_context(struct service_context_s *ctx)
{
    struct service_context_s *pctx=NULL;
    struct list_header_s *h=NULL;

    if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	h=ctx->service.browse.clist.h;
	if (h==NULL) return NULL;

	/* depending the type: nethost and netgroup have a browse parent, network has parent of type workspace */

	if (ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETWORK) {

	    pctx=(struct service_context_s *)((char *) h - offsetof(struct service_context_s, service.workspace.header));

	} else {

	    pctx=(struct service_context_s *)((char *) h - offsetof(struct service_context_s, service.browse.header));

	}

    } else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	h=ctx->service.filesystem.clist.h;
	if (h==NULL) return NULL;

	/* parent is a context of type browse (always) */

	pctx=(struct service_context_s *)((char *) h - offsetof(struct service_context_s, service.browse.header));

    }

    return pctx;

}

struct context_interface_s *get_parent_interface(struct context_interface_s *interface)
{

    if (interface) {
	struct service_context_s *ctx=(struct service_context_s *) (((char *) interface) - offsetof(struct service_context_s, interface));
	struct service_context_s *pctx=get_parent_context(ctx);

	return (pctx) ? &pctx->interface : NULL;

    }

    return NULL;
}

void add_service_context_workspace(struct workspace_mount_s *workspace, struct service_context_s *ctx)
{
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;
    struct service_context_s *root=NULL;
    struct list_header_s *h=&workspace->contexes;
    struct list_element_s *list=get_list_head(h, 0);

    init_service_ctx_lock(&ctxlock, NULL, ctx);
    if (list) root=(struct service_context_s *)((char *) list - offsetof(struct service_context_s, wlist));
    set_root_service_ctx_lock(root, &ctxlock);

    if (lock_service_context(&ctxlock, "w", "w")==1) {

	add_list_element_last(h, &ctx->wlist);
	unlock_service_context(&ctxlock, "w", "w");

    }

}

void set_parent_service_context_unlocked(struct service_context_s *pctx, struct service_context_s *ctx, const char *what)
{

    if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	if (pctx->type==SERVICE_CTX_TYPE_BROWSE) {

	    if (strcmp(what, "add")==0) {

		add_list_element_last(&pctx->service.browse.header, &ctx->service.filesystem.clist);

	    } else if (strcmp(what, "remove")==0) {

		remove_list_element(&ctx->service.filesystem.clist);

	    }

	} else {

	    logoutput_warning("set_parent_service_context_unlocked: cannot set parent");

	}

    } else if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	if (pctx->type==SERVICE_CTX_TYPE_BROWSE) {

	    if (strcmp(what, "add")==0) {

		add_list_element_last(&pctx->service.browse.header, &ctx->service.browse.clist);

	    } else if (strcmp(what, "remove")==0) {

		remove_list_element(&ctx->service.browse.clist);

	    }

	} else if (pctx->type==SERVICE_CTX_TYPE_WORKSPACE) {

	    if (strcmp(what, "add")==0) {

		add_list_element_last(&pctx->service.workspace.header, &ctx->service.browse.clist);

	    } else if (strcmp(what, "remove")==0) {

		remove_list_element(&ctx->service.browse.clist);

	    }

	} else {

	    logoutput_warning("set_parent_service_context_unlocked: cannot set parent");

	}

    } else {

	logoutput_warning("set_parent_service_context_unlocked: cannot set parent");

    }

}

static void _set_parent_service_context(struct service_context_s *pctx, struct service_context_s *ctx, const char *what)
{
    struct service_context_lock_s ctxlock=SERVICE_CTX_LOCK_INIT;

    init_service_ctx_lock(&ctxlock, pctx, ctx);

    if (lock_service_context(&ctxlock, "w", "p")==1) {

	set_parent_service_context_unlocked(pctx, ctx, what);
	unlock_service_context(&ctxlock, "w", "p");

    }

}

void set_parent_service_context(struct service_context_s *pctx, struct service_context_s *ctx)
{
    _set_parent_service_context(pctx, ctx, "add");
}

void unset_parent_service_context(struct service_context_s *pctx, struct service_context_s *ctx)
{
    _set_parent_service_context(pctx, ctx, "remove");
}

struct service_context_s *get_next_service_context_workspace(struct service_context_s *parent, struct service_context_s *context)
{
    struct workspace_mount_s *workspace=NULL;

    logoutput_debug("get_next_service_context_workspace");

    if (context) workspace=get_workspace_mount_ctx(context);
    if (workspace==NULL && parent) workspace=get_workspace_mount_ctx(parent);
    if (workspace==NULL) return NULL;

    struct list_element_s *list=((context) ? get_next_element(&context->wlist) : get_list_head(&workspace->contexes, 0));
    return (list) ? ((struct service_context_s *)(((char *) list) - offsetof(struct service_context_s, wlist))) : NULL;
}

struct service_context_s *get_next_service_context_tree(struct service_context_s *parent, struct service_context_s *context)
{

    logoutput_debug("get_next_service_context_tree");

    if (context) {
	struct list_element_s *list=NULL;

	if (context->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    list=get_next_element(&context->service.filesystem.clist);
	    if (list) return (struct service_context_s *)((char *) list - offsetof(struct service_context_s, service.filesystem.clist));

	} else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	    list=get_next_element(&context->service.browse.clist);
	    if (list) return (struct service_context_s *)((char *) list - offsetof(struct service_context_s, service.browse.clist));

	}

    } else if (parent) {

	/* get a startpoint: look for the header in the parent */

	if (parent->type==SERVICE_CTX_TYPE_WORKSPACE) {
	    struct list_element_s *list=get_list_head(&parent->service.workspace.header, 0);

	    if (list) return (struct service_context_s *)((char *) list - offsetof(struct service_context_s, service.browse.clist));

	} else if (parent->type==SERVICE_CTX_TYPE_BROWSE) {
	    struct list_element_s *list=get_list_head(&parent->service.browse.header, 0);

	    if (parent->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST) {

		/* the children of a nethost are filesystems */
		if (list) return (struct service_context_s *)((char *) list - offsetof(struct service_context_s, service.filesystem.clist));

	    } else {

		if (list) return (struct service_context_s *)((char *) list - offsetof(struct service_context_s, service.browse.clist));

	    }

	}

    }

    return NULL;

}

struct service_context_s *get_next_service_context(struct service_context_s *parent, struct service_context_s *context, const char *what)
{

    if (strcmp(what, "tree")==0) {

	return get_next_service_context_tree(parent, context);

    } else if (strcmp(what, "workspace")==0) {

	return get_next_service_context_workspace(parent, context);

    }

    return NULL;

}

struct context_interface_s *get_next_context_interface(struct context_interface_s *reference, struct context_interface_s *interface)
{
    struct service_context_s *ctx=NULL;
    struct context_interface_s *tmp=(interface) ? interface : reference;

    if (tmp) {

	ctx=(struct service_context_s *) (((char *) tmp) - offsetof(struct service_context_s, interface));
	ctx=get_next_service_context(get_parent_context(ctx), ctx, "workspace");

    }

    return ((ctx) ? (&ctx->interface) : NULL);

}

struct workspace_mount_s *get_workspace_mount_ctx(struct service_context_s *context)
{
    struct list_header_s *h=context->wlist.h;
    if (h) return (struct workspace_mount_s *)((char *) h - offsetof(struct workspace_mount_s, contexes));
    return NULL;
}

struct service_context_s *get_root_context_workspace(struct workspace_mount_s *w)
{
    struct list_header_s *h=&w->contexes;
    struct list_element_s *list=get_list_head(h, 0);

    return (list) ? (struct service_context_s *)((char *) list - offsetof(struct service_context_s, wlist)) : NULL;
}

struct service_context_s *get_root_context(struct service_context_s *context)
{
    struct list_header_s *h=NULL;

    if (context==NULL || context->type==SERVICE_CTX_TYPE_WORKSPACE) return context;

    h=context->wlist.h;

    if (h) {
	struct list_element_s *list=get_list_head(h, 0);

	if (list) return (struct service_context_s *)((char *) list - offsetof(struct service_context_s, wlist));

    }

    return NULL;

}

struct passwd *get_workspace_user_pwd(struct context_interface_s *i)
{
    struct service_context_s *ctx=(struct service_context_s *) (((char *) i) - offsetof(struct service_context_s, interface));
    struct list_header_s *h=ctx->wlist.h;

    if (h) {
	struct workspace_mount_s *w=(struct workspace_mount_s *)((char *) h - offsetof(struct workspace_mount_s, contexes));
	struct osns_user_s *user=w->user;

	if (user) return &user->pwd;

    }

    return NULL;

}

struct beventloop_s *get_workspace_eventloop(struct context_interface_s *interface)
{
    struct service_context_s *ctx=(struct service_context_s *) (((char *) interface) - offsetof(struct service_context_s, interface));
    struct service_context_s *root=get_root_context(ctx);

    return (root && root->type==SERVICE_CTX_TYPE_WORKSPACE) ? root->interface.backend.fuse.loop : NULL;

}

struct common_signal_s *get_workspace_signal(struct context_interface_s *interface)
{
    struct service_context_s *ctx=(struct service_context_s *) (((char *) interface) - offsetof(struct service_context_s, interface));
    struct service_context_s *root=get_root_context(ctx);

    return (root && root->type==SERVICE_CTX_TYPE_WORKSPACE) ? root->service.workspace.signal : NULL;

}

