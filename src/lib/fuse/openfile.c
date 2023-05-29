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

static void of_pread_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lo)
{
    reply_VFS_error(request, EIO);
}

static void of_pwrite_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lo)
{
    reply_VFS_error(request, EIO);
}

static void of_lseek_noop(struct fuse_openfile_s *openfile, struct fuse_request_s *request, off_t off, int whence)
{
    reply_VFS_error(request, EIO);
}

void init_fuse_openfile(struct fuse_openfile_s *openfile, struct service_context_s *ctx, struct inode_s *inode)
{

    memset(openfile, 0, sizeof(struct fuse_openfile_s));

    /* header */

    init_fuse_open_header(&openfile->header, ctx, inode);
    openfile->header.type=FUSE_OPEN_TYPE_FILE;

    /* ops */

    openfile->read=of_pread_noop;
    openfile->write=of_pwrite_noop;
    openfile->lseek=of_lseek_noop;

    openfile->error=0;
    openfile->flags=0;


}
