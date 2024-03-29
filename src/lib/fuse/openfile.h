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
#include "request.h"

#define FUSE_OPEN_CREATE					1

#define FUSE_OPEN_LOCK_FLAG_FLOCK				1
#define FUSE_OPEN_LOCK_FLAG_WAIT				2

#define FUSE_OPEN_FSYNC_DATASYNC				1

#define FUSE_OPEN_WRITE_CACHE					1
#define FUSE_OPEN_WRITE_KILL_PRIV				4

#define FUSE_OPEN_RELEASE_FLUSH					1
#define FUSE_OPEN_RELEASE_FLOCK_UNLOCK				2

struct fuse_request_s;

struct fuse_openfile_s {
    struct fuse_open_header_s					header;
    unsigned int						flags;
    unsigned int						error;
    void							(* read) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lo);
    void							(* write) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lo);
    void							(* lseek) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, off_t off, int whence);
};

/* prototypes */

void init_fuse_openfile(struct fuse_openfile_s *openfile, struct service_context_s *ctx, struct inode_s *inode);

#endif
