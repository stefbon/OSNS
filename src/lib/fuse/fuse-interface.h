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
typedef int (* signal_ctx2fuse_t)(void **p_ptr, const char *what, struct ctx_option_s *o);
typedef int (* signal_fuse2ctx_t)(void *ptr, const char *what, struct ctx_option_s *o);

#define FUSE_REQUEST_FLAG_INTERRUPTED			1
#define FUSE_REQUEST_FLAG_RESPONSE			2
#define FUSE_REQUEST_FLAG_ERROR				4
#define FUSE_REQUEST_FLAG_CB_INTERRUPTED		8

#define FUSESOCKET_INIT_FLAG_SIZE_INCLUDES_SOCKET	2

struct fuse_request_s {
    char						*ptr; /* pointer to fuse socket */
    void						*ctx; /* pointer to related requests following this like sftp request */
    void						*data; /* additional data for set_request_flags like sftp service context */
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

void notify_VFS_delete(char *ptr, uint64_t pino, uint64_t ino, char *name, unsigned int len);
void notify_VFS_create(char *ptr, uint64_t pino, char *name);
void notify_VFS_change(char *ptr, uint64_t ino, uint32_t mask);
void reply_VFS_data(struct fuse_request_s *r, char *buffer, size_t size);
void reply_VFS_error(struct fuse_request_s *r, unsigned int error);
void reply_VFS_nosys(struct fuse_request_s *r);
void reply_VFS_xattr(struct fuse_request_s *r, size_t size);

void register_fuse_function(char *ptr, uint32_t opcode, void (* cb) (struct fuse_request_s *request));

void disable_masking_userspace(char *ptr);
mode_t get_masked_permissions(char *ptr, mode_t perm, mode_t mask);

unsigned char signal_fuse_request_interrupted(char *ptr, uint64_t unique);
unsigned char signal_fuse_request_response(char *ptr, uint64_t unique);
unsigned char signal_fuse_request_error(char *ptr, uint64_t unique, unsigned int error);

void set_fuse_request_flags_cb(struct fuse_request_s *request, void (* cb)(struct fuse_request_s *request));
void set_fuse_request_interrupted(struct fuse_request_s *request, uint64_t ino);

pthread_mutex_t *get_fuse_pthread_mutex(char *ptr);
pthread_cond_t *get_fuse_pthread_cond(char *ptr);

struct timespec *get_fuse_attr_timeout(char *ptr);
struct timespec *get_fuse_entry_timeout(char *ptr);
struct timespec *get_fuse_negative_timeout(char *ptr);

int add_direntry_buffer(void *ptr, struct direntry_buffer_s *buffer, struct name_s *xname, struct stat *st);
int add_direntry_plus_buffer(void *ptr, struct direntry_buffer_s *buffer, struct name_s *xname, struct stat *st);

struct fs_connection_s *get_fs_connection_fuse(char *ptr);
signal_ctx2fuse_t get_signal_ctx2fuse(char *ptr);
void set_signal_fuse2ctx(char *ptr, signal_fuse2ctx_t cb);

void umount_fuse_interface(struct pathinfo_s *pathinfo);
void *create_fuse_interface();
void init_fusesocket(char *ptr, void *ctx, size_t size, unsigned char flags);
int connect_fusesocket(char *ptr, uid_t uid, char *source, char *mountpoint, char *name, unsigned int *error);
unsigned int get_fuse_buffer_size();
int read_fusesocket_event(int fd, void *ptr, uint32_t events);

void init_hashtable_fusesocket();

#endif
