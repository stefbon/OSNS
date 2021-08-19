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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
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

#include "main.h"
#include "log.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

void _fs_sftp_setxattr(struct service_context_s *context, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name, const char *value, size_t size, int flags)
{
    reply_VFS_error(f_request, ENODATA);
}

void _fs_sftp_getxattr(struct service_context_s *context, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name, size_t size)
{
    reply_VFS_error(f_request, ENODATA);
}

void _fs_sftp_listxattr(struct service_context_s *context, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, struct inode_s *inode, size_t size)
{
    reply_VFS_error(f_request, ENODATA);
}

void _fs_sftp_removexattr(struct service_context_s *context, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name)
{
    reply_VFS_error(f_request, ENODATA);
}


