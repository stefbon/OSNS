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
#ifndef LIB_SYSTEM_FSHANDLE_H
#define LIB_SYSTEM_FSHANDLE_H

#include "datatypes.h"
#include "network.h"

#include "path.h"
#include "open.h"
#include "location.h"
#include "stat.h"

#include "lib/sftp/acl-ace4.h"

#define COMMONHANDLE_TYPE_DIR				1
#define COMMONHANDLE_TYPE_FILE				2

#define COMMONHANDLE_FLAG_ALLOC				(1 << 0)
#define COMMONHANDLE_FLAG_CREATE			(1 << 1)
#define COMMONHANDLE_FLAG_FILE				(1 << 2)
#define COMMONHANDLE_FLAG_DIR				(1 << 3)
#define COMMONHANDLE_FLAG_NAME_ALLOC			(1 << 4)

#define COMMONHANDLE_FLAG_SFTP				(1 << 10)

#define INSERTHANDLE_TYPE_CREATE			1
#define INSERTHANDLE_TYPE_OPEN				2

#define INSERTHANDLE_STATUS_OK				1
#define INSERTHANDLE_STATUS_DENIED_LOCK			2

#define FILEHANDLE_BLOCK_READ				1
#define FILEHANDLE_BLOCK_WRITE				2
#define FILEHANDLE_BLOCK_DELETE				4

struct insert_filehandle_s {
    unsigned int						status;
    unsigned int						type;
    union {
	struct lockconflict_s {
	    uid_t						uid;
	    struct ip_address_s					address;
	} lock;
    } info;
};

#define FILEHANDLE_FLAG_ENABLED				1
#define FILEHANDLE_FLAG_CREATE				2
#define FILEHANDLE_FLAG_OPEN				4

struct filehandle_s {
    unsigned int					flags;
    int							(* open)(struct filehandle_s *fh, struct fs_location_s *location, unsigned int flags, struct fs_init_s *init);
    int							(* pwrite)(struct filehandle_s *fh, char *data, unsigned int size, off_t offset);
    int							(* pread)(struct filehandle_s *fh, char *data, unsigned int size, off_t offset);
    int							(* flush)(struct filehandle_s *fh);
    int							(* fsync)(struct filehandle_s *fh, unsigned int flags);
    int							(* fgetstat)(struct filehandle_s *fh, unsigned int mask, struct system_stat_s *st);
    int							(* fsetstat)(struct filehandle_s *fh, unsigned int mask, struct system_stat_s *st);
    off_t						(* lseek)(struct filehandle_s *fh, off_t off, int whence);
    void						(* close)(struct filehandle_s *fh);
    struct fs_socket_s					socket;
};

#define FS_DENTRY_FLAG_FILE				1
#define FS_DENTRY_FLAG_DIR				2
#define FS_DENTRY_FLAG_EOD				4

struct fs_dentry_s {
    uint16_t						flags;
    uint32_t						type;
    uint64_t						ino; /* ino on the server; some filesystems rely on this */
    uint16_t						len;
    char						*name;
};

#define FS_DENTRY_INIT					{0, 0, 0, 0, NULL}

#define DIRHANDLE_FLAG_ENABLED				1
#define DIRHANDLE_FLAG_EOD				2
#define DIRHANDLE_FLAG_ERROR				4
#define DIRHANDLE_FLAG_KEEP_DENTRY			8

struct dirhandle_s {
    unsigned int					flags;
    int							(* open)(struct dirhandle_s *dh, struct fs_location_s *location, unsigned int flags);
    struct fs_dentry_s					*(* readdentry)(struct dirhandle_s *dh);
    int							(* fstatat)(struct dirhandle_s *dh, char *name, unsigned int mask, struct system_stat_s *st);
    void						(* fsyncdir)(struct dirhandle_s *dh, unsigned int flags);
    void						(* close)(struct dirhandle_s *dh);
    void						(* set_keep_dentry)(struct dirhandle_s *dh);
    struct fs_socket_s					socket;
    char						*buffer;
    unsigned int					size;
    unsigned int					read;
    unsigned int					pos;
    struct fs_dentry_s					dentry;
};

struct commonhandle_s {
    unsigned char					flags;
    struct fs_location_s				location;
    union fs_handle_u {
	struct dirhandle_s				dir;
	struct filehandle_s				file;
    } type;
    void						(* clear_buffer)(struct commonhandle_s *handle);
    unsigned int					(* get_flags)(struct commonhandle_s *handle);
    unsigned int					(* get_access)(struct commonhandle_s *handle);
    unsigned int					size;
    char						buffer[];
};

/* prototypes */

void free_commonhandle(struct commonhandle_s **p_handle);
struct commonhandle_s *create_commonhandle(unsigned char type, struct fs_location_s *location, unsigned int size);

pid_t get_pid_commonhandle(struct commonhandle_s *handle);
int get_fd_commonhandle(struct commonhandle_s *handle);

#endif
