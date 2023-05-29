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
#include "libosns-threads.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-sftp.h"

#include "interface/sftp.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

#include "inode-stat.h"
#include "symlink.h"
#include "path.h"
#include "handle.h"
#include "getattr.h"
#include "setattr.h"
#include "lock.h"

#include <linux/fuse.h>

struct _cb_opendir_readlink_hlpr_s {
    struct inode_s                      *inode;
    unsigned int                        errcode;
    struct fuse_opendir_s               *opendir;
};

static void _cb_success_readlink(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_opendir_readlink_hlpr_s *hlpr=(struct _cb_opendir_readlink_hlpr_s *) ptr;

    if (reply->size<=4) {

        hlpr->errcode=EPROTO;

    } else {
        struct inode_s *inode=hlpr->inode;
        unsigned int len=get_uint32(reply->data);

        if (len + 4 < reply->size) {
	    struct context_interface_s *i=&ctx->interface;
	    unsigned int size=sftp_get_required_buffer_size_p2l(i, len);
	    char buffer[size+1];
	    struct fs_location_path_s path=FS_LOCATION_PATH_INIT;
	    char *link=(char *)(reply->data + 4);

            hlpr->errcode=set_inode_fuse_cache_symlink(ctx, inode, link);

        } else {

            hlpr->errcode=EPROTO;

        }

    }

}

static void _cb_error_readlink(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_opendir_readlink_hlpr_s *hlpr=(struct _cb_opendir_readlink_hlpr_s *) ptr;
    hlpr->errcode=errcode;
}

static unsigned char _cb_interrupted_readlink(void *ptr)
{
    struct _cb_opendir_readlink_hlpr_s *hlpr=(struct _cb_opendir_readlink_hlpr_s *) ptr;
    return ((hlpr->opendir->flags & (FUSE_OPENDIR_FLAG_EOD | FUSE_OPENDIR_FLAG_FINISH)) ? 1 : 0);
}

unsigned int _fs_sftp_opendir_readlink(struct fuse_opendir_s *opendir, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct _cb_opendir_readlink_hlpr_s hlpr;

    hlpr.inode=inode;
    hlpr.errcode=0;
    hlpr.opendir=opendir;

    _sftp_path_getattr(opendir->header.ctx, fpath, 0, 0, "readlink", _cb_success_readlink, _cb_error_readlink, _cb_interrupted_readlink, (void *) &hlpr);
    return hlpr.errcode;

}
