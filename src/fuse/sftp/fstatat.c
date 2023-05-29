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

#include "inode-stat.h"
#include "path.h"
#include "handle.h"
#include "lookup.h"

struct _cb_fstatat_hlpr_s {
    struct fuse_request_s 				*request;
    struct name_s 					*xname;
    struct inode_s					*pinode;
};

static void _cb_success_fstatat(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_fstatat_hlpr_s *hlpr=(struct _cb_fstatat_hlpr_s *) ptr;
    struct name_s *xname=hlpr->xname;
    struct entry_s *entry=NULL;
    struct create_entry_s ce;

    init_create_entry(&ce, xname, hlpr->pinode->alias, NULL, NULL, handle->ctx, NULL, (void *) (hlpr->request));

    ce.cache.ptr=(void *) reply;
    ce.pathlen=(handle->pathlen + 1 + xname->len);

    ce.cb_created=_sftp_lookup_entry_created;
    ce.cb_found=_sftp_lookup_entry_found;
    ce.cb_error=_sftp_lookup_entry_error;

    entry=create_entry_extended(&ce);

}

static void _cb_error_fstatat(struct fuse_handle_s *handle, unsigned int errcode, void *ptr)
{
    struct _cb_fstatat_hlpr_s *hlpr=(struct _cb_fstatat_hlpr_s *) ptr;

    reply_VFS_error(hlpr->request, errcode);

    if (errcode==ENOENT) {
	struct entry_s *entry=NULL;
	struct directory_s *directory=get_directory(handle->ctx, hlpr->pinode, 0);
	unsigned int tmp=0;

	entry=find_entry(directory, hlpr->xname, &tmp);
	if (entry) queue_inode_2forget(handle->ctx, get_ino_system_stat(&entry->inode->stat), 0, 0);

    }

}

static unsigned char _cb_interrupted_fstatat(void *ptr)
{
    struct _cb_fstatat_hlpr_s *hlpr=(struct _cb_fstatat_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_fstatat(struct fuse_handle_s *handle, struct fuse_request_s *request, struct inode_s *pinode, struct name_s *xname, struct fuse_path_s *fpath)
{
    struct service_context_s *ctx=handle->ctx;
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    unsigned int mask=(SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE | SYSTEM_STAT_UID | SYSTEM_STAT_GID | SYSTEM_STAT_MTIME | SYSTEM_STAT_CTIME | SYSTEM_STAT_SIZE); /* basic stats */
    struct _cb_fstatat_hlpr_s hlpr;

    hlpr.request=request;
    hlpr.xname=xname;
    hlpr.pinode=pinode;

    _sftp_handle_fstatat(handle, fpath, mask, 0, AT_SYMLINK_NOFOLLOW, _cb_success_fstatat, _cb_error_fstatat, _cb_interrupted_fstatat, (void *) &hlpr);

}

