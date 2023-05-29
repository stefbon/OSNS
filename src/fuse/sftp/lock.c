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
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"


#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"
#include "inode-stat.h"

/* no locking */

void _fs_sftp_flock(struct fuse_open_header_s *oh, struct fuse_request_s *request, uint64_t lo, unsigned char type)
{
    reply_VFS_error(request, ENOSYS);
}

void _fs_sftp_getlock(struct fuse_open_header_s *oh, struct fuse_request_s *request, struct flock *flock)
{
    reply_VFS_error(request, ENOSYS);
}

void _fs_sftp_setlock(struct fuse_open_header_s *oh, struct fuse_request_s *request, struct flock *flock, uint64_t lo, unsigned int flags)
{
    reply_VFS_error(request, ENOSYS);
}

