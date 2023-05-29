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

#ifndef LIB_SYSTEM_FSSOCKET_H
#define LIB_SYSTEM_FSSOCKET_H

#include "libosns-datatypes.h"
#include "libosns-fspath.h"

#include "stat.h"

#include "lib/sftp/acl-ace4.h"

struct fs_socket_s;
struct fs_dentry_s;
struct fs_init_s;

#define FS_DENTRY_TYPE_FILE				1
#define FS_DENTRY_TYPE_DIR				2

#define FS_DENTRY_FLAG_EOD				1

#define FS_FSYNC_FLAG_DATA                              1

struct fs_dentry_s {
    uint16_t						flags;
    uint16_t						type;
    uint64_t						ino; /* ino on the server; some filesystems rely on this */
    uint16_t						len;
    char						*name;
};

#define FS_DENTRY_INIT					{0, 0, 0, 0, NULL}

struct fs_socket_ops_s {
    int							(* open)(struct fs_socket_s *ref, struct fs_path_s *path, struct fs_socket_s *s, unsigned int flags, struct fs_init_s *init);
    int							(* fsync)(struct fs_socket_s *s, unsigned int flags);
    int							(* flush)(struct fs_socket_s *s, unsigned int flags);
    int							(* fgetstat)(struct fs_socket_s *s, unsigned int mask, struct system_stat_s *st);
    int							(* fsetstat)(struct fs_socket_s *s, unsigned int mask, struct system_stat_s *st);
    void                                                (* close)(struct fs_socket_s *s);
    union _filesystem_sops_u {
	struct file_sops_s {
	    int						(* pread)(struct fs_socket_s *s, char *buffer, unsigned int size, off_t off);
	    int						(* pwrite)(struct fs_socket_s *s, char *buffer, unsigned int size, off_t off);
	    off_t					(* lseek)(struct fs_socket_s *s, off_t off, int whence);
	} file;
	struct dir_sops_s {
	    int                                         (* get_dentry)(struct fs_socket_s *s, struct fs_dentry_s *dentry);
	    int                                         (* read_dentry)(struct fs_socket_s *s, struct fs_dentry_s *dentry);
	    int						(* fstatat)(struct fs_socket_s *s, struct fs_path_s *path, unsigned int mask, struct system_stat_s *st, unsigned int flags);
	    int						(* unlinkat)(struct fs_socket_s *s, struct fs_path_s *path);
	    int						(* rmdirat)(struct fs_socket_s *s, struct fs_path_s *path);
	    int						(* readlinkat)(struct fs_socket_s *s, struct fs_path_s *path, struct fs_path_s *target);
	} dir;
    } type;
};

#define FS_DIRECTORY_BUFFER_SIZE                        1024

#ifdef __linux__

struct linux_dirent {
    unsigned long                                       d_ino;
    unsigned long                                       d_off;
    unsigned short                                      d_reclen;
    char                                                d_name[];
//               char           pad;
//               char           d_type;
};

struct linux_dirent64 {
        ino64_t                                         d_ino;
        off64_t                                         d_off;
        unsigned short                                  d_reclen;
        unsigned char                                   d_type;
        char                                            d_name[];
};
#endif

struct fs_socket_directory_data_s {
    unsigned int				        flags;

#ifdef __linux__
    char					        *buffer;
    unsigned int				        size;
    unsigned int				        read;
    unsigned int				        pos;
    struct linux_dirent64                               *dirent;
#endif

};

#define FS_SOCKET_TYPE_FILE                             1
#define FS_SOCKET_TYPE_DIR                              2

struct fs_socket_s {
    unsigned int				        flags;
    unsigned short                                      type;
    void                                                (* close)(struct fs_socket_s *s);
    void                                                (* clear)(struct fs_socket_s *s);
#ifdef __linux__
    int							(* get_unix_fd)(struct fs_socket_s *sock);
    void						(* set_unix_fd)(struct fs_socket_s *sock, int fd);
    int						        fd;
#endif
    struct fs_socket_ops_s                              ops;
    union _fs_socket_data_u {
        struct fs_socket_directory_data_s               dir;
    } data;
};

struct fs_init_s {
#ifdef __linux__
    mode_t						mode;
#endif
};

/* prototypes */

void init_fs_socket(struct fs_socket_s *s, unsigned int type);

#endif
