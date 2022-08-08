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
#include "ssh-context.h"

struct check_create_context_s {
    struct workspace_mount_s 			*workspace;
    struct service_context_s 			*pctx;
    unsigned int				service;
    unsigned int				count;
};

static void check_create_context_cb(uint32_t unique, struct network_resource_s *nr, void *ptr)
{
    struct check_create_context_s *ccc=(struct check_create_context_s *) ptr;

    /* ignore services founc on teh localhost */
    if (nr->flags & NETWORK_RESOURCE_FLAG_LOCALHOST) {

	logoutput_debug("check_create_context_cb: ignore unique %u: localhost", unique);
	return;

    }

    logoutput_debug("check_create_context_cb: unique %u count %u port %u", unique, ccc->count, nr->data.service.port.nr);

    if (nr->parent_unique>0) {
	struct network_resource_s nr_host;

	memset(&nr_host, 0, sizeof(struct network_resource_s));
	nr_host.type=NETWORK_RESOURCE_TYPE_NETWORK_HOST;

	if (get_network_resource(nr->parent_unique, &nr_host)==1) {
	    struct network_resource_s nr_group;

	    memset(&nr_group, 0, sizeof(struct network_resource_s));
	    nr_group.type=NETWORK_RESOURCE_TYPE_NETWORK_GROUP;

	    if (get_network_resource(nr_host.parent_unique, &nr_group)==1) {
		struct service_context_s *ctx_group=check_create_install_context(ccc->workspace, ccc->pctx, nr_host.parent_unique, NULL, ccc->service, NULL, NULL);

		if (ctx_group) {

		    /* look at the type of context/resource installing:
			- SFTP over SSH: the host ctx is representing the SSH service, the shared sftp folders */

		    if ((nr->data.service.service==NETWORK_SERVICE_TYPE_SFTP) && (nr->data.service.transport==NETWORK_SERVICE_TYPE_SSH)) {

			struct service_context_s *ctx_host=NULL;
			unsigned char action=0;

			ctx_host=check_create_install_context(ccc->workspace, ctx_group, unique, NULL, ccc->service, NULL, &action);

			/* complete the ctx_shared context ... connect && start */

			if (action==CHECK_INSTALL_CTX_ACTION_ADD) start_thread_connect_ssh_host(ctx_host);

		    } else {
			struct service_context_s *ctx_host=NULL;

			ctx_host=check_create_install_context(ccc->workspace, ctx_group, unique, NULL, ccc->service, NULL, NULL);

		    }

		}

	    }

	}

    }

}

static void populate_network_context(struct workspace_mount_s *workspace, struct service_context_s *nctx, unsigned int service)
{
    struct check_create_context_s ccc0;
    uint32_t unique=nctx->service.browse.unique;

    block_delete_resources();

    ccc0.workspace=workspace;
    ccc0.pctx=nctx;			/* parent is the network context which stands for the name of the network */
    ccc0.service=service;		/* network service code for which this context is created */
    ccc0.count=0;			/* recursion depth */

    browse_every_network_service_resource(service, check_create_context_cb, (void *) &ccc0);

    unblock_delete_resources();
}

static void install_network_root_directory(struct workspace_mount_s *workspace, struct service_context_s *ctx)
{
    char buffer[HOST_HOSTNAME_FQDN_MAX_LENGTH + 1];
    unsigned int len=0;

    memset(buffer, 0, HOST_HOSTNAME_FQDN_MAX_LENGTH + 1);
    len=(* ctx->service.browse.fs->get_name)(ctx, buffer, HOST_HOSTNAME_FQDN_MAX_LENGTH);

    if (len>0) {
	struct entry_s *parent=&workspace->inodes.rootentry;
	struct entry_s *entry=NULL;

	entry=install_virtualnetwork_map(ctx, parent, buffer, "network", NULL);

	if (entry) {
	    struct inode_s *inode=entry->inode;
	    struct directory_s *directory=NULL;

	    logoutput_debug("install_network_root_directory: created %s", buffer);
	    use_service_browse_fs(NULL, inode);
	    directory=get_directory(workspace, inode, 0);

	    if (directory) {

		directory->ptr=&ctx->link;

	    } else {

		logoutput_debug("install_network_root_directory: unable to complete %s (unpredictable errors)", buffer);

	    }

	}

    } else {

	logoutput_debug("install_network_root_directory: received name sero length");

    }

}

void populate_network_workspace_mount(struct service_context_s *pctx)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(pctx);
    struct service_context_s *nctx=NULL;
    uint32_t unique=0;
    struct check_create_context_s ccc0;

    unique=get_root_network_resources();

    if (unique==0) {

	logoutput_debug("populate_network_workspace_mount: no network resources ... not initialized?");
	return;

    }

    logoutput_debug("populate_network_workspace_mount: network root unique %u", unique);

    nctx=create_network_browse_context(workspace, pctx, SERVICE_BROWSE_TYPE_NETWORK, unique, NETWORK_SERVICE_TYPE_SFTP, NULL);

    if (nctx) {

	logoutput("populate_network_workspace_mount: created network context for service %u", NETWORK_SERVICE_TYPE_SFTP);

	/* look in cache for resource for this network
	    - look for network sockets for the sftp service */

	populate_network_context(workspace, nctx, NETWORK_SERVICE_TYPE_SFTP);
	install_network_root_directory(workspace, nctx);

    } else {

	logoutput("populate_network_workspace_mount: unable to add network context");

    }

}
