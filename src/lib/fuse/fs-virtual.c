/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse.h"

#include "request.h"
#include "fs-common.h"
#include "openfile.h"
#include "opendir.h"

static struct fuse_fs_s virtual_dir_fs;
static struct fuse_fs_s virtual_nondir_fs;

static void _fs_forget(struct service_context_s *context, struct inode_s *inode)
{
}
static void _fs_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len)
{
    _fs_common_virtual_lookup(context, request, inode, name, len);
}

static void _fs_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    _fs_common_getattr(request, &inode->stat);
}

static void _fs_access(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, unsigned int mask)
{
    _fs_common_access(context, request, inode, mask);
}

static void _fs_setattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat)
{
    _fs_common_getattr(request, &inode->stat);
}

static void _fs_readlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    reply_VFS_error(request, EINVAL);
}

static void _fs_mkdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len, mode_t mode, mode_t umask)
{
    reply_VFS_error(request, EACCES);
}

static void _fs_mknod(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len, mode_t mode, dev_t rdev, mode_t umask)
{
    reply_VFS_error(request, EACCES);
}

static void _fs_symlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len0, const char *target, unsigned int len1)
{
    reply_VFS_error(request, EACCES);
}

static void _fs_rmdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len)
{
    reply_VFS_error(request, EACCES);
}

static void _fs_unlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len)
{
    reply_VFS_error(request, EACCES);
}

static void _fs_rename(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, struct inode_s *inode_new, const char *newname, unsigned int flags)
{
    reply_VFS_error(request, EACCES);
}

static void _fs_open(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags)
{
    openfile->error=EACCES;
    reply_VFS_error(request, EACCES);
}

static void _fs_create(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *name, unsigned int len, unsigned int flags, mode_t mode, mode_t mask)
{
    openfile->error=EACCES;
    reply_VFS_error(request, EACCES);
}

static void _fs_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags)
{
    _fs_common_virtual_opendir(opendir, request, flags);
}

static void _fs_setxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, const char *value, size_t size, int flags)
{
    reply_VFS_error(request, ENODATA);
}

static void _fs_getxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, size_t size)
{
    reply_VFS_error(request, ENODATA);
}

static void _fs_listxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, size_t size)
{
    reply_VFS_error(request, ENODATA);
}

static void _fs_removexattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name)
{
    reply_VFS_error(request, ENODATA);
}

static void _fs_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    _fs_common_statfs(context, request, inode);
}

void use_virtual_fs(struct service_context_s *context, struct inode_s *inode)
{

    logoutput_debug("use_virtual_fs");

    if (system_stat_test_ISDIR(&inode->stat)) {

	inode->fs=&virtual_dir_fs;

    } else {

	inode->fs=&virtual_nondir_fs;

    }

}

static void _set_virtual_fs(struct fuse_fs_s *fs)
{

    fs->forget=_fs_forget;
    fs->getattr=_fs_getattr;
    fs->setattr=_fs_setattr;
    fs->access=_fs_access;
    fs->readlink=_fs_readlink;

    if ((fs->flags & FS_SERVICE_FLAG_DIR) || (fs->flags & FS_SERVICE_FLAG_ROOT)) {

	fs->type.dir.use_fs=use_virtual_fs;
	fs->type.dir.lookup=_fs_lookup;

	fs->type.dir.create=_fs_create;
	fs->type.dir.mkdir=_fs_mkdir;
	fs->type.dir.mknod=_fs_mknod;
	fs->type.dir.symlink=_fs_symlink;

	fs->type.dir.unlink=_fs_unlink;
	fs->type.dir.rmdir=_fs_rmdir;
	fs->type.dir.rename=_fs_rename;

	fs->type.dir.opendir=_fs_opendir;

    } else {

	fs->type.nondir.open=_fs_open;

    }

    fs->setxattr=_fs_setxattr;
    fs->getxattr=_fs_getxattr;
    fs->listxattr=_fs_listxattr;
    fs->removexattr=_fs_removexattr;

    fs->statfs=_fs_statfs;

}

void init_virtual_fs()
{
    struct fuse_fs_s *fs=NULL;

    fs=&virtual_dir_fs;

    memset(fs, 0, sizeof(struct fuse_fs_s));
    fs->flags=FS_SERVICE_FLAG_DIR | FS_SERVICE_FLAG_VIRTUAL;
    _set_virtual_fs(fs);

    fs=&virtual_nondir_fs;

    memset(fs, 0, sizeof(struct fuse_fs_s));
    fs->flags=FS_SERVICE_FLAG_NONDIR | FS_SERVICE_FLAG_VIRTUAL;
    _set_virtual_fs(fs);

}

void set_virtual_fs(struct fuse_fs_s *fs)
{
    _set_virtual_fs(fs);
}
