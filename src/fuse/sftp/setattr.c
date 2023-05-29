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

void filter_setting_attributes(struct inode_s *inode, struct system_stat_s *stat)
{

    if (stat->mask & SYSTEM_STAT_MODE) {

	if (get_mode_system_stat(&inode->stat) == get_mode_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_MODE;

    }

    if (stat->mask & SYSTEM_STAT_SIZE) {

	if (get_size_system_stat(&inode->stat) == get_size_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_SIZE;

    }

    if (stat->mask & SYSTEM_STAT_UID) {

	if (get_uid_system_stat(&inode->stat) == get_uid_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_UID;

    }

    if (stat->mask & SYSTEM_STAT_GID) {

	if (get_gid_system_stat(&inode->stat) == get_gid_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_GID;

    }

    if (stat->mask & SYSTEM_STAT_ATIME) {

	if ((get_atime_sec_system_stat(&inode->stat) == get_atime_sec_system_stat(stat)) && (get_atime_nsec_system_stat(&inode->stat) == get_atime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_ATIME;

    }

    if (stat->mask & SYSTEM_STAT_MTIME) {

	if ((get_mtime_sec_system_stat(&inode->stat) == get_mtime_sec_system_stat(stat)) && (get_mtime_nsec_system_stat(&inode->stat) == get_mtime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_MTIME;

    }

    if (stat->mask & SYSTEM_STAT_CTIME) {

	if ((get_ctime_sec_system_stat(&inode->stat) == get_ctime_sec_system_stat(stat)) && (get_ctime_nsec_system_stat(&inode->stat) == get_ctime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_CTIME;

    }

    if (stat->mask & SYSTEM_STAT_BTIME) {

	if ((get_btime_sec_system_stat(&inode->stat) == get_btime_sec_system_stat(stat)) && (get_btime_nsec_system_stat(&inode->stat) == get_btime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_BTIME;

    }

}

struct _cb_setattr_hlpr_s {
    struct fuse_request_s 			*request;
    struct inode_s				*inode;
    struct system_stat_s			*stat;
};

static void _cb_success_setattr(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_setattr_hlpr_s *hlpr=(struct _cb_setattr_hlpr_s *) ptr;

    set_local_attributes(&ctx->interface, &hlpr->inode->stat, hlpr->stat);
    _fs_common_getattr(hlpr->request, &hlpr->inode->stat);

}

static void _cb_error_setattr(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_setattr_hlpr_s *hlpr=(struct _cb_setattr_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, errcode);
}

static unsigned char _cb_interrupted_setattr(void *ptr)
{
    struct _cb_setattr_hlpr_s *hlpr=(struct _cb_setattr_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

/* SETATTR */

void _fs_sftp_setattr(struct service_context_s *ctx, struct fuse_request_s *request, struct inode_s *inode, struct fuse_path_s *fpath, struct system_stat_s *stat)
{
    struct _cb_setattr_hlpr_s hlpr;

    /* test attributes really differ from the current */

    filter_setting_attributes(inode, stat);
    if (stat->mask==0) {

	reply_VFS_error(request, 0);
	return;

    }

    hlpr.request=request;
    hlpr.inode=inode;
    hlpr.stat=stat;

    _sftp_path_setattr(ctx, fpath, stat, _cb_success_setattr, _cb_error_setattr, _cb_interrupted_setattr, (void *) &hlpr);

}

/* FSETATTR */

static void _cb_success_fsetattr(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr)
{
    _cb_success_setattr(handle->ctx, reply, ptr);
}

static void _cb_error_fsetattr(struct fuse_handle_s *handle, unsigned int errcode, void *ptr)
{
    _cb_error_setattr(handle->ctx, errcode, ptr);
}

void _fs_sftp_fsetattr(struct fuse_open_header_s *oh, struct fuse_request_s *request, struct system_stat_s *stat)
{
    struct _cb_setattr_hlpr_s hlpr;
    struct inode_s *inode=oh->inode;

    /* test attributes really differ from the current */

    filter_setting_attributes(inode, stat);
    if (stat->mask==0) {

	reply_VFS_error(request, 0);
	return;

    }

    hlpr.request=request;
    hlpr.inode=inode;
    hlpr.stat=stat;

    _sftp_handle_fsetattr(oh->handle, stat, _cb_success_fsetattr, _cb_error_fsetattr, _cb_interrupted_setattr, (void *) &hlpr);

}

