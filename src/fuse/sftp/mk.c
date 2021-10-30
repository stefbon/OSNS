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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
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
#include "inode-stat.h"


/* CREATE a directory */

void _fs_sftp_mkdir(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct system_stat_s *stat)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct service_context_s *rootcontext=get_root_context(context);
    struct context_interface_s *interface=&context->interface;
    struct inode_s *inode=entry->inode;
    struct sftp_request_s sftp_r;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa=GSSA_INIT;
    unsigned int size=get_attr_buffer_size(interface, &r, stat, &gssa) + 5; /* uid and gid by server ?*/
    char buffer[size];
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];
    struct attr_buffer_s abuff;

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    set_attr_buffer_write(&abuff, buffer, size);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid);
    write_attributes_ctx(interface, &abuff, &r, stat, gssa.valid);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.mkdir.path=(unsigned char *) pathinfo->path;
    sftp_r.call.mkdir.len=pathinfo->len;
    sftp_r.call.mkdir.size=(unsigned int)(abuff.pos - abuff.buffer);
    sftp_r.call.mkdir.buff=(unsigned char *) abuff.buffer;

    if (send_sftp_mkdir_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    inode->nlookup++;
		    set_nlink_system_stat(&inode->stat, 2);

		    get_current_time_system_time(&inode->stime);
		    add_inode_context(context, inode);
		    _fs_common_cached_lookup(context, f_request, inode);
		    adjust_pathmax(workspace, pathinfo->len);
		    set_directory_dump(inode, get_dummy_directory());
		    unset_fuse_request_flags_cb(f_request);
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

    queue_inode_2forget(workspace, get_ino_system_stat(&inode->stat), 0, 0);

    out:

    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/* mknod not supported in sftp; emulate with create? */

void _fs_sftp_mknod(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct stat *st)
{
    reply_VFS_error(f_request, ENOSYS);
}

void _fs_sftp_mkdir_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct stat *st)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_mknod_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct stat *st)
{
    reply_VFS_error(f_request, ENOTCONN);
}

