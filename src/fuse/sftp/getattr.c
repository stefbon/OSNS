/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "handle.h"
#include "path.h"
#include "inode-stat.h"

/* GETATTR */

struct _cb_getattr_hlpr_s {
    struct fuse_request_s			*request;
    struct inode_s				*inode;
};

static void _cb_success_getattr(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_getattr_hlpr_s *hlpr=(struct _cb_getattr_hlpr_s *) ptr;
    struct inode_s *inode=hlpr->inode;
    struct context_interface_s *i=&ctx->interface;
    struct attr_context_s *attrctx=get_sftp_attr_context(&ctx->interface);
    struct attr_buffer_s abuff;

    /* read the atributes received from server */

    set_attr_buffer_read(&abuff, reply->data, reply->size);
    read_sftp_attributes(attrctx, &abuff, &inode->stat);

    _fs_common_getattr(hlpr->request, &inode->stat); /* reply to VFS */
    get_current_time_system_time(&inode->stime); /* adjust inodes stat synchronize time */
}

static void _cb_error_getattr(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_getattr_hlpr_s *hlpr=(struct _cb_getattr_hlpr_s *) ptr;

    reply_VFS_error(hlpr->request, errcode);

/*    if (errcode==ENOENT) {
	struct entry_s *entry=NULL;
	struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
	struct directory_s *directory=get_directory(w, hlpr->pinode, 0);

	entry=find_entry(directory, &hlpr->xname, &tmp);
	if (entry) queue_inode_2forget(w, get_ino_system_stat(&entry->inode->stat), 0, 0);

    }*/

}

static unsigned char _cb_interrupted_getattr(void *ptr)
{
    struct _cb_getattr_hlpr_s *hlpr=(struct _cb_getattr_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_getattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct _cb_getattr_hlpr_s hlpr;
    unsigned int mask=(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_SIZE); /* basic stats */

    hlpr.request=request;
    hlpr.inode=inode;

    _sftp_path_getattr(ctx, fpath, mask, 0, "getattr", _cb_success_getattr, _cb_error_getattr, _cb_interrupted_getattr, (void *) &hlpr);
}

/* FGETATTR */

static void _cb_success_fgetattr(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr)
{
    _cb_success_getattr(handle->ctx, reply, ptr);
}

static void _cb_error_fgetattr(struct fuse_handle_s *handle, unsigned int errcode, void *ptr)
{
    _cb_error_getattr(handle->ctx, errcode, ptr);
}

void _fs_sftp_fgetattr(struct fuse_open_header_s *oh, struct fuse_request_s *request)
{
    struct _cb_getattr_hlpr_s hlpr;
    unsigned int mask=(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_SIZE); /* basic stats */

    hlpr.request=request;
    hlpr.inode=oh->inode;

    _sftp_handle_fgetattr(oh->handle, mask, 0, _cb_success_fgetattr, _cb_error_fgetattr, _cb_interrupted_getattr, (void *) &hlpr);
}

