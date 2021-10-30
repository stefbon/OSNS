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
#include <fcntl.h>
#include <linux/fs.h>

#define LOGGING

#include "main.h"
#include "log.h"
#include "misc.h"

#include "workspace.h"
#include "workspace-interface.h"
#include "fuse.h"

static struct fuse_fs_s browse_fs;

static struct service_context_s *get_browse_context(struct inode_s *inode)
{
    struct data_link_s *link=NULL;
    fs_get_data_link(inode, &link);
    return ((struct service_context_s *) (link->link.ptr));
}

static void unset_context_inode(struct inode_s *inode)
{
    struct data_link_s *link=NULL;
    fs_get_data_link(inode, &link);
    link->type=0;
    link->link.ptr=NULL;
}

static void _fs_browse_forget(struct inode_s *inode)
{
    struct service_context_s *context=get_browse_context(inode);

    logoutput("FORGET context %s", context->name);

    (* context->interface.signal_interface)(&context->interface, "command:disconnect", NULL);
    (* context->interface.signal_interface)(&context->interface, "command:close", NULL);
    (* context->interface.signal_interface)(&context->interface, "command:free", NULL);
    remove_list_element(&context->wlist);
    unset_context_inode(inode);

}

/* LOOKUP */

static void _fs_browse_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    struct name_s xname={(char *)name, len, 0};
    struct entry_s *entry=NULL;
    struct directory_s *directory=NULL;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_LOOKUP);

    logoutput("_fs_browse_lookup: ino %li name %s", (unsigned long) pinode->stat.sst_ino, name);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    context=get_browse_context(pinode);

    if (context==NULL || (context->type != SERVICE_CTX_TYPE_BROWSE && context->type != SERVICE_CTX_TYPE_WORKSPACE)) {

	reply_VFS_error(request, EIO);
	logoutput_warning("_fs_browse_lookup: ino %li internal error wrong or no context", (unsigned long) pinode->stat.sst_ino);
	return;

    }

    fs=get_service_context_fs(context);
    directory=get_directory(pinode);
    calculate_nameindex(&xname);
    entry=find_entry(directory, &xname, &error);

    if (entry) {

	error=(* fs->access)(context, request, SERVICE_OP_TYPE_LOOKUP_EXISTING);

	if (error) {

	    reply_VFS_error(request, error);
	    return;

	}

	logoutput("_fs_browse_lookup: context %s (thread %i) %.*s (entry found)", context->name, (int) gettid(), len, name);

	if (check_entry_special(entry->inode)==0) {

	    _fs_common_cached_lookup(context, request, entry->inode);

	} else {

	    (* fs->lookup_existing)(context, request, entry, NULL);

	}

    } else {

	error=(* fs->access)(context, request, SERVICE_OP_TYPE_LOOKUP_NEW);

	if (error) {

	    reply_VFS_error(request, error);
	    return;

	}

	logoutput("_fs_browse_lookup: context %s (thread %i) %.*s (entry new)", context->name, (int) gettid(), len, name);

	(* fs->lookup_new)(context, request, pinode, &xname, NULL);

    }

}

/* GETATTR */

static void _fs_browse_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_GETATTR);

    logoutput("_fs_browse_getattr: context %s (thread %i) %.*s", context->name, (int) gettid(), inode->alias->name.len, inode->alias->name.name);

    if (error==0) {

	context=get_browse_context(inode);
	fs=get_service_context_fs(context);
	error=(* fs->access)(context, request, SERVICE_OP_TYPE_GETATTR);

	if (error==0) {

	    (* fs->getattr)(context, request, inode, NULL);

	} else {

	    reply_VFS_error(request, error);

	}

    } else {

	reply_VFS_error(request, error);

    }

}

static void _fs_browse_setattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat)
{
    struct service_fs_s *fs=get_service_context_fs(context);
    struct entry_s *entry=inode->alias;
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_SETATTR);

    logoutput("_fs_browse_setattr: context root %s (thread %i): %.*s", context->name, (int) gettid(), entry->name.len, entry->name.name);

    if (error==0) {

	context=get_browse_context(inode);
	fs=get_service_context_fs(context);
	error=(* fs->access)(context, request, SERVICE_OP_TYPE_SETATTR);

	if (error==0) {

	    (* fs->setattr)(context, request, inode, NULL, stat);

	} else {

	    reply_VFS_error(request, error);

	}

    } else {

	reply_VFS_error(request, error);

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
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_OPENDIR);

    logoutput("OPENDIR browse %s (thread %i) %li", context->name, (int) gettid(), opendir->inode->stat.sst_ino);

    if (error==0) {

	context=get_browse_context(opendir->inode);

	if (context==NULL || (context->type != SERVICE_CTX_TYPE_BROWSE && context->type != SERVICE_CTX_TYPE_WORKSPACE)) {

	    reply_VFS_error(request, EIO);
	    logoutput_warning("_fs_browse_opendir: ino %li internal error wrong or no context", (unsigned long) opendir->inode->stat.sst_ino);
	    return;

	}

	fs=get_service_context_fs(context);
	error=(* fs->access)(context, request, SERVICE_OP_TYPE_OPENDIR);

	if (error==0) {

	    opendir->context=context;
	    logoutput("OPENDIR browse %s (thread %i) %li", context->name, (int) gettid(), opendir->inode->stat.sst_ino);
	    (* fs->opendir)(opendir, request, NULL, flags);

	} else {

	    reply_VFS_error(request, error);

	}

    } else {

	reply_VFS_error(request, error);

    }

}

/* STATFS */

static void _fs_browse_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_STATFS);

    if (error==0) {

	context=get_browse_context(inode);
	fs=get_service_context_fs(context);
	error=(* fs->access)(context, request, SERVICE_OP_TYPE_STATFS);

	if (error==0) {

	    logoutput("STATFS %s (thread %i) %li", context->name, (int) gettid(), inode->stat.sst_ino);
	    (* fs->statfs)(context, request, NULL);

	} else {

	    reply_VFS_error(request, error);

	}

    } else {

	reply_VFS_error(request, error);

    }

}

/*
    set the context fs calls for the root of the service
    note:
    - open, lock and fattr calls are not used
    - path resolution is simple, its / or /%name% for lookup
*/

static void _set_browse_fs(struct fuse_fs_s *fs)
{

    set_virtual_fs(fs);

    fs->forget=_fs_browse_forget;
    fs->getattr=_fs_browse_getattr;
    fs->setattr=_fs_browse_setattr;

    fs->type.dir.use_fs=use_service_fs;
    fs->type.dir.lookup=_fs_browse_lookup;

    fs->type.dir.create=_fs_browse_create;
    fs->type.dir.mkdir=_fs_browse_mkdir;
    fs->type.dir.mknod=_fs_browse_mknod;
    fs->type.dir.symlink=_fs_browse_symlink;

    fs->type.dir.unlink=_fs_browse_unlink;
    fs->type.dir.rmdir=_fs_browse_rmdir;

    fs->type.dir.rename=_fs_browse_rename;

    fs->type.dir.opendir=_fs_browse_opendir;

    fs->type.dir.readdir=_fs_service_readdir;
    fs->type.dir.readdirplus=_fs_service_readdirplus;
    fs->type.dir.releasedir=_fs_service_releasedir;
    fs->type.dir.fsyncdir=_fs_service_fsyncdir;
    fs->type.dir.get_fuse_direntry=get_fuse_direntry_common;

    fs->statfs=_fs_browse_statfs;

}

void use_browse_fs(struct inode_s *inode)
{
    inode->fs=&browse_fs;
}

void init_browse_fs()
{
    memset(&browse_fs, 0, sizeof(struct fuse_fs_s));
    browse_fs.flags = FS_SERVICE_FLAG_ROOT | FS_SERVICE_FLAG_DIR;
    _set_browse_fs(&browse_fs);
}

void set_browse_fs(struct fuse_fs_s *fs)
{
    _set_browse_fs(fs);
}
