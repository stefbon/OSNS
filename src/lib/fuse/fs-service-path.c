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
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse.h"
#include "libosns-fuse-public.h"

#include "fs-virtual.h"
#include "fs-service-path.h"
#include "path.h"
#include "handle.h"
#include "fs-create.h"
#include "openfile.h"
#include "opendir.h"

static struct fuse_fs_s service_dir_fs;
static struct fuse_fs_s service_nondir_fs;

static void _fs_service_forget(struct service_context_s *ctx, struct inode_s *inode)
{
}

/* LOOKUP */

static void _fs_service_lookup(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    struct name_s xname={(char *)name, len, 0};
    struct fuse_handle_s *handle=NULL;
    struct path_service_fs_s *fs=NULL;

    calculate_nameindex(&xname);

    /* here try first the fstatat call if available 
	a handle can help to have a shortcut to stat of a name relative to the handle
	note that this construction only looks at the parent, not for example the handle
	of the root of the shared directory (TODO)

	20220910: no sftp server supports fstatat yet so this will fail
	            (it's not a standard sftp call, so it has to be an extension) */

    handle=get_fuse_handle(ctx, request->ino, FUSE_HANDLE_FLAG_FSTATAT);

    if (handle) {
	unsigned int pathlen=len;
	char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
	struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;

	init_fuse_path(fpath, pathlen + 1);
	append_name_fpath(fpath, &xname);
        fs=handle->ctx->service.filesystem.fs;
	(* fs->fstatat)(handle, request, pinode, &xname, fpath);

    } else {
	struct directory_s *directory=get_directory(ctx, pinode, 0);
	unsigned int pathlen=get_pathmax(ctx) + 1 + len;
	char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
	struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;

	init_fuse_path(fpath, pathlen + 1);
	append_name_fpath(fpath, &xname);
	get_path_root_context(directory, fpath);
	ctx=fpath->context;                             /* take the context found as root */
	fs=ctx->service.filesystem.fs;                  /* take the path based fs which is used with the root context (sftp,...) */
	logoutput_debug("LOOKUP %s:%s (thread %i)", ctx->name, fpath->pathstart, (int) gettid());
	(* fs->lookup)(ctx, request, pinode, &xname, fpath);

    }

}

/* GETATTR */

static void _fs_service_getattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode)
{
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=get_pathmax(ctx);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct directory_s *directory=NULL;
    struct path_service_fs_s *fs=NULL;

    init_fuse_path(fpath, pathlen + 1);

    if (system_stat_test_ISDIR(&inode->stat)) {

	directory=get_directory(ctx, inode, 0);
	start_directory_fpath(fpath);

    } else {

	directory=get_upper_directory_entry(entry);
	append_name_fpath(fpath, &entry->name);

    }

    get_path_root_context(directory, fpath);
    ctx=fpath->context;
    logoutput_debug("GETATTR %s:%s (thread %i)", ctx->name, fpath->pathstart, (int) gettid());
    fs=ctx->service.filesystem.fs;
    (* fs->getattr)(ctx, request, inode, fpath);

}

static void _fs_service_access(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, unsigned int mask)
{
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=get_pathmax(ctx);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct directory_s *directory=NULL;
    struct path_service_fs_s *fs=NULL;

    init_fuse_path(fpath, pathlen + 1);

    if (system_stat_test_ISDIR(&inode->stat)) {

	directory=get_directory(ctx, inode, 0);
	start_directory_fpath(fpath);

    } else {

	directory=get_upper_directory_entry(entry);
	append_name_fpath(fpath, &entry->name);

    }

    get_path_root_context(directory, fpath);
    ctx=fpath->context;
    logoutput_debug("ACCESS %s:%s (thread %i)", ctx->name, fpath->pathstart, (int) gettid());
    fs=ctx->service.filesystem.fs;
    (* fs->access)(ctx, request, inode, fpath, mask);

}

/* SETATTR */

static void _fs_service_setattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat)
{
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=get_pathmax(ctx);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct directory_s *directory=NULL;
    struct path_service_fs_s *fs=NULL;

    init_fuse_path(fpath, pathlen + 1);

    if (system_stat_test_ISDIR(&inode->stat)) {

	directory=get_directory(ctx, inode, 0);
	start_directory_fpath(fpath);

    } else {

	directory=get_upper_directory_entry(entry);
	append_name_fpath(fpath, &entry->name);

    }

    get_path_root_context(directory, fpath);
    ctx=fpath->context;
    logoutput("SETATTR %s:%s (thread %i)", ctx->name, fpath->pathstart, (int) gettid());
    fs=ctx->service.filesystem.fs;
    (* fs->setattr)(ctx, request, inode, fpath, stat);
}

struct _fs_mk_s {
    unsigned char					op;
    union _fs_browse_mk_s {
	struct _fs_mkdir_s {
	    mode_t					mode;
	    mode_t					mask;
	} mkdir;
	struct _fs_mknod_s {
	    mode_t					mode;
	    mode_t					mask;
	    dev_t					dev;
	    dev_t					rdev;
	} mknod;
	struct _fs_create_s {
	    struct fuse_openfile_s 			*openfile;
	    unsigned int				flags;
	    mode_t					mode;
	    mode_t					mask;
	    dev_t					dev;
	} create;
	struct _fs_symlink_s {
	    const char					*target;
	    unsigned int				len;
	} symlink;
    } mk;
};

static void set_mk_common_stat(struct system_stat_s *stat, struct fuse_request_s *request, mode_t type, mode_t perm)
{
    struct system_timespec_s time=SYSTEM_TIME_INIT;

    get_current_time_system_time(&time);

    memset(stat, 0, sizeof(struct system_stat_s));
    set_type_system_stat(stat, type);
    set_mode_system_stat(stat, perm);
    set_uid_system_stat(stat, request->uid);
    set_gid_system_stat(stat, request->gid);
    set_size_system_stat(stat, 0);
    set_blksize_system_stat(stat, 4096); 	/* TODO: get from local config/parameters */

    set_atime_system_stat(stat, &time);
    set_mtime_system_stat(stat, &time);
    set_ctime_system_stat(stat, &time);
    set_btime_system_stat(stat, &time);
}

static mode_t get_masked_permissions(mode_t perm, mode_t mask)
{
    return (perm & (S_IRWXU | S_IRWXG | S_IRWXO));
}

/* MK DIR, MK NOD and MK SYMLINK */

static void _fs_service_mk_common(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, struct _fs_mk_s *mk)
{
    struct name_s xname={(char *)name, len, 0};
    mode_t type=0;
    mode_t perm=0;
    struct entry_s *entry=NULL;
    struct system_stat_s stat;
    struct directory_s *directory=get_directory(ctx, pinode, 0);
    unsigned int pathlen=get_pathmax(ctx) + 1 + len;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct path_service_fs_s *fs=NULL;
    unsigned int error=0;

    init_fuse_path(fpath, pathlen + 1);
    append_name_fpath(fpath, &xname);
    get_path_root_context(directory, fpath);
    ctx=fpath->context;

    switch (mk->op) {

	case SERVICE_OP_TYPE_SYMLINK:

	    type=S_IFLNK;
	    perm=S_IRWXU | S_IRWXG | S_IRWXO;
	    set_mk_common_stat(&stat, request, type, perm);
	    break;

	case SERVICE_OP_TYPE_MKDIR:

	    type=S_IFDIR;
	    perm=get_masked_permissions(mk->mk.mkdir.mode & ~S_IFMT, mk->mk.mkdir.mask);
	    set_mk_common_stat(&stat, request, type, perm);
	    break;

	case SERVICE_OP_TYPE_MKNOD:

	    type=S_IFREG;
	    perm=get_masked_permissions(mk->mk.mknod.mode & ~S_IFMT, mk->mk.mknod.mask);
	    set_mk_common_stat(&stat, request, type, perm);
	    break;

	case SERVICE_OP_TYPE_CREATE:

	    type=S_IFREG;
	    perm=get_masked_permissions(mk->mk.create.mode & ~S_IFMT, mk->mk.create.mask);
	    set_mk_common_stat(&stat, request, type, perm);
	    break;

	case SERVICE_OP_TYPE_RMDIR:
	case SERVICE_OP_TYPE_UNLINK:

	    break;

	default:

	    reply_VFS_error(request, EINVAL);
	    return;

    }

    calculate_nameindex(&xname);

    if (mk->op==SERVICE_OP_TYPE_RMDIR || mk->op==SERVICE_OP_TYPE_UNLINK) {

	entry=find_entry(directory, &xname, &error);

    } else {

	entry=_fs_common_create_entry_unlocked(ctx, directory, &xname, &stat, 0, 0, &error);

    }

    /* entry created local and no error (no EEXIST!) */

    if (entry && error==0) {

        fs=ctx->service.filesystem.fs;

	if (mk->op==SERVICE_OP_TYPE_MKNOD) {

	    (* fs->mknod)(ctx, request, entry, fpath, &stat);

	} else if (mk->op==SERVICE_OP_TYPE_MKDIR) {

	    (* fs->mkdir)(ctx, request, entry, fpath, &stat);

	} else if (mk->op==SERVICE_OP_TYPE_SYMLINK) {
	    struct fs_location_path_s sub=FS_LOCATION_PATH_INIT;
	    char tmp[mk->mk.symlink.len + 1];

	    memcpy(tmp, mk->mk.symlink.target, mk->mk.symlink.len);
	    tmp[mk->mk.symlink.len]='\0';

	    if ((* fs->symlink_validate)(ctx, fpath, tmp, &sub)==0) {

		(* fs->symlink)(ctx, request, entry, fpath, &sub);

	    } else {

		reply_VFS_error(request, EINVAL);
		queue_inode_2forget(ctx, get_ino_system_stat(&entry->inode->stat), 0, 0);

	    }

	} else if (mk->op==SERVICE_OP_TYPE_CREATE) {
	    struct fuse_openfile_s *openfile=mk->mk.create.openfile;

	    openfile->header.ctx=ctx;
	    openfile->header.inode=entry->inode; /* now it's pointing to the right inode */

	    (* fs->open)(openfile, request, fpath, &stat, (mk->mk.create.flags | O_CREAT));

	    if (openfile->error>0) {
		struct inode_s *inode=openfile->header.inode;

		queue_inode_2forget(ctx, get_ino_system_stat(&inode->stat), 0, 0);
		openfile->header.inode=NULL;

	    }

	}

    } else {

	if (error==0) error=EIO;
	reply_VFS_error(request, error);
	if (entry) queue_inode_2forget(ctx, get_ino_system_stat(&entry->inode->stat), 0, 0);

    }

}

/* MKDIR */

void _fs_service_mkdir(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, mode_t mode, mode_t mask)
{
    struct _fs_mk_s mk;

    memset(&mk, 0, sizeof(struct _fs_mk_s));
    mk.op=SERVICE_OP_TYPE_MKDIR;
    mk.mk.mkdir.mode=mode;
    mk.mk.mkdir.mask=mask;
    _fs_service_mk_common(ctx, request, pinode, name, len, &mk);
}

/* MKNOD */

void _fs_service_mknod(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, mode_t mode, dev_t rdev, mode_t mask)
{
    struct _fs_mk_s mk;

    memset(&mk, 0, sizeof(struct _fs_mk_s));
    mk.op=SERVICE_OP_TYPE_MKNOD;
    mk.mk.mknod.mode=mode;
    mk.mk.mknod.mask=mask;
    mk.mk.mknod.rdev=rdev;
    _fs_service_mk_common(ctx, request, pinode, name, len, &mk);
}

/* SYMLINK */

void _fs_service_symlink(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, const char *target, unsigned int size)
{
    struct _fs_mk_s mk;

    memset(&mk, 0, sizeof(struct _fs_mk_s));
    mk.op=SERVICE_OP_TYPE_SYMLINK;
    mk.mk.symlink.target=target;
    mk.mk.symlink.len=size;
    _fs_service_mk_common(ctx, request, pinode, name, len, &mk);
}

/* REMOVE/UNLINK */

static void _fs_service_rm_common(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, unsigned char op)
{
    struct name_s xname={(char *)name, len, 0};
    struct entry_s *entry=NULL;
    struct directory_s *directory=get_directory(ctx, pinode, 0);
    unsigned int pathlen=get_pathmax(ctx) + 1 + len;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct path_service_fs_s *fs=NULL;
    unsigned int error=0;

    logoutput_debug("_fs_service_rm_common: context %s (thread %i) %.*s op %i", ctx->name, (int) gettid(), len, name, op);

    init_fuse_path(fpath, pathlen + 1);
    append_name_fpath(fpath, &xname);
    get_path_root_context(directory, fpath);
    ctx=fpath->context;

    calculate_nameindex(&xname);
    entry=find_entry(directory, &xname, &error);

    if (entry) {

        fs=ctx->service.filesystem.fs;

	if (op==SERVICE_OP_TYPE_RMDIR) {
	    struct inode_s *inode=entry->inode;

	    if (inode) {
		struct directory_s *subd=get_directory(ctx, inode, GET_DIRECTORY_FLAG_NOCREATE);

		if (subd && get_directory_count(subd)>0) {

		    reply_VFS_error(request, ENOTEMPTY);
		    return;

		}

	    }

	    (* fs->rmdir)(ctx, request, &entry, fpath);

	} else if (op==SERVICE_OP_TYPE_UNLINK) {

	    (* fs->unlink)(ctx, request, &entry, fpath);

	}

    } else {

	reply_VFS_error(request, ENOENT);

    }

}

/* RMDIR */

static void _fs_service_rmdir(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    _fs_service_rm_common(ctx, request, pinode, name, len, SERVICE_OP_TYPE_RMDIR);
}

/* UNLINK */

static void _fs_service_unlink(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    _fs_service_rm_common(ctx, request, pinode, name, len, SERVICE_OP_TYPE_UNLINK);
}

/* READLINK */

static void _fs_service_readlink(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode)
{
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=get_pathmax(ctx) + 1;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct directory_s *directory=get_upper_directory_entry(entry);
    struct path_service_fs_s *fs=NULL;

    init_fuse_path(fpath, pathlen + 1);
    append_name_fpath(fpath, &entry->name);
    get_path_root_context(directory, fpath);
    ctx=fpath->context;
    logoutput_debug("READLINK %s:%s (thread %i) %li", ctx->name, fpath->pathstart, (int) gettid(), get_ino_system_stat(&inode->stat));
    fs=ctx->service.filesystem.fs;
    (* fs->readlink)(ctx, request, inode, fpath);

}

/* OPEN and OPENDIR and ... */

static void _fs_service_open_common(struct fuse_open_header_s *header, struct fuse_request_s *request, unsigned int flags)
{
    struct service_context_s *ctx=header->ctx;
    struct inode_s *inode=header->inode;
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=get_pathmax(ctx) + 1;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct path_service_fs_s *fs=NULL;

    logoutput("_fs_service_open_common: pathlen %i entry %.*s", pathlen, entry->name.len, entry->name.name);
    init_fuse_path(fpath, pathlen + 1);

    if (header->type==FUSE_OPEN_TYPE_FILE) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) header;
	struct directory_s *directory=get_upper_directory_entry(entry);

	append_name_fpath(fpath, &entry->name);
	get_path_root_context(directory, fpath);
	ctx=fpath->context;
	openfile->header.ctx=ctx;
	logoutput_debug("OPEN %s:%s (thread %i)", ctx->name, fpath->pathstart, (int) gettid());
	fs=ctx->service.filesystem.fs;
	(* fs->open)(openfile, request, fpath, NULL, flags);

    } else if (header->type==FUSE_OPEN_TYPE_DIR) {
	struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) header;
	struct directory_s *directory=get_directory(ctx, inode, 0);

	start_directory_fpath(fpath);
	get_path_root_context(directory, fpath);
	ctx=fpath->context;
	opendir->header.ctx=ctx;
	logoutput_debug("OPENDIR %s:%s (thread %i)", ctx->name, fpath->pathstart, (int) gettid());
	fs=ctx->service.filesystem.fs;
	(* fs->opendir)(opendir, request, fpath, flags);

    }

}

static void _fs_service_open(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags)
{
    _fs_service_open_common(&openfile->header, request, flags);
}

/* CREATE */

static void _fs_service_create(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *name, unsigned int len, unsigned int flags, mode_t mode, mode_t mask)
{
    struct _fs_mk_s mk;

    memset(&mk, 0, sizeof(struct _fs_mk_s));
    mk.op=SERVICE_OP_TYPE_CREATE;
    mk.mk.create.flags=flags;
    mk.mk.create.mode=mode;
    mk.mk.create.mask=mask;
    mk.mk.create.openfile=openfile;
    _fs_service_mk_common(openfile->header.ctx, request, openfile->header.inode, name, len, &mk);
}

/* RENAME
    - 20210527: make this work
*/

static void _fs_service_rename(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, const char *name, struct inode_s *n_inode, const char *n_name, unsigned int flags)
{
    reply_VFS_error(request, ENOSYS);
}

/* OPENDIR */

static void _fs_service_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags)
{
    _fs_service_open_common(&opendir->header, request, flags);
}

struct _fs_xattr_s {
    unsigned char					op;
    union {
	struct _fs_setxattr_s {
	    const char					*name;
	    const char					*value;
	    size_t					size;
	    unsigned int				flags;
	} setxattr;
	struct _fs_getxattr_s {
	    const char					*name;
	    size_t					size;
	} getxattr;
	struct _fs_listxattr_s {
	    size_t					size;
	} listxattr;
	struct _fs_rmxattr_s {
	    const char					*name;
	    size_t					size;
	} rmxattr;
    } type;
};

static void _fs_service_xattr_common(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct _fs_xattr_s *xattr)
{
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=get_pathmax(ctx) + 1;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct directory_s *directory=NULL;
    struct path_service_fs_s *fs=NULL;

    init_fuse_path(fpath, pathlen + 1);

    if (system_stat_test_ISDIR(&inode->stat)) {

	directory=get_directory(ctx, inode, 0);
	start_directory_fpath(fpath);

    } else {

	directory=get_upper_directory_entry(entry);
	append_name_fpath(fpath, &entry->name);

    }

    get_path_root_context(directory, fpath);
    ctx=fpath->context;
    fs=ctx->service.filesystem.fs;

    switch (xattr->op) {

	case SERVICE_OP_TYPE_SETXATTR:

	    logoutput_debug("setxattr %s (thread %i): %s", ctx->name, (int) gettid(), xattr->type.setxattr.name);
	    (* fs->setxattr)(ctx, request, fpath, inode, xattr->type.setxattr.name, xattr->type.setxattr.value, xattr->type.setxattr.size, xattr->type.setxattr.flags);
	    break;

	case SERVICE_OP_TYPE_GETXATTR:

	    logoutput_debug("getxattr %s (thread %i): %s", ctx->name, (int) gettid(), xattr->type.getxattr.name);
	    (* fs->getxattr)(ctx, request, fpath, inode, xattr->type.getxattr.name, xattr->type.getxattr.size);
	    break;

	case SERVICE_OP_TYPE_LISTXATTR:

	    logoutput_debug("listxattr %s (thread %i)", ctx->name, (int) gettid());
	    (* fs->listxattr)(ctx, request, fpath, inode, xattr->type.listxattr.size);
	    break;

	case SERVICE_OP_TYPE_REMOVEXATTR:

	    logoutput_debug("removexattr %s (thread %i)", ctx->name, (int) gettid());
	    (* fs->removexattr)(ctx, request, fpath, inode, xattr->type.rmxattr.name);
	    break;

    }

}

static void _fs_service_setxattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, const char *name, const char *value, size_t size, int flags)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_SETXATTR;
    xattr.type.setxattr.name=name;
    xattr.type.setxattr.value=value;
    xattr.type.setxattr.size=size;
    xattr.type.setxattr.flags=flags;
    _fs_service_xattr_common(ctx, request, inode, &xattr);
}

static void _fs_service_getxattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, const char *name, size_t size)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_GETXATTR;
    xattr.type.getxattr.name=name;
    xattr.type.getxattr.size=size;
    _fs_service_xattr_common(ctx, request, inode, &xattr);
}

static void _fs_service_listxattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, size_t size)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_LISTXATTR;
    xattr.type.listxattr.size=size;
    _fs_service_xattr_common(ctx, request, inode, &xattr);
}

static void _fs_service_removexattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, const char *name)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_REMOVEXATTR;
    xattr.type.rmxattr.name=name;
    _fs_service_xattr_common(ctx, request, inode, &xattr);
}

/* STATFS */

static void _fs_service_statfs(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode)
{
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=get_pathmax(ctx) + 1;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct directory_s *directory=NULL;
    struct path_service_fs_s *fs=NULL;

    init_fuse_path(fpath, pathlen + 1);

    if (system_stat_test_ISDIR(&inode->stat)) {

	directory=get_directory(ctx, inode, GET_DIRECTORY_FLAG_NOCREATE);
	start_directory_fpath(fpath);

    } else {

	directory=get_upper_directory_entry(entry);
	append_name_fpath(fpath, &entry->name);

    }

    get_path_root_context(directory, fpath);
    ctx=fpath->context;
    logoutput("STATFS %s (thread %i)", ctx->name, (int) gettid());
    fs=ctx->service.filesystem.fs;
    (* fs->statfs)(ctx, request, inode, fpath);

}

void use_service_path_fs(struct service_context_s *ctx, struct inode_s *inode)
{

    if (system_stat_test_ISDIR(&inode->stat)) {

	inode->fs=&service_dir_fs;

    } else {

	inode->fs=&service_nondir_fs;

    }

}

static void _set_service_path_fs(struct fuse_fs_s *fs)
{

    set_virtual_fs(fs);

    fs->forget=_fs_service_forget;
    fs->getattr=_fs_service_getattr;
    fs->setattr=_fs_service_setattr;
    fs->access=_fs_service_access;
    fs->readlink=_fs_service_readlink;

    if (fs->flags & (FS_SERVICE_FLAG_DIR | FS_SERVICE_FLAG_ROOT)) {

	fs->type.dir.use_fs=use_service_path_fs;

	fs->type.dir.lookup=_fs_service_lookup;
	fs->type.dir.create=_fs_service_create;
	fs->type.dir.mkdir=_fs_service_mkdir;
	fs->type.dir.mknod=_fs_service_mknod;
	fs->type.dir.symlink=_fs_service_symlink;

	fs->type.dir.unlink=_fs_service_unlink;
	fs->type.dir.rmdir=_fs_service_rmdir;
	fs->type.dir.rename=_fs_service_rename;

	fs->type.dir.opendir=_fs_service_opendir;

    } else {

	/* NON DIRECTORY FS */

	fs->flags|=FS_SERVICE_FLAG_NONDIR;
	fs->type.nondir.open=_fs_service_open;

    }

    fs->getxattr=_fs_service_getxattr;
    fs->setxattr=_fs_service_setxattr;
    fs->listxattr=_fs_service_listxattr;
    fs->removexattr=_fs_service_removexattr;

    fs->statfs=_fs_service_statfs;

}

void init_service_path_fs()
{
    struct fuse_fs_s *fs=NULL;

    fs=&service_dir_fs;

    memset(fs, 0, sizeof(struct fuse_fs_s));
    fs->flags=FS_SERVICE_FLAG_DIR;
    _set_service_path_fs(fs);

    fs=&service_nondir_fs;

    memset(fs, 0, sizeof(struct fuse_fs_s));
    fs->flags=FS_SERVICE_FLAG_NONDIR;
    _set_service_path_fs(fs);

}

void set_service_path_fs(struct fuse_fs_s *fs)
{
    _set_service_path_fs(fs);
}

unsigned char check_service_path_fs(struct inode_s *inode)
{
    unsigned char isused=0;

    if (S_ISDIR(inode->stat.sst_mode)) {

	isused=(inode->fs==&service_dir_fs) ? 1 : 0;

    } else {

	isused=(inode->fs==&service_nondir_fs) ? 1 : 0;

    }

    return isused;

}
