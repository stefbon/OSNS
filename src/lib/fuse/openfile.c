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

#include "libosns-log.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"

#include "receive.h"
#include "request.h"
#include "openfile.h"

static void openfile_read_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    reply_VFS_error(request, EIO);
}

static void openfile_write_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    reply_VFS_error(request, EIO);
}

static void openfile_flush_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lock_owner)
{
    reply_VFS_error(request, 0);
}

static void openfile_fsync_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int datasync)
{
    reply_VFS_error(request, 0);
}

static void openfile_release_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lock_owner)
{
    reply_VFS_error(request, 0);
}

static void openfile_lseek_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, off_t off, int whence)
{
    reply_VFS_error(request, EIO);
}

static void openfile_fgetattr_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request)
{
    reply_VFS_error(request, EIO);
}

static void openfile_fsetattr_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct system_stat_s *stat)
{
    reply_VFS_error(request, EIO);
}

static void openfile_getlock_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock)
{
    reply_VFS_error(request, EIO);
}

static void openfile_setlock_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock, unsigned int flags)
{
    reply_VFS_error(request, EIO);
}

static void openfile_flock_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char type)
{
    reply_VFS_error(request, EIO);
}

void init_fuse_openfile(struct fuse_openfile_s *openfile, struct service_context_s *ctx, struct inode_s *inode)
{

    memset(openfile, 0, sizeof(struct fuse_openfile_s));

    openfile->context=ctx;
    openfile->inode=inode;
    openfile->error=0;
    openfile->flags=0;

    openfile->read=openfile_read_noop;
    openfile->write=openfile_write_noop;
    openfile->fsync=openfile_fsync_noop;
    openfile->flush=openfile_flush_noop;
    openfile->release=openfile_release_noop;
    openfile->lseek=openfile_lseek_noop;

    openfile->fgetattr=openfile_fgetattr_noop;
    openfile->fsetattr=openfile_fsetattr_noop;

    openfile->getlock=openfile_getlock_noop;
    openfile->setlock=openfile_setlock_noop;
    openfile->flock=openfile_flock_noop;


}

/*
	    void (*read) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
	    void (*write) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
	    void (*flush) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lock_owner);
	    void (*fsync) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char datasync);
	    void (*release) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lock_owner);
	    void (*lseek) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, off_t off, int whence);

	    void (*fgetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request);
	    void (*fsetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct system_stat_s *stat);

	    void (*getlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
	    void (*setlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
	    void (*setlockw) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);

	    void (*flock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char type);
*/
