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

#ifndef _LIB_FUSE_PATH_CACHING_H
#define _LIB_FUSE_PATH_CACHING_H

#include "fuse.h"
#include "workspaces.h"

/* fuse path relative to the root of a service context */

struct fuse_path_s {
    struct service_context_s 			*context;
    char					*pathstart;
    char					*path;
    unsigned int				len;
};

// Prototypes

int get_path_root(struct inode_s *inode, struct fuse_path_s *fpath);
int get_service_path_default(struct inode_s *inode, struct fuse_path_s *fpath);
unsigned int add_name_path(struct fuse_path_s *fpath, struct name_s *xname);
void init_fuse_path(struct fuse_path_s *fpath, char *path, unsigned int len);

void set_directory_pathcache(struct directory_s *directory, const char *what, char *path, struct service_context_s *c);
void release_directory_pathcache(struct directory_s *directory);
void remove_unused_pathcaches(struct service_context_s *c);

#endif
