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
    struct list_element_s *list=(w ? get_next_element(&w->list) : get_list_head(&session->workspaces));
    return ((list) ? (struct workspace_mount_s *) ((char *) list - offsetof(struct workspace_mount_s, list)) : NULL);
}

static void remove_workspace_inode_cb(struct inode_s *inode, void *ptr)
{
    struct system_dev_s *dev=(struct system_dev_s *) ptr;
    struct system_dev_s tmp=SYSTEM_DEV_INIT;

    get_dev_system_stat(&inode->stat, &tmp);

    if (get_unique_system_dev(&tmp)==get_unique_system_dev(dev)) {

        /* inode is part of this workspace */

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

    }

}

static void remove_inodes_workspace_thread(void *ptr)
{
    struct service_context_s *ctx=(struct service_context_s *) ptr;
    struct inode_s *rootinode=&ctx->service.workspace.rootinode;
    struct system_dev_s dev=SYSTEM_DEV_INIT;

    get_dev_system_stat(&rootinode->stat, &dev);
    process_workspace_inodes(remove_workspace_inode_cb, (void *)&dev);
    signal_unset_flag(ctx->service.workspace.signal, &ctx->service.workspace.status, SERVICE_WORKSPACE_FLAG_INODES);

}

static void remove_workspace_symlinks(struct service_context_s *ctx)
{
    struct list_header_s *h=&ctx->service.workspace.symlinks;
    struct list_element_s *list=remove_list_head(h);

    while (list) {
	struct fuse_symlink_s *syml=(struct fuse_symlink_s *)((char *) list - offsetof(struct fuse_symlink_s, list));

	free_fuse_symlink(syml);
	list=remove_list_head(h);

    }

}

static void remove_service_context_pathcache(struct service_context_s *ctx)
{
    struct list_header_s *h=&ctx->service.filesystem.pathcaches;
    struct list_element_s *list=remove_list_head(h);

    while (list) {
        struct cached_path_s *cp=(struct cached_path_s *)((char *) list - offsetof(struct cached_path_s, list));

        free(cp);
        list=remove_list_head(h);

    }

}

static void remove_workspace_directories(struct service_context_s *ctx)
{
    struct list_header_s *h=&ctx->service.workspace.directories;
    struct list_element_s *list=remove_list_head(h);

    while (list) {
	struct directory_s *d=(struct directory_s *)((char *) list - offsetof(struct directory_s, list));

	free_directory(d);
	list=remove_list_head(h);

    }

}

static void free_workspace_context(struct service_context_s *ctx)
{
    struct context_interface_s *i=&ctx->interface;

    logoutput_debug("free_workspace_context: close and free %s", ctx->name);

    (* i->iocmd.in)(i, "command:close:", NULL, i, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);
    (* i->iocmd.in)(i, "command:free:", NULL, i, INTERFACE_CTX_SIGNAL_TYPE_WORKSPACE);

    if (ctx->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	remove_list_element(&ctx->service.filesystem.clist);
	remove_service_context_pathcache(ctx);

    } else if (ctx->type==SERVICE_CTX_TYPE_BROWSE) {

	remove_list_element(&ctx->service.browse.clist);

    }

    free(ctx);

}

static void free_workspace_contexts(struct list_header_s *h)
{
    struct list_element_s *list=remove_list_tail(h);

    while (list) {
        struct service_context_s *ctx=(struct service_context_s *)((char *)list - offsetof(struct service_context_s, wlist));

        free_workspace_context(ctx);
        list=remove_list_tail(h);

    }

}

static void free_workspace_contexes_thread(void *ptr)
{
    struct list_header_s *h=(struct list_header_s *) ptr;
    free_workspace_contexts(h);
}

/* TODO: find out the method to free all resources in the most fastest way, more threads? */

static void _remove_workspace(struct workspace_mount_s *w)
{
    struct service_context_s *ctx=get_root_context_workspace(w);

    if (ctx) {

        remove_list_element(&ctx->wlist);

        if (ctx->service.workspace.nrinodes>1) {

            work_workerthread(NULL, 0, remove_inodes_workspace_thread, (void *) ctx);

        } else {

            signal_unset_flag(ctx->service.workspace.signal, &ctx->service.workspace.status, SERVICE_WORKSPACE_FLAG_INODES);

        }

        remove_workspace_symlinks(ctx);
        remove_workspace_directories(ctx);
        remove_workspace_forget(ctx);

    }

    if (w->contexes.count>0) free_workspace_contexts(&w->contexes);
    if (w->shared_contexes.count>0) free_workspace_contexts(&w->shared_contexes);

    if (ctx) {

        /* wait for the "remove_inodes" thread to finish */

        logoutput_debug("_remove_workspace: wait for remove inodes thread to finish");
        signal_wait_flag_unset(ctx->service.workspace.signal, &ctx->service.workspace.status, SERVICE_WORKSPACE_FLAG_INODES, NULL);
        free_workspace_context(ctx);

    }

    logoutput_debug("_remove_workspace: free workspace");
    free(w);

}

void remove_workspaces(struct client_session_s *session)
{
    struct list_header_s *h=&session->workspaces;
    struct list_element_s *list=remove_list_head(h);

    while (list) {
        struct workspace_mount_s *w=(struct workspace_mount_s *)((char *)list - offsetof(struct workspace_mount_s, list));

        _remove_workspace(w);
        list=remove_list_head(h);

    }

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
    struct list_header_s *h=NULL;
    struct service_context_s *ctx=NULL;
    struct workspace_mount_s *w=NULL;
    unsigned int error=0;
    unsigned int count=build_interface_ops_list(NULL, NULL, 0);
    struct interface_list_s ailist[count];
    struct interface_list_s *ilist=NULL;
    struct context_interface_s *i=NULL;
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

    w=create_workspace_mount(type);

    if (w==NULL) {

	logoutput("create_mount_context: unable to allocate memory for workspace");
	goto error;

    }

    w->signal=session->signal;
    ctx=create_service_context(w, NULL, ilist, SERVICE_CTX_TYPE_WORKSPACE, NULL);
    if (ctx==NULL) {

	logoutput("create_mount_context: failed to create mount service context");
	goto error;

    }

    logoutput("create_mount_context: created mount context for type %u", type);

    h=&session->workspaces;
    write_lock_list_header(h);
    add_list_element_first(h, &w->list);
    write_unlock_list_header(h);

    ctx->service.workspace.signal=w->signal;
    i=&ctx->interface;
    i->iocmd.out=iocmd_fuse2ctx;
    set_fuse_interface_eventloop(i, get_default_mainloop());

    mount.type=type;
    mount.maxread=maxread;
    mount.ptr=NULL;
    target.fuse=&mount;

    logoutput("create_mount_context: connect FUSE");

    result=(* i->connect)(i, &target, &param);

    if (result==-1) {

	logoutput("create_mount_context: failed to connect FUSE");
	goto error;

    }

    logoutput("create_mount_context: starting FUSE");

    if ((* i->start)(i)==0) {
	struct inode_s *inode=&ctx->service.workspace.rootinode;
	struct system_dev_s *dev=get_fuse_interface_system_dev(i);

	use_virtual_fs(ctx, inode);
	set_dev_system_stat(&inode->stat, dev); /* set the device major and minor coming from the fuse interface ... */
	logoutput("create_mount_context: FUSE started");

    } else {

	logoutput("create_mount_context: failed to start FUSE");
	goto error;

    }

    return ctx;

    error:

    if (w) {

        h=&session->workspaces;
        write_lock_list_header(h);
        remove_list_element(&w->list);
        write_unlock_list_header(h);
        _remove_workspace(w);

    }

    return NULL;

}

struct client_session_s *get_client_session_workspace(struct workspace_mount_s *workspace)
{
    struct client_session_s *session=NULL;
    struct list_header_s *h=workspace->list.h;

    if (h) session=(struct client_session_s *)((char *)h - offsetof(struct client_session_s, workspaces));
    return session;
}

int walk_interfaces_workspace(struct workspace_mount_s *w, int (* cb)(struct context_interface_s *i, void *ptr), void *ptr)
{
    struct list_header_s *h=&w->shared_contexes;
    struct service_context_s *ctx=NULL;
    int result=-1;

    /* walk every (shared) interface in this workspace */

    read_lock_list_header(h);
    ctx=get_next_shared_service_context(w, NULL);

    while (ctx) {
	struct context_interface_s *i=&ctx->interface;

        logoutput_debug("walk_interfaces_workspace: ctx %s", ctx->name);

	result=cb(i, ptr);
	if (result==0) break;
	ctx=get_next_shared_service_context(w, ctx);

    }

    read_unlock_list_header(h);
    return result;

}
