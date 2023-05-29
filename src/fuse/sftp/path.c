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

/* TODO:
    add a stat mask as parameter, since version 4 sftp stat calls include a valid flag

    a generic call to get info:
    - getattr
    - statvfs
    - readlink

    parameter mask is about the stat mask of values the client is interested in
    parameter property is about additional about mimetype for example
    parameter what is about the type of info
*/

void _sftp_path_getattr(struct service_context_s *ctx, struct fuse_path_s *fpath, unsigned int mask, unsigned int property, const char *what,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;
    unsigned int expected_reply=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);
    if (result==-1) goto out;

    logoutput("_sftp_path_getattr: path %s mask %u what %s", fpath->pathstart, mask, what);

    init_sftp_request(&sftp_r, i);

    if (strcmp(what, "getattr")==0) {
	struct attr_context_s *attrctx=get_sftp_attr_context(i);

	sftp_r.call.lstat.path=(unsigned char *) buffer;
	sftp_r.call.lstat.len=(unsigned int) result;
	expected_reply=SSH_FXP_ATTRS;

	if ((* attrctx->ops.get_property)(attrctx, SFTP_ATTR_PROPERTY_VALIDFIELD_STAT))
	    sftp_r.call.lstat.valid=translate_stat_mask_2_valid(attrctx, mask, 'r');

	/* send lstat cause not interested in target when dealing with symlink */
	result=send_sftp_lstat_ctx(i, &sftp_r);

    } else if (strcmp(what, "statvfs")==0) {

	sftp_r.call.statvfs.path=(unsigned char *) buffer;
	sftp_r.call.statvfs.len=(unsigned int) result;
	expected_reply=SSH_FXP_EXTENDED_REPLY;

	result=send_sftp_statvfs_ctx(i, &sftp_r);

    } else if (strcmp(what, "readlink")==0) {

	sftp_r.call.readlink.path=(unsigned char *) buffer;
	sftp_r.call.readlink.len=(unsigned int) result;
	expected_reply=SSH_FXP_NAME;

	result=send_sftp_readlink_ctx(i, &sftp_r);

    } else {

	result=-1;

    }

    if (result>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==expected_reply) {

		(* cb_success)(ctx, reply, ptr);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->linux_error) {

		    errcode=status->linux_error;
		    goto out;

		}

	    } else {

		errcode=(reply->error) ? reply->error : EPROTO;
		goto out;

	    }

	}

    }

    if (sftp_r.reply.error) errcode=sftp_r.reply.error;

    out:
    (* cb_error)(ctx, errcode, ptr);

}

void _sftp_path_open(struct service_context_s *ctx, struct fuse_path_s *fpath, struct system_stat_s *stat, unsigned int flags, const char *what,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa;
    unsigned int attrsize=((flags & O_CREAT) ? (get_attr_buffer_size(i, &r, stat, &gssa) + 5) : 0);
    char attrs[attrsize];
    struct attr_buffer_s abuff;
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);
    if (result==-1) goto out;

    logoutput("_sftp_path_open: path %s flags %u what %s", fpath->pathstart, flags, what);

    init_sftp_request(&sftp_r, i);

    if (strcmp(what, "open")==0) {

	sftp_r.call.open.path=(unsigned char *) buffer;
	sftp_r.call.open.len=(unsigned int) result;
	sftp_r.call.open.posix_flags=flags;

	if (flags & O_CREAT) {

	    /* when creating a file add attributes to set on server */

	    set_attr_buffer_write(&abuff, attrs, attrsize);
	    (* abuff.ops->rw.write.write_uint32)(&abuff, (gssa.valid.mask | gssa.valid.flags));
	    write_attributes_ctx(i, &abuff, &r, stat, &gssa.valid);

	    sftp_r.call.open.size=(unsigned int) abuff.pos;
	    sftp_r.call.open.buff=(unsigned char *) abuff.buffer;

	}

	result=send_sftp_open_ctx(i, &sftp_r);

    } else if (strcmp(what, "opendir")==0) {

	sftp_r.call.opendir.path=(unsigned char *) buffer;
	sftp_r.call.opendir.len=(unsigned int) result;

	result=send_sftp_opendir_ctx(i, &sftp_r);

    } else {

	result=-1;

    }

    if (result>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {

		(* cb_success)(ctx, reply, ptr);
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

    if (sftp_r.reply.error) errcode=(sftp_r.reply.error);

    out:
    (* cb_error)(ctx, errcode, ptr);

}

void _sftp_path_mkdir(struct service_context_s *ctx, struct fuse_path_s *fpath, struct system_stat_s *stat,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa;
    unsigned int attrsize=get_attr_buffer_size(i, &r, stat, &gssa) + 5; /* uid and gid by server ?*/
    char attrs[attrsize];
    struct attr_buffer_s abuff;
    unsigned int errcode=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);
    if (result==-1) goto out;

    /* compare the stat mask as asked by FUSE and the ones SFTP can set using this protocol version */
    if (gssa.stat_mask_result != gssa.stat_mask_asked)
	logoutput_warning("_fs_sftp_mkdir: not able to set every stat attr: asked %i to set %i", gssa.stat_mask_asked, gssa.stat_mask_result);

    set_attr_buffer_write(&abuff, attrs, attrsize);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid.mask);
    write_attributes_ctx(i, &abuff, &r, stat, &gssa.valid);

    init_sftp_request(&sftp_r, i);

    sftp_r.call.mkdir.path=(unsigned char *) buffer;
    sftp_r.call.mkdir.len=(unsigned int) result;
    sftp_r.call.mkdir.size=(unsigned int) abuff.pos;
    sftp_r.call.mkdir.buff=(unsigned char *) abuff.buffer;

    if (send_sftp_mkdir_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->code==0) {

		    (* cb_success)(ctx, reply, ptr);
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

    if (sftp_r.reply.error) errcode=(sftp_r.reply.error);
    out:
    (* cb_error)(ctx, errcode, ptr);

}

void _sftp_path_rm(struct service_context_s *ctx, struct fuse_path_s *fpath, const char *what,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);
    if (result==-1) goto out;

    init_sftp_request(&sftp_r, i);

    if (strcmp(what, "unlink")==0) {

	sftp_r.call.remove.path=(unsigned char *) buffer;
	sftp_r.call.remove.len=(unsigned int) result;

	result=send_sftp_remove_ctx(i, &sftp_r);

    } else if (strcmp(what, "rmdir")==0) {

	sftp_r.call.rmdir.path=(unsigned char *) buffer;
	sftp_r.call.rmdir.len=(unsigned int) result;

	result=send_sftp_rmdir_ctx(i, &sftp_r);

    } else {

	result=-1;

    }

    if (result>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->code==0) {

		    (* cb_success)(ctx, reply, ptr);
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

    if (sftp_r.reply.error) errcode=(sftp_r.reply.error);

    out:
    (* cb_error)(ctx, errcode, ptr);

}

void _sftp_path_setattr(struct service_context_s *ctx, struct fuse_path_s *fpath, struct system_stat_s *stat,
			void (* cb_success)(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr),
			void (* cb_error)(struct service_context_s *ctx, unsigned int errcode, void *ptr),
			unsigned char (* cb_interrupted)(void *ptr), void *ptr)
{
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int errcode=EIO;
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
    if (result==-1) goto out;

    /* compare the stat mask as asked by FUSE and the ones SFTP can set using this protocol version */

    if (gssa.stat_mask_result != gssa.stat_mask_asked)
	logoutput_warning("_sftp_path_setattr: not able to set every stat attr: asked %i to set %i", gssa.stat_mask_asked, gssa.stat_mask_result);

    set_attr_buffer_write(&abuff, attrs, attrsize);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid.mask);
    write_attributes_ctx(i, &abuff, &r, stat, &gssa.valid);

    init_sftp_request(&sftp_r, i);
    sftp_r.call.setstat.path=(unsigned char *) buffer;
    sftp_r.call.setstat.len=(unsigned int) result;
    sftp_r.call.setstat.size=(unsigned int) abuff.pos;
    sftp_r.call.setstat.buff=(unsigned char *) abuff.buffer;

    if (send_sftp_setstat_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout, cb_interrupted, ptr)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->code==0) {

		    (* cb_success)(ctx, reply, ptr);
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

    if (sftp_r.reply.error) errcode=(sftp_r.reply.error);

    out:
    (* cb_error)(ctx, errcode, ptr);

}
