/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021
  Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_FUSE_FS_FILESYSTEM_PATH_H
#define _LIB_FUSE_FS_FILESYSTEM_PATH_H

/* Prototypes */

void _fs_service_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
void _fs_service_readdirplus(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
void _fs_service_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync);
void _fs_service_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *request);

void init_service_fs();
void use_service_path_fs(struct inode_s *inode);
void set_service_fs(struct fuse_fs_s *fs);
unsigned char check_service_path_fs(struct inode_s *inode);

#endif
