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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"

#include "receive.h"
#include "request.h"
#include "opendir.h"

static void opendir_readdir_noop(struct fuse_opendir_s *opendir, struct fuse_request_s *request, size_t size, off_t off)
{
    reply_VFS_error(request, EIO);
}

static signed char hidefile_default(struct fuse_opendir_s *opendir, struct entry_s *entry)
{

    if (opendir->flags & FUSE_OPENDIR_FLAG_HIDE_SPECIALFILES) {

	if (entry->flags & _ENTRY_FLAG_SPECIAL) {

	    logoutput("hidefile_default: %.*s is special entry", entry->name.len, entry->name.name);
	    return 1;
	}

    }

    if (opendir->flags & FUSE_OPENDIR_FLAG_HIDE_DOTFILES) {

	if (entry->name.name[0]=='.' && entry->name.len>1 && entry->name.name[1]!='.') {

	    logoutput("hidefile_default: %.*s starts with dot", entry->name.len, entry->name.name);
	    return 1;

	}

    }

    return 0;

}

void init_fuse_opendir(struct fuse_opendir_s *opendir, struct service_context_s *ctx, struct inode_s *inode)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(ctx);

    memset(opendir, 0, sizeof(struct fuse_opendir_s));

    /* header */

    init_fuse_open_header(&opendir->header, ctx, inode);
    opendir->header.type=FUSE_OPEN_TYPE_DIR;

    opendir->error=0;
    opendir->flags=0;
    opendir->ino=0;

    opendir->readdir=opendir_readdir_noop;
    opendir->hidefile=hidefile_default;

    opendir->count_keep=0;
    opendir->count_found=0;
    opendir->count_created=0;

    init_list_header(&opendir->entries, SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_list_header(&opendir->symlinks, SIMPLE_LIST_TYPE_EMPTY, NULL);

    opendir->signal=workspace->signal;
    opendir->threads=0;

}

void set_flag_fuse_opendir(struct fuse_opendir_s *opendir, unsigned int flag)
{
    unsigned int set=signal_set_flag(opendir->signal, &opendir->flags, flag);
}

static void queue_fuse_direntry_common(struct fuse_opendir_s *opendir, struct list_header_s *header, struct entry_s *entry)
{
    struct fuse_direntry_s *direntry=malloc(sizeof(struct fuse_direntry_s));

    if (direntry) {
	struct shared_signal_s *signal=opendir->signal;

	memset(direntry, 0, sizeof(struct fuse_direntry_s));
	init_list_element(&direntry->list, NULL);
	direntry->entry=entry;

	signal_lock(signal);
	add_list_element_last(header, &direntry->list);
	signal_broadcast(signal);
	signal_unlock(signal);

    }

}

void queue_fuse_direntry(struct fuse_opendir_s *opendir, struct entry_s *entry)
{
    queue_fuse_direntry_common(opendir, &opendir->entries, entry);
}

void queue_fuse_symlink(struct fuse_opendir_s *opendir, struct entry_s *entry)
{
    queue_fuse_direntry_common(opendir, &opendir->symlinks, entry);
}

static struct entry_s *get_fuse_direntry_common(struct fuse_opendir_s *opendir, struct list_header_s *header, struct fuse_request_s *request)
{
    struct shared_signal_s *signal=opendir->signal;
    struct directory_s *directory=get_directory(opendir->header.ctx, opendir->header.inode, 0);
    struct list_element_s *list=NULL;
    struct fuse_direntry_s *direntry=NULL;
    struct entry_s *entry=NULL;
    unsigned int opendirflags=(FUSE_OPENDIR_FLAG_EOD | FUSE_OPENDIR_FLAG_FINISH | FUSE_OPENDIR_FLAG_INCOMPLETE | FUSE_OPENDIR_FLAG_ERROR);
    struct system_timespec_s expire=SYSTEM_TIME_INIT;
    int result=0;

    signal_lock(signal);

    get_current_time_system_time(&expire);
    system_time_add(&expire, SYSTEM_TIME_ADD_ZERO, 4); /* make configurable */

    checkandwait:

    if ((list=remove_list_head(header))) {

	goto unlock;

    } else if (request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	signal_unlock(signal);
	return NULL;

    } else if (opendir->flags & opendirflags) {

	signal_unlock(signal);
	return NULL;

    }

    result=signal_condtimedwait(signal, &expire);

    if (result>0) {

	signal_unlock(signal);
	return NULL;

    } else {

	goto checkandwait;

    }

    unlock:

    signal_unlock(signal);

    if (list) {

	direntry=(struct fuse_direntry_s *)((char *)list - offsetof(struct fuse_direntry_s, list));
	entry=direntry->entry;
	free(direntry);

    }

    return entry;
}

struct entry_s *get_fuse_direntry(struct fuse_opendir_s *opendir, struct fuse_request_s *request)
{
    return get_fuse_direntry_common(opendir, &opendir->entries, request);
}

void clear_opendir(struct fuse_opendir_s *opendir)
{
    struct list_header_s *h=&opendir->entries;
    struct list_element_s *list=NULL;

    freedirentry:

    list=remove_list_head(h);
    if (list) {

	struct fuse_direntry_s *direntry=(struct fuse_direntry_s *)((char *)list - offsetof(struct fuse_direntry_s, list));
	free(direntry);
	goto freedirentry;

    }

}
