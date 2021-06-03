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

#define _FS_CREATE_ENTRY_MAYEXIST				1
#define _FS_CREATE_ENTRY_CACHE					2

struct service_fs_s *get_service_context_fs(struct service_context_s *c);

int symlink_generic_validate(struct service_context_s *context, char *target);

void _fs_common_cached_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);
void _fs_common_virtual_lookup(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len);

void _fs_common_getattr(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);
void _fs_common_cached_create(struct service_context_s *context, struct fuse_request_s *request, struct fuse_openfile_s *openfile);

struct entry_s *get_fuse_direntry_virtual(struct fuse_opendir_s *opendir, struct list_header_s *h, struct fuse_request_s *request);

void _fs_common_virtual_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags);
void _fs_common_virtual_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
void _fs_common_virtual_readdirplus(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
void _fs_common_virtual_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *request);
void _fs_common_virtual_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync);

struct entry_s *_fs_common_create_entry(struct workspace_mount_s *workspace, struct entry_s *parent, struct name_s *xname, struct stat *st, unsigned int size, unsigned int flags, unsigned int *error);
struct entry_s *_fs_common_create_entry_unlocked(struct workspace_mount_s *workspace, struct directory_s *directory, struct name_s *xname, struct stat *st, unsigned int size, unsigned int flags, unsigned int *error);

void _fs_common_statfs(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, uint64_t blocks, uint64_t bfree, uint64_t bavail, uint32_t bsize);

void use_service_fs(struct service_context_s *context, struct inode_s *inode);

#endif