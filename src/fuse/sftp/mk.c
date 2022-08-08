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

/* CREATE a directory */

void _fs_sftp_mkdir(struct service_context_s *ctx, struct fuse_request_s *f_request, struct entry_s *entry, struct fuse_path_s *fpath, struct system_stat_s *stat)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct service_context_s *rootcontext=get_root_context(ctx);
    struct context_interface_s *i=&ctx->interface;
    struct inode_s *inode=entry->inode;
    struct sftp_request_s sftp_r;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa;
    unsigned int attrsize=get_attr_buffer_size(i, &r, stat, &gssa) + 5; /* uid and gid by server ?*/
    char attrs[attrsize];
    struct attr_buffer_s abuff;
    unsigned int error=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);

    if (result==-1) {

	logoutput_debug("_fs_sftp_mkdir: error converting local path");
	goto out;

    }

    /* compare the stat mask as asked by FUSE and the ones SFTP can set using this protocol version */

    if (gssa.stat_mask_result != gssa.stat_mask_asked)
	logoutput_warning("_fs_sftp_mkdir: not able to set every stat attr: asked %i to set %i", gssa.stat_mask_asked, gssa.stat_mask_result);

    /* enable writing of subseconds (only of course if one of the time attr is included)*/

    if (gssa.stat_mask_result & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME | SYSTEM_STAT_BTIME | SYSTEM_STAT_CTIME)) {

	if (enable_attributes_ctx(i, &gssa.valid, "subseconds")==1) {

	    logoutput_debug("_fs_sftp_mkdir: enabled setting subseconds");

	} else {

	    logoutput_debug("_fs_sftp_mkdir: subseconds not supported");

	}

    }

    set_attr_buffer_write(&abuff, attrs, attrsize);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid.mask);
    write_attributes_ctx(i, &abuff, &r, stat, &gssa.valid);

    init_sftp_request(&sftp_r, i, f_request);

    sftp_r.call.mkdir.path=(unsigned char *) buffer;
    sftp_r.call.mkdir.len=(unsigned int) result;
    sftp_r.call.mkdir.size=(unsigned int) abuff.pos;
    sftp_r.call.mkdir.buff=(unsigned char *) abuff.buffer;

    if (send_sftp_mkdir_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {
		    struct directory_s *d=get_upper_directory_entry(entry);

		    inode->nlookup++;
		    set_nlink_system_stat(&inode->stat, 2);
		    _fs_common_cached_lookup(ctx, f_request, inode);

		    add_inode_context(ctx, inode);
		    (* d->inode->fs->type.dir.use_fs)(ctx, inode);
		    adjust_pathmax(w, pathlen);
		    assign_directory_inode(w, inode);
		    unset_fuse_request_flags_cb(f_request);
		    get_current_time_system_time(&inode->stime);
		    return;

		}

		error=reply->response.status.linux_error;
		logoutput("_fs_sftp_create: status reply %i", error);

	    } else {

		error=EINVAL;

	    }

	}

    } else {

	error=(sftp_r.reply.error) ? sftp_r.reply.error : EIO;

    }

    queue_inode_2forget(w, get_ino_system_stat(&inode->stat), 0, 0);

    out:

    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/* mknod not supported in sftp; emulate with create? */

void _fs_sftp_mknod(struct service_context_s *ctx, struct fuse_request_s *f_request, struct entry_s *entry, struct fuse_path_s *fpath, struct system_stat_s *stat)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
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

	logoutput_debug("_fs_sftp_mknod: error converting local path");
	goto out;

    }

    /* compare the stat mask as asked by FUSE and the ones SFTP can set using this protocol version */

    if (gssa.stat_mask_result != gssa.stat_mask_asked)
	logoutput_warning("_fs_sftp_mknod: not able to set every stat attr: asked %i to set %i", gssa.stat_mask_asked, gssa.stat_mask_result);

    /* enable writing of subseconds (only of course if one of the time attr is included)*/

    if (gssa.stat_mask_result & (SYSTEM_STAT_ATIME | SYSTEM_STAT_MTIME | SYSTEM_STAT_BTIME | SYSTEM_STAT_CTIME)) {

	if (enable_attributes_ctx(i, &gssa.valid, "subseconds")==1) {

	    logoutput_info("_fs_sftp_mknod: enabled setting subseconds");

	} else {

	    logoutput_info("_fs_sftp_mknod: subseconds not supported");

	}

    }

    logoutput("_fs_sftp_mknod: path %s", fpath->pathstart);

    set_attr_buffer_write(&abuff, attrs, attrsize);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid.mask);
    write_attributes_ctx(i, &abuff, &r, stat, &gssa.valid);

    init_sftp_request(&sftp_r, i, f_request);

    sftp_r.call.create.path=(unsigned char *) buffer;
    sftp_r.call.create.len=(unsigned int) result;
    sftp_r.call.create.posix_flags=(O_CREAT | O_EXCL);
    sftp_r.call.create.size=(unsigned int) abuff.pos;
    sftp_r.call.create.buff=(unsigned char *) abuff.buffer;

    if (send_sftp_create_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    unset_fuse_request_flags_cb(f_request);

	    if (reply->type==SSH_FXP_HANDLE) {
		struct handle_response_s handle;
		struct inode_s *inode=entry->inode;
		struct directory_s *d=get_upper_directory_entry(entry);

		add_inode_context(ctx, inode);
		_fs_common_cached_lookup(ctx, f_request, inode);

		(* d->inode->fs->type.dir.use_fs)(ctx, inode);
		adjust_pathmax(w, pathlen);
		get_current_time_system_time(&inode->stime);

		/* send here a fgetstat to get the attributes as set on the server ?? */
		/* send a close ... reuse sftp request */

		handle.len=reply->response.handle.len;
		handle.name=reply->response.handle.name;
		init_sftp_request_minimal(&sftp_r, i);

		sftp_r.call.close.handle=(unsigned char *) handle.name;
		sftp_r.call.close.len=handle.len;
		get_sftp_request_timeout_ctx(i, &timeout);

		if (send_sftp_close_ctx(i, &sftp_r)>0) {

		    if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
			struct sftp_reply_s *reply=&sftp_r.reply;

			if (reply->type==SSH_FXP_STATUS) {

			    if (reply->response.status.code!=0) {

				error=reply->response.status.linux_error;

			    } else {

				logoutput_notice("_fs_sftp_mknod: filehandle closed");

			    }

			} else {

			    logoutput_warning("_fs_sftp_mknod: received reply type %i expecting %i", reply->type, SSH_FXP_STATUS);

			}

		    }

		}

		free(handle.name);
		handle.name=NULL;
		handle.len=0;
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
    unset_fuse_request_flags_cb(f_request);
}

void _fs_sftp_mkdir_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct fuse_path_s *fpath, struct system_stat_s *st)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_mknod_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct fuse_path_s *fpath, struct system_stat_s *st)
{
    reply_VFS_error(f_request, ENOTCONN);
}

