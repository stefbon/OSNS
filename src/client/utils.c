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

#include "fuse/browse-fs.h"
#include "fuse/disconnected-fs.h"
#include "fuse/sftp-fs.h"

#include "osns_client.h"
#include "network.h"
#include "utils.h"

struct find_service_context_hlpr_s {
    uint64_t                                    unique;
    struct service_context_s                    *primary;
    int                                         (* compare)(struct service_context_s *ctx, void *ptr);
    void                                        *ptr;
};

static int compare_service_context(struct service_context_s *ctx, void *ptr)
{
    struct find_service_context_hlpr_s *hlpr=(struct find_service_context_hlpr_s *) ptr;

    if (hlpr->unique>0) {

        if (((ctx->type==SERVICE_CTX_TYPE_SHARED) && (ctx->service.shared.unique!=hlpr->unique)) || ((ctx->type==SERVICE_CTX_TYPE_BROWSE) && (ctx->service.browse.unique!=hlpr->unique))) return 0;

    }

    if ((hlpr->primary)) {
        struct context_interface_s *i=&ctx->interface;

        if ((* i->get_primary)(i) != &hlpr->primary->interface) return 0;

    }

    if (hlpr->compare) return (* hlpr->compare)(ctx, hlpr->ptr);
    return 1;

}

struct service_context_s *create_network_shared_context(struct workspace_mount_s *w, uint64_t unique, unsigned int service, unsigned int transport, unsigned int itype, struct service_context_s *primary)
{
    struct service_context_s *ctx=NULL;
    unsigned int count=build_interface_ops_list(NULL, NULL, 0);
    struct interface_list_s ailist[count];
    struct interface_list_s *ilist=NULL;

    logoutput("create_network_shared_context: unique %lu service %u transport %u", unique, service, transport);

    count=build_interface_ops_list(NULL, ailist, 0);
    ilist=get_interface_list(ailist, count, itype);

    ctx=create_service_context(w, NULL, ilist, SERVICE_CTX_TYPE_SHARED, primary);

    if (ctx) {

	ctx->service.shared.unique=unique;
	ctx->service.shared.service=service;
	ctx->service.shared.transport=transport;
	set_name_service_context(ctx);

    }

    return ctx;

}

struct service_context_s *create_network_filesystem_context(struct workspace_mount_s *w, struct service_context_s *parent, char *name, unsigned int service, struct service_context_s *primary)
{
    struct service_context_s *context=NULL;
    unsigned int count=build_interface_ops_list(NULL, NULL, 0);
    struct interface_list_s ailist[count];
    struct interface_list_s *ilist=NULL;

    logoutput_debug("create_network_filesystem_context: name %s service %u", ((name) ? name : "-unknown-"), service);

    count=build_interface_ops_list(NULL, ailist, 0);

    if (primary) {

        ilist=get_interface_list(ailist, count, primary->interface.type);

    } else {

        if (service==NETWORK_SERVICE_TYPE_SFTP) {

            ilist=get_interface_list(ailist, count, _INTERFACE_TYPE_SFTP_CLIENT);

        }

    }

    context=create_service_context(w, parent, ilist, SERVICE_CTX_TYPE_FILESYSTEM, primary);

    if (context) {

	context->service.filesystem.service=service;
	context->service.filesystem.signal=w->signal;
	if (name) strcpy(context->service.filesystem.name, name);
	set_name_service_context(context);

	if ((service==NETWORK_SERVICE_TYPE_SFTP) && primary) {

	    set_context_filesystem_sftp(context);

        } else {

            set_context_filesystem_disconnected(context);

	}

    }

    return context;

}

struct service_context_s *create_network_browse_context(struct workspace_mount_s *w, struct service_context_s *parent, unsigned int btype, uint64_t unique, unsigned int service, struct service_context_s *primary)
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

struct service_context_s *check_create_install_context(struct workspace_mount_s *w, struct service_context_s *pctx, uint64_t unique, char *name, unsigned int service, struct service_context_s *primary, unsigned char *p_action)
{
    struct service_context_s *ctx=NULL;
    unsigned int type=0;
    unsigned int btype=0;

    logoutput_debug("check_create_install_context: unique=%lu service=%u", unique, service);

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

	logoutput_debug("check_create_install_context: parent type %u not reckognized", pctx->type);
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

    } else {

        if (p_action) *p_action=CHECK_INSTALL_CTX_ACTION_FOUND;

    }

    return ctx;
}

void remove_context(struct workspace_mount_s *w, struct service_context_s *ctx)
{
    struct list_header_s *h=NULL;
    struct context_interface_s *i=&ctx->interface;

    if (w) {

        h=((ctx->type==SERVICE_CTX_TYPE_SHARED) ? &w->shared_contexes : &w->contexes);

    } else {

        h=ctx->wlist.h;

    }

    if (h) {

        write_lock_list_header(h);
        remove_list_element(&ctx->wlist);
        write_unlock_list_header(h);

    }

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

int test_service_context_usable(struct service_context_s *ctx)
{
    struct context_interface_s *i=&ctx->interface;
    struct interface_status_s istatus;
    int result=0;

    init_interface_status(&istatus);

    if ((*i->get_interface_status)(i, &istatus)>0) {

        if (istatus.flags & (INTERFACE_STATUS_FLAG_ERROR | INTERFACE_STATUS_FLAG_DISCONNECTED)) {

            logoutput_debug("test_service_context_usable: unable to use existing context");
            result=-1;

        }

    }

    return result;

}
