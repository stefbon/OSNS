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
#include "sftp/attr-utils.h"

#include "interface/sftp.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

#include "inode-stat.h"

static void handle_sftp_attr_reply(struct service_context_s *ctx, struct fuse_request_s *f_request, struct sftp_reply_s *reply, struct inode_s *inode)
{
    struct context_interface_s *i=&ctx->interface;
    struct attr_buffer_s abuff;

    /* read the atributes received from server */

    set_attr_buffer_read_attr_response(&abuff, &reply->response.attr);
    read_sftp_attributes_ctx(i, &abuff, &inode->stat);

    /* reply to VFS */

    _fs_common_getattr(get_root_context(ctx), f_request, inode);

    /* adjust inodes stat synchronize time */

    get_current_time_system_time(&inode->stime);

    /* free */

    free(reply->response.attr.buff);
    reply->response.attr.buff=NULL;
    unset_fuse_request_flags_cb(f_request);

}

/* GETATTR */

void _fs_sftp_getattr(struct service_context_s *ctx, struct fuse_request_s *f_request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    logoutput_debug("_fs_sftp_getattr: %li %s", get_ino_system_stat(&inode->stat), fpath->pathstart);

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);

    if (result==-1) {

	logoutput_debug("_fs_sftp_getattr: error converting local path");
	goto out;

    }

    init_sftp_request(&sftp_r, i, f_request);
    sftp_r.id=0;
    sftp_r.call.lstat.path=(unsigned char *) buffer;
    sftp_r.call.lstat.len=(unsigned int) result;

    /* send lstat cause not interested in target when dealing with symlink */

    if (send_sftp_lstat_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);
	error=0;

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {

		handle_sftp_attr_reply(ctx, f_request, reply, inode);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;

	    } else {

		error=(reply->error) ? reply->error : EPROTO;

	    }

	}

    } else {

	error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

    }

    out:

    logoutput("_fs_sftp_getattr: error %i (%s)", error, strerror(error));
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/* FGETATTR */

void _fs_sftp_fgetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request)
{
    struct service_context_s *ctx=(struct service_context_s *) openfile->context;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, i, f_request);

    sftp_r.id=0;
    sftp_r.call.fstat.handle=(unsigned char *) openfile->handle->name;
    sftp_r.call.fstat.len=openfile->handle->len;

    /* send fstat cause a handle is available */

    if (send_sftp_fstat_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);
	error=0;

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {

		handle_sftp_attr_reply(ctx, f_request, reply, openfile->inode);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;

	    } else {

		error=EPROTO;

	    }

	}

    } else {

	error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

void _fs_sftp_getattr_disconnected(struct service_context_s *ctx, struct fuse_request_s *f_request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    _fs_common_getattr(get_root_context(ctx), f_request, inode);
}

void _fs_sftp_fgetattr_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request)
{
    struct service_context_s *ctx=openfile->context;
    _fs_common_getattr(get_root_context(ctx), f_request, openfile->inode);
}

