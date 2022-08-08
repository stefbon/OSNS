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

#ifndef LIB_FUSE_HANDLE_H
#define LIB_FUSE_HANDLE_H

#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"

#include "config.h"
#include "dentry.h"
#include "utils-public.h"

#define FUSE_HANDLE_FLAG_OPENFILE				1
#define FUSE_HANDLE_FLAG_OPENDIR				2
#define FUSE_HANDLE_FLAG_RELEASE				4

struct fuse_handle_s {
    struct service_context_s					*ctx;
    uint64_t							ino;
    struct list_element_s					list;
    unsigned int						refcount;
    unsigned int						flags;
    uint64_t							fh;
    unsigned int						len;
    char							name[];
};

/* prototypes */

void init_fuse_handle_hashtable();
struct fuse_handle_s *get_fuse_handle(struct service_context_s *ctx, uint64_t ino);
void post_fuse_handle(struct fuse_handle_s *handle, unsigned int flag);
void release_fuse_handle(struct fuse_handle_s *handle);
struct fuse_handle_s *create_fuse_handle(struct service_context_s *ctx, uint64_t ino, unsigned int type, char *name, unsigned int len, uint64_t fh);

#endif
