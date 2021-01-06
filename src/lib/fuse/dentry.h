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

#define MODEMASK 07777

#define _ENTRY_FLAG_TEMP					1
#define _ENTRY_FLAG_VIRTUAL					2
#define _ENTRY_FLAG_REMOTECHANGED				4
#define _ENTRY_FLAG_ROOT					8

#define _INODE_DIRECTORY_SIZE					4096
#define _DEFAULT_BLOCKSIZE					4096

#define INODE_LINK_TYPE_CONTEXT					1
#define INODE_LINK_TYPE_ID					2
#define INODE_LINK_TYPE_SPECIAL_ENTRY				3
#define INODE_LINK_TYPE_DATA					4
#define INODE_LINK_TYPE_DIRECTORY				5
#define INODE_LINK_TYPE_CACHE					6

#define INODE_FLAG_HASHED					1
#define INODE_FLAG_CACHED					2
#define INODE_FLAG_DELETED					4
#define INODE_FLAG_REMOVED					8

union datalink_u {
    void				*ptr;
    uint64_t				id;
};

struct inode_link_s {
    unsigned char			type;
    union datalink_u			link;
};

struct inode_s {
    unsigned char			flags;
    uint64_t				nlookup;
    struct list_element_s		list;
    struct entry_s 			*alias;
    struct stat				st;
    struct timespec			stim;
    struct fuse_fs_s			*fs;
    struct inode_link_s			link;
    unsigned int			cache_size;
    char				cache[];
};

struct name_s {
    char 				*name;
    size_t				len;
    unsigned long long			index;
};

struct entry_s {
    struct name_s			name;
    struct inode_s 			*inode;
    struct list_element_s		list;
    unsigned char			flags;
    char				buffer[];
};

// Prototypes

void calculate_nameindex(struct name_s *name);

void init_entry(struct entry_s *entry);
struct entry_s *create_entry(struct name_s *xname);
void destroy_entry(struct entry_s *entry);

void init_inode(struct inode_s *inode);
struct inode_s *create_inode(unsigned int s);
void free_inode(struct inode_s *inode);

void fill_inode_stat(struct inode_s *inode, struct stat *st);
void get_inode_stat(struct inode_s *inode, struct stat *st);

struct inode_s *realloc_inode(struct inode_s *inode, unsigned int new);

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

#endif
