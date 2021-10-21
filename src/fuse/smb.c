/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"

#include "discover/discover.h"

#include "interface/smb.h"
#include "interface/smb-signal.h"
#include "interface/smb-wait-response.h"

#include "network.h"
#include "workspace-fs.h"
#include "smb-fs.h"
#include "smb.h"

static int signal_smb2ctx(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    struct service_context_s *context=get_service_context(interface);

    logoutput_info("signal_smb2ctx: what %s", what);

    if (strlen(what)>=17 && strncmp(what, "event:disconnect:", 17)==0) {
	struct smb_signal_s *signal=get_smb_signal_ctx(interface);

	/* set the filesystem to disconnected mode */

	set_context_filesystem_smb(context, 1);

	/* signal the workspace */

	smb_signal_lock(signal);
	signal->flags |= SMB_SIGNAL_FLAG_DISCONNECTED;
	smb_signal_broadcast(signal);
	smb_signal_unlock(signal);

    /* } else if ... 

	more events, commands ?*/

    } else {

	return -1;

    }

    return (unsigned int) option->type;
}

struct service_context_s *create_smb_server_service_context(struct service_context_s *networkctx, struct interface_list_s *ilist, uint32_t unique)
{
    struct service_context_s *context=NULL;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(networkctx);

    logoutput("create_smb_server_service_context: (unique %i)", unique);

    /* no parent yet */

    context=create_service_context(workspace, NULL, ilist, SERVICE_CTX_TYPE_BROWSE, NULL);

    if (context) {

	context->service.browse.type=SERVICE_BROWSE_TYPE_NETHOST;
	context->service.browse.unique=unique;
	context->flags |= (networkctx->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND);

	set_context_filesystem_workspace(context);
	set_name_service_context(context);

    }

    return context;

}

static struct service_context_s *create_smb_share_service_context(struct service_context_s *hostctx, struct interface_list_s *ilist)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(hostctx);
    struct service_context_s *ctx=NULL;

    logoutput("create_smb_share_context: create smb context for filesystem");

    ctx=create_service_context(workspace, hostctx, ilist, SERVICE_CTX_TYPE_FILESYSTEM, NULL);

    if (ctx) {

	ctx->service.filesystem.inode=NULL; /* not known here, set it later */
	ctx->flags |= (hostctx->flags & SERVICE_CTX_FLAGS_REMOTEBACKEND);
	ctx->interface.signal_context=signal_smb2ctx;

	set_context_filesystem_smb(ctx, 0);
	set_name_service_context(ctx);

    }

    return ctx;

}

struct _add_smb_share_s {
    struct service_context_s 			*hostctx;
    struct interface_list_s 			*ilist;
};

static void _add_smb_share_cb(struct context_interface_s *hi, char *name, unsigned int type, unsigned int flags, void *ptr)
{
    struct _add_smb_share_s *_add_smb_share=(struct _add_smb_share_s *) ptr;
    struct service_context_s *hostctx=_add_smb_share->hostctx;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(hostctx);
    struct interface_list_s *ilist=_add_smb_share->ilist;
    struct service_context_s *ctx=NULL;
    struct context_interface_s *interface=NULL;
    struct service_address_s address;
    int fd=-1;
    unsigned int error=0;
    char *share=NULL;

    if (! (type==SMB_SHARE_TYPE_DISKTREE)) {

	logoutput("_add_smb_share_cb: ignoring shared service %s (type=%i)", name, type);
	return;

    }

    share=strdup(name);
    if (share==NULL) {

	logoutput_warning("_add_smb_share_cb: error allocating context");
	return;

    }

    /* create a smb share context */

    logoutput("_add_smb_share_cb: received shared disktree %s", name);

    ctx=create_smb_share_service_context(hostctx, ilist);
    if (ctx==NULL) {

	logoutput_warning("_add_smb_share_cb: error allocating context");
	goto error;

    }

    interface=&ctx->interface;

    memset(&address, 0, sizeof(struct service_address_s));
    address.type=_SERVICE_TYPE_SMB_SHARE;
    address.target.smb.share=name;
    address.target.smb.username=NULL; /* connect will take the default: local username */

    fd=(* interface->connect)(workspace->user->pwd.pw_uid, interface, NULL, &address);

    if (fd==-1) {

	logoutput("_add_smb_share_cb: error connecting");
	goto error;

    } else {

	logoutput("_add_smb_share_cb: connected to smb share %s (fd=%i)", name, fd);

    }

    if ((* interface->start)(interface, fd, NULL)==0) {

	logoutput("_add_smb_share_cb: smb share started");
	interface->backend.smb_share.name=share;

    } else {

	logoutput("_add_smb_share_cb: unable to start smb share");
	goto error;

    }

    return;

    error:

    if (ctx) {

	unset_parent_service_context(hostctx, ctx);
	(* interface->signal_interface)(interface, "command:close:", NULL);
	(* interface->signal_interface)(interface, "command:clear:", NULL);
	free_service_context(ctx);
	ctx=NULL;

    }

    if (share) {

	free(share);
	share=NULL;

    }

}

static void _enum_smb_shares(struct service_context_s *hostctx, struct host_address_s *host, struct discover_service_s *discover)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(hostctx);
    struct service_context_s *ctx=NULL;
    struct context_interface_s *interface=NULL;
    struct interface_list_s *ilist=NULL;
    struct service_address_s service;
    struct passwd *pwd=get_workspace_user_pwd(&hostctx->interface);
    int fd=-1;

    /* get the right interface calls to a smb share */

    ilist=get_interface_list(discover->ailist, discover->count, _INTERFACE_TYPE_SMB_SHARE);
    if (ilist==NULL) return;

    memset(&service, 0, sizeof(struct service_address_s));

    /* create a dummy temporary context for the IPC$ */

    ctx=create_service_context(workspace, hostctx, ilist, SERVICE_CTX_TYPE_DUMMY, NULL);
    if (ctx==NULL) return;
    interface=&ctx->interface;

    service.type=_SERVICE_TYPE_SMB_SHARE;
    service.target.smb.share="IPC$";
    service.target.smb.username=NULL;

    fd=(* interface->connect)(pwd->pw_uid, interface, host, &service);
    if (fd<0) goto out;

    logoutput("enum_smb_shares: connected to IPC$ with fd %i", fd);

    if ((* interface->start)(interface, fd, NULL)==0) {
	struct _add_smb_share_s add_smb_share;

	memset(&add_smb_share, 0, sizeof(struct _add_smb_share_s));
	add_smb_share.ilist=ilist;
	add_smb_share.hostctx=hostctx;

	if (smb_share_enum_async_ctx(interface, _add_smb_share_cb, (void *) &add_smb_share)==0) {

	    logoutput("enum_smb_shares: enumeration successfull");

	} else {

	    logoutput("enum_smb_shares: enumeration failed");

	}

    } else {

	logoutput("enum_smb_shares: failed to start smb share IPC$ fd %i", fd);

    }

    logoutput("enum_smb_shares: A");

    (* interface->signal_interface)(interface, "command:close:", NULL);
    logoutput("enum_smb_shares: B");
    (* interface->signal_interface)(interface, "command:free:", NULL);
    logoutput("enum_smb_shares: C");

    out:

    remove_list_element(&ctx->wlist);
    logoutput("enum_smb_shares: D");
    unset_parent_service_context(hostctx, ctx);
    logoutput("enum_smb_shares: E");
    free_service_context(ctx);

}

void get_remote_shares_smb_server(struct service_context_s *hostctx, void *ptr)
{
    struct discover_service_s *discover=(struct discover_service_s *) ptr;
    struct discover_resource_s *nethost_resource=NULL;

    nethost_resource=lookup_resource_id(hostctx->service.browse.unique);

    if (nethost_resource) {

	_enum_smb_shares(hostctx, &nethost_resource->service.host.address, discover);

    }

}
