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

#include <linux/fuse.h>

#define UINT32_T_MAX		0xFFFFFFFF

/* STATVFS */

void _fs_sftp_statfs(struct service_context_s *ctx, struct fuse_request_s *f_request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct context_interface_s *i=&ctx->interface;
    unsigned int error=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    struct sftp_request_s sftp_r;
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);

    if (result==-1) {

	logoutput_debug("_fs_sftp_statfs: error converting local path");
	goto out;

    }

    logoutput("_fs_sftp_statfs: path %s", fpath->pathstart);

    init_sftp_request(&sftp_r, i, f_request);
    sftp_r.call.statvfs.path=(unsigned char *) buffer;
    sftp_r.call.statvfs.len=(unsigned int) result;

    if (send_sftp_statvfs_ctx(i, &sftp_r, &error)>=0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_EXTENDED_REPLY) {
		struct fuse_statfs_out statfs_out;
		char *pos = (char *) reply->response.extension.buff;
		uint64_t f_flag=0;

		logoutput("_fs_sftp_statfs: size response %i", reply->response.extension.size);

		/*
		reply looks like

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

		memset(&statfs_out, 0, sizeof(struct fuse_statfs_out));

		statfs_out.st.bsize=get_uint64(pos);
		pos+=8;

		statfs_out.st.frsize=get_uint64(pos);
		pos+=8;

		statfs_out.st.blocks=get_uint64(pos);
		pos+=8;

		statfs_out.st.bfree=get_uint64(pos);
		pos+=8;

		statfs_out.st.bavail=get_uint64(pos);
		pos+=8;

		statfs_out.st.files=(uint64_t) w->inodes.nrinodes;
		pos+=8;

		statfs_out.st.ffree=(uint64_t) (UINT32_T_MAX - statfs_out.st.files);
		pos+=8;

		/* ignore favail */
		pos+=8;

		/* ignore fsid */
		pos+=8;

		/* ignore flag */
		f_flag=get_uint64(pos);
	        pos+=8;

		/* namelen as uint64??? sftp can handle very very long filenames; uint16 would be enough */
		statfs_out.st.namelen=get_uint64(pos);
		pos+=8;

		statfs_out.st.padding=0;

		reply_VFS_data(f_request, (char *) &statfs_out, sizeof(struct fuse_statfs_out));
		free(reply->response.extension.buff);
		reply->response.extension.buff=NULL;
		unset_fuse_request_flags_cb(f_request);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.linux_error==EOPNOTSUPP) {

		    logoutput("_fs_sftp_statfs: statvfs is unsupported ... ");

		    _fs_common_statfs(ctx, f_request, inode);
		    unset_fuse_request_flags_cb(f_request);
		    return;

		}

		error=reply->response.status.linux_error;

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

void _fs_sftp_statfs_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    _fs_common_statfs(context, f_request, inode);
}
