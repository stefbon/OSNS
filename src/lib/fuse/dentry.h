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

#ifndef _LIB_FUSE_DENTRY_H
#define _LIB_FUSE_DENTRY_H

#include "list.h"
#include "datatypes.h"
#include "system/stat.h"

#define MODEMASK 07777

#define _ENTRY_FLAG_TEMP					1
#define _ENTRY_FLAG_VIRTUAL					2
#define _ENTRY_FLAG_ROOT					4
#define _ENTRY_FLAG_NOBUFFER					8

#define _INODE_DIRECTORY_SIZE					4096
#define _DEFAULT_BLOCKSIZE					4096

#define DATA_LINK_TYPE_CONTEXT					1
#define DATA_LINK_TYPE_ID					2
#define DATA_LINK_TYPE_SPECIAL_ENTRY				3
#define DATA_LINK_TYPE_SYMLINK					4
#define DATA_LINK_TYPE_DATA					5
#define DATA_LINK_TYPE_DIRECTORY				6
#define DATA_LINK_TYPE_CACHE					7

#define INODE_FLAG_HASHED					1
#define INODE_FLAG_CACHED					2
#define INODE_FLAG_DELETED					4
#define INODE_FLAG_REMOVED					8
#define INODE_FLAG_REMOTECHANGED				16

#define INODECACHE_FLAG_STAT					1
#define INODECACHE_FLAG_READDIR					2
#define INODECACHE_FLAG_XATTR					4
#define INODECACHE_FLAG_STAT_EQUAL_READDIR			8

struct inodecache_s {
    ino_t				ino;
    unsigned char			flags;
    struct list_element_s		list;
    struct ssh_string_s			stat;
    struct ssh_string_s			readdir;
    struct ssh_string_s			xattr;
};

union datalink_u {
    void				*ptr;
    uint64_t				id;
};

struct data_link_s {
    unsigned char			type;
    union datalink_u			link;
};

struct inode_s {
    unsigned int			flags;
    uint64_t				nlookup;
    struct list_element_s		list;
    struct entry_s 			*alias;
    struct system_stat_s		stat;
    struct system_timespec_s		stime;
    struct fuse_fs_s			*fs;
    struct data_link_s			link;
    struct inodecache_s			*cache;
};

struct entry_s {
    struct name_s			name;
    struct inode_s 			*inode;
    struct list_element_s		list;
    struct data_link_s			link;
    struct entry_ops_s			*ops;
    unsigned char			flags;
    char				buffer[];
};

// Prototypes

void init_entry(struct entry_s *entry);
struct entry_s *create_entry(struct name_s *xname);
void destroy_entry(struct entry_s *entry);

void init_inode(struct inode_s *inode);
struct inode_s *create_inode();
void free_inode(struct inode_s *inode);

int check_create_inodecache(struct inode_s *inode, unsigned int size, char *buffer, unsigned int flag);

void fill_inode_stat(struct inode_s *inode, struct system_stat_s *stat);
void get_inode_stat(struct inode_s *inode, struct system_stat_s *stat);

#define INODE_INFORMATION_OWNER						(1 << 0)
#define INODE_INFORMATION_GROUP						(1 << 1)
#define INODE_INFORMATION_NAME						(1 << 2)
#define INODE_INFORMATION_NLOOKUP					(1 << 3)
#define INODE_INFORMATION_MODE						(1 << 4)
#define INODE_INFORMATION_NLINK						(1 << 5)
#define INODE_INFORMATION_SIZE						(1 << 6)
#define INODE_INFORMATION_MTIM						(1 << 7)
#define INODE_INFORMATION_CTIM						(1 << 8)
#define INODE_INFORMATION_ATIM						(1 << 9)
#define INODE_INFORMATION_STIM						(1 << 10)
#define INODE_INFORMATION_INODE_LINK					(1 << 11)
#define INODE_INFORMATION_FS_COUNT					(1 << 12)

void log_inode_information(struct inode_s *inode, uint64_t what);
void init_dentry_once();

#endif
