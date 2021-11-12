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
#include <sys/vfs.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "misc.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

extern void use_service_fs(struct service_context_s *context, struct inode_s *inode);

static struct fuse_fs_s service_dir_fs;
static struct fuse_fs_s service_nondir_fs;
static struct statfs default_statfs;

static void _fs_service_forget(struct inode_s *inode)
{
}

/* LOOKUP */

static void _fs_service_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct name_s xname={(char *)name, len, 0};
    struct entry_s *entry=NULL;
    struct directory_s *directory=get_directory(pinode);
    unsigned int pathlen=(* directory->getpath->get_pathlen)(context, directory) + 1 + len;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_LOOKUP);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    append_name_fpath(fpath, &xname);
    get_service_context_path(context, directory, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);
    context=fpath->context;

    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, SERVICE_OP_TYPE_LOOKUP);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    logoutput("LOOKUP %s (thread %i) %s", context->name, (int) gettid(), pathinfo.path);

    calculate_nameindex(&xname);
    entry=find_entry(directory, &xname, &error);

    if (entry) {

	(* fs->lookup_existing)(context, request, entry, &pathinfo);

    } else {

	(* fs->lookup_new)(context, request, pinode, &xname, &pathinfo);

    }

}

/* GETATTR */

static void _fs_service_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=(* entry->ops->get_pathlen)(context, entry);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_GETATTR);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    (* entry->ops->append_path)(context, entry, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, SERVICE_OP_TYPE_GETATTR);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    logoutput("GETATTR %s (thread %i): %s", context->name, (int) gettid(), pathinfo.path);
    (* fs->getattr)(context, request, inode, &pathinfo);

}

/* SETATTR */

static void _fs_service_setattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=(* entry->ops->get_pathlen)(context, entry);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_SETATTR);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    (* entry->ops->append_path)(context, entry, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, SERVICE_OP_TYPE_SETATTR);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    logoutput("SETATTR %s (thread %i): %s", context->name, (int) gettid(), pathinfo.path);
    (* fs->setattr)(context, request, inode, &pathinfo, stat);
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


/* MK DIR, MK NOD and MK SYMLINK */

static void _fs_service_mk_common(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, struct _fs_mk_s *mk)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(context);
    struct name_s xname={(char *)name, len, 0};
    mode_t type=0;
    mode_t perm=0;
    struct entry_s *entry=NULL;
    struct system_stat_s stat;
    struct system_timespec_s time=SYSTEM_TIME_INIT;
    struct directory_s *directory=get_directory(pinode);
    unsigned int pathlen=(* directory->getpath->get_pathlen)(context, directory) + 1 + len;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, mk->op);

    logoutput("_fs_service_mk_common: context %s (thread %i): %.*s op %i", context->name, (int) gettid(), len, name, mk->op);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    append_name_fpath(fpath, &xname);
    get_service_context_path(context, directory, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, mk->op);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    switch (mk->op) {

	case SERVICE_OP_TYPE_SYMLINK:

	    type=S_IFLNK;
	    perm=S_IRWXU | S_IRWXG | S_IRWXO;
	    break;

	case SERVICE_OP_TYPE_MKDIR:

	    type=(mk->mk.mkdir.mode & S_IFMT);

	    if (S_ISDIR(type)) {

		perm=get_masked_permissions(request->root, mk->mk.mkdir.mode - type, mk->mk.mkdir.mask);

	    } else {

		type=0;

	    }

	    break;

	case SERVICE_OP_TYPE_MKNOD:

	    type=(mk->mk.mknod.mode & S_IFMT);

	    if (S_ISREG(type)) {

		perm=get_masked_permissions(request->root, mk->mk.mknod.mode - type, mk->mk.mknod.mask);

	    } else {

		type=0;

	    }

	    break;

	case SERVICE_OP_TYPE_CREATE:

	    type=(mk->mk.create.mode & S_IFMT);

	    if (S_ISREG(type)) {

		perm=get_masked_permissions(request->root, mk->mk.create.mode - type, mk->mk.create.mask);

	    } else {

		type=0;

	    }

	    break;

    }

    if (type==0) {

	reply_VFS_error(request, EINVAL);
	return;

    }

    memset(&stat, 0, sizeof(struct system_stat_s));
    set_type_system_stat(&stat, type);
    set_mode_system_stat(&stat, perm);
    set_uid_system_stat(&stat, request->uid);
    set_gid_system_stat(&stat, request->gid);
    set_size_system_stat(&stat, 0);
    set_blksize_system_stat(&stat, 4096); 	/* TODO: get from local config/parameters */

    get_current_time_system_time(&time);
    set_atime_system_stat(&stat, &time);
    set_mtime_system_stat(&stat, &time);
    set_ctime_system_stat(&stat, &time);
    set_btime_system_stat(&stat, &time);

    calculate_nameindex(&xname);
    entry=_fs_common_create_entry_unlocked(w, directory, &xname, &stat, 0, 0, &error);

    /* entry created local and no error (no EEXIST!) */

    if (entry && error==0) {

	if (mk->op==SERVICE_OP_TYPE_MKNOD) {

	    (* fs->mknod)(context, request, entry, &pathinfo, &stat);

	} else if (mk->op==SERVICE_OP_TYPE_MKDIR) {

	    (* fs->mkdir)(context, request, entry, &pathinfo, &stat);

	} else if (mk->op==SERVICE_OP_TYPE_SYMLINK) {
	    char *remote_target=NULL;
	    char tmp[mk->mk.symlink.len + 1];

	    memcpy(tmp, mk->mk.symlink.target, mk->mk.symlink.len);
	    tmp[mk->mk.symlink.len]='\0';

	    if ((* fs->symlink_validate)(context, &pathinfo, tmp, &remote_target)==0) {

		(* fs->symlink)(context, request, entry, &pathinfo, remote_target);
		if (remote_target) free(remote_target);

	    } else {

		reply_VFS_error(request, EINVAL);
		queue_inode_2forget(w, entry->inode->stat.sst_ino, 0, 0);

	    }

	} else if (mk->op==SERVICE_OP_TYPE_CREATE) {
	    struct fuse_openfile_s *openfile=mk->mk.create.openfile;

	    openfile->context=context;
	    openfile->inode=entry->inode; /* now it's pointing to the right inode */

	    (* fs->create)(openfile, request, &pathinfo, &stat, mk->mk.create.flags);

	    if (openfile->error>0) {
		struct inode_s *inode=openfile->inode;

		queue_inode_2forget(w, inode->stat.sst_ino, 0, 0);
		openfile->inode=NULL;

	    }

	}

    } else {

	if (error==0) error=EIO;
	reply_VFS_error(request, error);
	if (entry) queue_inode_2forget(w, entry->inode->stat.sst_ino, 0, 0);

    }

}

/* MKDIR */

void _fs_service_mkdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, mode_t mode, mode_t mask)
{
    struct _fs_mk_s mk;

    memset(&mk, 0, sizeof(struct _fs_mk_s));
    mk.op=SERVICE_OP_TYPE_MKDIR;
    mk.mk.mkdir.mode=mode;
    mk.mk.mkdir.mask=mask;
    _fs_service_mk_common(context, request, pinode, name, len, &mk);
}

/* MKNOD */

void _fs_service_mknod(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, mode_t mode, dev_t rdev, mode_t mask)
{
    struct _fs_mk_s mk;

    memset(&mk, 0, sizeof(struct _fs_mk_s));
    mk.op=SERVICE_OP_TYPE_MKNOD;
    mk.mk.mknod.mode=mode;
    mk.mk.mknod.mask=mask;
    mk.mk.mknod.rdev=rdev;
    _fs_service_mk_common(context, request, pinode, name, len, &mk);
}

/* SYMLINK */

void _fs_service_symlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, const char *target, unsigned int size)
{
    struct _fs_mk_s mk;

    memset(&mk, 0, sizeof(struct _fs_mk_s));
    mk.op=SERVICE_OP_TYPE_SYMLINK;
    mk.mk.symlink.target=target;
    mk.mk.symlink.len=size;
    _fs_service_mk_common(context, request, pinode, name, len, &mk);
}

/* REMOVE/UNLINK */

static void _fs_service_rm_common(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len, unsigned char op)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(context);
    struct name_s xname={(char *)name, len, 0};
    struct entry_s *entry=NULL;
    struct directory_s *directory=get_directory(pinode);
    unsigned int pathlen=(* directory->getpath->get_pathlen)(context, directory) + 1 + len;
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, op);

    logoutput("_fs_service_rm_common: context %s (thread %i) %.*s", context->name, (int) gettid(), len, name);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    append_name_fpath(fpath, &xname);
    get_service_context_path(context, directory, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, op);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    calculate_nameindex(&xname);
    directory=get_directory(pinode);
    entry=find_entry(directory, &xname, &error);

    if (entry) {

	if (op==SERVICE_OP_TYPE_RMDIR) {
	    struct inode_s *inode=entry->inode;

	    if (inode) {
		struct directory_s *subd=get_directory(inode);

		if (subd && get_directory_count(subd)>0) {

		    reply_VFS_error(request, ENOTEMPTY);
		    return;

		}

	    }

	    (* fs->rmdir)(context, request, &entry, &pathinfo);

	    if (entry==NULL && inode) {
		struct directory_s *subd=remove_directory(inode, &error);

		if (subd) free_directory(subd);

	    }

	} else if (op==SERVICE_OP_TYPE_UNLINK) {

	    (* fs->unlink)(context, request, &entry, &pathinfo);

	}

    } else {

	reply_VFS_error(request, ENOENT);

    }

}

/* RMDIR */

static void _fs_service_rmdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    _fs_service_rm_common(context, request, pinode, name, len, SERVICE_OP_TYPE_RMDIR);
}

/* UNLINK */

static void _fs_service_unlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    _fs_service_rm_common(context, request, pinode, name, len, SERVICE_OP_TYPE_UNLINK);
}

/* READLINK */

static void _fs_service_readlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=(* entry->ops->get_pathlen)(context, entry);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=NULL;
    unsigned int error=0;

    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, SERVICE_OP_TYPE_READLINK);

    logoutput("READLINK %s (thread %i) %li", context->name, (int) gettid(), inode->stat.sst_ino);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    (* entry->ops->append_path)(context, entry, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, SERVICE_OP_TYPE_READLINK);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    (* fs->readlink)(context, request, inode, &pathinfo);

}

/* OPEN and OPENDIR and ... */

struct _fs_open_common_s {
    unsigned char			op;
    struct service_context_s		*ctx;
    union {
	struct fuse_openfile_s 		*openfile;
	struct fuse_opendir_s 		*opendir;
    } type;
};

static struct inode_s *get_opencommon_inode(struct _fs_open_common_s *oc)
{
    struct inode_s *inode=NULL;

    logoutput("get_opencommon_inode: op %i", oc->op);

    switch (oc->op) {
	case SERVICE_OP_TYPE_OPEN:
	    {
	    struct fuse_openfile_s *openfile=oc->type.openfile;
	    inode=openfile->inode;
	    break;
	    }

	case SERVICE_OP_TYPE_OPENDIR:
	    {
	    struct fuse_opendir_s *opendir=oc->type.opendir;
	    inode=opendir->inode;
	    break;
	    }

    }

    return inode;
}

static void _fs_service_open_common(struct _fs_open_common_s *opencommon, struct fuse_request_s *request, unsigned int flags)
{
    struct service_context_s *context=opencommon->ctx;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct inode_s *inode=get_opencommon_inode(opencommon);
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=(* entry->ops->get_pathlen)(context, entry);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, opencommon->op);

    logoutput("_fs_service_open_common: pathlen %i entry %.*s", pathlen, entry->name.len, entry->name.name);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    (* entry->ops->append_path)(context, entry, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);
    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, opencommon->op);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    if (opencommon->op==SERVICE_OP_TYPE_OPEN) {
	struct fuse_openfile_s *openfile=opencommon->type.openfile;

	openfile->context=context;
	logoutput("OPEN %s (thread %i): %s", context->name, (int) gettid(), pathinfo.path);
	(* fs->open)(openfile, request, &pathinfo, flags);

    } else if (opencommon->op==SERVICE_OP_TYPE_OPENDIR) {
	struct fuse_opendir_s *opendir=opencommon->type.opendir;
	struct directory_s *directory=get_directory(inode);

	opendir->context=context;
	logoutput("OPENDIR %s (thread %i) %s", context->name, (int) gettid(), pathinfo.path);
	(* fs->opendir)(opendir, request, &pathinfo, flags);
	set_directory_pathcache(context, directory, fpath);

    }

}

static void _fs_service_open(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags)
{
    struct _fs_open_common_s opencommon;

    opencommon.op=SERVICE_OP_TYPE_OPEN;
    opencommon.ctx=openfile->context;
    opencommon.type.openfile=openfile;

    _fs_service_open_common(&opencommon, request, flags);
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

    _fs_service_mk_common(openfile->context, request, openfile->inode, name, len, &mk);
}

/* READ */

void _fs_service_read(struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("READ %s (thread %i)", openfile->context->name, (int) gettid());
    (* fs->read)(openfile, request, size, off, flags, lock_owner);
}

/* WRITE */

void _fs_service_write(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("WRITE %s (thread %i)", openfile->context->name, (int) gettid());
    (* fs->write)(openfile, request, buff, size, off, flags, lock_owner);
}

/* FSYNC */

void _fs_service_fsync(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char datasync)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("FSYNC %s (thread %i)", openfile->context->name, (int) gettid());
    (* fs->fsync)(openfile, request, datasync);
}

/* FLUSH */

void _fs_service_flush(struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lockowner)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("FLUSH %s (thread %i) lockowner %li", openfile->context->name, (int) gettid(), (unsigned long int) lockowner);
    (* fs->flush)(openfile, request, lockowner);
}

/* FGETATTR */

void _fs_service_fgetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *request)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("FGETATTR %s (thread %i)", openfile->context->name, (int) gettid());
    (* fs->fgetattr)(openfile, request);
}

/* FSETATTR */

void _fs_service_fsetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct system_stat_s *stat)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("FSETATTR %s (thread %i)", openfile->context->name, (int) gettid());
    (* fs->fsetattr)(openfile, request, stat);
}

/* RELEASE */

void _fs_service_release(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lockowner)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("RELEASE %s (thread %i) lockowner %li", openfile->context->name, (int) gettid(), (unsigned long int) lockowner);
    (* fs->release)(openfile, request, flags, lockowner);
}

/* GETLOCK (bytelock) */

void _fs_service_getlock(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("GETLOCK %s (thread %i)", openfile->context->name, (int) gettid());
    // reply_VFS_error(request, ENOSYS);
    (* fs->getlock)(openfile, request, flock);
}

/* SETLOCK (bytelock) */

void _fs_service_setlock(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("SETLOCK %s (thread %i)", openfile->context->name, (int) gettid());
    reply_VFS_error(request, ENOSYS);
    (* fs->setlock)(openfile, request, flock);
}

/* SETLOCKW (bytelock) */

void _fs_service_setlockw(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);
    logoutput("SETLOCKW %s (thread %i)", openfile->context->name, (int) gettid());
    reply_VFS_error(request, ENOSYS);
    (* fs->setlockw)(openfile, request, flock);
}

/* FLOCK (filelock) */

void _fs_service_flock(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char type)
{
    struct service_fs_s *fs=get_service_context_fs(openfile->context);

    logoutput("FLOCK %s (thread %i) lock %i:%i", openfile->context->name, (int) gettid(), openfile->flock, type);

    /*
	type can be one of following:

	- LOCK_SH : shared lock
	- LOCK_EX : exlusive lock
	- LOCK_UN : remove current lock

	service fs has to deal with down/upgrades/release of locks
	previous lock is in openfile->flock
    */

    (* fs->flock)(openfile, request, type);

}

/* RENAME
    - 20210527: make this work
*/

static void _fs_service_rename(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, struct inode_s *n_inode, const char *n_name, unsigned int flags)
{
    reply_VFS_error(request, ENOSYS);
}

/* OPENDIR */

static void _fs_service_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags)
{
    struct _fs_open_common_s opencommon;

    opencommon.op=SERVICE_OP_TYPE_OPENDIR;
    opencommon.ctx=opendir->context;
    opencommon.type.opendir=opendir;

    _fs_service_open_common(&opencommon, request, flags);
}

/* READDIR */

void _fs_service_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset)
{
    struct service_fs_s *fs=get_service_context_fs(opendir->context);
    logoutput("READDIR %s (thread %i)", opendir->context->name, (int) gettid());
    (* fs->readdir)(opendir, request, size, offset);
}

/* READDIRPLUS */

void _fs_service_readdirplus(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset)
{
    struct service_fs_s *fs=get_service_context_fs(opendir->context);
    logoutput("READDIRPLUS %s (thread %i)", opendir->context->name, (int) gettid());
    (* fs->readdirplus)(opendir, request, size, offset);
}

/* FSYNCDIR */

void _fs_service_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync)
{
    struct service_fs_s *fs=get_service_context_fs(opendir->context);
    logoutput("FSYNCDIR %s (thread %i)", opendir->context->name, (int) gettid());
    (* fs->fsyncdir)(opendir, request, datasync);
}

/* RELEASEDIR */

void _fs_service_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *request)
{
    unsigned int error=0;
    struct directory_s *directory=get_directory(opendir->inode);
    struct service_fs_s *fs=get_service_context_fs(opendir->context);

    logoutput("RELEASEDIR %s (thread %i)", opendir->context->name, (int) gettid());

    (* fs->releasedir)(opendir, request);
    if (directory) release_directory_pathcache(directory);

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

static void _fs_service_xattr_common(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct _fs_xattr_s *xattr)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=(* entry->ops->get_pathlen)(context, entry);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, xattr->op);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    (* entry->ops->append_path)(context, entry, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, xattr->op);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    switch (xattr->op) {

	case SERVICE_OP_TYPE_SETXATTR:

	logoutput("setxattr %s (thread %i): %s:%s", context->name, (int) gettid(), pathinfo.path, xattr->type.setxattr.name);
	(* fs->setxattr)(context, request, &pathinfo, inode, xattr->type.setxattr.name, xattr->type.setxattr.value, xattr->type.setxattr.size, xattr->type.setxattr.flags);
	break;

	case SERVICE_OP_TYPE_GETXATTR:

	logoutput("getxattr %s (thread %i): %s:%s", context->name, (int) gettid(), pathinfo.path, xattr->type.getxattr.name);
	(* fs->getxattr)(context, request, &pathinfo, inode, xattr->type.getxattr.name, xattr->type.getxattr.size);
	break;

	case SERVICE_OP_TYPE_LISTXATTR:

	logoutput("listxattr %s (thread %i): %s", context->name, (int) gettid(), pathinfo.path);
	(* fs->listxattr)(context, request, &pathinfo, inode, xattr->type.listxattr.size);
	break;

	case SERVICE_OP_TYPE_REMOVEXATTR:

	logoutput("removexattr (thread %i): %s", context->name, (int) gettid(), pathinfo.path);
	(* fs->removexattr)(context, request, &pathinfo, inode, xattr->type.rmxattr.name);
	break;


    }

}

static void _fs_service_setxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, const char *value, size_t size, int flags)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_SETXATTR;
    xattr.type.setxattr.name=name;
    xattr.type.setxattr.value=value;
    xattr.type.setxattr.size=size;
    xattr.type.setxattr.flags=flags;

    _fs_service_xattr_common(context, request, inode, &xattr);

}

static void _fs_service_getxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, size_t size)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_GETXATTR;
    xattr.type.getxattr.name=name;
    xattr.type.getxattr.size=size;

    _fs_service_xattr_common(context, request, inode, &xattr);
}

static void _fs_service_listxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, size_t size)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_LISTXATTR;
    xattr.type.listxattr.size=size;

    _fs_service_xattr_common(context, request, inode, &xattr);
}

static void _fs_service_removexattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name)
{
    struct _fs_xattr_s xattr;

    xattr.op=SERVICE_OP_TYPE_REMOVEXATTR;
    xattr.type.rmxattr.name=name;

    _fs_service_xattr_common(context, request, inode, &xattr);
}

/* STATFS */

static void _fs_service_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct entry_s *entry=inode->alias;
    unsigned int pathlen=(* entry->ops->get_pathlen)(context, entry);
    char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
    struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
    struct pathinfo_s pathinfo=PATHINFO_INIT;
    struct service_fs_s *fs=get_service_context_fs(context);
    unsigned int error=(* fs->access)(context, request, SERVICE_OP_TYPE_STATFS);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    init_fuse_path(fpath, pathlen + 1);
    (* entry->ops->append_path)(context, entry, fpath);
    pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

    context=fpath->context;
    fs=get_service_context_fs(context);
    error=(* fs->access)(context, request, SERVICE_OP_TYPE_STATFS);

    if (error) {

	reply_VFS_error(request, error);
	return;

    }

    logoutput("STATFS %s (thread %i) %s", context->name, (int) gettid(), pathinfo.path);
    (* fs->statfs)(context, request, &pathinfo);

}

static void _set_service_fs(struct fuse_fs_s *fs)
{

    set_virtual_fs(fs);

    fs->forget=_fs_service_forget;
    fs->getattr=_fs_service_getattr;
    fs->setattr=_fs_service_setattr;

    if (fs->flags & (FS_SERVICE_FLAG_DIR | FS_SERVICE_FLAG_ROOT)) {

	fs->type.dir.use_fs=use_service_fs;
	fs->type.dir.lookup=_fs_service_lookup;

	fs->type.dir.create=_fs_service_create;
	fs->type.dir.mkdir=_fs_service_mkdir;
	fs->type.dir.mknod=_fs_service_mknod;
	fs->type.dir.symlink=_fs_service_symlink;

	fs->type.dir.unlink=_fs_service_unlink;
	fs->type.dir.rmdir=_fs_service_rmdir;

	fs->type.dir.rename=_fs_service_rename;

	fs->type.dir.opendir=_fs_service_opendir;
	fs->type.dir.readdir=_fs_service_readdir;
	fs->type.dir.readdirplus=_fs_service_readdirplus;
	fs->type.dir.releasedir=_fs_service_releasedir;
	fs->type.dir.fsyncdir=_fs_service_fsyncdir;
	fs->type.dir.get_fuse_direntry=get_fuse_direntry_common;

    } else {

	/* NON DIRECTORY FS */

	fs->flags|=FS_SERVICE_FLAG_NONDIR;
	fs->type.nondir.readlink=_fs_service_readlink;

	fs->type.nondir.open=_fs_service_open;
	fs->type.nondir.read=_fs_service_read;
	fs->type.nondir.write=_fs_service_write;
	fs->type.nondir.flush=_fs_service_flush;
	fs->type.nondir.fsync=_fs_service_fsync;
	fs->type.nondir.release=_fs_service_release;

	fs->type.nondir.fgetattr=_fs_service_fgetattr;
	fs->type.nondir.fsetattr=_fs_service_fsetattr;

	fs->type.nondir.getlock=_fs_service_getlock;
	fs->type.nondir.setlock=_fs_service_setlock;
	fs->type.nondir.setlockw=_fs_service_setlockw;

	fs->type.nondir.flock=_fs_service_flock;

    }

    fs->getxattr=_fs_service_getxattr;
    fs->setxattr=_fs_service_setxattr;
    fs->listxattr=_fs_service_listxattr;
    fs->removexattr=_fs_service_removexattr;

    fs->statfs=_fs_service_statfs;

}

void init_service_fs()
{
    struct fuse_fs_s *fs=NULL;

    if (statfs("/", &default_statfs)==-1) {

	logoutput_warning("init_virtual_fs: cannot stat root filesystem at /... taking default fs values");

	default_statfs.f_blocks=1000000;
	default_statfs.f_bfree=1000000;
	default_statfs.f_bavail=default_statfs.f_bfree;
	default_statfs.f_bsize=4096;

    }

    fs=&service_dir_fs;

    memset(fs, 0, sizeof(struct fuse_fs_s));
    fs->flags=FS_SERVICE_FLAG_DIR;
    _set_service_fs(fs);

    fs=&service_nondir_fs;

    memset(fs, 0, sizeof(struct fuse_fs_s));
    fs->flags=FS_SERVICE_FLAG_NONDIR;
    _set_service_fs(fs);

}

void use_service_path_fs(struct inode_s *inode)
{

    if (S_ISDIR(inode->stat.sst_mode)) {

	inode->fs=&service_dir_fs;

    } else {

	inode->fs=&service_nondir_fs;

    }

}

void set_service_fs(struct fuse_fs_s *fs)
{
    _set_service_fs(fs);
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
