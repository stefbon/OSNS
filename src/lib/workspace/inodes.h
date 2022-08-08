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

#ifndef _LIB_WORKSPACE_INODES_H
#define _LIB_WORKSPACE_INODES_H

#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-list.h"
#include "libosns-misc.h"

#include "fuse/dentry.h"
#include "fuse/directory.h"

#define FORGET_INODE_FLAG_FORGET				1
#define FORGET_INODE_FLAG_DELETED				2

/* prototypes */

struct inode_s *lookup_workspace_inode(struct workspace_mount_s *w, uint64_t ino);

void add_inode_workspace_hashtable(struct workspace_mount_s *w, struct inode_s *i);
void remove_inode_workspace_hashtable(struct workspace_mount_s *w, struct inode_s *i);

void add_inode_context(struct service_context_s *c, struct inode_s *i);
void inherit_fuse_fs_parent(struct service_context_s *c, struct inode_s *i);

void queue_inode_2forget(struct workspace_mount_s *w, ino_t ino, unsigned int flags, uint64_t forget);

#endif
