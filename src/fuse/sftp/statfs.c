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

#include "datatypes/ssh-uint.h"
#include "path.h"
#include "handle.h"

#include <linux/fuse.h>

#define UINT32_T_MAX		0xFFFFFFFF

/* STATVFS */

struct _cb_statvfs_hlpr_s {
    struct fuse_request_s *request;
};

static void _cb_success_statvfs(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_statvfs_hlpr_s *hlpr=(struct _cb_statvfs_hlpr_s *) ptr;
    struct service_context_s *rootctx=get_root_context(ctx);
    struct fuse_statfs_out out;
    char *pos = (char *) reply->data;

	/* reply looks like

	    8 f_bsize
	    8 f_frsize
	    8 f_blocks
	    8 f_bfree
	    8 f_bavail
	    8 f_files
	    8 f_ffree
	    8 f_favail
	    8 f_fsid
	    8 f_flag
	    8 f_namemax

	*/

    memset(&out, 0, sizeof(struct fuse_statfs_out));

    out.st.bsize=get_uint64(pos);
    pos+=8;
    out.st.frsize=get_uint64(pos);
    pos+=8;
    out.st.blocks=get_uint64(pos);
    pos+=8;
    out.st.bfree=get_uint64(pos);
    pos+=8;
    out.st.bavail=get_uint64(pos);
    pos+=8;
    out.st.files=(uint64_t) rootctx->service.workspace.nrinodes;
    pos+=8;
    out.st.ffree=(uint64_t) (UINT32_T_MAX - out.st.files);
    pos+=8;
    /* ignore favail */
    pos+=8;
    /* ignore fsid */
    pos+=8;
    /* ignore flag */
    pos+=8;
    /* namelen as uint64??? sftp can handle very very long filenames; uint16 would be enough */
    out.st.namelen=get_uint64(pos);
    pos+=8;

    reply_VFS_data(hlpr->request, (char *) &out, sizeof(struct fuse_statfs_out));

}

static void _cb_error_statvfs(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_statvfs_hlpr_s *hlpr=(struct _cb_statvfs_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, errcode);
}

static unsigned char _cb_interrupted_statvfs(void *ptr)
{
    struct _cb_statvfs_hlpr_s *hlpr=(struct _cb_statvfs_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_statfs(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct _cb_statvfs_hlpr_s hlpr;

    hlpr.request=request;
    _sftp_path_getattr(ctx, fpath, 0, 0, "statvfs", _cb_success_statvfs, _cb_error_statvfs, _cb_interrupted_statvfs, (void *) &hlpr);
}

