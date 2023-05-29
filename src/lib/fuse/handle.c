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
#include "handle.h"

#define FUSE_HANDLE_HASHTABLE_SIZE			73
static struct list_header_s hashtable[FUSE_HANDLE_HASHTABLE_SIZE];

void init_fuse_handle_hashtable()
{
    for (unsigned int i=0; i<FUSE_HANDLE_HASHTABLE_SIZE; i++) init_list_header(&hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
}

struct fuse_handle_s *get_fuse_handle(struct service_context_s *ctx, uint64_t ino, unsigned int flag)
{
    unsigned int hashvalue=(ino % FUSE_HANDLE_HASHTABLE_SIZE);
    struct list_header_s *header=&hashtable[hashvalue];
    struct list_element_s *list=NULL;
    struct fuse_handle_s *handle=NULL;

    read_lock_list_header(header);
    list=get_list_head(header);

    while (list) {

	handle=(struct fuse_handle_s *)((char *)list - offsetof(struct fuse_handle_s, list));

	if (handle->ino==ino && (get_root_context(handle->ctx)==ctx)) {

	    if ((handle->flags & FUSE_HANDLE_FLAG_RELEASE)==0 && (handle->flags & flag)) {

		handle->refcount++;

	    } else {

		handle=NULL;

	    }

	    break;

	}

	list=get_next_element(list);
	handle=NULL;

    }

    read_unlock_list_header(header);
    return handle;
}

void use_fuse_handle(struct fuse_handle_s *handle)
{
    unsigned int hashvalue=(handle->ino % FUSE_HANDLE_HASHTABLE_SIZE);
    struct list_header_s *header=&hashtable[hashvalue];

    write_lock_list_header(header);
    handle->refcount++;
    write_unlock_list_header(header);
}

void post_fuse_handle(struct fuse_handle_s *handle, unsigned int flag)
{
    unsigned int hashvalue=(handle->ino % FUSE_HANDLE_HASHTABLE_SIZE);
    struct list_header_s *header=&hashtable[hashvalue];
    unsigned char dofree=0;

    write_lock_list_header(header);

    if (handle->refcount>0) handle->refcount--;
    handle->flags |= flag;

    if ((handle->flags & FUSE_HANDLE_FLAG_RELEASE) && (handle->refcount==0)) {

	remove_list_element(&handle->list);
	dofree=1;

    }

    write_unlock_list_header(header);

    if (dofree) {

	(* handle->cb.release)(handle);
	if (handle->flags & FUSE_HANDLE_FLAG_ALLOC) free(handle);

    }

}

void release_fuse_handle(struct fuse_handle_s *handle)
{
    post_fuse_handle(handle, FUSE_HANDLE_FLAG_RELEASE);
}

/* shared cb for both file- and directory handles */

/*  static int cb_fgetsetstat_default(struct fuse_handle_s *h, struct fuse_request_s *r, unsigned int mask, struct system_stat_s *stat)
{
    return -EOPNOTSUPP;
}

static int cb_fsyncflush_default(struct fuse_handle_s *h, struct fuse_request_s *r, unsigned int flags)
{
    return -EOPNOTSUPP;
} */

static void cb_release_default(struct fuse_handle_s *h)
{
}
/* cb for a directory handle */

/*static int cb_fstatat_default(struct fuse_handle_s *h, struct fuse_request_s *r, struct fuse_path_s *fpath, unsigned int mask, struct system_stat_s *stat, unsigned int flags)
{
    return -EOPNOTSUPP;
}

static int cb_unlinkat_default(struct fuse_handle_s *h, struct fuse_request_s *r, struct fuse_path_s *fpath, unsigned int flags)
{
    return -EOPNOTSUPP;
}

static int cb_readlinkat_default(struct fuse_handle_s *h, struct fuse_request_s *r, struct fuse_path_s *fpath, struct fs_location_path_s *target)
{
    return -EOPNOTSUPP;
}*/

/* cb for a file handle */

/*static int cb_pread_default(struct fuse_handle_s *h, struct fuse_request_s *r, char *buffer, unsigned int size, off_t off)
{
    return -EOPNOTSUPP;
}

static int cb_pwrite_default(struct fuse_handle_s *h, struct fuse_request_s *r, char *buffer, unsigned int size, off_t off)
{
    return -EOPNOTSUPP;
}

static int cb_lseek_default(struct fuse_handle_s *h, struct fuse_request_s *r, off_t off, int whence)
{
    return -EOPNOTSUPP;
} */

void init_fuse_handle(struct fuse_handle_s *handle, unsigned int type, char *name, unsigned int len)
{

    handle->ctx=NULL;
    handle->ino=0;
    init_list_element(&handle->list, NULL);
    handle->refcount=1; /* the creator is also the user */
    handle->fh=0;
    handle->len=len;
    memcpy(handle->name, name, len);

/*    handle->cb.fgetstat=cb_fgetsetstat_default;
    handle->cb.fsetstat=cb_fgetsetstat_default;
    handle->cb.fsync=cb_fsyncflush_default;
    handle->cb.flush=cb_fsyncflush_default; */

    handle->cb.release=cb_release_default;

    /* if (type==FUSE_HANDLE_FLAG_OPENFILE) {

	handle->flags |= FUSE_HANDLE_FLAG_OPENFILE;
	handle->cb.type.file.pread=cb_pread_default;
	handle->cb.type.file.pwrite=cb_pwrite_default;
	handle->cb.type.file.lseek=cb_lseek_default;

    } else if (handle->flags & FUSE_HANDLE_FLAG_OPENDIR) {

	handle->flags |= FUSE_HANDLE_FLAG_OPENDIR;
	handle->cb.type.dir.fstatat=cb_fstatat_default;
	handle->cb.type.dir.unlinkat=cb_unlinkat_default;
	handle->cb.type.dir.readlinkat=cb_readlinkat_default;

    } */

}

struct fuse_handle_s *create_fuse_handle(struct service_context_s *ctx, uint64_t ino, unsigned int type, char *name, unsigned int len, uint64_t fh)
{
    struct fuse_handle_s *handle=NULL;

    if (name==NULL && len>0) {

	logoutput_warning("create_fuse_handle: invalid parameters ... name is not defined but len not zero");
	len=0;

    }

    handle=malloc(sizeof(struct fuse_handle_s) + len);

    if (handle) {
	unsigned int hashvalue=(ino % FUSE_HANDLE_HASHTABLE_SIZE);
	struct list_header_s *header=&hashtable[hashvalue];

	memset(handle, 0, sizeof(struct fuse_handle_s) + len);
	handle->flags=FUSE_HANDLE_FLAG_ALLOC;

	init_fuse_handle(handle, type, name, len);
	handle->ctx=ctx;
	handle->ino=ino;
	handle->fh=fh;

	write_lock_list_header(header);
	add_list_element_first(header, &handle->list);
	write_unlock_list_header(header);

    }

    return handle;

}
