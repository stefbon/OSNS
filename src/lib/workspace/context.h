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

#ifndef _LIB_WORKSPACE_CONTEXT_H
#define _LIB_WORKSPACE_CONTEXT_H

#include "misc.h"
#include "fuse.h"
#include "workspaces.h"
#include "commonsignal.h"

#define SERVICE_CTX_TYPE_DUMMY			0
#define SERVICE_CTX_TYPE_WORKSPACE		1
#define SERVICE_CTX_TYPE_NETWORK		2
#define SERVICE_CTX_TYPE_BROWSE			3
#define SERVICE_CTX_TYPE_FILESYSTEM		4

#define SERVICE_CTX_FLAG_ALLOC			( 1 << 0 )
#define SERVICE_CTX_FLAG_LOCKED			( 1 << 1 )
#define SERVICE_CTX_FLAG_TOBEDELETED		( 1 << 2 )
#define SERVICE_CTX_FLAG_WLIST			( 1 << 3 )
#define SERVICE_CTX_FLAG_CLIST			( 1 << 4 )

#define SERVICE_CTX_FLAG_SFTP			( 1 << 10 )
#define SERVICE_CTX_FLAG_SMB			( 1 << 11 )
#define SERVICE_CTX_FLAG_NFS			( 1 << 12 )
#define SERVICE_CTX_FLAG_WEBDAV			( 1 << 13 )
#define SERVICE_CTX_FLAG_GIT			( 1 << 14 )

/* backends like:
    - Internet Cloud Storage like Microsoft One Drive, Google Drive ... */

#define SERVICE_CTX_FLAG_ICS			( 1 << 15 )

#define SERVICE_CTX_FLAGS_REMOTEBACKEND		( SERVICE_CTX_FLAG_SFTP | SERVICE_CTX_FLAG_SMB | SERVICE_CTX_FLAG_NFS | SERVICE_CTX_FLAG_WEBDAV | SERVICE_CTX_FLAG_GIT | SERVICE_CTX_FLAG_ICS )

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

struct service_context_s {
    unsigned char				type;
    uint32_t					flags;
    char					name[32];
    struct data_link_s				link;
    union {
	struct workspace_context_s {
	    struct service_fs_s				*fs; 		/* the fs used for the workspace/root: networkfs or devicefs */
	    struct common_signal_s			*signal;
	    struct list_header_s			header;
	} workspace;
	struct network_context_s {
	    struct service_fs_s				*fs; 		/* the fs used for this service: pseudo, sftp, webdav, smb ... */
	    struct list_element_s			clist;		/* the list of the parent: part of a tree */
	    struct list_header_s			header;		/* has children */
	    struct system_timespec_s			refresh;
	    pthread_t					threadid;
	} network;
	struct browse_context_s {
	    struct service_fs_s				*fs; 		/* the fs used for browsing the network map */
	    struct list_element_s			clist;		/* the list of the parent: part of a tree */
	    unsigned int				type;		/* type: network, netgroup or nethost or netsocket */
	    uint32_t					unique;		/* unique id of the related resource record */
	    struct list_header_s			header;		/* has children */
	    struct system_timespec_s			refresh;	/* time of latest refresh: to keep cache and contexes in sync */
	    pthread_t					threadid;
	} browse;
	struct filesystem_context_s {
	    struct service_fs_s				*fs; 		/* the fs used for this service: pseudo, sftp, webdav, smb ... */
	    struct list_element_s			clist;		/* the list of the parent: part of a tree */
	    struct inode_s 				*inode; 	/* the inode the service is attached to */
	    pthread_mutex_t				mutex;		/* mutex to protect pathcache */
	    struct list_header_s			pathcaches;
	} filesystem;
    } service;
    struct list_element_s			wlist;
    struct context_interface_s			interface;
};

struct service_fs_s {

    void (*lookup_existing) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo);
    void (*lookup_new) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo);

    void (*getattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct pathinfo_s *pathinfo);
    void (*setattr) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct pathinfo_s *pathinfo, struct system_stat_s *stat);

    void (*readlink) (struct service_context_s *context, struct fuse_request_s *request, struct inode_s *inode, struct pathinfo_s *pathinfo);

    int (* access) (struct service_context_s *context, struct fuse_request_s *request, unsigned char what);
    unsigned int (* get_name)(struct service_context_s *context, char *buffer, unsigned int len);

    void (*mkdir) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct system_stat_s *stat);
    void (*mknod) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct system_stat_s *stat);
    void (*symlink) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct fs_location_path_s *link);
    int  (*symlink_validate)(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, struct fs_location_path_s *sub);

    void (*unlink) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry, struct pathinfo_s *pathinfo);
    void (*rmdir) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry, struct pathinfo_s *pathinfo);

    void (*rename) (struct service_context_s *context, struct fuse_request_s *request, struct entry_s **entry, struct pathinfo_s *pathinfo, struct entry_s **n_entry, struct pathinfo_s *n_pathinfo, unsigned int flags);

    void (*open) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct pathinfo_s *pathinfo, unsigned int flags);
    void (*read) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
    void (*write) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner);
    void (*flush) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, uint64_t lock_owner);
    void (*fsync) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char datasync);
    void (*release) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned int flags, uint64_t lock_owner);
    void (*create) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct system_stat_s *stat, unsigned int flags);

    void (*fgetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request);
    void (*fsetattr) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct system_stat_s *stat);

    void (*getlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
    void (*setlock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);
    void (*setlockw) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct flock *flock);

    void (*flock) (struct fuse_openfile_s *openfile, struct fuse_request_s *request, unsigned char type);

    void (*opendir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, struct pathinfo_s *pathinfo, unsigned int flags);
    void (*readdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
    void (*readdirplus) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
    void (*releasedir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request);
    void (*fsyncdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync);

    void (*setxattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name, const char *value, size_t size, int flags);
    void (*getxattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name, size_t size);
    void (*listxattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, size_t size);
    void (*removexattr) (struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo, struct inode_s *inode, const char *name);

    void (*statfs)(struct service_context_s *context, struct fuse_request_s *request, struct pathinfo_s *pathinfo);

};

/* prototypes */

struct service_context_s *get_service_context(struct context_interface_s *interface);
struct osns_user_s *get_user_context(struct service_context_s *context);

#include "ctx-init.h"
#include "ctx-lock.h"
#include "ctx-next.h"

#endif
