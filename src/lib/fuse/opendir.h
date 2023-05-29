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

#ifndef LIB_FUSE_OPENDIR_H
#define LIB_FUSE_OPENDIR_H

#include "libosns-interface.h"

#include "config.h"
#include "dentry.h"
#include "request.h"

#define FUSE_OPENDIR_FLAG_NONEMPTY				1 << 0
#define FUSE_OPENDIR_FLAG_VIRTUAL				1 << 1
#define FUSE_OPENDIR_FLAG_READDIRPLUS				1 << 2
#define FUSE_OPENDIR_FLAG_DATA_ATTR_BUFFER			1 << 3

#define FUSE_OPENDIR_FLAG_FINISH				1 << 10
#define FUSE_OPENDIR_FLAG_INCOMPLETE				1 << 11
#define FUSE_OPENDIR_FLAG_ERROR					1 << 12
#define FUSE_OPENDIR_FLAG_EOD					1 << 13
#define FUSE_OPENDIR_FLAG_THREAD				1 << 14
#define FUSE_OPENDIR_FLAG_RELEASE				1 << 15

#define FUSE_OPENDIR_FLAG_HIDE_SPECIALFILES			1 << 20
#define FUSE_OPENDIR_FLAG_HIDE_DOTFILES				1 << 21

#define FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS		1 << 22
#define FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS			1 << 23
#define FUSE_OPENDIR_FLAG_IGNORE_DOTFILES                       1 << 24
#define FUSE_OPENDIR_FLAG_IGNORE_SYMLINKS                       1 << 25

struct direntry_buffer_s;
struct fuse_request_s;

struct fuse_opendir_s {
    struct fuse_open_header_s					header;
    unsigned int						flags;
    ino_t							ino;
    unsigned int 						error;
    unsigned int						count_keep;
    unsigned int						count_created;
    unsigned int						count_found;
    void 							(* readdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
    signed char							(* hidefile)(struct fuse_opendir_s *opendir, struct entry_s *entry);
    int 							(* add_direntry)(struct fuse_config_s *c, struct direntry_buffer_s *b, struct name_s *xname, struct system_stat_s *stat);
    struct list_header_s					entries;
    struct list_header_s                                        symlinks;
    struct shared_signal_s					*signal;
    unsigned char						threads;
    union {
	uint64_t						nr;
	void							*ptr;
	struct attr_buffer_s					abuff;
    } data;
};

/* prototypes */

void init_fuse_opendir(struct fuse_opendir_s *opendir, struct service_context_s *ctx, struct inode_s *inode);
void set_flag_fuse_opendir(struct fuse_opendir_s *opendir, unsigned int flag);

void queue_fuse_direntry(struct fuse_opendir_s *opendir, struct entry_s *entry);
void queue_fuse_symlink(struct fuse_opendir_s *opendir, struct entry_s *entry);
struct entry_s *get_fuse_direntry(struct fuse_opendir_s *opendir, struct fuse_request_s *request);
void clear_opendir(struct fuse_opendir_s *opendir);

#endif

/*
	    void (*readdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
	    void (*readdirplus) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t offset);
	    void (*releasedir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request);
	    void (*fsyncdir) (struct fuse_opendir_s *opendir, struct fuse_request_s *request, unsigned char datasync);
*/
