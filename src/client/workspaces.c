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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "osns_client.h"

#include "client/network.h"
#include "fuse/browse-fs.h"
#include "interface/fuse.h"

struct workspace_mount_s *get_next_workspace_mount(struct client_session_s *session, struct workspace_mount_s *w)
{
    struct list_element_s *list=(w ? get_next_element(&w->list) : get_list_head(&session->workspaces, 0));
    return ((list) ? (struct workspace_mount_s *) ((char *) list - offsetof(struct workspace_mount_s, list)) : NULL);
}

static void remove_inodes_workspace_thread(void *ptr)
{
    struct workspace_mount_s *workspace=(struct workspace_mount_s *) ptr;
    struct workspace_inodes_s *inodes=&workspace->inodes;

    signal_set_flag(workspace->signal, &workspace->status, WORKSPACE_STATUS_LOCK_INODES);

    /* remove all inodes on this workspace by walink through the hashtable */

    for (unsigned int i=0; i<WORKSPACE_INODE_HASHTABLE_SIZE; i++) {
	struct list_element_s *list=get_list_head(&inodes->hashtable[i], SIMPLE_LIST_FLAG_REMOVE);

	while (list) {
	    struct inode_s *inode=(struct inode_s *)((char *) list - offsetof(struct inode_s, list));

	    if (inode->ptr) {
		struct data_link_s *link=inode->ptr;

		if (link->type == DATA_LINK_TYPE_SPECIAL_ENTRY) {
		    struct fuse_fs_s *fs=inode->fs;

		    (* fs->forget)(NULL, inode);

		}

	    }

	    inode->ptr=NULL;

	    if (inode->alias) {
		struct entry_s *entry=inode->alias;

		destroy_entry(entry);
		inode->alias=NULL;

	    }

	    free(inode);
	    list=get_list_head(&inodes->hashtable[i], SIMPLE_LIST_FLAG_REMOVE);

	}

    }

    signal_unset_flag(workspace->signal, &workspace->status, WORKSPACE_STATUS_LOCK_INODES);

}

static void remove_workspace_symlinks(struct workspace_mount_s *workspace)
{
    struct workspace_inodes_s *inodes=&workspace->inodes;
    struct list_element_s *list=get_list_head(&inodes->symlinks, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct fuse_symlink_s *syml=(struct fuse_symlink_s *)((char *) list - offsetof(struct fuse_symlink_s, list));

	free_fuse_symlink(syml);
	list=get_list_head(&inodes->symlinks, SIMPLE_LIST_FLAG_REMOVE);

    }

}

static void remove_workspace_directories(struct workspace_mount_s *workspace)
{
    struct workspace_inodes_s *inodes=&workspace->inodes;
    struct list_element_s *list=get_list_head(&inodes->directories, SIMPLE_LIST_FLAG_REMOVE);

    while (list) {
	struct directory_s *d=(struct directory_s *)((char *) list - offsetof(struct directory_s, list));

	free_directory(d);
	list=get_list_head(&inodes->directories, SIMPLE_LIST_FLAG_REMOVE);

    }

}

static void free_workspace_contexts(struct service_context_s *pctx, unsigned int level)
{
    struct service_context_s *ctx=get_next_service_context(pctx, NULL, "tree");

    /* free the children ctx's */

    while (ctx) {

	if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {
	    struct context_interface_s *interface=&ctx->interface;

	    /* a filesystem context is the endpoint, has no children context's
		so not neccesary to go another level deeper */

	    logoutput_debug("free_workspace_contexts: close and free %s (level=%i)", ctx->name, level);

	    (* interface->iocmd.in)(interface, "command:close:", NULL, interface, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);
	    (* interface->iocmd.in)(interface, "command:free:", NULL, interface, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);

	    remove_list_element(&ctx->service.filesystem.clist);

	    if (ctx->service.filesystem.pathcaches.count>0) {
		struct list_element_s *list=get_list_head(&ctx->service.filesystem.pathcaches, SIMPLE_LIST_FLAG_REMOVE);

		while (list) {

		    free_cached_path(list);
		    list=get_list_head(&ctx->service.filesystem.pathcaches, SIMPLE_LIST_FLAG_REMOVE);

		}

	    }

	} else if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {
	    struct context_interface_s *interface=&ctx->interface;

	    /* context is part of browse context (==hosts, groups etc,)
		can have children ctx's */

	    logoutput_debug("free_workspace_contexts: found browse ctx %s (level=%i)", ctx->name, level);

	    free_workspace_contexts(ctx, level+1);
	    (* interface->iocmd.in)(interface, "command:free:", NULL, interface, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);
	    remove_list_element(&ctx->service.browse.clist);

	} else {

	    logoutput_warning("free_workspace_contexts: context %s not reckognized", ctx->name);

	}

	remove_list_element(&ctx->wlist);
	free(ctx);

	ctx=get_next_service_context(pctx, NULL, "tree");

    }

}

static void _remove_workspace(struct workspace_mount_s *workspace)
{
    struct service_context_s *context=get_root_context_workspace(workspace);

    if (workspace->inodes.nrinodes>1) {

	logoutput_debug("_remove_workspace: start thread to remove inodes, entries and directories");
	work_workerthread(NULL, 0, remove_inodes_workspace_thread, (void *) workspace, NULL);

    }

    if (context) {
	struct context_interface_s *interface=&context->interface;

	logoutput_debug("_remove_workspace: close");

	(* interface->iocmd.in)(interface, "command:close:", NULL, interface, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);
	(* interface->iocmd.in)(interface, "command:free:", NULL, interface, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);

	logoutput_debug("_remove_workspace: free service contexts");

	free_workspace_contexts(context, 0);
	remove_workspace_symlinks(workspace);
	remove_workspace_directories(workspace);
	remove_list_element(&context->wlist);
	free(context);

    }

    remove_list_element(&workspace->list);

    /* wait for the "remove_inodes" thread to finish */
    signal_wait_flag_unset(workspace->signal, &workspace->status, WORKSPACE_STATUS_LOCK_INODES, NULL);

    logoutput_debug("_remove_workspace: free workspace");
    free(workspace);

}

static int iocmd_fuse2ctx(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type)
{

    /* process a command/event from fuse in the context
	what events ??? */

    logoutput_debug("iocmd_fuse2ctx: %s", what);
    return 0;
}

struct service_context_s *create_mount_context(struct client_session_s *session, unsigned int type, unsigned int maxread)
{
    struct service_context_s *context=NULL;
    struct workspace_mount_s *workspace=NULL;
    unsigned int error=0;
    unsigned int count=build_interface_ops_list(NULL, NULL, 0);
    struct interface_list_s ailist[count];
    struct interface_list_s *ilist=NULL;
    struct context_interface_s *interface=NULL;
    struct fuse_mount_s mount;
    union interface_target_u target;
    union interface_parameters_u param;
    int result=-1;

    logoutput("create_mount_context: type=%u maxread=%u", type, maxread);

    /* build the list with available interface ops
	important here are of course the ops to setup a ssh server context and a sftp server context (=ssh channel) */

    count=build_interface_ops_list(NULL, ailist, 0);
    ilist=get_interface_list(ailist, count, _INTERFACE_TYPE_FUSE);

    if (ilist==NULL) {

	logoutput_warning("create_mount_context: no handlers found to create workspace");
	goto error;

    }

    workspace=create_workspace_mount(type);

    if (workspace==NULL) {

	logoutput("create_mount_context: unable to allocate memory for workspace");
	goto error;

    }

    workspace->signal=session->signal;
    context=create_service_context(workspace, NULL, ilist, SERVICE_CTX_TYPE_WORKSPACE, NULL);
    if (context==NULL) {

	logoutput("create_mount_context: failed to create mount service context");
	goto error;

    }

    logoutput("create_mount_context: created mount context for type %u", type);
    add_list_element_first(&session->workspaces, &workspace->list);
    context->service.workspace.signal=workspace->signal;

    interface=&context->interface;
    interface->iocmd.out=iocmd_fuse2ctx;
    set_fuse_interface_eventloop(interface, get_default_mainloop());

    mount.type=type;
    mount.maxread=maxread;
    mount.ptr=NULL;
    target.fuse=&mount;

    logoutput("create_mount_context: connect FUSE");

    result=(* interface->connect)(interface, &target, &param);

    if (result==-1) {

	logoutput("create_mount_context: failed to connect FUSE");
	goto error;

    }

    logoutput("create_mount_context: starting FUSE");

    if ((* context->interface.start)(interface)==0) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	logoutput("create_mount_context: FUSE started");
	use_virtual_fs(context, inode);

    } else {

	logoutput("create_mount_context: failed to start FUSE");
	goto error;

    }

    return context;

    error:

    if (workspace) _remove_workspace(workspace);
    return NULL;

}

struct client_session_s *get_client_session_workspace(struct workspace_mount_s *workspace)
{
    struct client_session_s *session=NULL;
    struct list_header_s *h=workspace->list.h;

    if (h) session=(struct client_session_s *)((char *)h - offsetof(struct client_session_s, workspaces));
    return session;
}
