/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_WORKSPACE_CONTEXT_H
#define _LIB_WORKSPACE_CONTEXT_H

#include "misc.h"
#include "fuse.h"
#include "workspaces.h"

#define SERVICE_CTX_TYPE_DUMMY			0
#define SERVICE_CTX_TYPE_WORKSPACE		1
#define SERVICE_CTX_TYPE_FILESYSTEM		2
#define SERVICE_CTX_TYPE_CONNECTION		3
#define SERVICE_CTX_TYPE_SOCKET			4

#define SERVICE_CTX_FLAG_ALLOC			1

struct service_context_s {
    unsigned char				type;
    unsigned char				flags;
    char					name[32];
    struct workspace_mount_s			*workspace;
    struct service_context_s			*parent;
    unsigned int				fscount;
    unsigned int				serviceid;
    pthread_mutex_t				mutex;
    union {
	struct filesystem_context_s {
	    struct service_fs_s				*fs; 		/* the fs used for this service: pseudo, sftp, webdav, smb ... */
	    struct inode_s 				*inode; 	/* the inode the service is attached to */
	    struct list_header_s			pathcaches;
	} filesystem;
    } service;
    struct list_element_s			list;
    struct context_interface_s			interface;
};

struct service_fs_s {

    void (*lookup_existing) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo);
    void (*lookup_new) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo);

    void (*getattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct pathinfo_s *pathinfo);
    void (*setattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct pathinfo_s *pathinfo, struct stat *st, unsigned int set);

    void (*readlink) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct pathinfo_s *pathinfo);

    void (*mkdir) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct stat *st);
    void (*mknod) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct stat *st);
    void (*symlink) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo, const char *link);
    int  (*symlink_validate)(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, char **remote_target);

    void (*unlink) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry, struct pathinfo_s *pathinfo);
    void (*rmdir) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry, struct pathinfo_s *pathinfo);

    void (*rename) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry, struct pathinfo_s *pathinfo, struct entry_s **n_entry, struct pathinfo_s *n_pathinfo, unsigned int flags);

    void (*open) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct pathinfo_s *pathinfo, unsigned int flags);
    void (*read) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
    void (*write) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
    void (*flush) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lock_owner);
    void (*fsync) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char datasync);
    void (*release) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lock_owner);
    void (*create) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct stat *st, unsigned int flags);

    void (*fgetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request);
    void (*fsetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct stat *st, unsigned int set);

    void (*getlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
    void (*setlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
    void (*setlockw) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);

    void (*flock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char type);

    void (*opendir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, struct pathinfo_s *pathinfo, unsigned int flags);
    void (*readdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
    void (*readdirplus) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
    void (*releasedir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request);
    void (*fsyncdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync);

    void (*setxattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name, const char *value, size_t size, int flags);
    void (*getxattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name, size_t size);
    void (*listxattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, size_t size);
    void (*removexattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name);

    void (*statfs)(struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo);

};

/* prototypes */

struct service_context_s *get_service_context(struct context_interface_s *interface);
struct service_context_s *get_next_service_context(struct workspace_mount_s *workspace, struct service_context_s *context);
struct context_interface_s *get_next_context_interface(struct context_interface_s *reference, struct context_interface_s *interface);

struct service_context_s *create_service_context(struct workspace_mount_s *workspace, struct service_context_s *parent, struct interface_list_s *ilist, unsigned char type, struct service_context_s *primary);
void free_service_context(struct service_context_s *context);

void *get_root_ptr_context(struct service_context_s *context);
struct service_context_s *get_root_context(struct service_context_s *context);
struct osns_user_s *get_user_context(struct service_context_s *context);

struct service_context_s *get_container_context(struct list_element_s *list);
struct service_context_s *get_workspace_context(struct workspace_mount_s *workspace);

#endif
