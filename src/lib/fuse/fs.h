				/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_FUSE_FS_H
#define _LIB_FUSE_FS_H

#include <fcntl.h>

#include "dentry.h"
#include "fuse-interface.h"

#define FS_SERVICE_FLAG_VIRTUAL					1
#define FS_SERVICE_FLAG_ROOT					2
#define FS_SERVICE_FLAG_DIR					4
#define FS_SERVICE_FLAG_NONDIR					8

#define _FUSE_OPENDIR_FLAG_NONEMPTY				1 << 0
#define _FUSE_OPENDIR_FLAG_VIRTUAL				1 << 1

#define _FUSE_OPENDIR_FLAG_READDIR_FINISH			1 << 10
#define _FUSE_OPENDIR_FLAG_READDIR_INCOMPLETE			1 << 11
#define _FUSE_OPENDIR_FLAG_READDIR_ERROR			1 << 12
#define _FUSE_OPENDIR_FLAG_QUEUE_READY				1 << 13
#define _FUSE_OPENDIR_FLAG_THREAD				1 << 14
#define _FUSE_OPENDIR_FLAG_RELEASE				1 << 15

#define _FUSE_OPENDIR_FLAG_HIDE_SPECIALFILES			1 << 20
#define _FUSE_OPENDIR_FLAG_HIDE_DOTFILES			1 << 21
#define _FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS		1 << 22
#define _FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS			1 << 23

struct fuse_openfile_s {
    struct service_context_s 					*context;
    struct inode_s						*inode;
    unsigned char						flock;
    unsigned int						error;
    union {
	uint64_t						fd;
	void							*ptr;
	struct {
	    unsigned int					len;
	    char						*name;
	} name;
    } handle;
};

struct fuse_buffer_s {
    char							*data;
    char							*pos;
    int								left;
    unsigned int						size;
    signed char							eof;
    unsigned int						count;
};

struct fuse_direntry_s {
    struct list_element_s					list;
    struct entry_s						*entry;
};

struct fuse_opendir_s {
    uint32_t							flags;
    struct service_context_s 					*context;
    struct inode_s						*inode;
    ino_t							ino;
    unsigned int 						error;
    unsigned int						count_created;
    unsigned int						count_found;
    void 							(* readdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
    void 							(* readdirplus) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
    void 							(* releasedir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request);
    void 							(* fsyncdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync);
    signed char							(* hidefile)(struct fuse_opendir_s *opendir, struct entry_s *entry);
    struct entry_s 						*(* get_fuse_direntry)(struct fuse_opendir_s *opendir, struct list_header_s *h, struct fuse_request_s *request);
    int 							(* add_direntry_buffer)(void *ptr, struct direntry_buffer_s *buffer, struct name_s *xname, struct stat *st);
    void							(* clear)(struct fuse_opendir_s *opendir);
    struct list_header_s					entries;
    struct list_header_s					symlinks;
    pthread_mutex_t						*mutex;
    pthread_cond_t						*cond;
    unsigned char						threads;
    union {
	uint64_t						fd;
	void							*ptr;
	struct {
	    unsigned int					len;
	    char						*name;
	} name;
    } handle;
    union {
	uint64_t						nr;
	void							*ptr;
	struct fuse_buffer_s					buffer;
    } data;
};

/* fuse path relative to the root of a service context */

struct fuse_path_s {
    struct service_context_s 					*context;
    char							*pathstart;
    unsigned int						len;
    char							path[];
};

struct entry_ops_s {
    unsigned int 						(* get_pathlen)(struct service_context_s *c, struct entry_s *entry);
    void							(* append_path)(struct service_context_s *c, struct entry_s *entry, struct fuse_path_s *fp);
};

/* union of fs calls. types:
    - dir
    - nondir
*/

struct fuse_fs_s {

    unsigned int flags;

    int (*lock_datalink)(struct inode_s *inode);
    int (*unlock_datalink)(struct inode_s *inode);
    void (* get_data_link)(struct inode_s *inode, struct data_link_s **link);

    void (*forget) (struct inode_s *inode);

    void (*getattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);
    void (*setattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct stat *st, unsigned int fuse_set);

    union {
	struct nondir_fs_s {

	    void (*readlink) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);

	    void (*open) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags);
	    void (*read) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
	    void (*write) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
	    void (*flush) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lock_owner);
	    void (*fsync) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char datasync);
	    void (*release) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lock_owner);

	    void (*fgetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request);
	    void (*fsetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct stat *st, int fuse_set);

	    void (*getlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
	    void (*setlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
	    void (*setlockw) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);

	    void (*flock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char type);

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
	    void (*readdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
	    void (*readdirplus) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
	    void (*releasedir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request);
	    void (*fsyncdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync);
	    struct entry_s *(* get_fuse_direntry)(struct fuse_opendir_s *o, struct list_header_s *h, struct fuse_request_s *r);

	} dir;

    } type;

    void (*setxattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, const char *value, size_t size, int flags);
    void (*getxattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name, size_t size);
    void (*listxattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, size_t size);
    void (*removexattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, const char *name);

    void (*statfs)(struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode);

};

// Prototypes

void copy_fuse_fs(struct fuse_fs_s *to, struct fuse_fs_s *from);
unsigned char fuse_request_interrupted(struct fuse_request_s *request);

int fs_lock_datalink(struct inode_s *inode);
int fs_unlock_datalink(struct inode_s *inode);

void fs_get_data_link(struct inode_s *inode, struct data_link_s **p_link);

void fuse_fs_forget(struct fuse_request_s *request);
void fuse_fs_forget_multi(struct fuse_request_s *request);
void fuse_fs_lookup(struct fuse_request_s *request);
void fuse_fs_getattr(struct fuse_request_s *request);
void fuse_fs_setattr(struct fuse_request_s *request);
void fuse_fs_readlink(struct fuse_request_s *request);
void fuse_fs_mkdir(struct fuse_request_s *request);
void fuse_fs_mknod(struct fuse_request_s *request);
void fuse_fs_symlink(struct fuse_request_s *request);
void fuse_fs_unlink(struct fuse_request_s *request);
void fuse_fs_rmdir(struct fuse_request_s *request);
void fuse_fs_rename(struct fuse_request_s *request);
void fuse_fs_link(struct fuse_request_s *request);
void fuse_fs_open(struct fuse_request_s *request);
void fuse_fs_read(struct fuse_request_s *request);
void fuse_fs_write(struct fuse_request_s *request);
void fuse_fs_flush(struct fuse_request_s *request);
void fuse_fs_fsync(struct fuse_request_s *request);
void fuse_fs_release(struct fuse_request_s *request);
void fuse_fs_create(struct fuse_request_s *request);
void fuse_fs_opendir(struct fuse_request_s *request);
void fuse_fs_readdir(struct fuse_request_s *request);
void fuse_fs_readdirplus(struct fuse_request_s *request);
void fuse_fs_releasedir(struct fuse_request_s *request);
void fuse_fs_fsyncdir(struct fuse_request_s *request);
void fuse_fs_getlock(struct fuse_request_s *request);
void fuse_fs_lock(struct fuse_request_s *request);
void fuse_fs_lock_wait(struct fuse_request_s *request);
void fuse_fs_setxattr(struct fuse_request_s *request);
void fuse_fs_getxattr(struct fuse_request_s *request);
void fuse_fs_listxattr(struct fuse_request_s *request);
void fuse_fs_removexattr(struct fuse_request_s *request);
void fuse_fs_statfs(struct fuse_request_s *request);
void fuse_fs_interrupt(struct fuse_request_s *request);
void fuse_fs_init (struct fuse_request_s *request);
void fuse_fs_destroy (struct fuse_request_s *request);

void register_fuse_functions(struct context_interface_s *interface);

void init_fuse_buffer(struct fuse_buffer_s *buffer, char *data, unsigned int size, unsigned int count, signed char eof);
void clear_fuse_buffer(struct fuse_buffer_s *buffer);

void queue_fuse_direntry(struct fuse_opendir_s *opendir, struct entry_s *entry);
void queue_fuse_direntry_symlinks(struct fuse_opendir_s *opendir, struct entry_s *entry);

void queue_fuse_direntries_virtual(struct fuse_opendir_s *opendir);

struct entry_s *get_fuse_direntry_common(struct fuse_opendir_s *opendir, struct list_header_s *header, struct fuse_request_s *request);
struct entry_s *_fs_service_get_fuse_direntry(struct fuse_opendir_s *opendir, struct fuse_request_s *request);
struct entry_s *_fs_service_get_fuse_symlinks(struct fuse_opendir_s *opendir, struct fuse_request_s *request);

void set_flag_fuse_opendir(struct fuse_opendir_s *opendir, uint32_t flag);
void set_get_fuse_direntry_common(struct fuse_opendir_s *opendir);
void finish_get_fuse_direntry(struct fuse_opendir_s *opendir);

#endif
