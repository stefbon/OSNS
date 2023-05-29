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

#include "libosns-datatypes.h"
#include "libosns-network.h"
#include "libosns-fspath.h"
#include "libosns-socket.h"

#include "fssocket.h"
#include "stat.h"

#include "lib/sftp/acl-ace4.h"

#define FS_HANDLE_TYPE_DIR				1
#define FS_HANDLE_TYPE_FILE				2

#define INSERTHANDLE_TYPE_CREATE			1
#define INSERTHANDLE_TYPE_OPEN				2

#define INSERTHANDLE_STATUS_OK				1
#define INSERTHANDLE_STATUS_DENIED_LOCK			2

#define FS_HANDLE_BLOCK_READ				1
#define FS_HANDLE_BLOCK_WRITE				2
#define FS_HANDLE_BLOCK_DELETE				4

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

#define HANDLE_FLAG_ALLOC				1
#define HANDLE_FLAG_CREATE				4
#define HANDLE_FLAG_EOD				        32

#define HANDLE_SUBSYSTEM_TYPE_SFTP			1

struct fs_handle_s {
    struct list_element_s                               list;
    unsigned char                                       type;
    unsigned int					flags;
    unsigned int					subsystem;
    unsigned int                                        connectionid;
    dev_t                                               dev;
    uint64_t                                            ino;
    struct fs_socket_s				        socket;
    unsigned int					(* get_flags)(struct fs_handle_s *handle);
    unsigned int					(* get_access)(struct fs_handle_s *handle);
    unsigned int                                        (* write_handle)(struct fs_handle_s *handle, char *buffer, unsigned int size);
    unsigned int                                        size;
    char                                                buffer[];
};

/* prototypes */

void init_fs_handle_hashtable();

struct fs_handle_s *get_fs_handle(unsigned int connectionid, char *buffer, unsigned int size, unsigned int *p_count);
void insert_fs_handle(struct fs_handle_s *handle, unsigned int connectionid, dev_t dev, uint64_t ino);

unsigned int get_fs_handle_buffer_size();

void free_fs_handle(struct fs_handle_s **p_handle);
struct fs_handle_s *create_fs_handle(unsigned char type, unsigned int size);

#endif
