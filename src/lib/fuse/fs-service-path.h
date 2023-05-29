/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021
  Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_FUSE_FS_SERVICE_PATH_H
#define _LIB_FUSE_FS_SERVICE_PATH_H

struct fuse_path_s;

struct path_service_fs_s {

    unsigned int (* get_name)(struct service_context_s *ctx, char *buffer, unsigned int len);

    void (* lookup)(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct name_s *xname, struct fuse_path_s *path);
    void (* fstatat)(struct fuse_handle_s *handle, struct fuse_request_s *request, struct inode_s *inode, struct name_s *xname, struct fuse_path_s *path);

    void (* access)(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *path, unsigned int mask);
    void (* getattr)(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *path);
    void (* setattr)(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *path, struct system_stat_s *stat);

    void (* readlink)(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *path);

    void (* mkdir)(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s *entry, struct fuse_path_s *path, struct system_stat_s *stat);
    void (* mknod)(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s *entry, struct fuse_path_s *path, struct system_stat_s *stat);
    void (* symlink)(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s *entry, struct fuse_path_s *path, struct fs_location_path_s *link);
    int  (* symlink_validate)(struct service_context_s *ctx, struct fuse_path_s *path, char *target, struct fs_location_path_s *sub);

    void (* unlink)(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s **entry, struct fuse_path_s *path);
    void (* rmdir)(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s **entry, struct fuse_path_s *path);

    void (* rename)(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s **entry, struct fuse_path_s *path, struct entry_s **n_entry, struct fuse_path_s *n_path, unsigned int flags);

    void (* open)(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct fuse_path_s *path, struct system_stat_s *stat, unsigned int flags);
    void (* opendir)(struct fuse_opendir_s *opendir, struct fuse_request_s *request, struct fuse_path_s *path, unsigned int flags);

    void (* setxattr)(struct service_context_s *ctx, struct fuse_request_s *request, struct fuse_path_s *path, struct inode_s *inode, const char *name, const char *value, size_t size, int flags);
    void (* getxattr)(struct service_context_s *ctx, struct fuse_request_s *request, struct fuse_path_s *path, struct inode_s *inode, const char *name, size_t size);
    void (* listxattr)(struct service_context_s *ctx, struct fuse_request_s *request, struct fuse_path_s *path, struct inode_s *inode, size_t size);
    void (* removexattr)(struct service_context_s *ctx, struct fuse_request_s *request, struct fuse_path_s *path, struct inode_s *inode, const char *name);

    void (* statfs)(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *path);

};

/* Prototypes */

void init_service_path_fs();
void use_service_path_fs(struct service_context_s *c, struct inode_s *inode);
void set_service_path_fs(struct fuse_fs_s *fs);
unsigned char check_service_path_fs(struct inode_s *inode);

#endif
