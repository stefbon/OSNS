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

#ifndef _LIB_CONTEXT_CONTEXT_H
#define _LIB_CONTEXT_CONTEXT_H

#include "libosns-list.h"
#include "libosns-interface.h"
#include "libosns-fuse-public.h"

#define SERVICE_CTX_TYPE_DUMMY			0
#define SERVICE_CTX_TYPE_WORKSPACE		1
#define SERVICE_CTX_TYPE_BROWSE			2
#define SERVICE_CTX_TYPE_FILESYSTEM		3
#define SERVICE_CTX_TYPE_SHARED			4

/* TO DO:
    - CTX TYPE ID: fs based upon id's provided by remote server */

#define SERVICE_CTX_FLAG_ALLOC			( 1 << 0 )
#define SERVICE_CTX_FLAG_LOCKED			( 1 << 1 )
#define SERVICE_CTX_FLAG_TOBEDELETED		( 1 << 2 )
#define SERVICE_CTX_FLAG_WLIST			( 1 << 3 )
#define SERVICE_CTX_FLAG_CLIST			( 1 << 4 )

#define SERVICE_BROWSE_TYPE_NETWORK		1
#define SERVICE_BROWSE_TYPE_NETGROUP		2
#define SERVICE_BROWSE_TYPE_NETHOST		3
#define SERVICE_BROWSE_TYPE_NETSOCKET		4

#define SERVICE_OP_TYPE_LOOKUP			0
#define SERVICE_OP_TYPE_LOOKUP_EXISTING		1
#define SERVICE_OP_TYPE_LOOKUP_NEW		2

#define SERVICE_OP_TYPE_GETATTR			3
#define SERVICE_OP_TYPE_SETATTR			4
#define SERVICE_OP_TYPE_READLINK		5

#define SERVICE_OP_TYPE_MKDIR			6
#define SERVICE_OP_TYPE_MKNOD			7
#define SERVICE_OP_TYPE_SYMLINK			8
#define SERVICE_OP_TYPE_CREATE			9

#define SERVICE_OP_TYPE_UNLINK			10
#define SERVICE_OP_TYPE_RMDIR			11

#define SERVICE_OP_TYPE_RENAME			12

#define SERVICE_OP_TYPE_OPEN			13
#define SERVICE_OP_TYPE_READ			14
#define SERVICE_OP_TYPE_WRITE			15
#define SERVICE_OP_TYPE_FLUSH			16
#define SERVICE_OP_TYPE_FSYNC			17
#define SERVICE_OP_TYPE_RELEASE			18
#define SERVICE_OP_TYPE_FGETATTR		19
#define SERVICE_OP_TYPE_FSETATTR		20

#define SERVICE_OP_TYPE_GETLOCK			21
#define SERVICE_OP_TYPE_SETLOCK			22
#define SERVICE_OP_TYPE_SETLOCKW		23
#define SERVICE_OP_TYPE_FLOCK			24

#define SERVICE_OP_TYPE_OPENDIR			25
#define SERVICE_OP_TYPE_READDIR			26
#define SERVICE_OP_TYPE_READDIRPLUS		27
#define SERVICE_OP_TYPE_RELEASEDIR		28
#define SERVICE_OP_TYPE_FSYNCDIR		29

#define SERVICE_OP_TYPE_GETXATTR		30
#define SERVICE_OP_TYPE_SETXATTR		31
#define SERVICE_OP_TYPE_LISTXATTR		32
#define SERVICE_OP_TYPE_REMOVEXATTR		33

#define SERVICE_OP_TYPE_STATFS			34

/*
    TODO:
    - git
    - Google Drive
    - Microsoft One Drive
    - Amazon
    - Nextcloud
    - backup

*/

struct browse_service_fs_s;
struct path_service_fs_s;
struct fuse_path_s;

struct service_context_s {
    unsigned int				type;
    uint32_t					flags;
    char					name[32];
    struct data_link_s				link;
    union {
	struct workspace_context_s {
	    struct shared_signal_s			*signal;
	    struct list_header_s			header;
	    struct browse_service_fs_s			*fs; 		/* the fs used for browsing the network map */
	} workspace;
	struct browse_context_s {
	    struct browse_service_fs_s			*fs; 		/* the fs used for browsing the network map */
	    struct list_element_s			clist;		/* the list of the parent: part of a tree */
	    unsigned int				type;		/* type: network, netgroup or nethost or netsocket */
	    struct list_header_s			header;		/* has children */
	    struct system_timespec_s			refresh;	/* time of latest refresh: to keep cache and contexes in sync */
	    uint32_t					unique;
	    unsigned int				service;
	    pthread_t					threadid;
	} browse;
	struct filesystem_context_s {
	    struct path_service_fs_s			*fs; 		/* the path based fs used for this service: sftp, webdav, smb ... */
	    struct list_element_s			clist;		/* the list of the parent: part of a tree */
	    struct inode_s 				*inode; 	/* the inode the service is attached to */
	    struct shared_signal_s			*signal;
	    char					name[32];
	    unsigned int				service;
	    struct list_header_s			pathcaches;
	} filesystem;
	struct shared_context_s {
	    struct system_timespec_s			refresh;	/* time of latest refresh: to keep cache and contexes in sync */
	    uint32_t					unique;
	    unsigned int				service;
	    unsigned int				transport;
	} shared;
    } service;
    struct list_element_s			wlist;
    struct context_interface_s			interface;
};

/* prototypes */

struct service_context_s *get_service_context(struct context_interface_s *interface);

#endif
