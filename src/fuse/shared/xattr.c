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

#include "interface/sftp.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

void _fs_shared_setxattr(struct service_context_s *context, struct fuse_request_s *freq, struct fuse_path_s *path, struct inode_s *inode, const char *name, const char *value, size_t size, int flags)
{
    reply_VFS_error(freq, ENODATA);
}

void _fs_shared_getxattr(struct service_context_s *context, struct fuse_request_s *freq, struct fuse_path_s *path, struct inode_s *inode, const char *name, size_t size)
{
    reply_VFS_error(freq, ENODATA);
}

void _fs_shared_listxattr(struct service_context_s *context, struct fuse_request_s *freq, struct fuse_path_s *path, struct inode_s *inode, size_t size)
{
    reply_VFS_error(freq, ENODATA);
}

void _fs_shared_removexattr(struct service_context_s *context, struct fuse_request_s *freq, struct fuse_path_s *path, struct inode_s *inode, const char *name)
{
    reply_VFS_error(freq, ENODATA);
}
