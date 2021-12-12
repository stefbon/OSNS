/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef _FUSE_SFTP_READLINK_H
#define _FUSE_SFTP_READLINK_H

unsigned int check_target_symlink_sftp_client(struct context_interface_s *interface, struct fs_location_path_s *syml, struct fs_location_path_s *sub);

void _fs_sftp_readlink(struct service_context_s *c, struct fuse_request_s *f, struct inode_s *inode, struct pathinfo_s *pathinfo);
void _fs_sftp_symlink(struct service_context_s *c, struct fuse_request_s *f, struct entry_s *entry, struct pathinfo_s *pathinfo, struct fs_location_path_s *target);
int _fs_sftp_symlink_validate(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, struct fs_location_path_s *sub);

void _fs_sftp_readlink_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo);
void _fs_sftp_symlink_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct fs_location_path_s *target);
int _fs_sftp_symlink_validate_disconnected(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, struct fs_location_path_s *sub);

#endif
