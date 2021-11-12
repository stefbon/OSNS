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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
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
#include <sys/syscall.h>
#include <sys/fsuid.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "log.h"

#include "misc.h"
#include "options.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "fs-common.h"

#define UINT32_T_MAX		0xFFFFFFFF

const char *rootpath="/";
const char *dotdotname="..";
const char *dotname=".";
const struct name_s dotxname={".", 1, 0};
const struct name_s dotdotxname={"..", 2, 0};

extern struct fs_options_s fs_options;

struct service_fs_s *get_service_context_fs(struct service_context_s *c)
{
    struct service_fs_s *fs=NULL;

    switch (c->type) {

	case SERVICE_CTX_TYPE_FILESYSTEM:

	    logoutput("get_service_context_fs: found filesystem ctx");
	    fs=c->service.filesystem.fs;
	    break;

	case SERVICE_CTX_TYPE_BROWSE:

	    logoutput("get_service_context_fs: found browse ctx");
	    fs=c->service.browse.fs;
	    break;

	case SERVICE_CTX_TYPE_WORKSPACE:

	    logoutput("get_service_context_fs: found workspace ctx");
	    fs=c->service.workspace.fs;
	    break;

    }

    return fs;

}

/* provides stat to lookup when entry already exists (is cached) */

void _fs_common_cached_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct fuse_entry_out entry_out;
    struct timespec *attr_timeout=NULL;
    struct timespec *entry_timeout=NULL;

    logoutput_debug("_fs_common_cached_lookup: ino %li name %.*s", inode->stat.sst_ino, inode->alias->name.len, inode->alias->name.name);
    log_inode_information(inode, INODE_INFORMATION_NAME | INODE_INFORMATION_NLOOKUP | INODE_INFORMATION_MODE | INODE_INFORMATION_SIZE | INODE_INFORMATION_MTIM | INODE_INFORMATION_INODE_LINK | INODE_INFORMATION_FS_COUNT);

    context=get_root_context(context);
    attr_timeout=get_fuse_attr_timeout(request->root);
    entry_timeout=get_fuse_entry_timeout(request->root);

    memset(&entry_out, 0, sizeof(struct fuse_entry_out));

    inode->nlookup++;

    entry_out.nodeid=inode->stat.sst_ino;
    entry_out.generation=0; /* todo: add a generation field to reuse existing inodes */

    entry_out.entry_valid=entry_timeout->tv_sec;
    entry_out.entry_valid_nsec=entry_timeout->tv_nsec;

    entry_out.attr_valid=attr_timeout->tv_sec;
    entry_out.attr_valid_nsec=attr_timeout->tv_nsec;

    fill_fuse_attr_system_stat(&entry_out.attr, &inode->stat);

    reply_VFS_data(request, (char *) &entry_out, sizeof(entry_out));

}

void _fs_common_cached_create(struct service_context_s *context, struct fuse_request_s *request, struct fuse_openfile_s *openfile)
{
    struct inode_s *inode=openfile->inode;
    struct fuse_entry_out entry_out;
    struct fuse_open_out open_out;
    unsigned int size_entry_out=sizeof(struct fuse_entry_out);
    unsigned int size_open_out=sizeof(struct fuse_open_out);
    struct timespec *attr_timeout=NULL;
    struct timespec *entry_timeout=NULL;

    context=get_root_context(context);
    attr_timeout=get_fuse_attr_timeout(request->root);
    entry_timeout=get_fuse_entry_timeout(request->root);
    char buffer[size_entry_out + size_open_out];

    // inode->nlookup++;
    memset(&entry_out, 0, sizeof(struct fuse_entry_out));

    entry_out.nodeid=inode->stat.sst_ino;
    entry_out.generation=0; /* todo: add a generation field to reuse existing inodes */

    entry_out.entry_valid=entry_timeout->tv_sec;
    entry_out.entry_valid_nsec=entry_timeout->tv_nsec;

    entry_out.attr_valid=attr_timeout->tv_sec;
    entry_out.attr_valid_nsec=attr_timeout->tv_nsec;

    fill_fuse_attr_system_stat(&entry_out.attr, &inode->stat);

    open_out.fh=(uint64_t) openfile;
    open_out.open_flags=FOPEN_KEEP_CACHE;

    /* put entry_out and open_out in one buffer */

    memcpy(buffer, &entry_out, size_entry_out);
    memcpy(buffer+size_entry_out, &open_out, size_open_out);

    reply_VFS_data(request, buffer, size_entry_out + size_open_out);

}

void _fs_common_virtual_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *pinode, const char *name, unsigned int len)
{
    struct entry_s *parent=pinode->alias, *entry=NULL;
    struct name_s xname={NULL, 0, 0};
    unsigned int error=0;
    struct directory_s *directory=NULL;

    logoutput("_fs_common_virtual_lookup: name %.*s parent %li (thread %i)", len, name, (long) pinode->stat.sst_ino, (int) gettid());

    xname.name=(char *)name;
    xname.len=strlen(name);
    calculate_nameindex(&xname);
    directory=get_directory(pinode);

    entry=find_entry(directory, &xname, &error);

    if (entry) {
	struct inode_s *inode=entry->inode;
	struct data_link_s *link=NULL;

	/* it's possible that the entry represents the root of a service
	    in that case do a lookup of the '.' on the root of the service using the service specific fs calls
	*/

	log_inode_information(inode, INODE_INFORMATION_NAME | INODE_INFORMATION_NLOOKUP | INODE_INFORMATION_MODE | INODE_INFORMATION_SIZE | INODE_INFORMATION_MTIM | INODE_INFORMATION_INODE_LINK | INODE_INFORMATION_FS_COUNT);

	logoutput("_fs_common_virtual_lookup: found entry %.*s ino %li nlookup %i", entry->name.len, entry->name.name, inode->stat.sst_ino, inode->nlookup);
	inode->nlookup++;
	fs_get_data_link(inode, &link);

	if (link->type==DATA_LINK_TYPE_CONTEXT) {
	    struct service_context_s *service_context=(struct service_context_s *) link->link.ptr;
	    struct pathinfo_s pathinfo=PATHINFO_INIT;
	    unsigned int pathlen=0;
	    char path[3];
	    struct service_fs_s *fs=NULL;

	    logoutput("_fs_common_virtual_lookup: use context %s", service_context->name);

	    path[2]='\0';
	    path[1]='.';
	    path[0]='/';

	    pathinfo.len=2;
	    pathinfo.path=path;

	    fs=service_context->service.filesystem.fs;

	    (* fs->lookup_existing)(service_context, request, entry, &pathinfo);
	    return;

	}

	_fs_common_cached_lookup(context, request, inode);

    } else {

	logoutput("_fs_common_virtual_lookup: enoent lookup %i", (entry) ? entry->inode->nlookup : 0);
	reply_VFS_error(request, ENOENT);

    }

}

/*
    common function to get the attributes
    NOTO: dealing with a read-only filesystem, so do not care about refreshing, just get out of the cache
*/

void _fs_common_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct fuse_attr_out attr_out;
    struct timespec *attr_timeout=NULL;

    logoutput("_fs_common_getattr: context %s", context->name);

    memset(&attr_out, 0, sizeof(struct fuse_attr_out));
    context=get_root_context(context);
    attr_timeout=get_fuse_attr_timeout(request->root);

    attr_out.attr_valid=attr_timeout->tv_sec;
    attr_out.attr_valid_nsec=attr_timeout->tv_nsec;

    fill_fuse_attr_system_stat(&attr_out.attr, &inode->stat);

    reply_VFS_data(request, (char *) &attr_out, sizeof(attr_out));

}

void _fs_common_virtual_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags)
{
    unsigned int error=0;
    struct directory_s *directory=NULL;
    struct fuse_open_out open_out;

    logoutput("_fs_common_virtual_opendir: ino %li", opendir->inode->stat.sst_ino);
    directory=get_directory(opendir->inode);

    if (directory && get_directory_count(directory)>0) opendir->flags |= _FUSE_OPENDIR_FLAG_NONEMPTY;

    open_out.fh=(uint64_t) opendir;
    open_out.open_flags=0;
    reply_VFS_data(request, (char *) &open_out, sizeof(open_out));

    opendir->flags |= (_FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS | _FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS) ; /* sane flags */
    if (fs_options.sftp.hideflags & _OPTIONS_SFTP_HIDE_FLAG_DOTFILE) opendir->flags |= _FUSE_OPENDIR_FLAG_HIDE_DOTFILES;

    queue_fuse_direntries_virtual(opendir);
    finish_get_fuse_direntry(opendir);

}

struct entry_s *get_fuse_direntry_virtual(struct fuse_opendir_s *opendir, struct list_header_s *h, struct fuse_request_s *request)
{
    struct entry_s *entry=NULL;
    struct directory_s *directory=get_directory(opendir->inode);
    struct list_element_s *list=(struct list_element_s *) opendir->handle.ptr;

    while (list) {

	entry=(struct entry_s *) ((char *) list - offsetof(struct entry_s, list));
	memcpy(&entry->inode->stime, &directory->synctime, sizeof (struct system_timespec_s));

	if ((* opendir->hidefile)(opendir, entry)==1) {

	    list=list->n;
	    entry=NULL;

	} else {

	    opendir->handle.ptr=(void *) list->n;
	    break;

	}

    }

    return entry;
}

static void _fs_common_virtual_readdir_common(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset, unsigned char plus)
{
    struct service_context_s *context=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct directory_s *directory=NULL;
    struct name_s *xname=NULL;
    struct inode_s *inode=NULL;
    struct entry_s *entry=NULL;
    char buff[size];
    struct direntry_buffer_s direntries;
    unsigned int error=0;
    struct simple_lock_s rlock;
    uint64_t ino2keep=0;

    logoutput("_fs_common_virtual_readdir_common: size %i off %i flags %i", (unsigned int) size, (unsigned int) offset, opendir->flags);
    directory=get_directory(opendir->inode);

    if (opendir->flags & _FUSE_OPENDIR_FLAG_READDIR_FINISH) {

	char dummy='\0';

	logoutput("_fs_common_virtual_readdir_common: finish");
	reply_VFS_data(request, &dummy, 0);
	return;

    }

    if (rlock_directory(directory, &rlock)==-1) {

	reply_VFS_error(request, EAGAIN);
	return;

    }

    direntries.data=buff;
    direntries.pos=buff;
    direntries.size=size;
    direntries.left=size;
    direntries.offset=offset;

    if (plus) {

	opendir->add_direntry_buffer=add_direntry_plus_buffer;

    } else {

	opendir->add_direntry_buffer=add_direntry_buffer;

    }

    while (direntries.left>0 && (request->flags & FUSE_REQUEST_FLAG_INTERRUPTED)==0 && (opendir->flags & (_FUSE_OPENDIR_FLAG_READDIR_FINISH | _FUSE_OPENDIR_FLAG_READDIR_INCOMPLETE | _FUSE_OPENDIR_FLAG_READDIR_ERROR))==0) {

	entry=NULL;
	xname=NULL;

	if (direntries.offset==0) {

	    opendir->count_keep=get_directory_count(directory);

	    inode=opendir->inode;

    	    /* the . entry */

	    xname=(struct name_s *) &dotxname;
	    ino2keep=0;

    	} else if (direntries.offset==1) {
    	    struct directory_s *directory=NULL;

	    inode=opendir->inode;

	    /* the .. entry */

	    directory=get_directory(inode);
	    if (directory) inode=directory->inode;
	    xname=(struct name_s *) &dotdotxname;
	    ino2keep=0;

    	} else {

	    if (opendir->ino>0) {

		inode=lookup_workspace_inode(workspace, opendir->ino);
		opendir->ino=0;
		if (inode) entry=inode->alias;

	    }

	    readdir:

	    if (entry==NULL) {

		if ((entry=_fs_service_get_fuse_direntry(opendir, request))==NULL) break;
		inode=entry->inode;
		memcpy(&inode->stime, &directory->synctime, sizeof (struct timespec));

	    }

	    xname=&entry->name;
	    ino2keep=inode->stat.sst_ino;

	}

	/* add entry to the buffer to send to VFS: does it fit, if no keep the ino for adding later next batch */

	if ((* opendir->add_direntry_buffer)(request->root, &direntries, xname, &inode->stat)==-1) {

	    logoutput("_fs_common_virtual_readdir_common: %.*s does not fit in buffer", xname->len, xname->name);
	    opendir->ino=ino2keep; /* keep it for the next batch */
	    break;

	}

	logoutput("_fs_common_virtual_readdir_common: add %.*s", xname->len, xname->name);

    }

    unlock_directory(directory, &rlock);
    reply_VFS_data(request, direntries.data, (size_t) (direntries.pos - direntries.data));
    // if (opendir->flags & _FUSE_OPENDIR_FLAG_QUEUE_READY) opendir->flags |= _FUSE_OPENDIR_FLAG_READDIR_FINISH;

}

void _fs_common_virtual_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset)
{
    _fs_common_virtual_readdir_common(opendir, request, size, offset, 0);
}

void _fs_common_virtual_readdirplus(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset)
{
    _fs_common_virtual_readdir_common(opendir, request, size, offset, 1);
}

void _fs_common_virtual_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *request)
{
    set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_FINISH);
    // logoutput("_fs_common_virtual_releasedir");
    reply_VFS_error(request, 0);
}

void _fs_common_virtual_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync)
{
    reply_VFS_error(request, 0);
}


void _fs_common_remove_nonsynced_dentries(struct fuse_opendir_s *opendir)
{
    struct service_context_s *context=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct directory_s *directory=get_directory(opendir->inode);
    struct simple_lock_s wlock;

    if (wlock_directory(directory, &wlock)==0) {

	/*
		only check when there are deleted entries:
		- the entries found on server (opendir->created plus the already found opendir->count)
		is not equal to the number of entries in this local directory
	*/

	if ((opendir->count_keep > opendir->count_found) || (opendir->count_created + opendir->count_found != get_directory_count(directory))) {
	    struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
	    struct inode_s *inode=NULL;
	    struct entry_s *entry=NULL;
	    struct list_element_s *next=NULL;
	    struct list_element_s *list=get_list_head(&sl->header, 0);

	    while (list) {

		next=get_next_element(list);
		entry=(struct entry_s *)((char *) list - offsetof(struct entry_s, list));

		inode=entry->inode;
		if (check_entry_special(inode)) goto next;

		if (inode->stime.tv_sec < directory->synctime.tv_sec || (inode->stime.tv_sec == directory->synctime.tv_sec && inode->stime.tv_nsec < directory->synctime.tv_nsec)) {

		    logoutput("_fs_common_remove_nonsynced_dentries: queue inode %li to remove", inode->stat.sst_ino);
		    queue_inode_2forget(workspace, inode->stat.sst_ino, FORGET_INODE_FLAG_DELETED, 0);

		}

		next:
		list=next;

	    }

	}

	unlock_directory(directory, &wlock);

    }

}

struct entry_s *_fs_common_create_entry(struct workspace_mount_s *workspace, struct entry_s *parent, struct name_s *xname, struct system_stat_s *stat, unsigned int size, unsigned int flags, unsigned int *error)
{
    struct service_context_s *context=get_root_context_workspace(workspace);
    struct create_entry_s ce;
    unsigned int dummy=0;

    // logoutput("_fs_common_create_entry");

    if (error==0) error=&dummy;
    init_create_entry(&ce, xname, parent, NULL, NULL, context, stat, NULL);
    return create_entry_extended(&ce);
}

struct entry_s *_fs_common_create_entry_unlocked(struct workspace_mount_s *workspace, struct directory_s *directory, struct name_s *xname, struct system_stat_s *stat, unsigned int size, unsigned int flags, unsigned int *error)
{
    struct service_context_s *context=get_root_context_workspace(workspace);
    struct create_entry_s ce;
    unsigned int dummy=0;

    // logoutput("_fs_common_create_entry_unlocked");

    if (error==0) error=&dummy;
    init_create_entry(&ce, xname, NULL, directory, NULL, context, stat, NULL);
    return create_entry_extended_batch(&ce);
}

void _fs_common_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, uint64_t blocks, uint64_t bfree, uint64_t bavail, uint32_t bsize)
{
    struct fuse_statfs_out statfs_out;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

    /*
	howto determine these values ??
	it's not possible to determine those for all the backends/subfilesystems
    */

    memset(&statfs_out, 0, sizeof(struct fuse_statfs_out));

    statfs_out.st.blocks=blocks;
    statfs_out.st.bfree=bfree;
    statfs_out.st.bavail=bavail;
    statfs_out.st.bsize=bsize;

    statfs_out.st.files=(uint64_t) workspace->inodes.nrinodes;
    statfs_out.st.ffree=(uint64_t) (UINT32_T_MAX - statfs_out.st.files);

    statfs_out.st.namelen=255; /* a sane default */
    statfs_out.st.frsize=bsize; /* use the same as block size */

    reply_VFS_data(request, (char *) &statfs_out, sizeof(struct fuse_statfs_out));

}

/*
    test a (absolute) symlink is subdirectory of the context directory
*/

int symlink_generic_validate(struct service_context_s *context, char *target)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    unsigned int len=strlen(target);
    struct pathinfo_s *mountpoint=&workspace->mountpoint;

    if (len>mountpoint->len) {

	if (strncmp(target, mountpoint->path, mountpoint->len)==0 && target[mountpoint->len]=='/') {
	    unsigned int error=0;
	    struct directory_s *directory=get_directory(context->service.filesystem.inode);

	    if (directory) {
		struct pathinfo_s pathinfo=PATHINFO_INIT;
		char *pos=&target[mountpoint->len];
		unsigned int pathlen=get_pathmax(workspace);
		char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
		struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
		unsigned int len_s=0;

		/* get the path of the directory representing this context (relative to the mountpoint) */

		init_fuse_path(fpath, pathlen);
		len_s+=get_path_root(directory, fpath);

		if (len > mountpoint->len + len_s) {

		    if (strncmp(pos, fpath->pathstart, len_s)==0 && target[mountpoint->len + len_s]=='/') {

			/* absolute symlink is pointing to object in this context */

			return (mountpoint->len + len_s);

		    }

		}

	    }

	}

    }

    return 0;

}

void use_service_fs(struct service_context_s *context, struct inode_s *inode)
{

    logoutput("use_service_fs: context %s ino %li", context->name, inode->stat.sst_ino);

    if (context->type==SERVICE_CTX_TYPE_WORKSPACE) {

	use_browse_fs(inode);

    } else if (context->type==SERVICE_CTX_TYPE_BROWSE) {

	use_browse_fs(inode);

    } else if (context->type==SERVICE_CTX_TYPE_FILESYSTEM) {

	use_service_path_fs(inode);

    } else {

	use_virtual_fs(context, inode);

    }

}
