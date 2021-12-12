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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "main.h"
#include "log.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"
#include "interface/sftp-extensions.h"
#include "datatypes/ssh-uint.h"

#define UINT32_T_MAX		0xFFFFFFFF

static struct statfs fallback_statfs;

static void _fs_sftp_statfs_unsupp(struct service_context_s *context, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct fuse_statfs_out statfs_out;

    memset(&statfs_out, 0, sizeof(struct fuse_statfs_out));

    statfs_out.st.blocks=fallback_statfs.f_blocks;
    statfs_out.st.bfree=fallback_statfs.f_bfree;
    statfs_out.st.bavail=fallback_statfs.f_bavail;
    statfs_out.st.bsize=fallback_statfs.f_bsize;

    statfs_out.st.frsize=fallback_statfs.f_bsize;

    statfs_out.st.files=(uint64_t) workspace->inodes.nrinodes;
    statfs_out.st.ffree=(uint64_t) (UINT32_T_MAX - statfs_out.st.files);

    statfs_out.st.namelen=255;
    statfs_out.st.padding=0;

    reply_VFS_data(f_request, (char *) &statfs_out, sizeof(struct fuse_statfs_out));

}

/* STATVFS */

void _fs_sftp_statfs(struct service_context_s *context, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];
    struct sftp_request_s sftp_r;

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    logoutput("_fs_sftp_statfs: path %.*s", pathinfo->len, pathinfo->path);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.statvfs.path=(unsigned char *)pathinfo->path;
    sftp_r.call.statvfs.len=pathinfo->len;

    if (send_sftp_statvfs_ctx(interface, &sftp_r, &error)>=0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
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

		statfs_out.st.files=(uint64_t) workspace->inodes.nrinodes;
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

		    _fs_sftp_statfs_unsupp(context, f_request, pathinfo);
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

void set_fallback_statfs_sftp(struct statfs *fallback)
{
    memcpy(&fallback_statfs, fallback, sizeof(struct statfs));
}

void _fs_sftp_statfs_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo)
{
    _fs_sftp_statfs_unsupp(context, f_request, pathinfo);
}
