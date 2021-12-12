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

#include <pwd.h>
#include <grp.h>

#include "eventloop.h"
#include "workspace-interface.h"
#include "list.h"
#include "misc.h"
#include "fuse/dentry.h"
#include "fuse/directory.h"

struct service_context_s;

#define WORKSPACE_TYPE_DEVICES			1
#define WORKSPACE_TYPE_NETWORK			2
#define WORKSPACE_TYPE_FILE			4
#define WORKSPACE_TYPE_BACKUP			8

#define WORKSPACE_FLAG_ALLOC			1
#define WORKSPACE_FLAG_UNMOUNTING		2
#define WORKSPACE_FLAG_UNMOUNTED		4

#define WORKSPACE_FLAG_CTXSLOCKED		( 1 << 20 )

#define WORKSPACE_INODE_HASHTABLE_SIZE		512

#define FORGET_INODE_FLAG_FORGET		1
#define FORGET_INODE_FLAG_DELETED		2

#define WORKSPACE_MOUNT_EVENT_MOUNT		1
#define WORKSPACE_MOUNT_EVENT_UMOUNT		2

struct directory_s;

struct workspace_inodes_s {
    struct inode_s 				rootinode;
    struct entry_s				rootentry;
    struct directory_s				dummy_directory;
    unsigned long long 				nrinodes;
    uint64_t					inoctr;
    pthread_mutex_t				mutex;
    pthread_cond_t				cond;
    unsigned char				thread;
    struct list_header_s			directories;
    struct list_header_s			symlinks;
    struct list_header_s			forget;
    struct list_header_s			hashtable[WORKSPACE_INODE_HASHTABLE_SIZE];
};

/* fuse mountpoint */

struct workspace_mount_s {
    unsigned int				flags;
    unsigned int				status;
    unsigned char 				type;
    struct osns_user_s 				*user;
    unsigned int				pathmax;
    pthread_mutex_t				mutex;
    struct pathinfo_s 				mountpoint;
    struct system_timespec_s			syncdate;
    struct list_header_s			contexes;
    struct simple_locking_s			*locking;
    void					(* mountevent)(struct workspace_mount_s *mount, unsigned char event);
    void					(* remove_context)(struct list_element_s *l);
    void					(* free)(struct workspace_mount_s *mount);
    struct list_element_s			list;
    struct list_element_s			list_g;
    struct workspace_inodes_s			inodes;
};

/* prototypes */

void init_workspaces_once();

int lock_workspaces();
int unlock_workspaces();
struct workspace_mount_s *get_next_workspace_mount(struct workspace_mount_s *w);

void adjust_pathmax(struct workspace_mount_s *w, unsigned int len);
unsigned int get_pathmax(struct workspace_mount_s *w);

int init_workspace_mount(struct workspace_mount_s *w, unsigned int *error);
void free_workspace_mount(struct workspace_mount_s *workspace);

struct workspace_mount_s *get_container_workspace(struct list_element_s *list);

struct inode_s *lookup_workspace_inode(struct workspace_mount_s *workspace, uint64_t ino);
void add_inode_workspace_hashtable(struct workspace_mount_s *workspace, struct inode_s *inode);
void remove_inode_workspace_hashtable(struct workspace_mount_s *workspace, struct inode_s *inode);

void add_inode_context(struct service_context_s *context, struct inode_s *inode);
void set_inode_fuse_fs(struct service_context_s *context, struct inode_s *inode);

void queue_inode_2forget(struct workspace_mount_s *workspace, ino_t ino, unsigned int flags, uint64_t forget);

#endif
