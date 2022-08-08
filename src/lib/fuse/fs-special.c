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

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/statfs.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-fuse.h"

#include "defaults,h"

static struct fuse_fs_s fs;
static char *desktopentryname=".directory";
static pthread_mutex_t desktopmutex=PTHREAD_MUTEX_INITIALIZER;
static struct statfs statfs_keep;

struct special_path_s {
    uint64_t					ino;
    struct data_link_s				link;
    unsigned int				size;
    char					path[];
};

static void _fs_special_forget(struct service_context_s *context, struct inode_s *inode)
{
    struct data_link_s *link=inode->ptr;

    logoutput("_fs_special_forget");

    if (link) {

	if (link->type==DATA_LINK_TYPE_SPECIAL_ENTRY) {
	    struct special_path_s *s=(struct special_path_s *) ((char *) link - offsetof(struct special_path_s, link));

	    free(s);
	    inode->ptr=NULL;

	}

    }


}

static void _fs_special_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    _fs_common_getattr(context, request, inode);
}

static void _fs_special_setattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat)
{

    if (stat->mask & SYSTEM_STAT_ATIME) {

	_fs_common_getattr(context, request, inode);
	return;

    }

    reply_VFS_error(request, EPERM);


}

static void _fs_special_open(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags)
{
    unsigned int error=EIO;
    struct inode_s *inode=openfile->inode;
    int fd=0;
    struct data_link_s *link=inode->ptr;

    if (link) {

	if (link->type==DATA_LINK_TYPE_SPECIAL_ENTRY) {
	    struct special_path_s *s=(struct special_path_s *) ((char *) link - offsetof(struct special_path_s, link));

	    fd=open((char *) s->path, flags);

	    if (fd>0) {
		struct fuse_open_out open_out;

		openfile->handle.fd=fd;

		open_out.fh=(uint64_t) openfile;
		open_out.open_flags=0; //FOPEN_KEEP_CACHE;
		open_out.padding=0;
		reply_VFS_data(request, (char *) &open_out, sizeof(open_out));
		return;

	    }

	    error=errno;

	}

    }

    openfile->error=error;
    reply_VFS_error(request, error);

}

void _fs_special_read(struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    unsigned int error=EIO;
    char buffer[size];
    ssize_t read=0;

    read=pread(openfile->handle.fd, (void *) buffer, size, off);

    if (read==-1) {

	error=errno;
	reply_VFS_error(request, error);
	return;

    }

    reply_VFS_data(request, buffer, read);

}

void _fs_special_write(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buffer, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    reply_VFS_error(request, EPERM);
}

void _fs_special_flush(struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lock_owner)
{
    reply_VFS_error(request, 0);
}

void _fs_special_fsync(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char sync)
{
    reply_VFS_error(request, 0);
}

void _fs_special_release(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lock_owner)
{
    close(openfile->handle.fd);
    openfile->handle.fd=0;
    reply_VFS_error(request, 0);
}

void _fs_special_fsetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct system_stat_s *stat)
{
    _fs_special_setattr(openfile->context, request, openfile->inode, stat);
}

void _fs_special_fgetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *request)
{
    _fs_special_getattr(openfile->context, request, openfile->inode);
}

static void _fs_special_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode)
{
    struct fuse_statfs_out statfs_out;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

    memset(&statfs_out, 0, sizeof(struct fuse_statfs_out));

    statfs_out.st.blocks=statfs_keep.f_blocks;
    statfs_out.st.bfree=statfs_keep.f_bfree;
    statfs_out.st.bavail=statfs_keep.f_bavail;
    statfs_out.st.bsize=statfs_keep.f_bsize;

    statfs_out.st.files=(uint64_t) workspace->inodes.nrinodes;
    statfs_out.st.ffree=(uint64_t) (UINT32_T_MAX - statfs_out.st.files);

    statfs_out.st.namelen=255;
    statfs_out.st.frsize=statfs_out.st.bsize;

    statfs_out.st.padding=0;

    reply_VFS_data(request, (char *) &statfs_out, sizeof(struct fuse_statfs_out));

}

void init_special_fs()
{
    unsigned int error=0;

    statfs("/", &statfs_keep);

    memset(&fs, 0, sizeof(struct fuse_fs_s));
    set_virtual_fs(&fs);

    fs.forget=_fs_special_forget;
    fs.getattr=_fs_special_getattr;
    fs.setattr=_fs_special_setattr;
    fs.type.nondir.open=_fs_special_open;
    fs.type.nondir.read=_fs_special_read;
    fs.type.nondir.write=_fs_special_write;
    fs.type.nondir.flush=_fs_special_flush;
    fs.type.nondir.fsync=_fs_special_fsync;
    fs.type.nondir.release=_fs_special_release;
    fs.type.nondir.fsetattr=_fs_special_fsetattr;
    fs.type.nondir.fgetattr=_fs_special_fgetattr;
    fs.statfs=_fs_special_statfs;

}

void free_special_fs()
{
}

void set_fs_special(struct inode_s *inode)
{
    if (! S_ISDIR(inode->stat.sst_mode)) inode->fs=&fs;
}

static void create_desktopentry_file(char *path, struct entry_s *parent, struct workspace_mount_s *workspace)
{
    struct system_stat_s stat;
    struct fs_location_path_s location=FS_LOCATION_PATH_INIT;
    struct directory_s *directory=get_directory(workspace, parent->inode, 0);
    struct osns_lock_s wlock;

    location.ptr=path;
    location.len=strlen(path);

    if (system_getstat(&location, SYSTEM_STAT_MODE | SYSTEM_STAT_TYPE, &stat)<0) {

	logoutput("create_desktopentry_file: %s does not exist");
	return;

    } else if (! S_ISREG(stat.sst_mode)) {

	logoutput("create_desktopentry_file: %s is not a file");
	return;

    }

    if (wlock_directory(directory, &wlock)==0) {
	struct entry_s *entry=NULL;
	struct name_s xname;
	unsigned int error=0;

	xname.name=desktopentryname;
	xname.len=strlen(xname.name);
	calculate_nameindex(&xname);

	entry=find_entry_batch(directory, &xname, &error);

	/* only install if not exists */

	if (entry==NULL) {

	    logoutput("create_desktopentry_file: %s in %s (file=%s)", xname.name, parent->name.name, path);

	    error=0;
	    entry=_fs_common_create_entry_unlocked(workspace, directory, &xname, &stat, 0, 0, &error);

	    if (entry) {
		unsigned int size=sizeof(struct special_path_s) + strlen(path) + 1;  /* inlcuding terminating zero */
		struct special_path_s *s=malloc(size);

		if (s) {
		    struct inode_s *inode=entry->inode;

		    memset(s, 0, size);

		    inode->fs=&fs;
		    s->ino=inode->stat.sst_ino;
		    strcpy(s->path, path);
		    s->size=strlen(path);
		    inode->ptr=&s->link;
		    s->link.type=DATA_LINK_TYPE_SPECIAL_ENTRY;

		}

	    }

	}

	unlock_directory(directory, &wlock);

    }

}

void create_network_desktopentry_file(struct entry_s *parent, struct workspace_mount_s *workspace)
{
    create_desktopentry_file("/etc/fs-workspace/desktopentry.network", parent, workspace);
}
void create_netgroup_desktopentry_file(struct entry_s *parent, struct workspace_mount_s *workspace)
{
    create_desktopentry_file("/etc/fs-workspace/desktopentry.netgroup", parent, workspace);
}
void create_netserver_desktopentry_file(struct entry_s *parent, struct workspace_mount_s *workspace)
{
    create_desktopentry_file("/etc/fs-workspace/desktopentry.netserver", parent, workspace);
}
void create_netshare_desktopentry_file(struct entry_s *parent, struct workspace_mount_s *workspace)
{
    create_desktopentry_file("/etc/fs-workspace/desktopentry.sharedmap", parent, workspace);
}

int check_entry_special(struct inode_s *inode)
{
    if (! S_ISDIR(inode->stat.sst_mode)) return ((inode->fs==&fs) ? 1 : 0);
    return 0;
}
