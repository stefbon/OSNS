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

#ifndef _LIB_FUSE_UTILS_CREATE_H
#define _LIB_FUSE_UTILS_CREATE_H

#include "dentry.h"
#include "opendir.h"

struct create_entry_s {
    struct name_s			*name;
    union tree_u {
	struct entry_s			*parent;
	struct directory_s 		*directory;
	struct fuse_opendir_s		*opendir;
    } tree;
    struct service_context_s		*context;
    struct entry_s 			*(*cb_create_entry)(struct name_s *n);
    struct inode_s			*(*cb_create_inode)();
    struct entry_s			*(*cb_insert_entry)(struct directory_s *d, struct name_s *n, unsigned int f, unsigned int *error);
    void				(*cb_created)(struct entry_s *e, struct create_entry_s *ce);
    void				(*cb_found)(struct entry_s *e, struct create_entry_s *ce);
    void				(*cb_error)(struct entry_s *p, struct name_s *n, struct create_entry_s *ce, unsigned int e);
    unsigned int			(*cb_cache_size)(struct create_entry_s *ce);
    int					(*cb_check)(struct create_entry_s *ce);
    void				(*cb_cache_created)(struct entry_s *e, struct create_entry_s *ce);
    void				(*cb_cache_found)(struct entry_s *e, struct create_entry_s *ce);
    void				(*cb_adjust_pathmax)(struct create_entry_s *ce);
    void				(*cb_context_created)(struct create_entry_s *ce, struct entry_s *e);
    void				(*cb_context_found)(struct create_entry_s *ce, struct entry_s *e);
    struct directory_s 			*(* get_directory)(struct create_entry_s *ce);
    unsigned int			pathlen;
    unsigned int			cache_size;
    struct _cache_s {
	struct system_stat_s		stat;
	void				*ptr;
	struct attr_buffer_s		*abuff;
    } cache;
    unsigned int			flags;
    void				*ptr;
    unsigned int			error;
};

/* prototypes */

void init_create_entry(struct create_entry_s *ce, struct name_s *n, struct entry_s *p, struct directory_s *d, struct fuse_opendir_s *fo, struct service_context_s *c, struct system_stat_s *st, void *ptr);
struct entry_s *create_entry_extended(struct create_entry_s *ce);

void disable_ce_extended_adjust_pathmax(struct create_entry_s *ce);
void enable_ce_extended_adjust_pathmax(struct create_entry_s *ce);
void enable_ce_extended_batch(struct create_entry_s *ce);
void disable_ce_extended_batch(struct create_entry_s *ce);
void disable_ce_extended_cache(struct create_entry_s *ce);

#endif
