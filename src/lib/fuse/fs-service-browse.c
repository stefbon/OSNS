/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021
  Stef Bon <stefbon@gmail.com>

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
#include "libosns-workspace.h"
#include "libosns-interface.h"
#include "libosns-context.h"

#include "dentry.h"
#include "opendir.h"
#include "request.h"
#include "fs-common.h"

#ifndef FUSE_ROOT_ID
#define FUSE_ROOT_ID	1
#endif

#include "fs-virtual.h"
#include "fs-service-browse.h"
#include "fs-service-path.h"

/* get the context which is linked to the inode
    this is used only here since in the browse part of the virtual network path every inode is linked via data link
    to a context */

static struct service_context_s *get_browse_context(struct service_context_s *context, struct inode_s *inode)
{
    struct data_link_s *link=inode->ptr;
    struct service_context_s *ctx=NULL;

    if (get_ino_system_stat(&inode->stat)==FUSE_ROOT_ID) {

	return context;

    } else if (link==NULL) {

	logoutput_debug("get_browse_context: no link");
	return NULL;

    } else if (link->type==DATA_LINK_TYPE_DIRECTORY) {
	struct directory_s *directory=(struct directory_s *) ((char *) link - offsetof(struct directory_s, link));

	link=directory->ptr;
	if (link==NULL) {

	    logoutput_debug("get_browse_context: directory points not to a context");
	    return NULL;

	}

    }

    if (link->type==DATA_LINK_TYPE_CONTEXT) {

	ctx=(struct service_context_s *) ((char *) link - offsetof(struct service_context_s, link));

    } else {

	logoutput_debug("get_browse_context: link is not pointing to context (type=%u)", link->type);

    }

    out:
    return ctx;
}

static void _fs_browse_forget(struct service_context_s *context, struct inode_s *inode)
{
    struct data_link_s *link=inode->ptr;

    if (link) {

	if (link->type==DATA_LINK_TYPE_DIRECTORY) {
	    struct directory_s *directory=(struct directory_s *) ((char *) link - offsetof(struct directory_s, link));

	    logoutput("FORGET directory link (refount=%i)", link->refcount);

	    link->refcount--;
	    inode->ptr=NULL;

	}

    }

}

static void fs_filesystem_root_lookup(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s *entry)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    unsigned int pathlen=get_pathmax(w) + 1;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct path_service_fs_s *fs=ctx->service.filesystem.fs;

    logoutput("fs_filesystem_root_lookup: use context %s", ctx->name);

    init_fuse_path(fpath, pathlen + 1);
    start_directory_fpath(fpath);
    fpath->context=ctx;

    (* fs->lookup_existing)(ctx, request, entry, fpath);

}

/* LOOKUP */

static void _fs_browse_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    struct name_s xname={(char *)name, len, 0};
    struct entry_s *entry=NULL;
    struct directory_s *directory=NULL;
    struct workspace_mount_s *w=get_workspace_mount_ctx(context);
    unsigned int error=0;

    logoutput("_fs_browse_lookup: ino %li name %s", (unsigned long) get_ino_system_stat(&pinode->stat), name);

    context=get_browse_context(context, pinode);

    if (context==NULL) {

	reply_VFS_error(request, EIO);
	logoutput_warning("_fs_browse_lookup: ino %li internal error wrong or no context", (unsigned long) pinode->stat.sst_ino);
	return;

    }

    directory=get_directory(w, pinode, 0);
    calculate_nameindex(&xname);
    entry=find_entry(directory, &xname, &error);

    if (entry) {
	struct inode_s *inode=entry->inode;

	logoutput("_fs_browse_lookup: context %s (thread %i) %.*s (entry found)", context->name, (int) gettid(), len, name);

	if (inode->ptr) {
	    struct data_link_s *link=inode->ptr;

	    if (link->type==DATA_LINK_TYPE_DIRECTORY) {
		struct directory_s *tmp=(struct directory_s *)((char *) link - offsetof(struct directory_s, link));

		link=tmp->ptr;

		if (link) {

		    if (link->type==DATA_LINK_TYPE_CONTEXT) {
			struct service_context_s *tmpctx=(struct service_context_s *)((char *) link - offsetof(struct service_context_s, link));

			if (tmpctx->type == SERVICE_CTX_TYPE_FILESYSTEM) {

			    fs_filesystem_root_lookup(tmpctx, request, entry);
			    return;

			}

		    }

		}

	    }

	}

	_fs_common_cached_lookup(context, request, inode);

    } else {

	logoutput("_fs_browse_lookup: context %s (thread %i) %.*s (entry new)", context->name, (int) gettid(), len, name);

	if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	    (* context->service.workspace.fs->lookup_new)(context, request, pinode, &xname);

	} else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	    (* context->service.browse.fs->lookup_new)(context, request, pinode, &xname);

	} else {

	    reply_VFS_error(request, EIO);

	}

    }

}

/* GETATTR */

static void _fs_browse_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{

    context=get_browse_context(context, inode);

    if (context==NULL) {

	reply_VFS_error(request, EIO);
	logoutput_warning("_fs_browse_getattr: ino %li internal error wrong or no context", (unsigned long) get_ino_system_stat(&inode->stat));
	return;

    }

    if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	(* context->service.workspace.fs->getattr)(context, request, inode);

    } else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	(* context->service.browse.fs->getattr)(context, request, inode);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void _fs_browse_access(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, unsigned int mask)
{

    context=get_browse_context(context, inode);

    if (context==NULL) {

	reply_VFS_error(request, EIO);
	logoutput_warning("_fs_browse_access: ino %li internal error wrong or no context", (unsigned long) get_ino_system_stat(&inode->stat));
	return;

    }

    if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	(* context->service.workspace.fs->access)(context, request, inode, mask);

    } else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	(* context->service.browse.fs->access)(context, request, inode, mask);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void _fs_browse_setattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat)
{

    context=get_browse_context(context, inode);

    if (context==NULL) {

	reply_VFS_error(request, EIO);
	logoutput_warning("_fs_browse_setattr: ino %li internal error wrong or no context", (unsigned long) get_ino_system_stat(&inode->stat));
	return;

    }

    if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	(* context->service.workspace.fs->setattr)(context, request, inode, stat);

    } else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	(* context->service.browse.fs->setattr)(context, request, inode, stat);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/* MKDIR */

void _fs_browse_mkdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, mode_t mode, mode_t umask)
{
    reply_VFS_error(request, EPERM);
}

/* MKNOD */

void _fs_browse_mknod(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, mode_t mode, dev_t rdev, mode_t mask)
{
    reply_VFS_error(request, EPERM);
}

/* SYMLINK */

void _fs_browse_symlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, const char *target, unsigned int size)
{
    reply_VFS_error(request, EPERM);
}

/* REMOVE/UNLINK */

static void _fs_browse_rm_common(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, unsigned char op)
{
    reply_VFS_error(request, EPERM);
}

/* RMDIR */

static void _fs_browse_rmdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    reply_VFS_error(request, EPERM);
}

/* UNLINK */

static void _fs_browse_unlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    reply_VFS_error(request, EPERM);
}

/* RENAME */

static void _fs_browse_rename(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, struct inode_s *n_inode, const char *n_name, unsigned int flags)
{
    reply_VFS_error(request, ENOSYS);
}

/* LINK */

static void _fs_browse_link(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, struct inode_s *l_inode, const char *l_name)
{
    reply_VFS_error(request, ENOSYS);
}

/* CREATE */

static void _fs_browse_create(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *name, unsigned int len, unsigned int flags, mode_t mode, mode_t mask)
{
    reply_VFS_error(request, EPERM);
}

/* OPENDIR */

static void _fs_browse_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags)
{
    struct service_context_s *context=opendir->context;

    context=get_browse_context(context, opendir->inode);

    if (context==NULL) {

	reply_VFS_error(request, EIO);
	logoutput_warning("_fs_browse_opendir: ino %li internal error wrong or no context", (unsigned long) get_ino_system_stat(&opendir->inode->stat));
	return;

    }

    opendir->context=context;
    logoutput("OPENDIR browse %s (thread %i) %li", context->name, (int) gettid(), get_ino_system_stat(&opendir->inode->stat));

    if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	(* context->service.workspace.fs->opendir)(opendir, request, flags);

    } else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	(* context->service.browse.fs->opendir)(opendir, request, flags);

    } else {

	logoutput_debug("_fs_browse_opendir: error ... context %u", context->type);
	reply_VFS_error(request, EIO);

    }

}

/* STATFS */

static void _fs_browse_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{

    if (inode) context=get_browse_context(context, inode);

    logoutput("STATFS %s (thread %i) %li", context->name, (int) gettid(), get_ino_system_stat(&inode->stat));
    _fs_common_statfs(context, request, inode);
}

static struct fuse_fs_s browse_fs;

void use_service_browse_fs(struct service_context_s *ctx, struct inode_s *inode)
{
    inode->fs=&browse_fs;
}

/*
    set the context fs calls for the root of the service
    note:
    - open, lock and fattr calls are not used
    - path resolution is simple, its / or /%name% for lookup
*/

static void _set_service_browse_fs(struct fuse_fs_s *fs)
{

    set_virtual_fs(fs);

    fs->forget=_fs_browse_forget;
    fs->getattr=_fs_browse_getattr;
    fs->setattr=_fs_browse_setattr;
    fs->access=_fs_browse_access;

    fs->type.dir.use_fs=use_service_browse_fs;
    fs->type.dir.lookup=_fs_browse_lookup;

    fs->type.dir.create=_fs_browse_create;
    fs->type.dir.mkdir=_fs_browse_mkdir;
    fs->type.dir.mknod=_fs_browse_mknod;
    fs->type.dir.symlink=_fs_browse_symlink;

    fs->type.dir.unlink=_fs_browse_unlink;
    fs->type.dir.rmdir=_fs_browse_rmdir;
    fs->type.dir.rename=_fs_browse_rename;

    fs->type.dir.opendir=_fs_browse_opendir;

    fs->statfs=_fs_browse_statfs;

}

void init_service_browse_fs()
{
    memset(&browse_fs, 0, sizeof(struct fuse_fs_s));
    browse_fs.flags = FS_SERVICE_FLAG_ROOT | FS_SERVICE_FLAG_DIR;
    _set_service_browse_fs(&browse_fs);
}

void set_service_browse_fs(struct fuse_fs_s *fs)
{
    _set_service_browse_fs(fs);
}
