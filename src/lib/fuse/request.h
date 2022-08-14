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

#ifndef LIB_FUSE_REQUEST_H
#define LIB_FUSE_REQUEST_H

#include "libosns-interface.h"

#include "dentry.h"
#include "config.h"
#include "opendir.h"
#include "openfile.h"
#include "utils-public.h"
#include "handle.h"

#define FUSE_REQUEST_CB_MAX					50

#define FS_SERVICE_FLAG_VIRTUAL					1
#define FS_SERVICE_FLAG_ROOT					2
#define FS_SERVICE_FLAG_DIR					4
#define FS_SERVICE_FLAG_NONDIR					8

struct service_context_s;
struct fuse_request_s;

struct fuse_direntry_s {
    struct list_element_s					list;
    struct entry_s						*entry;
};

/* fuse path relative to the root of a service context */

struct fuse_path_s {
    struct service_context_s 					*context;
    char							*pathstart;
    unsigned int						len;
    char							path[];
};

/* union of fs calls. types:
    - dir
    - nondir
*/

struct fuse_fs_s {

    unsigned int flags;

    void (* forget) (struct service_context_s *context, struct inode_s *inode);

    void (* getattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);
    void (* setattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat);
    void (* access) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, unsigned int mask);
    void (* readlink) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);

    union {
	struct nondir_fs_s {

	    void (*open) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags);

	} nondir;
	struct dir_fs_s {

	    void (*use_fs)(struct service_context_s *context, struct inode_s *inode);

	    void (*lookup) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len);
	    void (*create) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *name, unsigned int len, unsigned int flags, mode_t mode, mode_t mask);
	    void (*mkdir) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode,  const char *name, unsigned int len, mode_t mode, mode_t umask);
	    void (*mknod) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode,  const char *name, unsigned int len, mode_t mode, dev_t rdev, mode_t umask);
	    void (*symlink) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode,  const char *name, unsigned int l0, const char *link, unsigned int l1);

	    void (*unlink) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len);
	    void (*rmdir) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, unsigned int len);
	    void (*rename) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, struct inode_s *inode_new, const char *newname, unsigned int flags);

	    void (*opendir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags);

	} dir;

    } type;

    void (*setxattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, const char *value, size_t size, int flags);
    void (*getxattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, size_t size);
    void (*listxattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, size_t size);
    void (*removexattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name);

    void (*statfs)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);

};

#define FUSE_REQUEST_FLAG_INTERRUPTED		1
#define FUSE_REQUEST_FLAG_ERROR			2

struct fuse_request_s {
    struct context_interface_s			*interface;
    void					*ptr;
    struct osns_socket_s			*sock;
    unsigned int				opcode;
    uint64_t					ino;
    unsigned int				uid;
    unsigned int				gid;
    unsigned int				pid;
    uint64_t					unique;
    struct list_element_s			list;
    unsigned int				flags;
    void					(* set_flags)(struct fuse_request_s *r);
    unsigned int				size;
};

/* prototypes */

void register_fuse_functions(struct context_interface_s *interface, void (* add)(struct context_interface_s *interface, unsigned int ctr, unsigned int code, void (* cb)(struct fuse_request_s *request, char *data)));

int reply_VFS_data(struct fuse_request_s *request, char *data, unsigned int size);
int reply_VFS_error(struct fuse_request_s *request, unsigned int errcode);
int notify_VFS_delete(struct context_interface_s *interface, uint64_t pino, uint64_t ino, char *name, unsigned int len);

#endif
