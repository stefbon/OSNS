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

#ifndef LIB_FUSE_OPENFILE_H
#define LIB_FUSE_OPENFILE_H

#include "libosns-interface.h"
#include "config.h"
#include "dentry.h"

#define FUSE_OPENFILE_FLAG_CREATE				1

#define FUSE_OPENFILE_LOCK_FLAG_FLOCK				1
#define FUSE_OPENFILE_LOCK_FLAG_WAIT				2

#define FUSE_OPENFILE_FSYNC_FLAG_DATASYNC			1

#define FUSE_OPENFILE_WRITE_CACHE				1
#define FUSE_OPENFILE_WRITE_KILL_PRIV				4

#define FUSE_OPENFILE_RELEASE_FLUSH				1
#define FUSE_OPENFILE_RELEASE_FLOCK_UNLOCK			2

struct fuse_request_s;

struct fuse_openfile_s {
    struct service_context_s 					*context;
    struct inode_s						*inode;
    unsigned int						flags;
    unsigned int						error;
    void							(* read) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
    void							(* write) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
    void							(* flush) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lock_owner);
    void							(* fsync) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags);
    void							(* release) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lock_owner);
    void							(* lseek) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, off_t off, int whence);
    void							(* fgetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request);
    void							(* fsetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct system_stat_s *stat);
    void							(* getlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
    void							(* setlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock, unsigned int flags);
    void							(* flock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char type);
    struct fuse_handle_s					*handle;
};

/* prototypes */

void init_fuse_openfile(struct fuse_openfile_s *openfile, struct service_context_s *ctx, struct inode_s *inode);

#endif
