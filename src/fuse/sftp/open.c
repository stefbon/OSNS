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
#include "getattr.h"
#include "setattr.h"
#include "lock.h"
#include "path.h"
#include "handle.h"

#include <linux/fuse.h>

/* READ a file */

struct _cb_common_hlpr_s {
    struct fuse_request_s 				*request;
    size_t						size;
};

static void _cb_success_pread(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_common_hlpr_s *hlpr=(struct _cb_common_hlpr_s *) ptr;
    reply_VFS_data(hlpr->request, (char *) reply->data, reply->size);
}

static void _cb_error_pread(struct fuse_handle_s *handle, unsigned int errcode, void *ptr)
{
    struct _cb_common_hlpr_s *hlpr=(struct _cb_common_hlpr_s *) ptr;

    if (errcode==ENODATA) {

	/* means eof/eod */
	reply_VFS_data(hlpr->request, NULL, 0);

    } else {

	reply_VFS_error(hlpr->request, errcode);

    }

}

static unsigned char _cb_interrupted_common(void *ptr)
{
    struct _cb_common_hlpr_s *hlpr=(struct _cb_common_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_pread(struct fuse_openfile_s *openfile, struct fuse_request_s *request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    struct _cb_common_hlpr_s hlpr;

    hlpr.size=size;
    hlpr.request=request;
    _sftp_handle_pread(openfile->header.handle, size, off, _cb_success_pread, _cb_error_pread, _cb_interrupted_common, (void *)&hlpr);
}

/* WRITE to a file */

static void _cb_success_pwrite(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_common_hlpr_s *hlpr=(struct _cb_common_hlpr_s *) ptr;
    struct fuse_write_out out;

    /* is there a way ** all bytes ** have been written ?? with the current SFTP versions not ... */

    memset(&out, 0, sizeof(struct fuse_write_out));
    out.size=hlpr->size;
    reply_VFS_data(hlpr->request, (char *) &out, sizeof(struct fuse_write_out));
}

static void _cb_error_common(struct fuse_handle_s *handle, unsigned int errcode, void *ptr)
{
    struct _cb_common_hlpr_s *hlpr=(struct _cb_common_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, errcode);
}

void _fs_sftp_pwrite(struct fuse_openfile_s *openfile, struct fuse_request_s *request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    struct _cb_common_hlpr_s hlpr;

    hlpr.size=size;
    hlpr.request=request;
    _sftp_handle_pwrite(openfile->header.handle, buff, size, off, _cb_success_pwrite, _cb_error_common, _cb_interrupted_common, (void *)&hlpr);
}

/* FLUSH */

void _fs_sftp_flush(struct fuse_open_header_s *oh, struct fuse_request_s *request, uint64_t lock_owner)
{
    reply_VFS_error(request, ENOSYS);
}

/* FSYNC */

static void _cb_success_fsync(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_common_hlpr_s *hlpr=(struct _cb_common_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, 0);
}

void _fs_sftp_fsync(struct fuse_open_header_s *oh, struct fuse_request_s *request, unsigned int flags)
{
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) oh;
    struct _cb_common_hlpr_s hlpr;

    hlpr.size=0;
    hlpr.request=request;
    _sftp_handle_fsync(oh->handle, flags, _cb_success_fsync, _cb_error_common, _cb_interrupted_common, (void *)&hlpr);
}

/* RELEASE */

void _fs_sftp_release(struct fuse_open_header_s *oh, struct fuse_request_s *f_request, unsigned int flags, uint64_t lock_owner)
{
    struct fuse_openfile_s *openfile=(struct fuse_openfile_s *) oh;

    reply_VFS_error(f_request, 0);

    if (oh->handle) {

	release_fuse_handle(oh->handle);
	oh->handle=NULL;

    }

}

/* LSEEK */

void _fs_sftp_lseek(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, off_t off, int whence)
{
    /* no support for (binary) lseek in SFTP */
    reply_VFS_error(f_request, ENOSYS);
}

/* OPEN 
    if O_CREAT is one of the flags a file will be created */

struct _cb_open_hlpr_s {
    struct fuse_request_s 				*request;
    struct fuse_openfile_s 				*openfile;
};

static void _cb_success_open(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_open_hlpr_s *hlpr=(struct _cb_open_hlpr_s *) ptr;
    struct fuse_openfile_s *openfile=hlpr->openfile;
    struct inode_s *inode=openfile->header.inode;
    struct fuse_open_out open_out;
    struct fuse_handle_s *handle=create_fuse_handle(ctx, get_ino_system_stat(&inode->stat), FUSE_HANDLE_FLAG_OPENFILE, (char *) reply->data, reply->size, 0);

    if (handle==NULL) {

	reply_VFS_error(hlpr->request, ENOMEM);
	openfile->error=ENOMEM;
	return;

    }

    memset(&open_out, 0, sizeof(struct fuse_open_out));
    open_out.fh=(uint64_t) openfile;

    if (inode->flags & INODE_FLAG_REMOTECHANGED) {

	/* VFS will free any cached data for this file */

	open_out.open_flags=0;
	inode->flags &= ~INODE_FLAG_REMOTECHANGED;

    } else {

	/* if there is a local cache it's usable since in sync with remote data */

	open_out.open_flags=FOPEN_KEEP_CACHE;

    }

    reply_VFS_data(hlpr->request, (char *) &open_out, sizeof(open_out));

    openfile->header.handle=handle;
    handle->flags |= (FUSE_HANDLE_FLAG_FGETSTAT | FUSE_HANDLE_FLAG_FSETSTAT | FUSE_HANDLE_FLAG_PREAD | FUSE_HANDLE_FLAG_PWRITE);
    if (get_index_sftp_extension_fsync(&ctx->interface)>0) handle->flags |= FUSE_HANDLE_FLAG_FSYNC;

    /* TODO: add fstatvfs (sftp supports this) */
    handle->cb.release=_sftp_handle_release;

    openfile->read=_fs_sftp_pread;
    openfile->write=_fs_sftp_pwrite;
    openfile->lseek=_fs_sftp_lseek;

    openfile->header.fgetattr=_fs_sftp_fgetattr;
    openfile->header.fsetattr=_fs_sftp_fsetattr;
    openfile->header.flush=_fs_sftp_flush;
    openfile->header.fsync=_fs_sftp_fsync;
    openfile->header.release=_fs_sftp_release;
    openfile->header.getlock=_fs_sftp_getlock;
    openfile->header.setlock=_fs_sftp_setlock;
    openfile->header.flock=_fs_sftp_flock;
}

static void _cb_error_open(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_open_hlpr_s *hlpr=(struct _cb_open_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, errcode);
    hlpr->openfile->error=errcode;
}

static unsigned char _cb_interrupted_open(void *ptr)
{
    struct _cb_open_hlpr_s *hlpr=(struct _cb_open_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_open(struct fuse_openfile_s *openfile, struct fuse_request_s *request, struct fuse_path_s *fpath, struct system_stat_s *stat, unsigned int flags)
{
    struct _cb_open_hlpr_s hlpr;

    hlpr.request=request;
    hlpr.openfile=openfile;

    _sftp_path_open(openfile->header.ctx, fpath, stat, flags, "open", _cb_success_open, _cb_error_open, _cb_interrupted_open, (void *) &hlpr);
}

