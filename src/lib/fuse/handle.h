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

#define FUSE_HANDLE_FLAG_ALLOC					(1 << 0)

#define FUSE_HANDLE_FLAG_OPENFILE				(1 << 1)
#define FUSE_HANDLE_FLAG_OPENDIR				(1 << 2)
#define FUSE_HANDLE_FLAG_RELEASE				(1 << 3)

/* flags to set the cb is supported by the underlying protocol (sftp, ...) */
#define FUSE_HANDLE_FLAG_FGETSTAT				(1 << 4)
#define FUSE_HANDLE_FLAG_FSETSTAT				(1 << 5)
#define FUSE_HANDLE_FLAG_FSYNC					(1 << 6)
#define FUSE_HANDLE_FLAG_FLUSH					(1 << 7)

#define FUSE_HANDLE_FLAG_FSTATAT				(1 << 20)
#define FUSE_HANDLE_FLAG_UNLINKAT				(1 << 21)
#define FUSE_HANDLE_FLAG_READLINKAT				(1 << 22)

#define FUSE_HANDLE_FLAG_PREAD					(1 << 20)
#define FUSE_HANDLE_FLAG_PWRITE					(1 << 21)
#define FUSE_HANDLE_FLAG_LSEEK					(1 << 22)

struct fuse_request_s;
struct fuse_path_s;

struct fuse_handle_s {
    struct service_context_s					*ctx;
    uint64_t							ino;
    struct list_element_s					list;
    unsigned int						refcount;
    unsigned int						flags;
    uint64_t							fh;
    unsigned int						pathlen;

    struct _handle_cb_s {
	/* int							(* fgetstat)(struct fuse_handle_s *h, struct fuse_request_s *r, struct system_stat_s *stat);
	int							(* fsetstat)(struct fuse_handle_s *h, struct fuse_request_s *r, struct system_stat_s *stat2set, struct system_stat_s *stat);
	int							(* fsync)(struct fuse_handle_s *h, struct fuse_request_s *r, unsigned int flags);
	int							(* flush)(struct fuse_handle_s *h, struct fuse_request_s *r, unsigned int flags);*/
	void							(* release)(struct fuse_handle_s *h);
	/*union _handle_cb_u {
	    struct dir_handle_cb_s {
		int						(* fstatat)(struct fuse_handle_s *h, struct fuse_request_s *r, struct fuse_path_s *fpath, unsigned int mask, unsigned int property, unsigned int flags, char *buffer, unsigned int size);
		int						(* unlinkat)(struct fuse_handle_s *h, struct fuse_request_s *r, struct fuse_path_s *fpath, unsigned int flags);
		int						(* readlinkat)(struct fuse_handle_s *h, struct fuse_request_s *r, struct fuse_path_s *fpath, struct fs_location_path_s *target);
	    } dir;
	    struct file_hande_cb_s {
		int						(* pread)(struct fuse_handle_s *h, struct fuse_request_s *r, char *buffer, unsigned int size, off_t off);
		int						(* pwrite)(struct fuse_handle_s *h, struct fuse_request_s *r, char *buffer, unsigned int size, off_t off);
		int						(* lseek)(struct fuse_handle_s *h, struct fuse_request_s *r, off_t off, int whence);
	    } file;
	} type;*/
    } cb;

    unsigned int						len;
    char							name[];
};

/* prototypes */

void init_fuse_handle_hashtable();
struct fuse_handle_s *get_fuse_handle(struct service_context_s *ctx, uint64_t ino, unsigned int flag);
void use_fuse_handle(struct fuse_handle_s *handle);
void post_fuse_handle(struct fuse_handle_s *handle, unsigned int flag);
void release_fuse_handle(struct fuse_handle_s *handle);

void init_fuse_handle(struct fuse_handle_s *handle, unsigned int type, char *name, unsigned int len);
struct fuse_handle_s *create_fuse_handle(struct service_context_s *ctx, uint64_t ino, unsigned int type, char *name, unsigned int len, uint64_t fh);

#endif
