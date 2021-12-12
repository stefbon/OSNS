/*
  2010, 2011, 2012, 2013, 2014 Stef Bon <stefbon@gmail.com>

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

#ifndef _LIB_FUSE_DIRECTORY_H
#define _LIB_FUSE_DIRECTORY_H

#include "dentry.h"
#include "sl.h"

#define _DIRECTORY_FLAG_REMOVE					1
#define _DIRECTORY_FLAG_DUMMY					2
#define _DIRECTORY_FLAG_ALLOC					4

#define _DIRECTORY_LOCK_READ					1
#define _DIRECTORY_LOCK_PREEXCL					2
#define _DIRECTORY_LOCK_EXCL					3

#define GET_DIRECTORY_FLAG_NOCREATE				1
#define DIRECTORY_OP_FLAG_LOCKED				1

struct workspace_mount_s;

struct dops_s {
    struct directory_s			*(* get_directory)(struct workspace_mount_s *w, struct inode_s *inode, unsigned int flags, struct simple_lock_s *l);
    struct directory_s			*(* remove_directory)(struct workspace_mount_s *w, struct directory_s *directory, struct simple_lock_s *l);
    unsigned int			(* get_count)(struct directory_s *directory);
    struct entry_s			*(* find_entry)(struct directory_s *d, struct name_s *n, unsigned int *error, unsigned int flags);
    void				(* remove_entry)(struct directory_s *d, struct entry_s *e, unsigned int *error, unsigned int flags);
    struct entry_s			*(* insert_entry)(struct directory_s *d, struct entry_s *e, unsigned int *error, unsigned int flags);
};

struct directory_s {
    unsigned char 			flags;
    struct system_timespec_s 		synctime;
    struct inode_s 			*inode;
    struct list_element_s		list;
    struct simple_locking_s		locking;
    struct dops_s 			*dops;
    struct data_link_s			link;
    struct data_link_s 			*ptr;
    struct getpath_s			*getpath;
    unsigned int			size;
    char				buffer[];
};

int init_directory(struct directory_s *directory, unsigned char maxlanes);
struct directory_s *_create_directory(struct inode_s *inode);

struct directory_s *get_upper_directory_entry(struct entry_s *entry);
struct entry_s *get_parent_entry(struct entry_s *entry);
struct entry_s *get_next_entry(struct entry_s *entry);
struct entry_s *get_prev_entry(struct entry_s *entry);

void clear_directory(struct directory_s *directory);
void free_directory(struct directory_s *directory);

void init_directory_readlock(struct directory_s *directory, struct simple_lock_s *lock);
void init_directory_writelock(struct directory_s *directory, struct simple_lock_s *lock);
int rlock_directory(struct directory_s *directory, struct simple_lock_s *lock);
int wlock_directory(struct directory_s *directory, struct simple_lock_s *lock);

int lock_directory(struct directory_s *directory, struct simple_lock_s *lock);
int unlock_directory(struct directory_s *directory, struct simple_lock_s *lock);
int upgradelock_directory(struct directory_s *directory, struct simple_lock_s *lock);
int prelock_directory(struct directory_s *directory, struct simple_lock_s *lock);

struct directory_s *get_directory(struct workspace_mount_s *w, struct inode_s *inode, unsigned int flags);
struct directory_s *remove_directory(struct workspace_mount_s *w, struct inode_s *inode);
unsigned int get_directory_count(struct directory_s *d);

struct entry_s *find_entry(struct directory_s *d, struct name_s *ln, unsigned int *error);
void remove_entry(struct directory_s *d, struct entry_s *e, unsigned int *error);
struct entry_s *insert_entry(struct directory_s *d, struct entry_s *e, unsigned int *error);

struct entry_s *find_entry_batch(struct directory_s *d, struct name_s *ln, unsigned int *error);
void remove_entry_batch(struct directory_s *d, struct entry_s *e, unsigned int *error);
struct entry_s *insert_entry_batch(struct directory_s *d, struct entry_s *e, unsigned int *error);

void assign_directory_inode(struct workspace_mount_s *w, struct inode_s *inode);
void init_dummy_directory(struct directory_s *d);

#endif
