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

#ifndef _LIB_FUSE_FS_CREATE_H
#define _LIB_FUSE_FS_CREATE_H

void _fs_common_cached_create(struct service_context_s *context, struct fuse_request_s *request, struct fuse_openfile_s *openfile);
struct entry_s *_fs_common_create_entry(struct workspace_mount_s *workspace, struct entry_s *parent, struct name_s *xname, struct system_stat_s *stat, unsigned int size, unsigned int flags, unsigned int *error);
struct entry_s *_fs_common_create_entry_unlocked(struct workspace_mount_s *workspace, struct directory_s *directory, struct name_s *xname, struct system_stat_s *stat, unsigned int size, unsigned int flags, unsigned int *error);

#endif