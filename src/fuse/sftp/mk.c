/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-misc.h"
#include "libosns-interface.h"
#include "libosns-workspace.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"

#include "interface/sftp.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"
#include "inode-stat.h"
#include "path.h"
#include "handle.h"

/* CREATE a directory */

struct _cb_mk_hlpr_s {
    struct fuse_request_s 	*request;
    struct inode_s		*inode;
    unsigned int		pathlen;
    unsigned int		errcode;
    char			*handle;
    unsigned int		size;
};

static void _cb_success_mk(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct _cb_mk_hlpr_s *hlpr=(struct _cb_mk_hlpr_s *) ptr;
    struct inode_s *inode=hlpr->inode;
    struct entry_s *entry=inode->alias;
    struct directory_s *d=NULL;
    uint32_t nlink=1;

    inode->nlookup++;

    nlink=(system_stat_test_ISDIR(&inode->stat) ? 2 : 1);
    set_nlink_system_stat(&inode->stat, nlink);
    _fs_common_cached_lookup(ctx, hlpr->request, inode);

    add_inode_context(ctx, inode);
    d=get_upper_directory_entry(entry);
    (* d->inode->fs->type.dir.use_fs)(ctx, inode);
    adjust_pathmax(get_root_context(ctx), hlpr->pathlen);
    if (system_stat_test_ISDIR(&inode->stat)) assign_directory_inode(inode);
    get_current_time_system_time(&inode->stime);

}

static void _cb_error_mk(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_mk_hlpr_s *hlpr=(struct _cb_mk_hlpr_s *) ptr;
    struct inode_s *inode=NULL;

    reply_VFS_error(hlpr->request, errcode);

    inode=hlpr->inode;
    queue_inode_2forget(get_root_context(ctx), get_ino_system_stat(&inode->stat), 0, 0);
}

static unsigned char _cb_interrupted_mk(void *ptr)
{
    struct _cb_mk_hlpr_s *hlpr=(struct _cb_mk_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_mkdir(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s *entry, struct fuse_path_s *fpath, struct system_stat_s *stat)
{
    struct _cb_mk_hlpr_s hlpr;

    hlpr.request=request;
    hlpr.inode=entry->inode;
    hlpr.pathlen=(unsigned int)(fpath->path + fpath->len - fpath->pathstart); /* to determine the buffer is still big enough to hold every path on the workspace */
    hlpr.errcode=0;
    hlpr.handle=NULL;
    hlpr.size=0;

    _sftp_path_mkdir(ctx, fpath, stat, _cb_success_mk, _cb_error_mk, _cb_interrupted_mk, (void *) &hlpr);
}

/* MKNOD 
    mknod not supported in sftp; emulate with create */

static void _cb_success_mknod(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_mk_hlpr_s *hlpr=(struct _cb_mk_hlpr_s *) ptr;

    _cb_success_mk(ctx, reply, ptr);

    /* keep the received handle */

    hlpr->handle=reply->data;
    hlpr->size=reply->size;

    reply->data=NULL;
    reply->size=0;

}

void _fs_sftp_mknod(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s *entry, struct fuse_path_s *fpath, struct system_stat_s *stat)
{
    struct _cb_mk_hlpr_s hlpr;

    hlpr.request=request;
    hlpr.inode=entry->inode;
    hlpr.pathlen=(unsigned int)(fpath->path + fpath->len - fpath->pathstart); /* to determine the buffer is still big enough to hold every path on the workspace */
    hlpr.errcode=0;
    hlpr.handle=NULL;
    hlpr.size=0;

    /* sftp does not have a mknod,
        do this by using create and close it directly
        (emulate it using create/close) */

    _sftp_path_open(ctx, fpath, stat, (O_CREAT | O_EXCL), "open", _cb_success_mknod, _cb_error_mk, _cb_interrupted_mk, (void *) &hlpr);

    if (hlpr.handle && hlpr.size>0) {

	/* received a handle: operation success ... close it directly ... create a tmp fuse handle for that */

	unsigned int len=sizeof(struct fuse_handle_s) + hlpr.size;
	char buffer[len];
	struct fuse_handle_s *handle=(struct fuse_handle_s *) buffer;

	memset(handle, 0, len);
	init_fuse_handle(handle, FUSE_HANDLE_FLAG_OPENFILE, hlpr.handle, hlpr.size);
	release_fuse_handle(handle);

	free(hlpr.handle);
	hlpr.handle=NULL;
	hlpr.size=0;

    }

}

