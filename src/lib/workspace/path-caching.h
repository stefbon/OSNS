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

// struct fuse_path_s {
//    struct service_context_s 			*context;
//    char					*pathstart;
//    unsigned int				len;
//    char					path[];
//};

#define GETPATH_TYPE_0						1
#define GETPATH_TYPE_1						2
#define GETPATH_TYPE_X						3
#define GETPATH_TYPE_CUSTOM					4

#define GETPATH_TIMEOUT_DEFAULT					15

struct getpath_s {
    unsigned char				type;
    unsigned int				(* get_pathlen)(struct service_context_s *ctx, struct directory_s *d);
    void					(* append_path)(struct service_context_s *ctx, struct directory_s *d, struct fuse_path_s *fpath);
    char					buffer[];
};

/* Prototypes */

int get_path_root(struct directory_s *directory, struct fuse_path_s *fpath);
int get_service_path_default(struct directory_s *directory, struct fuse_path_s *fpath);

void init_fuse_path(struct fuse_path_s *fpath, unsigned int len);
void append_name_fpath(struct fuse_path_s *fpath, struct name_s *xname);

void get_service_context_path(struct service_context_s *ctx, struct directory_s *directory, struct fuse_path_s *fpath);
char *get_pathinfo_fpath(struct fuse_path_s *fpath, unsigned int *p_len);

void set_directory_pathcache_zero(struct directory_s *d);
void set_directory_pathcache_one(struct directory_s *d);
void set_directory_pathcache_x(struct directory_s *d);

void set_directory_pathcache(struct service_context_s *ctx, struct directory_s *d, struct fuse_path_s *fpath);
void set_directory_getpath(struct directory_s *d);

void release_directory_pathcache(struct directory_s *d);

#endif
