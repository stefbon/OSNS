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
#include "sftp/rw-attr-generic.h"

#include "interface/sftp.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

#include "inode-stat.h"
#include "getattr.h"
#include "setattr.h"
#include "lock.h"

#include <linux/fuse.h>

void _sftp_handle_fgetattr(struct fuse_handle_s *handle, unsigned int mask, unsigned int property,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct service_context_s *ctx=(struct service_context_s *) handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct attr_context_s *attrctx=get_sftp_attr_context(i);
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;

    init_sftp_request(&sftp_r, i);
    sftp_r.call.fgetstat.handle=(unsigned char *) handle->name;
    sftp_r.call.fgetstat.len=handle->len;
    sftp_r.call.fgetstat.valid=translate_stat_mask_2_valid(attrctx, mask, 'r');

    if (send_sftp_fstat_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {

		(* cb_success)(handle, reply, ptr);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *response=&reply->response.status;

		if (response->linux_error) errcode=response->linux_error;
		goto out;

	    } else {

		errcode=((reply->error) ? reply->error : EPROTO);
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;
    out:
    (* cb_error)(handle, errcode, ptr);
}

void _sftp_handle_fsetattr(struct fuse_handle_s *handle, struct system_stat_s *stat2set,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct service_context_s *ctx=handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa;
    unsigned int size=get_attr_buffer_size(i, &r, stat2set, &gssa) + 4;
    char buffer[size];
    struct attr_buffer_s abuff;

    /* compare the stat mask as asked by FUSE and the ones SFTP can set using this protocol version */

    if (gssa.stat_mask_result != gssa.stat_mask_asked)
	logoutput_warning("_sftp_handle_fsetattr: not able to set every stat attr: asked %i result %i", gssa.stat_mask_asked, gssa.stat_mask_result);

    stat2set->mask=gssa.stat_mask_result;
    set_attr_buffer_write(&abuff, buffer, size);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid.mask);
    write_attributes_ctx(i, &abuff, &r, stat2set, &gssa.valid);

    init_sftp_request(&sftp_r, i);
    sftp_r.call.fsetstat.handle=(unsigned char *) handle->name;
    sftp_r.call.fsetstat.len=handle->len;
    sftp_r.call.fsetstat.size=(unsigned int) abuff.pos;
    sftp_r.call.fsetstat.buff=(unsigned char *) abuff.buffer;

    if (send_sftp_fsetstat_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    (* cb_success)(handle, reply, ptr);
		    return;

		}

		if (reply->response.status.linux_error) {

		    errcode=reply->response.status.linux_error;
		    goto out;

		}

	    } else {

		errcode=EPROTO;
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;

    out:
    (* cb_error)(handle, errcode, ptr);

}

/* FSYNC a file */

void _sftp_handle_fsync(struct fuse_handle_s *handle, unsigned int flags,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct service_context_s *ctx=handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;

    init_sftp_request(&sftp_r, i);
    sftp_r.call.fsync.handle=(unsigned char *) handle->name;
    sftp_r.call.fsync.len=handle->len;

    if (send_sftp_fsync_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    (* cb_success)(handle, reply, ptr);
		    return;

		}

		if (reply->response.status.linux_error) {

		    errcode=reply->response.status.linux_error;
		    goto out;

		}

	    } else {

		errcode=EPROTO;
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;

    out:
    (* cb_error)(handle, errcode, ptr);

}

/* RELEASE a handle
    a release of a handle is done somewehere in the background (when not needed anymore)
    there is no direct 1-1 relation with a fuse request which can be interrupted */

static unsigned char _cb_interrupted_dummy(void *ptr)
{
    return 0;
}

void _sftp_handle_release(struct fuse_handle_s *handle)
{
    struct service_context_s *ctx=handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;

    init_sftp_request(&sftp_r, i);
    sftp_r.call.close.handle=(unsigned char *) handle->name;
    sftp_r.call.close.len=handle->len;

    if (send_sftp_close_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, _cb_interrupted_dummy, NULL)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code>0) {
		    unsigned int errcode=reply->response.status.linux_error;

		    logoutput_debug("_sftp_handle_release: got reply %i:%s", errcode, strerror(errcode));

		}

	    }

	}

    }

}

void _sftp_handle_fstatat(struct fuse_handle_s *handle, struct fuse_path_s *fpath, unsigned int mask, unsigned int property, unsigned int flags,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct service_context_s *ctx=handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    struct attr_context_s *attrctx=get_sftp_attr_context(i);
    unsigned int valid=translate_stat_mask_2_valid(attrctx, mask, 'r');
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);
    if (result==-1) goto out;

    init_sftp_request(&sftp_r, i);
    sftp_r.call.fstatat.handle.handle=(unsigned char *) handle->name;
    sftp_r.call.fstatat.handle.len=handle->len;
    sftp_r.call.fstatat.path.path=(unsigned char *) buffer;
    sftp_r.call.fstatat.path.len=(unsigned int) result;
    sftp_r.call.fstatat.valid=valid;
    sftp_r.call.fstatat.property=property;
    sftp_r.call.fstatat.flags=(flags & (AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW));

    if (send_sftp_fstatat_ctx(i, &sftp_r)>=0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {

		(* cb_success)(handle, reply, ptr);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->linux_error) errcode=status->linux_error;
		goto out;

	    } else {

		errcode=EPROTO;
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;

    out:
    (* cb_error)(handle, errcode, ptr);

}

/* READ a file */

void _sftp_handle_pread(struct fuse_handle_s *handle, size_t size, off_t off,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct service_context_s *ctx=handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;

    init_sftp_request(&sftp_r, i);
    sftp_r.call.read.handle=(unsigned char *) handle->name;
    sftp_r.call.read.len=handle->len;
    sftp_r.call.read.offset=(uint64_t) off;
    sftp_r.call.read.size=(uint64_t) size;

    if (send_sftp_read_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_DATA) {

		(* cb_success)(handle, reply, ptr);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->linux_error) {

		    errcode=status->linux_error;
		    goto out;

		}

	    } else {

		errcode=EPROTO;
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;
    out:
    (* cb_error)(handle, errcode, ptr);

}

/* WRITE to a file */

void _sftp_handle_pwrite(struct fuse_handle_s *handle, const char *buff, size_t size, off_t off,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct service_context_s *ctx=handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;

    init_sftp_request(&sftp_r, i);
    sftp_r.call.write.handle=(unsigned char *) handle->name;
    sftp_r.call.write.len=handle->len;
    sftp_r.call.write.offset=(uint64_t) off;
    sftp_r.call.write.size=(uint64_t) size;
    sftp_r.call.write.data=(char *) buff;

    if (send_sftp_write_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->code==0) {

		    (* cb_success)(handle, reply, ptr);
		    return;

		}

		if (status->linux_error) {

		    errcode=status->linux_error;
		    goto out;

		}

	    } else {

		errcode=EPROTO;
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;
    out:
    (* cb_error)(handle, errcode, ptr);

}

void _sftp_handle_lock(struct fuse_handle_s *handle, off_t off, unsigned int size, unsigned int blockmask,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{

    (* cb_error)(handle, ENOSYS, ptr);

}

void _sftp_handle_unlock(struct fuse_handle_s *handle, off_t off, unsigned int size,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    (* cb_error)(handle, ENOSYS, ptr);
}

void _sftp_handle_readdir(struct fuse_handle_s *handle, size_t size, off_t off,
			void (* cb_success)(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct fuse_handle_s *handle, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct service_context_s *ctx=handle->ctx;
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;
    int result=-1;

    logoutput_debug("_sftp_handle_readdir: off %lu size %lu", off, size);

    init_sftp_request(&sftp_r, i);
    sftp_r.call.readdir.handle=(unsigned char *) handle->name;
    sftp_r.call.readdir.len=handle->len;

    if (send_sftp_readdir_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_NAME) {

		(* cb_success)(handle, reply, ptr);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->linux_error) {

		    errcode=status->linux_error;
		    goto out;

		}

	    } else {

		errcode=EPROTO;
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;
    out:
    (* cb_error)(handle, errcode, ptr);

}

