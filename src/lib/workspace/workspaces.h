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

#ifndef _LIB_WORKSPACE_WORKSPACES_H
#define _LIB_WORKSPACE_WORKSPACES_H

#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-list.h"
#include "libosns-misc.h"

#include "fuse/dentry.h"
#include "fuse/directory.h"

struct service_context_s;

#define WORKSPACE_INODE_HASHTABLE_SIZE				512

#define WORKSPACE_FLAG_ALLOC					1

#define WORKSPACE_STATUS_LOCK_PATHMAX				(1 << 0)
#define WORKSPACE_STATUS_LOCK_CONTEXES				(1 << 1)
#define WORKSPACE_STATUS_LOCK_INODES				(1 << 2)
#define WORKSPACE_STATUS_LOCK_DELETE_INODES_THREAD		(1 << 3)
#define WORKSPACE_STATUS_LOCK_FORGET				(1 << 4)
#define WORKSPACE_STATUS_LOCK_SYMLINK				(1 << 5)

struct directory_s;

struct workspace_inodes_s {
    struct inode_s 				rootinode;
    struct entry_s				rootentry;
    struct directory_s				dummy_directory;
    uint64_t 					nrinodes;
    uint64_t					inoctr;
    unsigned char				thread;
    struct list_header_s			directories;
    struct list_header_s			symlinks;
    struct list_header_s			forget;
    struct list_header_s			hashtable[WORKSPACE_INODE_HASHTABLE_SIZE];
};

struct workspace_mount_s {
    unsigned int				flags;
    unsigned int				status;
    unsigned char 				type;
    unsigned int				pathmax;
    struct shared_signal_s			*signal;
    struct system_timespec_s			syncdate;
    struct list_header_s			contexes;
    struct list_header_s			shared_contexes;
    struct list_element_s			list;
    struct workspace_inodes_s			inodes;
};

/* prototypes */

void adjust_pathmax(struct workspace_mount_s *w, unsigned int len);
unsigned int get_pathmax(struct workspace_mount_s *w);

struct workspace_mount_s *create_workspace_mount(unsigned char type);

#endif
