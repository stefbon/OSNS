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

#include "libosns-threads.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-fuse.h"
#include "libosns-context.h"
#include "libosns-log.h"

#include "lock.h"

struct workspace_mount_s *get_workspace_mount_ctx(struct service_context_s *ctx)
{
    struct workspace_mount_s *w=NULL;
    struct list_header_s *h=ctx->wlist.h;

    if (h) {
	unsigned int offset=((ctx->type==SERVICE_CTX_TYPE_SHARED) ? offsetof(struct workspace_mount_s, shared_contexes) : offsetof(struct workspace_mount_s, contexes));

	w=(struct workspace_mount_s *)((char *)h - offset);

    }

    return w;
}

struct list_header_s *get_list_header_context(struct service_context_s *ctx)
{
    struct list_header_s *h=NULL;

    if (ctx==NULL) return NULL;

    if (ctx->type==SERVICE_CTX_TYPE_WORKSPACE) {

	h=&ctx->service.workspace.header;

    } else if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	h=&ctx->service.browse.header;

    }

    return h;

}

struct list_element_s *get_list_element_context(struct service_context_s *ctx, const char *what)
{
    struct list_element_s *list=NULL;

    if (strcmp(what, "tree")==0) {

	if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	    list=&ctx->service.browse.clist;

	} else if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    list=&ctx->service.filesystem.clist;

	}

    } else if (strcmp(what, "workspace")==0) {

	list=&ctx->wlist;

    }

    return list;

}

unsigned int get_offset_list_header_context(unsigned int type)
{
    unsigned int offset=0;

    if (type==SERVICE_CTX_TYPE_WORKSPACE) {

	offset=offsetof(struct service_context_s, service.workspace.header);

    } else if (type==SERVICE_CTX_TYPE_BROWSE) {

	offset=offsetof(struct service_context_s, service.browse.header);

    }

    return offset;
}

unsigned int get_offset_list_element_context(unsigned int type, const char *what)
{
    unsigned int offset=0;

    if (strcmp(what, "tree")==0) {

	if (type==SERVICE_CTX_TYPE_BROWSE) {

	    offset=offsetof(struct service_context_s, service.browse.clist);

	} else if (type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    offset=offsetof(struct service_context_s, service.filesystem.clist);

	}

    } else if (strcmp(what, "workspace")==0) {

	offset=offsetof(struct service_context_s, wlist);

    }

    return offset;
}

struct service_context_s *get_parent_context(struct service_context_s *ctx)
{
    struct service_context_s *pctx=NULL;
    struct list_element_s *list=get_list_element_context(ctx, "tree");
    struct list_header_s *h=((list) ? list->h : NULL);

    if (h) {
	unsigned char ptype=((ctx->type==SERVICE_CTX_TYPE_BROWSE && ctx->service.browse.type==SERVICE_BROWSE_TYPE_NETWORK) ? SERVICE_CTX_TYPE_WORKSPACE : SERVICE_CTX_TYPE_BROWSE);
	unsigned int offset=get_offset_list_header_context(ptype);

	pctx=(struct service_context_s *)((char *)h - offset);

    }

    return pctx;

}

void add_service_context_workspace(struct workspace_mount_s *w, struct service_context_s *ctx)
{

    if (w && ctx) {
	struct list_header_s *h=((ctx->type==SERVICE_CTX_TYPE_SHARED) ? &w->shared_contexes : &w->contexes);

	add_list_element_last(h, &ctx->wlist);

    }

}

static void _set_parent_service_context(struct service_context_s *pctx, struct service_context_s *ctx, const char *what)
{

    if (strcmp(what, "remove")==0) {
	struct list_element_s *list=get_list_element_context(ctx, "tree");

	if (list) remove_list_element(list);

    } else if (strcmp(what, "add")==0) {
	struct list_header_s *h=get_list_header_context(pctx);
	struct list_element_s *list=get_list_element_context(ctx, "tree");

	if (h && list) add_list_element_last(h, list);

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

static struct service_context_s *get_next_service_context_tree(struct service_context_s *parent, struct service_context_s *ctx)
{
    struct list_element_s *list=NULL;
    unsigned int type=0;

    if (ctx) {

	type=ctx->type;
	list=get_list_element_context(ctx, "tree");
	if (list) list=get_next_element(list);

    } else if (parent) {
	struct list_header_s *h=get_list_header_context(parent);

	/* get a startpoint: look for the header in the parent */

	if (h) list=get_list_head(h, 0);
	type=((parent->type==SERVICE_CTX_TYPE_BROWSE && parent->service.browse.type==SERVICE_BROWSE_TYPE_NETHOST) ? SERVICE_CTX_TYPE_FILESYSTEM : SERVICE_CTX_TYPE_BROWSE);


    }

    if (list) {
	unsigned int offset=get_offset_list_element_context(type, "tree");

	ctx=(struct service_context_s *)((char *)list - offset);

    } else {

	ctx=NULL;

    }

    return ctx;

}

struct service_context_s *get_next_service_context(struct service_context_s *parent, struct service_context_s *context, const char *what)
{

    if (strcmp(what, "tree")==0) return get_next_service_context_tree(parent, context);
    return NULL;
}

struct service_context_s *get_next_shared_service_context(struct workspace_mount_s *w, struct service_context_s *ctx)
{
    struct list_element_s *list=NULL;

    if (ctx==NULL) {

	list=(w ? get_list_head(&w->shared_contexes, 0) : NULL);

    } else {

	list=get_next_element(&ctx->wlist);

    }

    return ((list) ? ((struct service_context_s *)(((char *) list) - offsetof(struct service_context_s, wlist))) : NULL);
}

struct service_context_s *get_root_context_workspace(struct workspace_mount_s *w)
{
    struct list_element_s *list=(w ? get_list_head(&w->contexes, 0) : NULL);

    return (list) ? (struct service_context_s *)((char *) list - offsetof(struct service_context_s, wlist)) : NULL;
}

struct service_context_s *get_root_context(struct service_context_s *ctx)
{
    struct workspace_mount_s *w=NULL;

    if (ctx==NULL || ctx->type==SERVICE_CTX_TYPE_WORKSPACE) return ctx;
    w=get_workspace_mount_ctx(ctx);
    return get_root_context_workspace(w);

}

int signal_selected_ctx(struct context_interface_s *i, unsigned char shared, const char *what, struct io_option_s *option, unsigned int type, int (* select)(struct context_interface_s *i, void *ptr), void *ptr)
{
    struct service_context_s *ctx=get_service_context(i);
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct list_header_s *h=(shared ? &w->shared_contexes : &w->contexes);
    struct service_context_s *tmp=NULL;
    int count=0;

    read_lock_list_header(h);
    tmp=get_next_shared_service_context(w, NULL);

    while (tmp) {

	int result=(* select)(&tmp->interface, ptr);

	if (result==1) {

	    (* tmp->interface.iocmd.out)(&tmp->interface, what, option, i, type);
	    count++;

	} else if (result==-1) {

	    break;

	}

	tmp=get_next_shared_service_context(w, tmp);

    }

    read_unlock_list_header(h);
    return count;
}
