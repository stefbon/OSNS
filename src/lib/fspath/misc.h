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

#ifndef LIB_FSPATH_MISC_H
#define LIB_FSPATH_MISC_H

/* prototypes */

unsigned int fs_path_get_length(struct fs_path_s *path);
char *fs_path_get_start(struct fs_path_s *path);

int fs_path_get_target_unix_symlink(struct fs_path_s *path, struct fs_path_s *target);

unsigned int fs_path_remove_unneeded_elements(struct fs_path_s *path);

#endif
