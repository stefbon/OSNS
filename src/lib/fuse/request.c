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
#include "libosns-list.h"
#include "libosns-system.h"
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

#define FUSE_OPEN_HASHTABLE_SIZE			64
static struct list_header_s hashtable[FUSE_OPEN_HASHTABLE_SIZE];

static void add_fuse_open_hashtable(struct fuse_open_header_s *oh)
{
    unsigned int hashvalue=(((uint64_t) oh) % FUSE_OPEN_HASHTABLE_SIZE);
    struct list_header_s *h=&hashtable[hashvalue];

    logoutput_debug("add_fuse_open_hashtable: add fh %lu", (uint64_t) oh);

    write_lock_list_header(h);
    add_list_element_first(h, &oh->list);
    write_unlock_list_header(h);
}

static struct fuse_open_header_s *get_fuse_open_header(uint64_t fh)
{
    unsigned int hashvalue=(fh % FUSE_OPEN_HASHTABLE_SIZE);
    struct list_header_s *h=&hashtable[hashvalue];
    struct list_element_s *list=NULL;
    struct fuse_open_header_s *oh=NULL;

    read_lock_list_header(h);
    list=get_list_head(h);

    while (list) {

	oh=(struct fuse_open_header_s *)((char *) list - offsetof(struct fuse_open_header_s, list));

	if (((uint64_t) oh) == fh) break;
	list=get_next_element(list);

    }

    read_unlock_list_header(h);
    return oh;

}

static void remove_fuse_open_hashtable(struct fuse_open_header_s *oh)
{
    unsigned int hashvalue=(((uint64_t) oh) % FUSE_OPEN_HASHTABLE_SIZE);
    struct list_header_s *h=&hashtable[hashvalue];

    write_lock_list_header(h);
    remove_list_element(&oh->list);
    write_unlock_list_header(h);
}

static void oh_fgetattr_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request)
{
    reply_VFS_error(request, EIO);
}

static void oh_fsetattr_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request, struct system_stat_s *stat)
{
    reply_VFS_error(request, EIO);
}

static void oh_flush_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request, uint64_t lo)
{
    reply_VFS_error(request, 0);
}

static void oh_fsync_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request, unsigned int flags)
{
    reply_VFS_error(request, 0);
}

static void oh_release_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request, unsigned int flags, uint64_t lo)
{
    reply_VFS_error(request, 0);
}

static void oh_getlock_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request, struct flock *flock)
{
    reply_VFS_error(request, ENOSYS);
}

static void oh_setlock_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request, struct flock *flock, uint64_t owner, unsigned int flags)
{
    reply_VFS_error(request, ENOSYS);
}

static void oh_flock_noop(struct fuse_open_header_s *oh, struct fuse_request_s *request, uint64_t owner, unsigned char type)
{
    reply_VFS_error(request, ENOSYS);
}

void init_fuse_open_header(struct fuse_open_header_s *oh, struct service_context_s *ctx, struct inode_s *inode)
{
    oh->ctx=ctx;
    oh->inode=inode;
    oh->type=0;

    oh->fgetattr=oh_fgetattr_noop;
    oh->fsetattr=oh_fsetattr_noop;
    oh->flush=oh_flush_noop;
    oh->fsync=oh_fsync_noop;
    oh->release=oh_release_noop;
    oh->getlock=oh_getlock_noop;
    oh->setlock=oh_setlock_noop;
    oh->flock=oh_flock_noop;

    init_list_element(&oh->list, NULL);
    oh->handle=NULL;
}


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

/* shared FUSE function to do a cb on an inode */

static void _fuse_fs_cb_common(struct fuse_request_s *request, char *data, void (* cb)(struct service_context_s *c, struct fuse_request_s *r, struct inode_s *i, char *d))
{
    struct service_context_s *ctx=get_service_context(request->interface);

    if (request->ino==FUSE_ROOT_ID) {

	(* cb)(ctx, request, &ctx->service.workspace.rootinode, data);

    } else {
	struct inode_s *inode=lookup_workspace_inode(request->ino);

	if (inode) {

	    (* cb)(ctx, request, inode, data);

	} else {

	    logoutput("_fuse_fs_cb_common: %li not found", request->ino);
	    reply_VFS_error(request, ENOENT);

	}

    }

}

static void fuse_fs_forget(struct fuse_request_s *request, char *data)
{
    struct fuse_forget_in *in=(struct fuse_forget_in *) data;
    struct service_context_s *ctx=get_service_context(request->interface);

    /*
    logoutput("FORGET (thread %i): ino %lli forget %i", (int) gettid(), (long long) request->ino, in->nlookup);
    */

    queue_inode_2forget(ctx, request->ino, FORGET_INODE_FLAG_FORGET, in->nlookup);
}

static void fuse_fs_forget_multi(struct fuse_request_s *request, char *data)
{
    struct service_context_s *ctx=get_service_context(request->interface);
    struct fuse_batch_forget_in *in=(struct fuse_batch_forget_in *) data;
    struct fuse_forget_one *forgets=(struct fuse_forget_one *) (data + sizeof(struct fuse_batch_forget_in));
    unsigned int i=0;

    /* logoutput("FORGET_MULTI: (thread %i) count %i", (int) gettid(), batch_forget_in->count); */

    for (i=0; i<in->count; i++)
	queue_inode_2forget(ctx, forgets[i].nodeid, FORGET_INODE_FLAG_FORGET, forgets[i].nlookup);

}

static void _fuse_fs_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->type.dir.lookup)(context, request, inode, data, strlen(data));
}

static void fuse_fs_lookup(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_lookup);
}

static void _fuse_fs_access(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_access_in *access_in=(struct fuse_access_in *) data;
    (* inode->fs->access)(ctx, request, inode, access_in->mask);
}

static void fuse_fs_access(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_access);
}

static void _fuse_fs_getattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->getattr)(ctx, request, inode);
}

static void fuse_fs_getattr(struct fuse_request_s *request, char *data)
{
    struct fuse_getattr_in *getattr_in=(struct fuse_getattr_in *) data;

    if ((getattr_in->getattr_flags & FUSE_GETATTR_FH) && getattr_in->fh>0) {
	struct fuse_open_header_s *oh=get_fuse_open_header(getattr_in->fh);

	if (oh && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {

	    (* oh->fgetattr)(oh, request);

	} else {

	    reply_VFS_error(request, EIO);

	}

    } else {

	_fuse_fs_cb_common(request, NULL, _fuse_fs_getattr);

    }

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

/* SETATTR */

static void _fuse_fs_setattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->setattr)(ctx, request, inode, (struct system_stat_s *) data);
}

static void fuse_fs_setattr(struct fuse_request_s *request, char *data)
{
    struct fuse_setattr_in *setattr_in=(struct fuse_setattr_in *) data;
    struct system_stat_s stat2set;

    memset(&stat2set, 0, sizeof(struct system_stat_s));
    set_stat_mask_from_fuse_setattr(setattr_in, &stat2set);

    if (setattr_in->valid & FATTR_FH) {
	struct fuse_open_header_s *oh=get_fuse_open_header(setattr_in->fh); /* can be a file or a directory */

	if (oh && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {

	    (* oh->fsetattr)(oh, request, &stat2set);

	} else {

	    reply_VFS_error(request, EIO);

	}

    } else {

	_fuse_fs_cb_common(request, (void *) &stat2set, _fuse_fs_setattr);

    }

}

/* READLINK */

static void _fuse_fs_readlink(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->readlink)(ctx, request, inode);
}

static void fuse_fs_readlink(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_readlink);
}

/* MKDIR */

static void _fuse_fs_mkdir(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_mkdir_in *mkdir_in=(struct fuse_mkdir_in *) data;
    char *name=(char *) (data + sizeof(struct fuse_mkdir_in));
    unsigned int len=strlen(name);

    (* inode->fs->type.dir.mkdir)(ctx, request, inode, name, len, mkdir_in->mode, mkdir_in->umask);
}

static void fuse_fs_mkdir(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_mkdir);
}

/* MKNOD */

static void _fuse_fs_mknod(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_mknod_in *mknod_in=(struct fuse_mknod_in *) data;
    char *name=(char *) (data + sizeof(struct fuse_mknod_in));
    unsigned int len=strlen(name);

    (* inode->fs->type.dir.mknod)(ctx, request, inode, name, len, mknod_in->mode, mknod_in->rdev, mknod_in->umask);
}

static void fuse_fs_mknod(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_mknod);
}

/* SYMLINK */

static void _fuse_fs_symlink(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    char *name=(char *) data;
    unsigned int len0=strlen(name);
    char *target=(char *) (data + len0);
    unsigned int len1=strlen(target);

    (* inode->fs->type.dir.symlink)(ctx, request, inode, name, len0, target, len1);
}

static void fuse_fs_symlink(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_symlink);
}

/* UNLINK */

static void _fuse_fs_unlink(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->type.dir.unlink)(ctx, request, inode, data, strlen(data));
}

static void fuse_fs_unlink(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_unlink);
}

/* RMDIR */

static void _fuse_fs_rmdir(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->type.dir.rmdir)(ctx, request, inode, data, strlen(data));
}

static void fuse_fs_rmdir(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_rmdir);
}

/* RENAME */

static void fuse_fs_rename(struct fuse_request_s *request, char *data)
{
    struct service_context_s *ctx=get_service_context(request->interface);
    uint64_t ino=request->ino;
    struct fuse_rename_in *rename_in=(struct fuse_rename_in *) data;
    char *oldname=(char *) (data + sizeof(struct fuse_rename_in));
    uint64_t newino=rename_in->newdir;
    char *newname=(char *) (data + sizeof(struct fuse_rename_in) + strlen(oldname));

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&ctx->service.workspace.rootinode;

	if (newino==FUSE_ROOT_ID) {
	    struct inode_s *newinode=&ctx->service.workspace.rootinode;

	    (* inode->fs->type.dir.rename)(ctx, request, inode, oldname, newinode, newname, 0);

	} else {
	    struct inode_s *newinode=lookup_workspace_inode(newino);

	    if (newinode) {

		(* inode->fs->type.dir.rename)(ctx, request, inode, oldname, newinode, newname, 0);

	    } else {

		reply_VFS_error(request, ENOENT);

	    }

	}

    } else {
	struct inode_s *inode=lookup_workspace_inode(ino);

	if (inode) {

	    if (newino==FUSE_ROOT_ID) {
		struct inode_s *newinode=&ctx->service.workspace.rootinode;

		(* inode->fs->type.dir.rename)(ctx, request, inode, oldname, newinode, newname, 0);

	    } else {
		struct inode_s *newinode=lookup_workspace_inode(newino);

		if (newinode) {

		    (* inode->fs->type.dir.rename)(ctx, request, inode, oldname, newinode, newname, 0);

		} else {

		    reply_VFS_error(request, ENOENT);

		}

	    }

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

/* (HARD) LINK (not supported) */

static void fuse_fs_link(struct fuse_request_s *request, char *data)
{
    reply_VFS_error(request, ENOSYS);
}

static void convert_fuse_lock2flock(struct fuse_file_lock *lock, struct flock *flock)
{
    flock->l_type=lock->type;
    flock->l_whence=SEEK_SET;
    flock->l_start=lock->start;
    flock->l_len=(lock->end==OFFSET_MAX) ? 0 : lock->end - lock->start + 1;
    flock->l_pid=lock->pid;
}

void convert_flock2fuse_lock(struct flock *flock, struct fuse_file_lock *lock)
{
    lock->type=flock->l_type;
    lock->start=flock->l_start;
    lock->end=((flock->l_len==0) ? OFFSET_MAX : (lock->start + flock->l_len - 1));
    lock->pid=flock->l_pid;
}

/* LOCKS */

static void fuse_fs_getlock(struct fuse_request_s *request, char *data)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(lk_in->fh);

    if (oh && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	struct flock flock;

	convert_fuse_lock2flock(&lk_in->lk, &flock);
	(* oh->getlock) (oh, request, &flock);
	return;

    }

    reply_VFS_error(request, EIO);
}

/*
    generic function to set a lock
    it's called to set a posix lock and to set a flock lock
    depending in the presence of FUSE_LK_FLOCK in the flags
    this function is used when both locks are used (set in the init phase)
*/

static void _fuse_fs_setlock(struct fuse_request_s *request, char *data, unsigned char wait)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(lk_in->fh);

    if (oh && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {

	if (lk_in->lk_flags & FUSE_LK_FLOCK) {
	    unsigned char type=((wait) ? 0 : LOCK_NB);

	    /* dealing with a BSD stype file lock */

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

	    (* oh->flock) (oh, request, lk_in->owner, type);

	} else {
	    unsigned int flags=((wait) ? FUSE_OPEN_LOCK_FLAG_WAIT : 0);
	    struct flock flock;

	    convert_fuse_lock2flock(&lk_in->lk, &flock);
	    (* oh->setlock)(oh, request, &flock, lk_in->owner, flags);

	}

    } else {

	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_setlock(struct fuse_request_s *request, char *data)
{
    _fuse_fs_setlock(request, data, 0);
}

static void fuse_fs_setlockw(struct fuse_request_s *request, char *data)
{
    _fuse_fs_setlock(request, data, 1);
}

/* READ (from a file) */

static void fuse_fs_read(struct fuse_request_s *request, char *data)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(read_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_FILE) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) oh;
	uint64_t lock_owner=(read_in->flags & FUSE_READ_LOCKOWNER) ? read_in->lock_owner : 0;

	read_in->flags &= ~FUSE_READ_LOCKOWNER;
	(* openfile->read)(openfile, request, read_in->size, read_in->offset, read_in->flags, lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/* WRITE (to a file) */

static void fuse_fs_write(struct fuse_request_s *request, char *data)
{
    struct fuse_write_in *write_in=(struct fuse_write_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(write_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_FILE) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) oh;
	char *buffer=(char *) (data + sizeof(struct fuse_write_in));
	uint64_t lock_owner=(write_in->flags & FUSE_WRITE_LOCKOWNER) ? write_in->lock_owner : 0;

	write_in->flags &= ~FUSE_WRITE_LOCKOWNER;
	(* openfile->write) (openfile, request, buffer, write_in->size, write_in->offset, write_in->flags, lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/* FLUSH (a file) */

static void fuse_fs_flush(struct fuse_request_s *request, char *data)
{
    struct fuse_flush_in *flush_in=(struct fuse_flush_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(flush_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_FILE) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {

	(* oh->flush)(oh, request, flush_in->lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/* FSYNC (a file) */

static void fuse_fs_fsync(struct fuse_request_s *request, char *data)
{
    struct fuse_fsync_in *fsync_in=(struct fuse_fsync_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(fsync_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_FILE) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	unsigned int flags=0;

	if (fsync_in->fsync_flags & FUSE_FSYNC_FDATASYNC) flags |= FUSE_OPEN_FSYNC_DATASYNC;
	(* oh->fsync)(oh, request, flags);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/* RELEASE (a file handle) */

static void fuse_fs_release(struct fuse_request_s *request, char *data)
{
    struct fuse_release_in *release_in=(struct fuse_release_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(release_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_FILE) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	struct inode_s *inode=oh->inode;
	uint64_t lock_owner=(release_in->release_flags & FUSE_RELEASE_FLOCK_UNLOCK) ? release_in->lock_owner : 0;
	unsigned int flags=0;

	if (release_in->flags & FUSE_RELEASE_FLUSH) flags |= FUSE_OPEN_RELEASE_FLUSH;
	if (release_in->flags & FUSE_RELEASE_FLOCK_UNLOCK) flags |= FUSE_OPEN_RELEASE_FLOCK_UNLOCK;
	(* oh->release)(oh, request, flags, lock_owner);

	remove_fuse_open_hashtable(oh);
	free((void *) oh);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/* CREATE (a file) */

static void fuse_fs_create(struct fuse_request_s *request, char *data)
{
    struct service_context_s *context=get_service_context(request->interface);
    struct inode_s *inode=lookup_workspace_inode(request->ino);

    if (inode) {
	struct fuse_create_in *create_in=(struct fuse_create_in *) data;
	char *name=(char *) (data + sizeof(struct fuse_create_in));
	unsigned int len=request->size - sizeof(struct fuse_create_in);
	struct fuse_openfile_s *openfile=NULL;

	openfile=malloc(sizeof(struct fuse_openfile_s));

	if (openfile) {

	    init_fuse_openfile(openfile, context, inode);
	    create_in->flags |= O_CREAT;
	    (* inode->fs->type.dir.create)(openfile, request, name, len, create_in->flags, create_in->mode, create_in->umask);

	    if (openfile->error==0) {

		add_fuse_open_hashtable(&openfile->header);

	    } else {

		/* subcall has send a reply to VFS already, here only free */

		free(openfile);

	    }

	} else {

	    reply_VFS_error(request, ENOMEM);

	}

    } else {

	reply_VFS_error(request, ENOENT);

    }

}

/* OPEN (a file) */

static void _fuse_fs_open(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_open_in *open_in=(struct fuse_open_in *) data;
    struct fuse_openfile_s *openfile=NULL;

    openfile=malloc(sizeof(struct fuse_openfile_s));

    if (openfile) {

	init_fuse_openfile(openfile, ctx, inode);
	(* inode->fs->type.nondir.open)(openfile, request, (open_in->flags & (O_ACCMODE | O_APPEND | O_TRUNC)));

	if (openfile->error==0) {

	    add_fuse_open_hashtable(&openfile->header);

	} else {

	    /* subcall has send a reply to VFS already, here only free */
	    free(openfile);

	}

    } else {

	reply_VFS_error(request, ENOMEM);

    }

}

static void fuse_fs_open(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_open);
}

/* OPEN (a directory) */

static void _fuse_fs_opendir(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_open_in *open_in=(struct fuse_open_in *) data;
    struct fuse_opendir_s *opendir=NULL;

    opendir=malloc(sizeof(struct fuse_opendir_s));

    if (opendir) {

	init_fuse_opendir(opendir, ctx, inode);
	add_fuse_open_hashtable(&opendir->header);
	(* inode->fs->type.dir.opendir)(opendir, request, open_in->flags);

	if (opendir->error>0) {

	    /* subcall has send a reply to VFS already, here only free */
	    remove_fuse_open_hashtable(&opendir->header);
	    clear_opendir(opendir);
	    free(opendir);
	    opendir=NULL;

	}

    } else {

	reply_VFS_error(request, ENOMEM);

    }

}

static void fuse_fs_opendir(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_opendir);
}

/* READ (a directory) */

static void _fuse_fs_readdir(struct fuse_request_s *request, char *data, unsigned int flag)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(read_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_DIR) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) oh; /* fuse_open_header_s is first struct in fuse_opendir_s*/

	logoutput_debug("fuse_fs_readdir: ino %li", request->ino);

	opendir->flags |= flag;
	(* opendir->readdir)(opendir, request, read_in->size, read_in->offset);

    } else {

	logoutput_debug("fuse_fs_readdir: ino %li open header not found (fh=%lu)", request->ino, read_in->fh);
	reply_VFS_error(request, EIO);

    }

}

static void fuse_fs_readdir(struct fuse_request_s *request, char *data)
{
    _fuse_fs_readdir(request, data, 0);
}

static void fuse_fs_readdirplus(struct fuse_request_s *request, char *data)
{
    _fuse_fs_readdir(request, data, FUSE_OPENDIR_FLAG_READDIRPLUS);
}

/* RELEASE (a directory) */

static void fuse_fs_releasedir(struct fuse_request_s *request, char *data)
{
    struct fuse_release_in *release_in=(struct fuse_release_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(release_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_DIR) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) oh;
	unsigned int flags=0;

	logoutput_debug("fuse_fs_releasedir: ino %li", request->ino);

	if (release_in->flags & FUSE_RELEASE_FLUSH) flags |= FUSE_OPEN_RELEASE_FLUSH;
	if (release_in->flags & FUSE_RELEASE_FLOCK_UNLOCK) flags |= FUSE_OPEN_RELEASE_FLOCK_UNLOCK;
	(* oh->release)(oh, request, flags, release_in->lock_owner);

	remove_fuse_open_hashtable(oh);
	clear_opendir(opendir);
	free(opendir);

    } else {

	logoutput_debug("fuse_fs_releasedir: ino %li open header not found (fh=%lu)", request->ino, release_in->fh);
	reply_VFS_error(request, EIO);

    }

}

/* FSYNC (a directory) */

static void fuse_fs_fsyncdir(struct fuse_request_s *request, char *data)
{
    struct fuse_fsync_in *fsync_in=(struct fuse_fsync_in *) data;
    struct fuse_open_header_s *oh=get_fuse_open_header(fsync_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_DIR) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	unsigned int flags=0;

	if (fsync_in->fsync_flags & FUSE_FSYNC_FDATASYNC) flags |= FUSE_OPEN_FSYNC_DATASYNC;
	(* oh->fsync)(oh, request, flags);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/* XATTR */

static void _fuse_fs_setxattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_setxattr_in *setxattr_in=(struct fuse_setxattr_in *) data;
    char *name=(char *) ((char *) setxattr_in + sizeof(struct fuse_setxattr_in));
    char *value=(char *) (name + strlen(name) + 1);

    (* inode->fs->setxattr)(ctx, request, inode, name, value, setxattr_in->size, setxattr_in->flags);
}

static void fuse_fs_setxattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_setxattr);
}

static void _fuse_fs_getxattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) data;
    char *name=(char *) ((char *) getxattr_in + sizeof(struct fuse_getxattr_in));

    (* inode->fs->getxattr)(ctx, request, inode, name, getxattr_in->size);
}

static void fuse_fs_getxattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_getxattr);
}

static void _fuse_fs_listxattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) data;
    (* inode->fs->listxattr)(ctx, request, inode, getxattr_in->size);
}

static void fuse_fs_listxattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_listxattr);
}

static void _fuse_fs_removexattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, char *data)
{
    (* inode->fs->removexattr)(ctx, request, inode, data);
}

static void fuse_fs_removexattr(struct fuse_request_s *request, char *data)
{
    _fuse_fs_cb_common(request, data, _fuse_fs_removexattr);
}

static void fuse_fs_statfs(struct fuse_request_s *request, char *data)
{
    struct service_context_s *ctx=get_service_context(request->interface);
    uint64_t ino=request->ino;

    logoutput("fuse_fs_statfs: ino %li", ino);

    if (ino==FUSE_ROOT_ID || ino==0) {
	struct inode_s *inode=&ctx->service.workspace.rootinode;

	(* inode->fs->statfs)(ctx, request, inode);

    } else {
	struct inode_s *inode=lookup_workspace_inode(ino);

	if (inode) {

	    (* inode->fs->statfs)(ctx, request, inode);

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
    struct fuse_open_header_s *oh=get_fuse_open_header(lseek_in->fh);

    if (oh && (oh->type==FUSE_OPEN_TYPE_FILE) && (get_ino_system_stat(&oh->inode->stat)==request->ino)) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) oh;

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
    {.code=FUSE_SETLK, .cb=fuse_fs_setlock},
    {.code=FUSE_SETLKW, .cb=fuse_fs_setlockw},

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

void init_fuse_open_hashtable()
{
    for (unsigned int i=0; i<FUSE_OPEN_HASHTABLE_SIZE; i++) init_list_header(&hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
}
