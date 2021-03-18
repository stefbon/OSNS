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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#define OFFSET_MAX 0x7fffffffffffffffLL

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#define LOGGING
#include "log.h"
#include "misc.h"
#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

extern int check_entry_special(struct inode_s *inode);
extern int get_fuse_direntry_custom(struct fuse_opendir_s *opendir, struct fuse_request_s *request, struct name_s *xname, struct stat *st);

void copy_fuse_fs(struct fuse_fs_s *to, struct fuse_fs_s *from)
{
    memset(to, 0, sizeof(struct fuse_fs_s));
    memcpy(to, from, sizeof(struct fuse_fs_s));
}

void fs_inode_forget(struct inode_s *inode)
{
    (* inode->fs->forget)(inode);
}

int fs_lock_datalink(struct inode_s *inode)
{
    return (* inode->fs->lock_datalink)(inode);
}

int fs_unlock_datalink(struct inode_s *inode)
{
    return (* inode->fs->unlock_datalink)(inode);
}

void fs_get_inode_link(struct inode_s *inode, struct inode_link_s **link)
{
    (* inode->fs->get_inode_link)(inode, link);
}

struct context_interface_s *get_fuse_request_interface(struct fuse_request_s *request)
{
    return (struct context_interface_s *)( request->ctx);
}

void fuse_fs_forget(struct fuse_request_s *request)
{
    struct fuse_forget_in *in=(struct fuse_forget_in *) request->buffer;
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);

    // logoutput("FORGET (thread %i): ino %lli forget %i", (int) gettid(), (long long) request->ino, in->nlookup);

    queue_inode_2forget(context->workspace, request->ino, FORGET_INODE_FLAG_FORGET, in->nlookup);
}

void fuse_fs_forget_multi(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct fuse_batch_forget_in *in=(struct fuse_batch_forget_in *)request->buffer;
    struct fuse_forget_one *forgets=(struct fuse_forget_one *) (request->buffer + sizeof(struct fuse_batch_forget_in));
    unsigned int i=0;

    // logoutput("FORGET_MULTI: (thread %i) count %i", (int) gettid(), batch_forget_in->count);

    for (i=0; i<in->count; i++)
	queue_inode_2forget(context->workspace, forgets[i].nodeid, FORGET_INODE_FLAG_FORGET, forgets[i].nlookup);

}

void fuse_fs_lookup(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    char *name=(char *) request->buffer;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;

    logoutput("fuse_fs_lookup");

    if (request->ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	(* inode->fs->type.dir.lookup)(context, request, inode, name, request->size - 1);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, request->ino);

	if (inode) {

	    (* inode->fs->type.dir.lookup)(context, request, inode, name, request->size - 1);

	} else {

	    logoutput("fuse_fs_lookup: %li not found", request->ino);
	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_getattr(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct fuse_getattr_in *getattr_in=(struct fuse_getattr_in *) request->buffer;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;

    logoutput("fuse_fs_getattr: ino %li", request->ino);

    if (request->ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	if ((getattr_in->getattr_flags & FUSE_GETATTR_FH) && getattr_in->fh>0) {
	    struct fuse_openfile_s *openfile= (struct fuse_openfile_s *) getattr_in->fh;

	    // logoutput("fuse_fs_getattr: fgetattr");

	    (* inode->fs->type.nondir.fgetattr) (openfile, request);

	} else {

	    // logoutput("fuse_fs_getattr: getattr");

	    (* inode->fs->getattr)(context, request, inode);

	}

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, request->ino);

	if (inode) {

	    if ((getattr_in->getattr_flags & FUSE_GETATTR_FH) && getattr_in->fh>0) {
		struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) getattr_in->fh;

		// logoutput("fuse_fs_getattr: fgetattr");

		(* inode->fs->type.nondir.fgetattr) (openfile, request);

	    } else {

		// logoutput("fuse_fs_getattr: getattr");

		(* inode->fs->getattr)(context, request, inode);

	    }

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

static void fill_stat_attr(struct fuse_setattr_in *attr, struct stat *st)
{
    st->st_mode=attr->mode;
    st->st_uid=attr->uid;
    st->st_gid=attr->gid;
    st->st_size=attr->size;
    st->st_atim.tv_sec=attr->atime;
    st->st_atim.tv_nsec=attr->atimensec;
    st->st_mtim.tv_sec=attr->mtime;
    st->st_mtim.tv_nsec=attr->mtimensec;
    st->st_ctim.tv_sec=attr->ctime;
    st->st_ctim.tv_nsec=attr->ctimensec;
}

void fuse_fs_setattr(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct fuse_setattr_in *setattr_in=(struct fuse_setattr_in *) request->buffer;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;
	struct stat st;

	if (setattr_in->valid & FATTR_FH) {
	    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) setattr_in->fh;

	    setattr_in->valid -= FATTR_FH;
	    fill_stat_attr(setattr_in, &st);
	    (* inode->fs->type.nondir.fsetattr) (openfile, request, &st, setattr_in->valid);

	} else {

	    fill_stat_attr(setattr_in, &st);
	    (* inode->fs->setattr)(context, request, inode, &st, setattr_in->valid);

	}

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {
	    struct stat st;

	    if (setattr_in->valid & FATTR_FH) {
		struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) setattr_in->fh;

		setattr_in->valid -= FATTR_FH;
		fill_stat_attr(setattr_in, &st);
		(* inode->fs->type.nondir.fsetattr) (openfile, request, &st, setattr_in->valid);

	    } else {

		fill_stat_attr(setattr_in, &st);
		(* inode->fs->setattr)(context, request, inode, &st, setattr_in->valid);

	    }

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_readlink(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    struct inode_s *inode=lookup_workspace_inode(workspace, request->ino);

    if (inode) {

	(* inode->fs->type.nondir.readlink)(context, request, inode);

    } else {

	reply_VFS_error(request, ENOENT);

    }

}

void fuse_fs_mkdir(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    struct fuse_mkdir_in *mkdir_in=(struct fuse_mkdir_in *)request->buffer;
    char *name=(char *) (request->buffer + sizeof(struct fuse_mkdir_in));
    unsigned int len=strlen(name);

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	(* inode->fs->type.dir.mkdir)(context, request, inode, name, len, mkdir_in->mode, mkdir_in->umask);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {

	    (* inode->fs->type.dir.mkdir)(context, request, inode, name, len, mkdir_in->mode, mkdir_in->umask);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_mknod(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    struct fuse_mknod_in *mknod_in=(struct fuse_mknod_in *) request->buffer;
    char *name=(char *) (request->buffer + sizeof(struct fuse_mknod_in));
    unsigned int len=strlen(name);

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	(* inode->fs->type.dir.mknod)(context, request, inode, name, len, mknod_in->mode, mknod_in->rdev, mknod_in->umask);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {

	    (* inode->fs->type.dir.mknod)(context, request, inode, name, len, mknod_in->mode, mknod_in->rdev, mknod_in->umask);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_symlink(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    char *name=(char *) request->buffer;
    unsigned int len0=strlen(name);
    char *target=(char *) (request->buffer + len0);
    unsigned int len1=strlen(target);

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;

	(* inode->fs->type.dir.symlink)(context, request, inode, name, len0, target, len1);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {

	    (* inode->fs->type.dir.symlink)(context, request, inode, name, len0, target, len1);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_unlink(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    char *name=(char *) request->buffer;
    struct inode_s *inode=lookup_workspace_inode(workspace, ino);

    if (inode) {

	(* inode->fs->type.dir.unlink)(context, request, inode, name, strlen(name));

    } else {

	reply_VFS_error(request, ENOENT);

    }

}

void fuse_fs_rmdir(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    char *name=(char *) request->buffer;
    struct inode_s *inode=lookup_workspace_inode(workspace, ino);

    if (inode) {

	(* inode->fs->type.dir.rmdir)(context, request, inode, name, strlen(name));

    } else {

	reply_VFS_error(request, (ino==FUSE_ROOT_ID) ? EACCES : ENOENT);

    }

}

void fuse_fs_rename(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    struct fuse_rename_in *rename_in=(struct fuse_rename_in *) request->buffer;
    char *oldname=(char *) (request->buffer + sizeof(struct fuse_rename_in));
    uint64_t newino=rename_in->newdir;
    char *newname=(char *) (request->buffer + sizeof(struct fuse_rename_in) + strlen(oldname));

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

void fuse_fs_link(struct fuse_request_s *request)
{
    reply_VFS_error(request, ENOSYS);
}

void fuse_fs_open(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    struct inode_s *inode=lookup_workspace_inode(workspace, ino);

    if (inode) {
	struct fuse_open_in *open_in=(struct fuse_open_in *) request->buffer;
	struct fuse_openfile_s *openfile=NULL;

	openfile=malloc(sizeof(struct fuse_openfile_s));

	if (openfile) {

	    memset(openfile, 0, sizeof(struct fuse_openfile_s));

	    openfile->context=context;
	    openfile->inode=inode;
	    openfile->error=0;
	    openfile->flock=0;

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

void fuse_fs_read(struct fuse_request_s *request)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) read_in->fh;

    if (openfile) {
	struct inode_s *inode=openfile->inode;
	uint64_t lock_owner=(read_in->flags & FUSE_READ_LOCKOWNER) ? read_in->lock_owner : 0;

	(* inode->fs->type.nondir.read) (openfile, request, read_in->size, read_in->offset, read_in->flags, lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_write(struct fuse_request_s *request)
{
    struct fuse_write_in *write_in=(struct fuse_write_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) write_in->fh;

    if (openfile) {
	struct inode_s *inode=openfile->inode;
	char *buffer=(char *) (request->buffer + sizeof(struct fuse_write_in));
	uint64_t lock_owner=(write_in->flags & FUSE_WRITE_LOCKOWNER) ? write_in->lock_owner : 0;

	(* inode->fs->type.nondir.write) (openfile, request, buffer, write_in->size, write_in->offset, write_in->flags, lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_flush(struct fuse_request_s *request)
{
    struct fuse_flush_in *flush_in=(struct fuse_flush_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) flush_in->fh;

    if (openfile) {
	struct inode_s *inode=openfile->inode;

	(* inode->fs->type.nondir.flush) (openfile, request, flush_in->lock_owner);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_fsync(struct fuse_request_s *request)
{
    struct fuse_fsync_in *fsync_in=(struct fuse_fsync_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) fsync_in->fh;

    if (openfile) {
	struct inode_s *inode=openfile->inode;

	(* inode->fs->type.nondir.fsync) (openfile, request, fsync_in->fsync_flags & 1);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_release(struct fuse_request_s *request)
{
    struct fuse_release_in *release_in=(struct fuse_release_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) release_in->fh;

    if (openfile) {
	struct inode_s *inode=openfile->inode;
	uint64_t lock_owner=(release_in->release_flags & FUSE_RELEASE_FLOCK_UNLOCK) ? release_in->lock_owner : 0;

	(* inode->fs->type.nondir.release) (openfile, request, release_in->release_flags, lock_owner);

	free(openfile);
	openfile=NULL;

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_create(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;
    struct inode_s *inode=lookup_workspace_inode(workspace, ino);

    if (inode) {
	struct fuse_create_in *create_in=(struct fuse_create_in *) request->buffer;
	char *name=(char *) (request->buffer + sizeof(struct fuse_create_in));
	unsigned int len=request->size - sizeof(struct fuse_create_in);
	struct fuse_openfile_s *openfile=NULL;

	openfile=malloc(sizeof(struct fuse_openfile_s));

	if (openfile) {

	    memset(openfile, 0, sizeof(struct fuse_openfile_s));

	    openfile->context=context;
	    openfile->inode=inode;
	    openfile->error=0;
	    openfile->flock=0;

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

static signed char hidefile_default(struct fuse_opendir_s *opendir, struct entry_s *entry)
{

    if (opendir->flags & _FUSE_OPENDIR_FLAG_HIDE_SPECIALFILES) {

	if (check_entry_special(entry->inode)) {

	    logoutput("hidefile_default: %.*s is special entry", entry->name.len, entry->name.name);
	    return 1;
	}

    }

    if (opendir->flags & _FUSE_OPENDIR_FLAG_HIDE_DOTFILES) {

	if (entry->name.name[0]=='.' && entry->name.len>1 && entry->name.name[1]!='.') {

	    logoutput("hidefile_default: %.*s starts with dot", entry->name.len, entry->name.name);
	    return 1;

	}

    }

    return 0;

}

static int get_fuse_direntry_null(struct fuse_opendir_s *opendir, struct fuse_request_s *request, struct name_s *name, struct stat *st)
{
    return -1;
}

void _fuse_fs_opendir(struct service_context_s *context, struct inode_s *inode, struct fuse_request_s *request, struct fuse_open_in *open_in)
{
    struct fuse_opendir_s *opendir=NULL;

    opendir=malloc(sizeof(struct fuse_opendir_s));

    if (opendir) {

	memset(opendir, 0, sizeof(struct fuse_opendir_s));

	opendir->flags=0;
	opendir->context=context;
	opendir->inode=inode;
	opendir->ino=0;
	opendir->count_created=0;
	opendir->count_found=0;
	opendir->error=0;
	opendir->readdir=inode->fs->type.dir.readdir;
	opendir->readdirplus=inode->fs->type.dir.readdirplus;
	opendir->releasedir=inode->fs->type.dir.releasedir;
	opendir->fsyncdir=inode->fs->type.dir.fsyncdir;
	opendir->get_fuse_direntry=inode->fs->type.dir.get_fuse_direntry;

	/* make hide file depend on the context? 
	    like:
	    hide files starting with a dot on unix like systems when the fs is sftp/ssh for example
	*/

	opendir->hidefile=hidefile_default;

	init_list_header(&opendir->entries, SIMPLE_LIST_TYPE_EMPTY, NULL);
	init_list_header(&opendir->symlinks, SIMPLE_LIST_TYPE_EMPTY, NULL);
	opendir->mutex = get_fuse_pthread_mutex(request->ptr);
	opendir->cond = get_fuse_pthread_cond(request->ptr);

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

void fuse_fs_opendir(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;

    logoutput("fuse_fs_opendir");

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;
	struct fuse_open_in *open_in=(struct fuse_open_in *) request->buffer;

	_fuse_fs_opendir(context, inode, request, open_in);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {
	    struct fuse_open_in *open_in=(struct fuse_open_in *) request->buffer;

	    _fuse_fs_opendir(context, inode, request, open_in);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_readdir(struct fuse_request_s *request)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) request->buffer;
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) read_in->fh;

    logoutput("fuse_fs_readdir");

    if (opendir) {
	struct inode_s *inode=opendir->inode;

	(* opendir->readdir)(opendir, request, read_in->size, read_in->offset);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_readdirplus(struct fuse_request_s *request)
{
    struct fuse_read_in *read_in=(struct fuse_read_in *) request->buffer;
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) read_in->fh;

    logoutput("fuse_fs_readdirplus");

    if (opendir) {
	struct inode_s *inode=opendir->inode;

	(* opendir->readdirplus)(opendir, request, read_in->size, read_in->offset);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_releasedir(struct fuse_request_s *request)
{
    struct fuse_release_in *release_in=(struct fuse_release_in *) request->buffer;
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) release_in->fh;

    logoutput("fuse_fs_releasedir");

    if (opendir) {
	struct inode_s *inode=opendir->inode;

	(* opendir->releasedir)(opendir, request);

	free(opendir);
	opendir=NULL;
	release_in->fh=0;

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_fsyncdir(struct fuse_request_s *request)
{
    struct fuse_fsync_in *fsync_in=(struct fuse_fsync_in *) request->buffer;
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) (uintptr_t) fsync_in->fh;

    if (opendir) {
	struct inode_s *inode=opendir->inode;

	(* opendir->fsyncdir)(opendir, request, fsync_in->fsync_flags & 1);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/*
    get information about a lock
    if this is the case it returns the same lock with type F_UNLCK
    used for posix locks
*/

void fuse_fs_getlock(struct fuse_request_s *request)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) request->buffer;

    if (lk_in->fh>0) {
	struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

	if (openfile) {
	    struct inode_s *inode=openfile->inode;
	    struct flock flock;

	    flock.l_type=lk_in->lk.type;
	    flock.l_whence=SEEK_SET;
	    flock.l_start=lk_in->lk.start;
	    flock.l_len=(lk_in->lk.end==OFFSET_MAX) ? 0 : lk_in->lk.end - lk_in->lk.start + 1;
	    flock.l_pid=lk_in->lk.pid;

	    (* inode->fs->type.nondir.getlock) (openfile, request, &flock);

	} else {

	    reply_VFS_error(request, EIO);

	}

    } else {

	reply_VFS_error(request, EIO);

    }
}

static void _fuse_fs_flock_lock(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct fuse_lk_in *lk_in, unsigned char type)
{
    struct inode_s *inode=openfile->inode;

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

    (* inode->fs->type.nondir.flock) (openfile, request, type);

}

static void _fuse_fs_posix_lock(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct fuse_lk_in *lk_in)
{
    struct inode_s *inode=openfile->inode;
    struct flock flock;

    flock.l_type=lk_in->lk.type;
    flock.l_whence=SEEK_SET;
    flock.l_start=lk_in->lk.start;
    flock.l_len=(lk_in->lk.end==OFFSET_MAX) ? 0 : lk_in->lk.end - lk_in->lk.start + 1;
    flock.l_pid=lk_in->lk.pid;

    (* inode->fs->type.nondir.setlock) (openfile, request, &flock);

}

static void _fuse_fs_posix_lock_wait(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct fuse_lk_in *lk_in)
{
    struct inode_s *inode=openfile->inode;
    struct flock flock;

    flock.l_type=lk_in->lk.type;
    flock.l_whence=SEEK_SET;
    flock.l_start=lk_in->lk.start;
    flock.l_len=(lk_in->lk.end==OFFSET_MAX) ? 0 : lk_in->lk.end - lk_in->lk.start + 1;
    flock.l_pid=lk_in->lk.pid;

    (* inode->fs->type.nondir.setlockw) (openfile, request, &flock);

}

/*
    generic function to set a lock
    it's called to set a posix lock and to set a flock lock
    depending in the presence of FUSE_LK_FLOCK in the flags
    this function is used when both locks are used (set in the init phase)
*/

void fuse_fs_lock(struct fuse_request_s *request)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

    if (openfile) {
	struct inode_s *inode=openfile->inode;

	if (lk_in->lk_flags & FUSE_LK_FLOCK) {

	    _fuse_fs_flock_lock(openfile, request, lk_in, LOCK_NB);

	} else {

	    _fuse_fs_posix_lock(openfile, request, lk_in);

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

void fuse_fs_lock_wait(struct fuse_request_s *request)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

    if (openfile) {
	struct inode_s *inode=openfile->inode;

	if (lk_in->lk_flags & FUSE_LK_FLOCK) {

	    _fuse_fs_flock_lock(openfile, request, lk_in, 0);

	} else {

	    _fuse_fs_posix_lock_wait(openfile, request, lk_in);

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

void fuse_fs_posix_lock(struct fuse_request_s *request)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

    if (openfile) {

	_fuse_fs_posix_lock(openfile, request, lk_in);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_posix_lock_wait(struct fuse_request_s *request)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

    if (openfile) {

	_fuse_fs_posix_lock_wait(openfile, request, lk_in);

    } else {

	reply_VFS_error(request, EIO);

    }

}

/*
    function to set a flock lock
    called when only flock locks are used
    (so every lock is a flock lock)
*/

void fuse_fs_flock_lock(struct fuse_request_s *request)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

    if (openfile) {

	_fuse_fs_flock_lock(openfile, request, lk_in, LOCK_NB);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_flock_lock_wait(struct fuse_request_s *request)
{
    struct fuse_lk_in *lk_in=(struct fuse_lk_in *) request->buffer;
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) (uintptr_t) lk_in->fh;

    if (openfile) {

	_fuse_fs_flock_lock(openfile, request, lk_in, 0);

    } else {

	reply_VFS_error(request, EIO);

    }

}

void fuse_fs_setxattr(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;

    reply_VFS_error(request, ENODATA);
    return;

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;
	struct fuse_setxattr_in *setxattr_in=(struct fuse_setxattr_in *) request->buffer;
	char *name=(char *) ((char *) setxattr_in + sizeof(struct fuse_setxattr_in));
	char *value=(char *) (name + strlen(name) + 1);

	(* inode->fs->setxattr)(context, request, inode, name, value, setxattr_in->size, setxattr_in->flags);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {
	    struct fuse_setxattr_in *setxattr_in=(struct fuse_setxattr_in *) request->buffer;
	    char *name=(char *) ((char *) setxattr_in + sizeof(struct fuse_setxattr_in));
	    char *value=(char *) (name + strlen(name) + 1);

	    (* inode->fs->setxattr)(context, request, inode, name, value, setxattr_in->size, setxattr_in->flags);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_getxattr(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;

    reply_VFS_error(request, ENODATA);
    return;

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;
	struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) request->buffer;
	char *name=(char *) ((char *) getxattr_in + sizeof(struct fuse_getxattr_in));

	logoutput("fuse_fs_getxattr: root ino %li name %s", ino, name);

	(* inode->fs->getxattr)(context, request, inode, name, getxattr_in->size);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {
	    struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) request->buffer;
	    char *name=(char *) ((char *) getxattr_in + sizeof(struct fuse_getxattr_in));
	    struct entry_s *entry=inode->alias;

	    logoutput("fuse_fs_getxattr: ino %li entry %.*s name %s", ino, entry->name.len, entry->name.name, name);

	    (* inode->fs->getxattr)(context, request, inode, name, getxattr_in->size);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_listxattr(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;

    reply_VFS_error(request, ENODATA);
    return;

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;
	struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) request->buffer;

	(* inode->fs->listxattr)(context, request, inode, getxattr_in->size);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {
	    struct fuse_getxattr_in *getxattr_in=(struct fuse_getxattr_in *) request->buffer;

	    (* inode->fs->listxattr)(context, request, inode, getxattr_in->size);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_removexattr(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
    uint64_t ino=request->ino;

    if (ino==FUSE_ROOT_ID) {
	struct inode_s *inode=&workspace->inodes.rootinode;
	char *name=(char *) request->buffer;

	(* inode->fs->removexattr)(context, request, inode, name);

    } else {
	struct inode_s *inode=lookup_workspace_inode(workspace, ino);

	if (inode) {
	    char *name=(char *) request->buffer;

	    (* inode->fs->removexattr)(context, request, inode, name);

	} else {

	    reply_VFS_error(request, ENOENT);

	}

    }

}

void fuse_fs_statfs(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=context->workspace;
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

void fuse_fs_interrupt(struct fuse_request_s *request)
{
    struct fuse_interrupt_in *in=(struct fuse_interrupt_in *) request->buffer;

    logoutput("INTERRUPT (thread %i) unique %lu", (int) gettid(), in->unique);
    set_fuse_request_interrupted(request, in->unique);

}

static void get_fusefs_flags(struct fuse_init_in *in, struct fuse_init_out *out, struct context_interface_s *interface, const char *what, uint64_t code)
{

    if (in->flags & code) {
	struct ctx_option_s option;
	unsigned int len=7 + strlen(what) + 1;
	char buffer[len];

	memset(&option, 0, sizeof(struct ctx_option_s));
	option.type=_CTX_OPTION_TYPE_INT;
	snprintf(buffer, len, "option:%s", what);

	if ((* interface->signal_context)(interface, buffer, &option)>=0) {

	    if (option.type==_CTX_OPTION_TYPE_INT && option.value.integer>0) {

		out->flags |= code;
		logoutput("get_fusefs_flags: kernel and fs support %s", what);

	    } else {

		logoutput("fuse_fs_init: kernel supports %s but fs does not", what);

	    }

	}

    }

}

void fuse_fs_init(struct fuse_request_s *request)
{
    struct context_interface_s *interface=get_fuse_request_interface(request);
    struct fuse_init_in *in=(struct fuse_init_in *) request->buffer;
    struct service_context_s *context=get_service_context(interface);

    logoutput("INIT (thread %i)", (int) gettid());

    logoutput("fuse_fs_init: kernel proto %i:%i", in->major, in->minor);
    logoutput("fuse_fs_init: userspace proto %i:%i", FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR_VERSION);
    logoutput("fuse_fs_init: kernel max readahead %i", in->max_readahead);

    // logoutput("fuse_fs_init: request ptr%sdefined", (request->ptr) ? " " : " not ");

    if (in->major<7) {

	logoutput("fuse_fs_init: unsupported kernel protocol version");
	reply_VFS_error(request, EPROTO);
	return;

    } else {
	struct fuse_init_out out;

	memset(&out, 0, sizeof(struct fuse_init_out));

	out.major=FUSE_KERNEL_VERSION;
	out.minor=FUSE_KERNEL_MINOR_VERSION;
	out.flags=0;

	if (in->major>7) {

	    reply_VFS_data(request, (char *) &out, sizeof(struct fuse_init_out));
	    return;

	}


#ifdef FUSE_ASYNC_READ
	get_fusefs_flags(in, &out, interface, "async-read", FUSE_ASYNC_READ);
#endif

#ifdef FUSE_POSIX_LOCKS
	get_fusefs_flags(in, &out, interface, "posix-locks", FUSE_POSIX_LOCKS);
#endif

#ifdef FUSE_FILE_OPS
	get_fusefs_flags(in, &out, interface, "file-ops", FUSE_FILE_OPS);
#endif

#ifdef FUSE_ATOMIC_O_TRUNC
	get_fusefs_flags(in, &out, interface, "atomic-o-trunc", FUSE_ATOMIC_O_TRUNC);
#endif

#ifdef FUSE_EXPORT_SUPPORT
	get_fusefs_flags(in, &out, interface, "export-support", FUSE_EXPORT_SUPPORT);
#endif

#ifdef FUSE_BIG_WRITES
	get_fusefs_flags(in, &out, interface, "big-writes", FUSE_BIG_WRITES);
#endif

#ifdef FUSE_DONT_MASK
	get_fusefs_flags(in, &out, interface, "dont-mask", FUSE_DONT_MASK);
#endif

#ifdef FUSE_SPLICE_WRITE
	get_fusefs_flags(in, &out, interface, "splice-write", FUSE_SPLICE_WRITE);
#endif

#ifdef FUSE_SPLICE_MOVE
	get_fusefs_flags(in, &out, interface, "splice-move", FUSE_SPLICE_MOVE);
#endif

#ifdef FUSE_SPLICE_READ
	get_fusefs_flags(in, &out, interface, "splice-read", FUSE_SPLICE_READ);
#endif

#ifdef FUSE_FLOCK_LOCKS
	get_fusefs_flags(in, &out, interface, "flock-locks", FUSE_FLOCK_LOCKS);
#endif

#ifdef FUSE_HAS_IOCTL_DIR
	get_fusefs_flags(in, &out, interface, "has-ioctl-dir", FUSE_HAS_IOCTL_DIR);
#endif

#ifdef FUSE_AUTO_INVAL_DATA
	get_fusefs_flags(in, &out, interface, "auto-inval-data", FUSE_AUTO_INVAL_DATA);
#endif

#ifdef FUSE_DO_READDIRPLUS
	get_fusefs_flags(in, &out, interface, "do-readdirplus", FUSE_DO_READDIRPLUS);
#endif

#ifdef FUSE_READDIRPLUS_AUTO
	get_fusefs_flags(in, &out, interface, "readdirplus-auto", FUSE_READDIRPLUS_AUTO);
#endif

#ifdef FUSE_ASYNC_DIO
	get_fusefs_flags(in, &out, interface, "async-dio", FUSE_ASYNC_DIO);
#endif

#ifdef FUSE_WRITEBACK_CACHE
	get_fusefs_flags(in, &out, interface, "writeback-cache", FUSE_WRITEBACK_CACHE);
#endif

#ifdef FUSE_NO_OPEN_SUPPORT
	get_fusefs_flags(in, &out, interface, "no-open-support", FUSE_NO_OPEN_SUPPORT);
#endif

#ifdef FUSE_PARALLEL_DIROPS
	get_fusefs_flags(in, &out, interface, "parallel-dirops", FUSE_PARALLEL_DIROPS);
#endif

#ifdef FUSE_POSIX_ACL
	get_fusefs_flags(in, &out, interface, "posix-acl", FUSE_POSIX_ACL);
#endif

	out.max_readahead = in->max_readahead;
	out.max_write = 4096; /* 4K */
	out.max_background=(1 << 16) - 1;
	out.congestion_threshold=(3 * out.max_background) / 4;

	reply_VFS_data(request, (char *) &out, sizeof(struct fuse_init_out));

	/*	adjust various callbacks
		- flock or posix locks	*/

	if ((out.flags & FUSE_FLOCK_LOCKS) && !(out.flags & FUSE_POSIX_LOCKS)) {

	    /* only flocks */

	    register_fuse_function(interface->buffer, FUSE_SETLK, fuse_fs_flock_lock);
	    register_fuse_function(interface->buffer, FUSE_SETLKW, fuse_fs_flock_lock_wait);

	} else if (!(out.flags & FUSE_FLOCK_LOCKS) && (out.flags & FUSE_POSIX_LOCKS)) {

	    /* only posix locks */

	    register_fuse_function(interface->buffer, FUSE_SETLK, fuse_fs_posix_lock);
	    register_fuse_function(interface->buffer, FUSE_SETLKW, fuse_fs_posix_lock_wait);

	}

	if (!(out.flags & FUSE_DONT_MASK)) {

	    /* do not apply mask to permissions: kernel has done it already */

	    disable_masking_userspace(interface->buffer);

	}

    }

}

void fuse_fs_destroy (struct fuse_request_s *request)
{
    logoutput("DESTROY (thread %i)", (int) gettid());
}

void register_fuse_functions(struct context_interface_s *interface)
{
    char *ptr=interface->buffer;

    register_fuse_function(ptr, FUSE_INIT, fuse_fs_init);
    register_fuse_function(ptr, FUSE_DESTROY, fuse_fs_destroy);

    register_fuse_function(ptr, FUSE_LOOKUP, fuse_fs_lookup);
    register_fuse_function(ptr, FUSE_FORGET, fuse_fs_forget);
    register_fuse_function(ptr, FUSE_BATCH_FORGET, fuse_fs_forget_multi);

    register_fuse_function(ptr, FUSE_GETATTR, fuse_fs_getattr);
    register_fuse_function(ptr, FUSE_SETATTR, fuse_fs_setattr);

    register_fuse_function(ptr, FUSE_MKDIR, fuse_fs_mkdir);
    register_fuse_function(ptr, FUSE_MKNOD, fuse_fs_mknod);
    register_fuse_function(ptr, FUSE_SYMLINK, fuse_fs_symlink);

    register_fuse_function(ptr, FUSE_RMDIR, fuse_fs_rmdir);
    register_fuse_function(ptr, FUSE_UNLINK, fuse_fs_unlink);

    register_fuse_function(ptr, FUSE_READLINK, fuse_fs_readlink);
    register_fuse_function(ptr, FUSE_RENAME, fuse_fs_rename);
    register_fuse_function(ptr, FUSE_LINK, fuse_fs_link);

    register_fuse_function(ptr, FUSE_OPENDIR, fuse_fs_opendir);
    register_fuse_function(ptr, FUSE_READDIR, fuse_fs_readdir);
    register_fuse_function(ptr, FUSE_READDIRPLUS, fuse_fs_readdirplus);
    register_fuse_function(ptr, FUSE_RELEASEDIR, fuse_fs_releasedir);
    register_fuse_function(ptr, FUSE_FSYNCDIR, fuse_fs_fsyncdir);

    register_fuse_function(ptr, FUSE_CREATE, fuse_fs_create);
    register_fuse_function(ptr, FUSE_OPEN, fuse_fs_open);
    register_fuse_function(ptr, FUSE_READ, fuse_fs_read);
    register_fuse_function(ptr, FUSE_WRITE, fuse_fs_write);
    register_fuse_function(ptr, FUSE_FSYNC, fuse_fs_fsync);
    register_fuse_function(ptr, FUSE_FLUSH, fuse_fs_flush);
    register_fuse_function(ptr, FUSE_RELEASE, fuse_fs_release);

    register_fuse_function(ptr, FUSE_GETLK, fuse_fs_getlock);
    register_fuse_function(ptr, FUSE_SETLK, fuse_fs_lock);
    register_fuse_function(ptr, FUSE_SETLKW, fuse_fs_lock_wait);

    register_fuse_function(ptr, FUSE_STATFS, fuse_fs_statfs);

    register_fuse_function(ptr, FUSE_LISTXATTR, fuse_fs_listxattr);
    register_fuse_function(ptr, FUSE_GETXATTR, fuse_fs_getxattr);
    register_fuse_function(ptr, FUSE_SETXATTR, fuse_fs_setxattr);
    register_fuse_function(ptr, FUSE_REMOVEXATTR, fuse_fs_removexattr);

    register_fuse_function(ptr, FUSE_INTERRUPT, fuse_fs_interrupt);

};

void init_fuse_buffer(struct fuse_buffer_s *buffer, char *data, unsigned int size, unsigned int count, signed char eof)
{
    buffer->data=data;
    buffer->pos=data;
    buffer->size=size;
    buffer->left=size;
    buffer->count=count;
    buffer->eof=eof;
}

void clear_fuse_buffer(struct fuse_buffer_s *buffer)
{

    if (buffer->data) {

	free(buffer->data);
	buffer->data=NULL;

    }

    init_fuse_buffer(buffer, NULL, 0, 0, -1);

}

void queue_fuse_direntry_symlinks(struct fuse_opendir_s *opendir, struct entry_s *entry)
{
    struct fuse_direntry_s *direntry=malloc(sizeof(struct fuse_direntry_s));

    logoutput("queue_fuse_direntry: %.*s", entry->name.len, entry->name.name);

    if (direntry) {

	memset(direntry, 0, sizeof(struct fuse_direntry_s));
	direntry->entry=entry;
	init_list_element(&direntry->list, NULL);

	pthread_mutex_lock(opendir->mutex);
	add_list_element_last(&opendir->symlinks, &direntry->list);
	pthread_cond_broadcast(opendir->cond);
	pthread_mutex_unlock(opendir->mutex);

    }

}

void queue_fuse_direntry(struct fuse_opendir_s *opendir, struct entry_s *entry)
{
    struct fuse_direntry_s *direntry=malloc(sizeof(struct fuse_direntry_s));

    logoutput("queue_fuse_direntry: %.*s", entry->name.len, entry->name.name);

    if (direntry) {

	memset(direntry, 0, sizeof(struct fuse_direntry_s));
	direntry->entry=entry;
	init_list_element(&direntry->list, NULL);

	pthread_mutex_lock(opendir->mutex);
	add_list_element_last(&opendir->entries, &direntry->list);
	pthread_cond_broadcast(opendir->cond);
	pthread_mutex_unlock(opendir->mutex);

    }

}

void set_flag_fuse_opendir(struct fuse_opendir_s *opendir, uint32_t flag)
{
    pthread_mutex_lock(opendir->mutex);
    opendir->flags |= flag;
    pthread_cond_broadcast(opendir->cond);
    pthread_mutex_unlock(opendir->mutex);
}
