/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include <linux/fuse.h>

#include "libosns-log.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"

#include "receive.h"
#include "request.h"
#include "defaults.h"
#include "handle.h"
#include "openfile.h"
#include "opendir.h"

extern void signal_fuse_request_interrupted(struct context_interface_s *interface, uint64_t unique);
extern int fuse_notify_VFS_delete(struct context_interface_s *interface, uint64_t pino, uint64_t ino, char *name, unsigned int len);
extern int fuse_reply_VFS_data(struct context_interface_s *i, uint64_t unique, char *data, unsigned int len);
extern int fuse_reply_VFS_error(struct context_interface_s *i, uint64_t unique, unsigned int errcode);

int reply_VFS_data(struct fuse_request_s *request, char *data, unsigned int size)
{
    return fuse_reply_VFS_data(request->interface, request->unique, data, size);
}

int reply_VFS_error(struct fuse_request_s *request, unsigned int errcode)
{
    return fuse_reply_VFS_error(request->interface, request->unique, errcode);
}

int notify_VFS_delete(struct context_interface_s *interface, uint64_t pino, uint64_t ino, char *name, unsigned int len)
{
    return fuse_notify_VFS_delete(interface, pino, ino, name, len);
}

static void _fuse_fs_cb_common(struct fuse_request_s *request, char *data, void (* cb)(struct service_context_s *c, struct fuse_request_s *r, struct inode_s *i, char *d))
{
    struct context_interface_s *interface=request->interface;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

    if (request->ino==FUSE_ROOT_ID) {

	(* cb)(context, request, &workspace->inodes.rootinode, data);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, request->ino);

	if (inode) {

	    (* cb)(context, request, inode, data);

	} else {

	    logoutput("_fuse_fs_cb_common: %li not found", request->ino);
	    reply_VFS_error(request, ENOENT);

	}

    }

}

static void fuse_fs_forget(struct fuse_request_s *request, char *data)
{
    struct fuse_forget_in *in=(struct fuse_forget_in *) data;
    struct context_interface_s *interface=request->interface;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

    /*
    logoutput("FORGET (thread %i): ino %lli forget %i", (int) gettid(), (long long) request->ino, in->nlookup);
    */

    queue_inode_2forget(workspace, request->ino, FORGET_INODE_FLAG_FORGET, in->nlookup);
}

static void fuse_fs_forget_multi(struct fuse_request_s *request, char *data)
{
    struct context_interface_s *interface=request->interface;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct fuse_batch_forget_in *in=(struct fuse_batch_forget_in *) data;
    struct fuse_forget_one *forgets=(struct fuse_forget_one *) (data + sizeof(struct fuse_batch_forget_in));
    unsigned int i=0;

    /* logoutput("FORGET_MULTI: (thread %i) count %i", (int) gettid(), batch_forget_in->count); */

    for (i=0; i<in->count; i++)
	queue_inode_2forget(workspace, forgets[i].nodeid, FORGET_INODE_FLAG_FORGET, forgets[i].nlookup);

}

static void _fuse_fs_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->type.dir.lookup)(context, request, inode, data, request->size - 1);
}

static void fuse_fs_lookup(struct fuse_request_s *request, char *data)
{
    logoutput("fuse_fs_lookup: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_lookup);
}

static void _fuse_fs_access(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_access_in *access_in=(struct fuse_access_in *) data;
    (* inode->fs->access)(context, request, inode, access_in->mask);
}

static void fuse_fs_access(struct fuse_request_s *request, char *data)
{
    logoutput("fuse_fs_access: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_access);
}

static void _fuse_fs_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_getattr_in *getattr_in=(struct fuse_getattr_in *) data;

    if ((getattr_in->getattr_flags & FUSE_GETATTR_FH) && getattr_in->fh>0) {
	struct fuse_openfile_s *openfile= (struct fuse_openfile_s *) getattr_in->fh;

	(* openfile->fgetattr) (openfile, request);

    } else {

	(* inode->fs->getattr)(context, request, inode);

    }

}

static void fuse_fs_getattr(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_getattr: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_getattr);
}

/*
    translate the attributes to set from fuse to a buffer sftp understands

    FUSE (20161123) :

    FATTR_MODE
    FATTR_UID
    FATTR_GID
    FATTR_SIZE
    FATTR_ATIME
    FATTR_MTIME
    FATTR_FH
    FATTR_ATIME_NOW
    FATTR_MTIME_NOW
    FATTR_LOCKOWNER
    FATTR_CTIME

    to

    SYSTEM STAT used by OSNS

    - size
    - uid
    - gid
    - type
    - permissions
    - access time
    - modify time
    - change time

    TODO:
    find out about lock owner

*/

static void set_stat_mask_from_fuse_setattr(struct fuse_setattr_in *attr, struct system_stat_s *stat)
{

    if (attr->valid & FATTR_MODE) {

	set_type_system_stat(stat, attr->mode);
	set_mode_system_stat(stat, attr->mode);

    }

    if (attr->valid & FATTR_SIZE) set_size_system_stat(stat, attr->size);
    if (attr->valid & FATTR_UID) set_uid_system_stat(stat, attr->uid);
    if (attr->valid & FATTR_GID) set_gid_system_stat(stat, attr->gid);

    if (attr->valid & FATTR_ATIME) {
	struct system_timespec_s time=SYSTEM_TIME_INIT;

	if (attr->valid & FATTR_ATIME_NOW) {

	    get_current_time_system_time(&time);

	} else {

	    set_system_time(&time, attr->atime, attr->atimensec);

	}

	set_atime_system_stat(stat, &time);

    }

    if (attr->valid & FATTR_MTIME) {
	struct system_timespec_s time=SYSTEM_TIME_INIT;

	if (attr->valid & FATTR_MTIME_NOW) {

	    get_current_time_system_time(&time);

	} else {

	    set_system_time(&time, attr->mtime, attr->mtimensec);

	}

	set_mtime_system_stat(stat, &time);

    }

    if (attr->valid & FATTR_CTIME) {
	struct system_timespec_s time=SYSTEM_TIME_INIT;

	set_system_time(&time, attr->ctime, attr->ctimensec);
	set_ctime_system_stat(stat, &time);

    }

}

static void _fuse_fs_setattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_setattr_in *setattr_in=(struct fuse_setattr_in *) data;
    struct system_stat_s stat;

    memset(&stat, 0, sizeof(struct system_stat_s));
    set_stat_mask_from_fuse_setattr(setattr_in, &stat);

    if (setattr_in->valid & FATTR_FH) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) setattr_in->fh;

	setattr_in->valid &= ~FATTR_FH;
	(* openfile->fsetattr) (openfile, request, &stat);

    } else {

	(* inode->fs->setattr)(context, request, inode, &stat);

    }

}

static void fuse_fs_setattr(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_setattr: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_setattr);
}

static void _fuse_fs_readlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->readlink)(context, request, inode);
}

static void fuse_fs_readlink(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_readlink: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_readlink);
}

static void _fuse_fs_mkdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_mkdir_in *mkdir_in=(struct fuse_mkdir_in *) data;
    char *name=(char *) (data + sizeof(struct fuse_mkdir_in));
    unsigned int len=strlen(name);

    (* inode->fs->type.dir.mkdir)(context, request, inode, name, len, mkdir_in->mode, mkdir_in->umask);
}

static void fuse_fs_mkdir(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_mkdir: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_mkdir);
}

static void _fuse_fs_mknod(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_mknod_in *mknod_in=(struct fuse_mknod_in *) data;
    char *name=(char *) (data + sizeof(struct fuse_mknod_in));
    unsigned int len=strlen(name);

    (* inode->fs->type.dir.mknod)(context, request, inode, name, len, mknod_in->mode, mknod_in->rdev, mknod_in->umask);
}

static void fuse_fs_mknod(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_mknod: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_mknod);
}

static void _fuse_fs_symlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    char *name=(char *) data;
    unsigned int len0=strlen(name);
    char *target=(char *) (data + len0);
    unsigned int len1=strlen(target);

    (* inode->fs->type.dir.symlink)(context, request, inode, name, len0, target, len1);
}

static void fuse_fs_symlink(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_symlink: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_symlink);
}

static void _fuse_fs_unlink(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->type.dir.unlink)(context, request, inode, data, strlen(data));
}

static void fuse_fs_unlink(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_unlink: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_unlink);
}

static void _fuse_fs_rmdir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->type.dir.rmdir)(context, request, inode, data, strlen(data));
}

static void fuse_fs_rmdir(struct fuse_request_s *request, char *data)
{

    logoutput("fuse_fs_rmdir: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_rmdir);
}

static void fuse_fs_rename(struct fuse_request_s *request, char *data)
{
    struct context_interface_s *interface=request->interface;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    uint64_t ino=request->ino;
    struct fuse_rename_in *rename_in=(struct fuse_rename_in *) data;
    char *oldname=(char *) (data + sizeof(struct fuse_rename_in));
    uint64_t newino=rename_in->newdir;
    char *newname=(char *) (data + sizeof(struct fuse_rename_in) + strlen(oldname));

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	if (newino==FUSE_ROOT_ID) {
	    struct inode_s *newinode=&workspace->inodes.rootinode;

	    (* inode->fs->type.dir.rename)(context, request, inode, oldname, newinode, newname, 0);

	} else {
	    struct inode_s *newinode=lookup_workspace_inode(workspace, newino);

	    if (newinode) {

		(* inode->fs->type.dir.rename)(context, request, inode, oldname, newinode, newname, 0);

	    } else {

		reply_VFS_error(request, ENOENT);

	    }

	}

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {

	    if (newino==FUSE_ROOT_ID) {
		struct inode_s *newinode=&workspace->inodes.rootinode;

		(* inode->fs->type.dir.rename)(context, request, inode, oldname, newinode, newname, 0);

	    } else {
		struct inode_s *newinode=lookup_workspace_inode(workspace, newino);

		if (newinode) {

		    (* inode->fs->type.dir.rename)(context, request, inode, oldname, newinode, newname, 0);

		} else {

		    reply_VFS_error(request, ENOENT);

		}

	    }

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

static void fuse_fs_link(struct fuse_request_s *request, char *data)
{
    reply_VFS_error(request, ENOSYS);
}

/*
    get information about a lock
    if this is the case it returns the same lock with type F_UNLCK
    used for posix locks
*/

static void fuse_fs_getlock(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;

    if (lk_in->fh>0) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;
	struct flock flock;

	flock.l_type=lk_in->lk.type;
	flock.l_whence=SEEK_SET;
	flock.l_start=lk_in->lk.start;
	flock.l_len=(lk_in->lk.end==OFFSET_MAX) ? 0 : lk_in->lk.end - lk_in->lk.start + 1;
	flock.l_pid=lk_in->lk.pid;

	(* openfile->getlock) (openfile, request, &flock);
	return;

    }

    reply_VFS_error(request, EIO);
}

static void _fuse_fs_flock_lock(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct fuse_lk_in *lk_in, unsigned char type)
{

    switch (lk_in->lk.type) {
	case F_RDLCK:
	    type|=LOCK_SH;
	    break;
	case F_WRLCK:
	    type|=LOCK_EX;
	    break;
	case F_UNLCK:
	    type|=LOCK_UN;
	    break;
    }

    (* openfile->flock) (openfile, request, type);

}

static void _fuse_fs_posix_lock(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct fuse_lk_in *lk_in, unsigned int flags)
{
    struct flock flock;

    flock.l_type=lk_in->lk.type;
    flock.l_whence=SEEK_SET;
    flock.l_start=lk_in->lk.start;
    flock.l_len=(lk_in->lk.end==OFFSET_MAX) ? 0 : lk_in->lk.end - lk_in->lk.start + 1;
    flock.l_pid=lk_in->lk.pid;

    (* openfile->setlock) (openfile, request, &flock, flags);

}

/*
    generic function to set a lock
    it's called to set a posix lock and to set a flock lock
    depending in the presence of FUSE_LK_FLOCK in the flags
    this function is used when both locks are used (set in the init phase)
*/

static void fuse_fs_lock(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;

    if (lk_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

	if (lk_in->lk_flags & FUSE_LK_FLOCK) {

	    _fuse_fs_flock_lock(openfile, request, lk_in, LOCK_NB);

	} else {

	    _fuse_fs_posix_lock(openfile, request, lk_in, 0);

	}

    } else {

	reply_VFS_error(request, EIO);

    }

}

/*
    generic function to set a lock and wait for a release
    it's called to set a posix lock and to set a flock lock
    depending in the presence of FUSE_LK_FLOCK in the flags
    this function is used when both locks are used (set in the init phase)
*/

static void fuse_fs_lock_wait(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;

    if (lk_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

	if (lk_in->lk_flags & FUSE_LK_FLOCK) {

	    _fuse_fs_flock_lock(openfile, request, lk_in, 0);

	} else {

	    _fuse_fs_posix_lock(openfile, request, lk_in, FUSE_OPENFILE_LOCK_FLAG_WAIT);

	}

    } else {

	reply_VFS_error(request, EIO);

    }

}

/*
    function to set a posix lock
    called when only posix locks are used
    (so every lock is a posix lock)
*/

static void fuse_fs_posix_lock(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;

    if (lk_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

	_fuse_fs_posix_lock(openfile, request, lk_in, 0);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_posix_lock_wait(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;

    if (lk_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

	_fuse_fs_posix_lock(openfile, request, lk_in, FUSE_OPENFILE_LOCK_FLAG_WAIT);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/*
    function to set a flock lock
    called when only flock locks are used
    (so every lock is a flock lock)
*/

static void fuse_fs_flock_lock(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;

    if (lk_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

	_fuse_fs_flock_lock(openfile, request, lk_in, LOCK_NB);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_flock_lock_wait(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

    if (openfile) {

	_fuse_fs_flock_lock(openfile, request, lk_in, FUSE_OPENFILE_LOCK_FLAG_WAIT);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_read(struct fuse_request_s *request, char *data)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) data;

    if (read_in->fh>0) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) read_in->fh;
	uint64_t lock_owner=(read_in->flags & FUSE_READ_LOCKOWNER) ? read_in->lock_owner : 0;

	read_in->flags &= ~FUSE_READ_LOCKOWNER;
	(* openfile->read)(openfile, request, read_in->size, read_in->offset, read_in->flags, lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_write(struct fuse_request_s *request, char *data)
{
    struct fuse_write_in *write_in=(struct fuse_write_in *) data;

    if (write_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) write_in->fh;
	char *buffer=(char *) (data + sizeof(struct fuse_write_in));
	uint64_t lock_owner=(write_in->flags & FUSE_WRITE_LOCKOWNER) ? write_in->lock_owner : 0;

	write_in->flags &= ~FUSE_WRITE_LOCKOWNER;
	(* openfile->write) (openfile, request, buffer, write_in->size, write_in->offset, write_in->flags, lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_flush(struct fuse_request_s *request, char *data)
{
    struct fuse_flush_in *flush_in=(struct fuse_flush_in *) data;

    if (flush_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) flush_in->fh;

	(* openfile->flush) (openfile, request, flush_in->lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_fsync(struct fuse_request_s *request, char *data)
{
    struct fuse_fsync_in *fsync_in=(struct fuse_fsync_in *) data;

    if (fsync_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) fsync_in->fh;

	(* openfile->fsync) (openfile, request, (fsync_in->fsync_flags & FUSE_FSYNC_FDATASYNC));

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_release(struct fuse_request_s *request, char *data)
{
    struct fuse_release_in *release_in=(struct fuse_release_in *) data;

    if (release_in->fh) {
        struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) release_in->fh;
	struct inode_s *inode=openfile->inode;
	uint64_t lock_owner=(release_in->release_flags & FUSE_RELEASE_FLOCK_UNLOCK) ? release_in->lock_owner : 0;

	release_in->release_flags &= ~FUSE_RELEASE_FLOCK_UNLOCK;
	(* openfile->release) (openfile, request, release_in->release_flags, lock_owner);

	free(openfile);
	openfile=NULL;

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_create(struct fuse_request_s *request, char *data)
{
    struct context_interface_s *interface=request->interface;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    uint64_t ino=request->ino;
    struct inode_s *inode=lookup_workspace_inode(workspace, ino);

    if (inode) {
	struct fuse_create_in *create_in=(struct fuse_create_in *) data;
	char *name=(char *) (data + sizeof(struct fuse_create_in));
	unsigned int len=request->size - sizeof(struct fuse_create_in);
	struct fuse_openfile_s *openfile=NULL;

	openfile=malloc(sizeof(struct fuse_openfile_s));

	if (openfile) {

	    init_fuse_openfile(openfile, context, inode);
	    openfile->flags |= FUSE_OPENFILE_FLAG_CREATE;
	    (* inode->fs->type.dir.create)(openfile, request, name, len, create_in->flags, create_in->mode, create_in->umask);

	    if (openfile->error>0) {

		/* subcall has send a reply to VFS already, here only free */

		free(openfile);
		openfile=NULL;

	    }

	} else {

	    reply_VFS_error(request, ENOMEM);

	}

    } else {

	reply_VFS_error(request, ENOENT);

    }

}

static void fuse_fs_open(struct fuse_request_s *request, char *data)
{
    struct context_interface_s *interface=request->interface;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    uint64_t ino=request->ino;
    struct inode_s *inode=lookup_workspace_inode(workspace, ino);

    if (inode) {
	struct fuse_open_in *open_in=(struct fuse_open_in *) data;
	struct fuse_openfile_s *openfile=NULL;

	openfile=malloc(sizeof(struct fuse_openfile_s));

	if (openfile) {

	    init_fuse_openfile(openfile, context, inode);
	    (* inode->fs->type.nondir.open)(openfile, request, open_in->flags & (O_ACCMODE | O_APPEND | O_TRUNC));

	    if (openfile->error>0) {

		/* subcall has send a reply to VFS already, here only free */

		free(openfile);
		openfile=NULL;

	    }

	} else {

	    reply_VFS_error(request, ENOMEM);

	}

    } else {

	reply_VFS_error(request, ENOENT);

    }

}

static void _fuse_fs_opendir(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_open_in *open_in=(struct fuse_open_in *) data;
    struct fuse_opendir_s *opendir=NULL;

    opendir=malloc(sizeof(struct fuse_opendir_s));

    if (opendir) {

	init_fuse_opendir(opendir, context, inode);

	logoutput("_fuse_fs_opendir: ino %li", get_ino_system_stat(&inode->stat));
	(* inode->fs->type.dir.opendir)(opendir, request, open_in->flags);

	if (opendir->error>0) {

	    /* subcall has send a reply to VFS already, here only free */

	    free(opendir);
	    opendir=NULL;

	}

    } else {

	reply_VFS_error(request, ENOMEM);

    }

}

static void fuse_fs_opendir(struct fuse_request_s *request, char *data)
{
    logoutput("fuse_fs_opendir: ino %li", request->ino);
    _fuse_fs_cb_common(request, data, _fuse_fs_opendir);
}

static void fuse_fs_readdir(struct fuse_request_s *request, char *data)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) data;

    logoutput("fuse_fs_readdir");

    if (read_in->fh) {
	struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) read_in->fh;

	(* opendir->readdir)(opendir, request, read_in->size, read_in->offset);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_readdirplus(struct fuse_request_s *request, char *data)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) data;

    logoutput("fuse_fs_readdirplus");

    if (read_in->fh) {
        struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) read_in->fh;

	opendir->flags |= FUSE_OPENDIR_FLAG_READDIRPLUS;
	(* opendir->readdir)(opendir, request, read_in->size, read_in->offset);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_releasedir(struct fuse_request_s *request, char *data)
{
    struct fuse_release_in *release_in=(struct fuse_release_in *) data;

    logoutput("fuse_fs_releasedir");

    if (release_in->fh) {
	struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) release_in->fh;
	struct shared_signal_s *signal=opendir->signal;
	unsigned char dofree=1;

	(* opendir->releasedir)(opendir, request);
	release_in->fh=0;

	signal_lock(signal);
	opendir->flags |= (FUSE_OPENDIR_FLAG_RELEASE | FUSE_OPENDIR_FLAG_FINISH | FUSE_OPENDIR_FLAG_QUEUE_READY);
	if (opendir->flags & FUSE_OPENDIR_FLAG_THREAD) dofree=0;
	signal_unlock(signal);

	if (dofree) {

	    free(opendir);
	    opendir=NULL;

	}

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_fsyncdir(struct fuse_request_s *request, char *data)
{
    struct fuse_fsync_in *fsync_in=(struct fuse_fsync_in *) data;

    logoutput("fuse_fs_fsyncdir");

    if (fsync_in->fh) {
	struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) fsync_in->fh;

	(* opendir->fsyncdir)(opendir, request, fsync_in->fsync_flags & 1);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void _fuse_fs_setxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_setxattr_in *setxattr_in=(struct fuse_setxattr_in *) data;
    char *name=(char *) ((char *) setxattr_in + sizeof(struct fuse_setxattr_in));
    char *value=(char *) (name + strlen(name) + 1);

    logoutput("_fuse_fs_setxattr: ino %li name %s", request->ino, name);
    (* inode->fs->setxattr)(context, request, inode, name, value, setxattr_in->size, setxattr_in->flags);
}

static void fuse_fs_setxattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_setxattr);
}

static void _fuse_fs_getxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) data;
    char *name=(char *) ((char *) getxattr_in + sizeof(struct fuse_getxattr_in));

    logoutput("_fuse_fs_getxattr: ino %li name %s", request->ino, name);
    (* inode->fs->getxattr)(context, request, inode, name, getxattr_in->size);
}

static void fuse_fs_getxattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_getxattr);
}

static void _fuse_fs_listxattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) data;

    logoutput("_fuse_fs_listxattr: ino %li", request->ino);
    (* inode->fs->listxattr)(context, request, inode, getxattr_in->size);
}

static void fuse_fs_listxattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_listxattr);
}

static void _fuse_fs_removexattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    logoutput("fuse_fs_removexattr: ino %li", request->ino);
    (* inode->fs->removexattr)(context, request, inode, data);
}

static void fuse_fs_removexattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_removexattr);
}

static void fuse_fs_statfs(struct fuse_request_s *request, char *data)
{
    struct context_interface_s *interface=request->interface;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    uint64_t ino=request->ino;

    logoutput("fuse_fs_statfs: ino %li", ino);

    if (ino==FUSE_ROOT_ID || ino==0) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	(* inode->fs->statfs)(context, request, inode);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {

	    (* inode->fs->statfs)(context, request, inode);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

static void fuse_fs_interrupt(struct fuse_request_s *request, char *data)
{
    struct fuse_interrupt_in *in=(struct fuse_interrupt_in *) data;
    logoutput("INTERRUPT (thread %i) unique %lu", (int) gettid(), in->unique);
    signal_fuse_request_interrupted(request->interface, in->unique);
}

static void fuse_fs_lseek(struct fuse_request_s *request, char *data)
{
    struct fuse_lseek_in *lseek_in=(struct fuse_lseek_in *) data;

    if (lseek_in->fh) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lseek_in->fh;

	(* openfile->lseek) (openfile, request, lseek_in->offset, lseek_in->whence);

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_destroy (struct fuse_request_s *request, char *data)
{
    logoutput("DESTROY (thread %i)", (int) gettid());
}

struct fuse_cb_mapping_s {
    unsigned int					code;
    void						(* cb)(struct fuse_request_s *request, char *data);
};

static struct fuse_cb_mapping_s mapping[] = {

    {.code=FUSE_DESTROY, .cb=fuse_fs_destroy},

    {.code=FUSE_LOOKUP, .cb=fuse_fs_lookup},
    {.code=FUSE_FORGET, .cb=fuse_fs_forget},

#ifdef FUSE_BATCH_FORGET
    {.code=FUSE_BATCH_FORGET, .cb=fuse_fs_forget_multi},
#endif

    {.code=FUSE_ACCESS, .cb=fuse_fs_access},

    {.code=FUSE_GETATTR, .cb=fuse_fs_getattr},
    {.code=FUSE_SETATTR, .cb=fuse_fs_setattr},

    {.code=FUSE_MKDIR, .cb=fuse_fs_mkdir},
    {.code=FUSE_MKNOD, .cb=fuse_fs_mknod},
    {.code=FUSE_SYMLINK, .cb=fuse_fs_symlink},

    {.code=FUSE_RMDIR, .cb=fuse_fs_rmdir},
    {.code=FUSE_UNLINK, .cb=fuse_fs_unlink},

    {.code=FUSE_READLINK, .cb=fuse_fs_readlink},
    {.code=FUSE_RENAME, .cb=fuse_fs_rename},
    {.code=FUSE_LINK, .cb=fuse_fs_link},

    {.code=FUSE_OPENDIR, .cb=fuse_fs_opendir},
    {.code=FUSE_READDIR, .cb=fuse_fs_readdir},
    {.code=FUSE_READDIRPLUS, .cb=fuse_fs_readdirplus},
    {.code=FUSE_RELEASEDIR, .cb=fuse_fs_releasedir},
    {.code=FUSE_FSYNCDIR, .cb=fuse_fs_fsyncdir},

    {.code=FUSE_CREATE, .cb=fuse_fs_create},
    {.code=FUSE_OPEN, .cb=fuse_fs_open},
    {.code=FUSE_READ, .cb=fuse_fs_read},
    {.code=FUSE_WRITE, .cb=fuse_fs_write},
    {.code=FUSE_FSYNC, .cb=fuse_fs_fsync},
    {.code=FUSE_FLUSH, .cb=fuse_fs_flush},
    {.code=FUSE_RELEASE, .cb=fuse_fs_release},

    {.code=FUSE_GETLK, .cb=fuse_fs_getlock},
    {.code=FUSE_SETLK, .cb=fuse_fs_lock},
    {.code=FUSE_SETLKW, .cb=fuse_fs_lock_wait},

    {.code=FUSE_STATFS, .cb=fuse_fs_statfs},

    {.code=FUSE_LISTXATTR, .cb=fuse_fs_listxattr},
    {.code=FUSE_GETXATTR, .cb=fuse_fs_getxattr},
    {.code=FUSE_SETXATTR, .cb=fuse_fs_setxattr},
    {.code=FUSE_REMOVEXATTR, .cb=fuse_fs_removexattr},

    {.code=FUSE_INTERRUPT, .cb=fuse_fs_interrupt},

#ifdef FUSE_LSEEK

    {.code=FUSE_LSEEK, .cb=fuse_fs_lseek},

#endif

    {.code=0, .cb=NULL},

};

void register_fuse_functions(struct context_interface_s *interface, void (* add)(struct context_interface_s *interface, unsigned int ctr, unsigned int code, void (* cb)(struct fuse_request_s *request, char *data)))
{
    unsigned int ctr=0;

    while (mapping[ctr].code>0) {

	add(interface, ctr, mapping[ctr].code, mapping[ctr].cb);
	ctr++;

    }

}
