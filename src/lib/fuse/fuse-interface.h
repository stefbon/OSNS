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

#ifndef _LIB_FUSE_INTERFACE_H
#define _LIB_FUSE_INTERFACE_H

#include "linux/fuse.h"
#include "list.h"
#include "workspace-interface.h"

struct fuse_request_s;
typedef void (* fuse_cb_t)(struct fuse_request_s *request);
typedef int (* signal_ctx2fuse_t)(struct context_interface_s *i, const char *what, struct ctx_option_s *o);
typedef int (* signal_fuse2ctx_t)(struct context_interface_s *i, const char *what, struct ctx_option_s *o);

#define FUSE_REQUEST_FLAG_INTERRUPTED			1
#define FUSE_REQUEST_FLAG_RESPONSE			2
#define FUSE_REQUEST_FLAG_ERROR				4
#define FUSE_REQUEST_FLAG_CB_INTERRUPTED		8

#define FUSESOCKET_INIT_FLAG_SIZE_INCLUDES_SOCKET	2

struct fuse_request_s {
    struct context_interface_s				*root; /* pointer to root interface/fuse socket */
    void						*followup; /* pointer to related requests following this like sftp request */
    uint32_t						opcode;
    unsigned char					flags;
    struct list_element_s 				list;
    void						(* set_request_flags)(struct fuse_request_s *request);
    unsigned int					error;
    uint64_t						unique;
    uint64_t						ino;
    uint32_t						uid;
    uint32_t						gid;
    uint32_t						pid;
    unsigned int					size;
    char 						buffer[];
};

struct direntry_buffer_s {
    char						*data;
    char						*pos;
    unsigned int					size;
    int							left;
    off_t						offset;
};

/* prototypes */

void notify_VFS_delete(struct context_interface_s *root, uint64_t pino, uint64_t ino, char *name, unsigned int len);
void notify_VFS_create(struct context_interface_s *r, uint64_t pino, char *name);
void notify_VFS_change(struct context_interface_s *r, uint64_t ino, uint32_t mask);
void reply_VFS_data(struct fuse_request_s *r, char *buffer, size_t size);
void reply_VFS_error(struct fuse_request_s *r, unsigned int error);
void reply_VFS_nosys(struct fuse_request_s *r);
void reply_VFS_xattr(struct fuse_request_s *r, size_t size);

void register_fuse_function(struct context_interface_s *root, uint32_t opcode, void (* cb) (struct fuse_request_s *request));

void disable_masking_userspace(struct context_interface_s *root);
mode_t get_masked_permissions(struct context_interface_s *root, mode_t perm, mode_t mask);

unsigned char signal_fuse_request_interrupted(struct context_interface_s *root, uint64_t unique);
unsigned char signal_fuse_request_response(struct context_interface_s *root, uint64_t unique);
unsigned char signal_fuse_request_error(struct context_interface_s *root, uint64_t unique, unsigned int error);

void set_fuse_request_flags_cb(struct fuse_request_s *request, void (* cb)(struct fuse_request_s *request));
void unset_fuse_request_flags_cb(struct fuse_request_s *request);

void set_fuse_request_interrupted(struct fuse_request_s *request, uint64_t ino);

struct common_signal_s *get_fusesocket_signal(struct context_interface_s *root);

struct system_timespec_s *get_fuse_attr_timeout(struct context_interface_s *root);
struct system_timespec_s *get_fuse_entry_timeout(struct context_interface_s *root);
struct system_timespec_s *get_fuse_negative_timeout(struct context_interface_s *root);

int add_direntry_buffer(struct context_interface_s *root, struct direntry_buffer_s *buffer, struct name_s *xname, struct system_stat_s *stat);
int add_direntry_plus_buffer(struct context_interface_s *root, struct direntry_buffer_s *buffer, struct name_s *xname, struct system_stat_s *st);

struct fs_connection_s *get_fs_connection_fuse(struct context_interface_s *root);
signal_ctx2fuse_t get_signal_ctx2fuse(struct context_interface_s *root);
void set_signal_fuse2ctx(struct context_interface_s *root, signal_fuse2ctx_t cb);

void umount_fuse_interface(struct pathinfo_s *pathinfo);
void *create_fuse_interface();
void init_fusesocket(struct context_interface_s *root, size_t size, unsigned char flags);
int connect_fusesocket(struct context_interface_s *root, uid_t uid, char *source, char *mountpoint, char *name, unsigned int *error);
unsigned int get_fuse_buffer_size();
void read_fusesocket_event(int fd, void *ptr, struct event_s *event);

void init_hashtable_fusesocket();

#endif
