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

#ifndef _LIB_FUSE_FS_SERVICE_BROWSE_H
#define _LIB_FUSE_FS_SERVICE_BROWSE_H

struct browse_service_fs_s {

    unsigned int (* get_name)(struct service_context_s *context, char *buffer, unsigned int len);

    void (* lookup)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct name_s *xname);

    void (* access)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, unsigned int mask);

    void (* getattr)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);
    void (* setattr)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct system_stat_s *stat);

    void (* readlink)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);

    void (* mkdir)(struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct system_stat_s *stat);
    void (* mknod)(struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct system_stat_s *stat);
    void (* symlink)(struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct fs_location_path_s *link);
    int  (* symlink_validate)(struct service_context_s *context, char *target, struct fs_location_path_s *sub);

    void (* unlink)(struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry);
    void (* rmdir)(struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry);

    void (* rename)(struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry, struct entry_s **n_entry, unsigned int flags);

    void (* open)(struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags);
    void (* create)(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct entry_s *entry, struct system_stat_s *stat, unsigned int flags);

    void (* opendir)(struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned int flags);

    void (* setxattr)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, const char *value, size_t size, int flags);
    void (* getxattr)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, size_t size);
    void (* listxattr)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, size_t size);
    void (* removexattr)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name);

    void (* statfs)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);

};

/* Prototypes */

void use_service_browse_fs(struct service_context_s *c, struct inode_s *inode);
void init_service_browse_fs();
void set_service_browse_fs(struct fuse_fs_s *fs);

#endif
