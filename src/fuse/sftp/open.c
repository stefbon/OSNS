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

#include <linux/fuse.h>

/* READ a file */

void _fs_sftp_read(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.read.handle=(unsigned char *) openfile->handle->name;
    sftp_r.call.read.len=openfile->handle->len;
    sftp_r.call.read.offset=(uint64_t) off;
    sftp_r.call.read.size=(uint64_t) size;

    /* ignore flags and lockowner */

    if (send_sftp_read_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_DATA) {

		logoutput("_fs_sftp_read: received %i bytes", reply->response.data.size);

		reply_VFS_data(f_request, (char *) reply->response.data.data, reply->response.data.size);
		free(reply->response.data.data);
		reply->response.data.data=NULL;
		unset_fuse_request_flags_cb(f_request);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;

		if (error==ENODATA) {
		    char dummy='\0';

		    reply_VFS_data(f_request, &dummy, 0);
		    unset_fuse_request_flags_cb(f_request);
		    return;

		}

		logoutput("_fs_sftp_read: status reply %i", error);

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

/* WRITE to a file */

void _fs_sftp_write(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);
    sftp_r.call.write.handle=(unsigned char *) openfile->handle->name;
    sftp_r.call.write.len=openfile->handle->len;
    sftp_r.call.write.offset=(uint64_t) off;
    sftp_r.call.write.size=(uint64_t) size;
    sftp_r.call.write.data=(char *)buff;

    if (send_sftp_write_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(&context->interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {
		    struct fuse_write_out write_out;

		    write_out.size=size;
		    write_out.padding=0;

		    reply_VFS_data(f_request, (char *) &write_out, sizeof(struct fuse_write_out));
		    unset_fuse_request_flags_cb(f_request);
		    return;

		} else {

		    error=reply->response.status.linux_error;
		    logoutput("_fs_sftp_write: status reply %i", error);

		}

	    } else {

		error=EPROTO;

	    }

	}

    } else {

	error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);;

}

/* FSYNC a file */

void _fs_sftp_fsync(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned int flags)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);
    sftp_r.call.fsync.handle=(unsigned char *) openfile->handle->name;
    sftp_r.call.fsync.len=openfile->handle->len;

    /* TODO: add f_request */

    if (send_sftp_fsync_ctx(interface, &sftp_r, &error)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		/* send ok reply to VFS no matter what the sftp server reports */

		reply_VFS_error(f_request, 0);

		if (reply->response.status.linux_error==EOPNOTSUPP) {

		    logoutput_warning("_fs_sftp_fsync: server response fsync not supported");

		} else if (reply->response.status.code>0) {

		    error=reply->response.status.linux_error;
		    logoutput_notice("_fs_sftp_fsync: status reply %i:%s", error, strerror(error));

		}

		unset_fuse_request_flags_cb(f_request);
		return;

	    } else {

		error=EPROTO;

	    }

	} else {

	    error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

	}

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/* FLUSH a file */

void _fs_sftp_flush(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, uint64_t lockowner)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;

    /* no support for flush in sftp */
    reply_VFS_error(f_request, 0);
}

/* CLOSE a file */

void _fs_sftp_release(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned int flags, uint64_t lock_owner)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.close.handle=(unsigned char *) openfile->handle->name;
    sftp_r.call.close.len=openfile->handle->len;

    /*
	TODO:
	- handle flush?
	- unlock when lock set (flock)
    */

    if (send_sftp_close_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {
		struct entry_s *entry=openfile->inode->alias;

		/* send ok reply to VFS no matter what the sftp server reports */

		reply_VFS_error(f_request, 0);

		if (openfile->handle) {

		    release_fuse_handle(openfile->handle);
		    openfile->handle=NULL;

		}

		if (reply->response.status.code!=0) {

		    error=reply->response.status.linux_error;
		    logoutput_notice("_fs_sftp_release: status reply %i:%s", error, strerror(error));

		}

		unset_fuse_request_flags_cb(f_request);
		return;

	    } else {

		error=EPROTO;

	    }

	}

    } else {

	error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

    }

    out:

    reply_VFS_error(f_request, error);

    if (openfile->handle) {

	release_fuse_handle(openfile->handle);
	openfile->handle=NULL;

    }
    unset_fuse_request_flags_cb(f_request);

}

/* OPEN a file */

void _fs_sftp_open(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct fuse_path_s *fpath, unsigned int flags)
{
    struct service_context_s *ctx=(struct service_context_s *) openfile->context;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);

    if (result==-1) {

	logoutput_debug("_fs_sftp_open: error converting local path");
	goto out;

    }

    logoutput("_fs_sftp_open: path %s flags %i", fpath->pathstart, flags);

    init_sftp_request(&sftp_r, i, f_request);
    sftp_r.call.open.path=(unsigned char *) buffer;
    sftp_r.call.open.len=(unsigned int) result;
    sftp_r.call.open.posix_flags=flags;

    if (send_sftp_open_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {
		struct inode_s *inode=openfile->inode;
		struct fuse_open_out open_out;
		struct fuse_handle_s *handle=create_fuse_handle(ctx, get_ino_system_stat(&inode->stat), FUSE_HANDLE_FLAG_OPENFILE, (char *) reply->response.handle.name, reply->response.handle.len, 0);

		/* handle name is defined in sftp_r.response.handle.name: take it "over" */

		if (handle==NULL) {

		    error=ENOMEM;
		    goto out;

		}

		openfile->handle=handle;
		open_out.fh=(uint64_t) openfile;

		if (inode->flags & INODE_FLAG_REMOTECHANGED) {

		    /* VFS will free any cached data for this file */

		    open_out.open_flags=0;
		    inode->flags &= ~INODE_FLAG_REMOTECHANGED;

		} else {

		    /* if there is a local cache it's usable since in sync with remote data */

		    open_out.open_flags=FOPEN_KEEP_CACHE;

		}

		open_out.padding=0;
		reply_VFS_data(f_request, (char *) &open_out, sizeof(open_out));
		unset_fuse_request_flags_cb(f_request);

		openfile->read=_fs_sftp_read;
		openfile->write=_fs_sftp_write;
		openfile->flush=_fs_sftp_flush;
		openfile->fsync=_fs_sftp_fsync;
		// openfile->lseek=_fs_sftp_lseek; /* supported ? */
		openfile->fgetattr=_fs_sftp_fgetattr;
		openfile->fsetattr=_fs_sftp_fsetattr;
		openfile->getlock=_fs_sftp_getlock;
		openfile->setlock=_fs_sftp_setlock;
		openfile->flock=_fs_sftp_flock;

		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;
		logoutput("_fs_sftp_open: status reply %i", error);

	    } else {

		error=EPROTO;

	    }

	}

    } else {

	error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

    }

    out:

    openfile->error=error;
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/* CREATE a file */

void _fs_sftp_create(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct fuse_path_s *fpath, struct system_stat_s *stat, unsigned int flags)
{
    struct service_context_s *ctx=(struct service_context_s *) openfile->context;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa;
    unsigned int attrsize=get_attr_buffer_size(i, &r, stat, &gssa) + 5;
    char attrs[attrsize];
    struct attr_buffer_s abuff;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);

    if (result==-1) {

	logoutput_debug("_fs_sftp_create: error converting local path");
	goto out;

    }

    /* compare the stat mask as asked by FUSE and the ones SFTP can set using this protocol version */

    if (gssa.stat_mask_result != gssa.stat_mask_asked)
	logoutput_warning("_fs_sftp_create: not able to set every stat attr: asked %i to set %i", gssa.stat_mask_asked, gssa.stat_mask_result);

    /* enable writing of subseconds (only of course if one of the time attr is included)*/

    if (gssa.stat_mask_result & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME | SYSTEM_STAT_BTIME | SYSTEM_STAT_CTIME)) {

	if (enable_attributes_ctx(i, &gssa.valid, "subseconds")==1) {

	    logoutput_info("_fs_sftp_create: enabled setting subseconds");

	} else {

	    logoutput_info("_fs_sftp_create: subseconds not supported");

	}

    }

    logoutput("_fs_sftp_create: path %s", fpath->pathstart);

    set_attr_buffer_write(&abuff, attrs, attrsize);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid.mask);
    write_attributes_ctx(i, &abuff, &r, stat, &gssa.valid);

    init_sftp_request(&sftp_r, i, f_request);

    sftp_r.call.create.path=(unsigned char *) buffer;
    sftp_r.call.create.len=(unsigned int) result;
    sftp_r.call.create.posix_flags=(flags | O_CREAT);
    sftp_r.call.create.size=(unsigned int) abuff.pos;
    sftp_r.call.create.buff=(unsigned char *) abuff.buffer;

    if (send_sftp_create_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {
		struct inode_s *inode=openfile->inode;
		struct fuse_handle_s *handle=create_fuse_handle(ctx, get_ino_system_stat(&inode->stat), FUSE_HANDLE_FLAG_OPENFILE, (char *) reply->response.handle.name, reply->response.handle.len, 0);

		if (handle==NULL) {

		    error=ENOMEM;
		    goto out;

		}

		/* handle name is defined in sftp_r.response.handle.name: take it "over" */

		openfile->handle=handle;
		add_inode_context(ctx, openfile->inode);

		/* note: how the entry is created on the remote server does not have to be the same .... */

		_fs_common_cached_create(ctx, f_request, openfile);
		unset_fuse_request_flags_cb(f_request);

		openfile->read=_fs_sftp_read;
		openfile->write=_fs_sftp_write;
		openfile->flush=_fs_sftp_flush;
		openfile->fsync=_fs_sftp_fsync;
		// openfile->lseek=_fs_sftp_lseek; /* supported ? */
		openfile->fgetattr=_fs_sftp_fgetattr;
		openfile->fsetattr=_fs_sftp_fsetattr;
		openfile->getlock=_fs_sftp_getlock;
		openfile->setlock=_fs_sftp_setlock;
		openfile->flock=_fs_sftp_flock;

		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;
		logoutput("_fs_sftp_create: status reply %i", error);

		/* set an error open/create understands */

		error=EINVAL;

	    } else {

		error=EINVAL;

	    }

	}

    } else {

	error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

    }

    out:
    openfile->error=error;
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

void _fs_sftp_open_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct fuse_path_s *fpath, unsigned int flags)
{
    reply_VFS_error(f_request, ENOTCONN);
}
void _fs_sftp_create_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct fuse_path_s *fpath, struct stat *st, unsigned int flags)
{
    reply_VFS_error(f_request, ENOTCONN);
}
void _fs_sftp_read_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    reply_VFS_error(f_request, ENOTCONN);
}
void _fs_sftp_write_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, const char *buff, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    reply_VFS_error(f_request, ENOTCONN);
}
void _fs_sftp_fsync_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned char datasync)
{
    reply_VFS_error(f_request, ENOTCONN);
}
void _fs_sftp_flush_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, uint64_t lockowner)
{
    reply_VFS_error(f_request, ENOTCONN);
}
void _fs_sftp_release_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned int flags, uint64_t lock_owner)
{
    reply_VFS_error(f_request, ENOTCONN);
}
