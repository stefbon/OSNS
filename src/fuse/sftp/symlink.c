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

/* READLINK */

struct _cb_readlink_hlpr_s {
    struct inode_s                                      *inode;
    unsigned int                                        errcode;
    struct fuse_opendir_s                               *opendir;
    struct fuse_request_s                               *request;
};

static void _cb_success_readlink(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_readlink_hlpr_s *hlpr=(struct _cb_readlink_hlpr_s *) ptr;

    if (reply->size<=4) {

        hlpr->errcode=EPROTO;

    } else {
        struct inode_s *inode=hlpr->inode;
        unsigned int len=get_uint32(reply->data);

        if (len + 4 < reply->size) {
	    char *link=(char *)(reply->data + 4);

            hlpr->errcode=set_inode_fuse_cache_symlink(ctx, inode, link);

        } else {

            hlpr->errcode=EPROTO;

        }

    }

}

static void _cb_error_readlink(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_readlink_hlpr_s *hlpr=(struct _cb_readlink_hlpr_s *) ptr;
    hlpr->errcode=errcode;
}

static unsigned char _cb_interrupted_readlink(void *ptr)
{
    struct _cb_readlink_hlpr_s *hlpr=(struct _cb_readlink_hlpr_s *) ptr;

    if (hlpr->opendir) return ((hlpr->opendir->flags & FUSE_OPENDIR_FLAG_FINISH) ? 1 : 0);
    if (hlpr->request) return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
    return 0;
}

unsigned int _fs_sftp_getlink(struct service_context_s *ctx, struct fuse_request_s *request, struct fuse_opendir_s *opendir, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct _cb_readlink_hlpr_s hlpr;

    hlpr.inode=inode;
    hlpr.errcode=0;
    hlpr.opendir=opendir;
    hlpr.request=request;

    _sftp_path_getattr(opendir->header.ctx, fpath, 0, 0, "readlink", _cb_success_readlink, _cb_error_readlink, _cb_interrupted_readlink, (void *) &hlpr);
    return hlpr.errcode;

}


void _fs_sftp_readlink(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct fuse_symlink_s *cs=get_inode_fuse_cache_symlink(inode);
    unsigned int errcode=0;

    /* try cached target symlink if there is any (only if remote inode -- and thus the symlink self -- is not changed) */

    if (cs==NULL || (inode->flags & INODE_FLAG_REMOTECHANGED)) {

        errcode=_fs_sftp_getlink(ctx, request, NULL, inode, fpath);

    }

    if (errcode>0) {

	reply_VFS_error(request, errcode);
	return;

    }

    cs=get_inode_fuse_cache_symlink(inode);

    if (cs) {
	struct context_interface_s *i=&ctx->interface;
	unsigned int size=sftp_get_required_buffer_size_p2l(&ctx->interface, cs->len);
	char buffer[size+1];
	struct fs_location_path_s path=FS_LOCATION_PATH_INIT;
	int tmp=0;

	memset(buffer, 0, size+1);
	tmp=sftp_convert_path_p2l(i, buffer, size, cs->path, cs->len);

	if (tmp==-1) {

	    reply_VFS_error(request, EIO);
	    return;

	}

	set_location_path(&path, 'c', (void *) buffer);

	if (remove_unneeded_path_elements(&path)>=0) {

	    if (path.back>0) {

		/* no matter what going one (or more) level higher is not allowed ...
		    also when the result is still a subdirectory of the shared directory like:

		    take the following example:
		    prefix: /home/guest with inside a directory doc
		    and the relative target is ../guest/doc will result in the directory /home/guest/doc, which is ok
		    but here is chosen that going higher/up at the prefix is not allowed in --> any <-- case
		*/

		reply_VFS_error(request, EXDEV);
		logoutput_debug("_cb_success_readlink: path has too many backslashes");

	    } else if (path.ptr[0]=='/') {

		tmp=sftp_compare_path(i, path.ptr, path.len, SFTP_COMPARE_PATH_PREFIX_SUBDIR);

		if (tmp>=0) {

		    /* target of link is a subdirectory of (remote) prefix */

		    reply_VFS_data(request, &path.ptr[tmp], (unsigned int)(path.len - tmp));

		} else {

		    reply_VFS_error(request, EXDEV);

		}

	    } else {

		size=strlen(buffer);
		reply_VFS_data(request, path.ptr, path.len);

	    }

	} else {

	    reply_VFS_error(request, EIO);

	}

    } else {

        reply_VFS_error(request, EIO);

    }


}

/* SYMLINK */

void _fs_sftp_symlink(struct service_context_s *context, struct fuse_request_s *request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct fs_location_path_s *target)
{
    reply_VFS_error(request, ENOSYS);
}

/*
    test the symlink pointing to target is valid
    - a symlink is valid when it stays inside the "root" directory of the shared map: target is a subdirectory of the root
*/

int _fs_sftp_symlink_validate(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, struct fs_location_path_s *sub)
{

    return -1;

}

