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

#include "path.h"
#include "handle.h"

/* REMOVE a file */

struct _cb_rm_hlpr_s {
    struct fuse_request_s 		*request;
    struct service_context_s 		*ctx;
    struct entry_s 			**p_entry;
};

static void _cb_success_rm(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_rm_hlpr_s *hlpr=(struct _cb_rm_hlpr_s *) ptr;
    struct entry_s **p_entry=hlpr->p_entry;

    reply_VFS_error(hlpr->request, 0);

    if (p_entry) {
	struct entry_s *entry=*p_entry;
	struct inode_s *inode=entry->inode;

	if (inode) queue_inode_2forget(ctx, get_ino_system_stat(&inode->stat), FORGET_INODE_FLAG_RM, 0);

    }

}

static void _cb_error_rm(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_rm_hlpr_s *hlpr=(struct _cb_rm_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, errcode);
}

static unsigned char _cb_interrupted_rm(void *ptr)
{
    struct _cb_rm_hlpr_s *hlpr=(struct _cb_rm_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_unlink(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s **p_entry, struct fuse_path_s *fpath)
{
    struct _cb_rm_hlpr_s hlpr;

    hlpr.request=request;
    hlpr.ctx=ctx;
    hlpr.p_entry=p_entry;

    _sftp_path_rm(ctx, fpath, "unlink", _cb_success_rm, _cb_error_rm, _cb_interrupted_rm, (void *) &hlpr);
}

void _fs_sftp_rmdir(struct service_context_s *ctx, struct fuse_request_s *request, struct entry_s **p_entry, struct fuse_path_s *fpath)
{
    struct _cb_rm_hlpr_s hlpr;

    hlpr.request=request;
    hlpr.ctx=ctx;
    hlpr.p_entry=p_entry;

    _sftp_path_rm(ctx, fpath, "rmdir", _cb_success_rm, _cb_error_rm, _cb_interrupted_rm, (void *) &hlpr);
}

