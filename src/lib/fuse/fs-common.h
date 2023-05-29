/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_FUSE_FS_COMMON_H
#define _LIB_FUSE_FS_COMMON_H

void _fs_common_cached_lookup(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode);
void _fs_common_virtual_lookup(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len);

void _fs_common_getattr(struct fuse_request_s *request, struct system_stat_s *stat);

void _fs_common_virtual_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags);
void _fs_common_virtual_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
void _fs_common_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);

void _fs_common_virtual_releasedir(struct fuse_open_header_s *oh, struct fuse_request_s *request, unsigned int flags, uint64_t lo);
void _fs_common_virtual_fsyncdir(struct fuse_open_header_s *oh, struct fuse_request_s *request, unsigned int flags);

void _fs_common_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);

void _fs_common_remove_nonsynced_dentries(struct fuse_opendir_s *opendir);


#endif