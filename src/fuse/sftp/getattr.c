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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "sftp/attr-utils.h"

#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

#include "inode-stat.h"

static void handle_sftp_attr_reply(struct service_context_s *context, struct fuse_request_s *f_request, struct sftp_reply_s *reply, struct inode_s *inode)
{
    struct context_interface_s *interface=&context->interface;
    struct attr_buffer_s abuff;

    /* read the atributes received from server */

    set_attr_buffer_read_attr_response(&abuff, &reply->response.attr);
    read_sftp_attributes_ctx(interface, &abuff, &inode->stat);

    /* reply to VFS */

    _fs_common_getattr(get_root_context(context), f_request, inode);

    /* adjust inodes stat synchronize time */

    get_current_time_system_time(&inode->stime);

    /* free */

    free(reply->response.attr.buff);
    unset_fuse_request_flags_cb(f_request);

}

/* GETATTR */

void _fs_sftp_getattr(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo)
{
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    logoutput("_fs_sftp_getattr: %li %s", get_ino_system_stat(&inode->stat), pathinfo->path);

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.id=0;
    sftp_r.call.lstat.path=(unsigned char *) pathinfo->path;
    sftp_r.call.lstat.len=pathinfo->len;

    /* send lstat cause not interested in target when dealing with symlink */

    if (send_sftp_lstat_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);
	error=0;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {

		handle_sftp_attr_reply(context, f_request, reply, inode);
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
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.id=0;
    sftp_r.call.fstat.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.fstat.len=openfile->handle.name.len;

    /* send fstat cause a handle is available */

    if (send_sftp_fstat_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);
	error=0;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {

		handle_sftp_attr_reply(context, f_request, reply, openfile->inode);
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

void _fs_sftp_getattr_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo)
{
    _fs_common_getattr(get_root_context(context), f_request, inode);
}

void _fs_sftp_fgetattr_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    _fs_common_getattr(get_root_context(context), f_request, openfile->inode);
}

