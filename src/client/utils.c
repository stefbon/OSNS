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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-threads.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-resources.h"

#include "fuse/browse-fs.h"

#include "osns_client.h"
#include "network.h"
#include "utils.h"

static int compare_dummy(struct service_context_s *ctx, void *ptr)
{
    return -1;
}

struct service_context_s *create_network_shared_context(struct workspace_mount_s *w, uint32_t unique, unsigned int service, unsigned int transport, unsigned int itype, struct service_context_s *primary, int (* compare)(struct service_context_s *ctx, void *ptr), void *ptr)
{
    struct service_context_s *ctx=NULL;
    struct list_header_s *h=&w->shared_contexes;

    logoutput("create_network_shared_context: unique %lu service %u transport %u", unique, service, transport);
    if (compare==NULL) compare=compare_dummy;

    write_lock_list_header(h);

    ctx=get_next_shared_service_context(w, NULL);

    while (ctx) {

	if (unique>0) if (ctx->service.shared.unique==unique) break;

	if (primary && (ctx->interface.flags & (_INTERFACE_FLAG_SECONDARY_1TO1 | _INTERFACE_FLAG_SECONDARY_1TON))) {

	    if (ctx->interface.link.primary==&primary->interface) {

		if ((* compare)(ctx, ptr)==0) break;

	    }

	}

	ctx=get_next_shared_service_context(w, ctx);

    }

    if (ctx==NULL) {
	unsigned int count=build_interface_ops_list(NULL, NULL, 0);
	struct interface_list_s ailist[count];
	struct interface_list_s *ilist=NULL;

	count=build_interface_ops_list(NULL, ailist, 0);
	ilist=get_interface_list(ailist, count, itype);

	ctx=create_service_context(w, NULL, ilist, SERVICE_CTX_TYPE_SHARED, primary);

	if (ctx) {

	    ctx->service.shared.unique=unique;
	    ctx->service.shared.service=service;
	    ctx->service.shared.transport=transport;
	    set_name_service_context(ctx);

	}

    }

    write_unlock_list_header(h);
    return ctx;

}

struct service_context_s *create_network_filesystem_context(struct workspace_mount_s *w, struct service_context_s *parent, char *name, unsigned int service, struct service_context_s *primary)
{
    struct service_context_s *context=NULL;

    logoutput("create_network_filesystem_context: name %s service %u", name, service);

    if (primary) {
	unsigned int count=build_interface_ops_list(NULL, NULL, 0);
	struct interface_list_s ailist[count];
	struct interface_list_s *ilist=NULL;

	count=build_interface_ops_list(NULL, ailist, 0);
	ilist=get_interface_list(ailist, count, primary->interface.type);

	context=create_service_context(w, parent, ilist, SERVICE_CTX_TYPE_FILESYSTEM, primary);

    } else {

	context=create_service_context(w, parent, NULL, SERVICE_CTX_TYPE_FILESYSTEM, NULL);

    }

    if (context) {

	context->service.filesystem.service=service;
	context->service.filesystem.signal=w->signal;
	strcpy(context->service.filesystem.name, name);
	set_name_service_context(context);

	if (service==NETWORK_SERVICE_TYPE_SFTP) {

	    set_context_filesystem_sftp(context, 0);

	}

    }

    return context;

}

struct service_context_s *create_network_browse_context(struct workspace_mount_s *w, struct service_context_s *parent, unsigned int btype, uint32_t unique, unsigned int service, struct service_context_s *primary)
{
    struct service_context_s *context=NULL;

    logoutput("create_network_browse_context: type %i unique %lu service %u", btype, unique, service);

    if (primary) {
	unsigned int count=build_interface_ops_list(NULL, NULL, 0);
	struct interface_list_s ailist[count];
	struct interface_list_s *ilist=NULL;

	count=build_interface_ops_list(NULL, ailist, 0);
	ilist=get_interface_list(ailist, count, primary->interface.type);

	context=create_service_context(w, parent, ilist, SERVICE_CTX_TYPE_BROWSE, primary);

    } else {

	context=create_service_context(w, parent, NULL, SERVICE_CTX_TYPE_BROWSE, NULL);

    }

    if (context) {

	context->service.browse.type=btype;
	context->service.browse.unique=unique;
	context->service.browse.service=service;
	set_name_service_context(context);
	set_browse_context_fs(context);

    }

    return context;

}

struct service_context_s *check_create_install_context(struct workspace_mount_s *w, struct service_context_s *pctx, uint32_t unique, char *name, unsigned int service, struct service_context_s *primary, unsigned char *p_action)
{
    struct service_context_s *ctx=NULL;
    unsigned int type=0;
    unsigned int btype=0;

    logoutput_debug("check_create_install_context: unique=%u service=%u", unique, service);

    if (pctx->type==SERVICE_CTX_TYPE_BROWSE) {

	switch (pctx->service.browse.type) {

	    case SERVICE_BROWSE_TYPE_NETWORK:

		type=SERVICE_CTX_TYPE_BROWSE;
		btype=SERVICE_BROWSE_TYPE_NETGROUP;
		break;

	    case SERVICE_BROWSE_TYPE_NETGROUP:

		type=SERVICE_CTX_TYPE_BROWSE;
		btype=SERVICE_BROWSE_TYPE_NETHOST;
		break;

	    case SERVICE_BROWSE_TYPE_NETHOST:

		type=SERVICE_CTX_TYPE_FILESYSTEM;
		btype=0;
		break;

	}

    } else if (pctx->type==SERVICE_CTX_TYPE_WORKSPACE) {

	type=SERVICE_CTX_TYPE_BROWSE;
	btype=SERVICE_BROWSE_TYPE_NETWORK;

    }

    if (type==0) {

	logoutput_debug("");
	return NULL;

    }

    ctx=get_next_service_context(pctx, NULL, "tree");

    while (ctx) {

	if (ctx->type==type &&
	    ((type==SERVICE_CTX_TYPE_BROWSE && ctx->service.browse.type==btype && ctx->service.browse.unique==unique) ||
	    (type==SERVICE_CTX_TYPE_FILESYSTEM && strcmp(ctx->service.filesystem.name, name)==0))) break;

	ctx=get_next_service_context(pctx, ctx, "tree");

    }

    if (ctx==NULL) {

	if (type==SERVICE_CTX_TYPE_BROWSE) {

	    ctx=create_network_browse_context(w, pctx, btype, unique, service, primary);

	} else if (type==SERVICE_CTX_TYPE_FILESYSTEM) {

	    ctx=create_network_filesystem_context(w, pctx, name, service, primary);

	}

	if (ctx) {

	    logoutput_debug("check_create_install_context: created service context type=%u unique=%u service=%u", type, unique, service);
	    if (p_action) *p_action=CHECK_INSTALL_CTX_ACTION_ADD;

	} else {

	    logoutput_warning("check_create_install_context: unable to create service context type=%u unique=%u service=%u", type, unique, service);

	}

    }

    return ctx;
}

void remove_context(struct workspace_mount_s *w, struct service_context_s *ctx)
{
    struct list_header_s *h=((ctx->type==SERVICE_CTX_TYPE_SHARED) ? &w->shared_contexes : &w->contexes);
    struct context_interface_s *i=&ctx->interface;

    write_lock_list_header(h);
    remove_list_element(&ctx->wlist);
    write_unlock_list_header(h);

    if (i->link.primary) {
	struct context_interface_s *primary=i->link.primary;

	if (primary->flags & _INTERFACE_FLAG_PRIMARY_1TO1) {

	    primary->link.secondary.interface=NULL;

	} else if (primary->flags & _INTERFACE_FLAG_PRIMARY_1TON) {

	    primary->link.secondary.refcount--;

	}

	i->link.primary=NULL;

    }

    if (! (ctx->type==SERVICE_CTX_TYPE_SHARED)) {

	unset_parent_service_context(NULL, ctx);

    }

}
