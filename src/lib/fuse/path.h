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

#ifndef _LIB_FUSE_PATH_H
#define _LIB_FUSE_PATH_H

#include "libosns-fuse-public.h"

struct fuse_path_s;

struct cached_path_s {
    unsigned int				refcount;
    struct list_element_s			list;
    struct data_link_s				link;
    unsigned int				len;
    char					path[];
};

/* Prototypes */

int get_path_root_workspace(struct directory_s *directory, struct fuse_path_s *fpath);
int get_path_root_context(struct directory_s *directory, struct fuse_path_s *fpath);

void init_fuse_path(struct fuse_path_s *fpath, unsigned int len);
void append_name_fpath(struct fuse_path_s *fpath, struct name_s *xname);
void start_directory_fpath(struct fuse_path_s *fpath);

void cache_fuse_path(struct directory_s *directory, struct fuse_path_s *fpath);
void release_cached_path(struct directory_s *directory);
void free_cached_path(struct list_element_s *list);

#endif
