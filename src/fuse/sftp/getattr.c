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
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"
#include "inode-stat.h"

static const char *rootpath="/.";

/* GETATTR */

void _fs_sftp_getattr(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo)
{
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    logoutput("_fs_sftp_getattr: %li %s", inode->st.st_ino, pathinfo->path);

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.lstat.path=(unsigned char *) pathinfo->path;
    sftp_r.call.lstat.len=pathinfo->len;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;
    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	reply_VFS_error(f_request, EINTR);
	return;

    }

    /* send lstat cause not interested in target when dealing with symlink */

    if (send_sftp_lstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);
	error=0;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {
		struct sftp_attr_s attr;

		memset(&attr, 0, sizeof(struct sftp_attr_s));
		read_sftp_attributes_ctx(interface, &reply->response.attr, &attr);
		fill_inode_attr_sftp(interface, &inode->st, &attr);
		_fs_common_getattr(get_root_context(context), f_request, inode);
		get_current_time(&inode->stim);
		free(reply->response.attr.buff);
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

}

/* FGETATTR */

void _fs_sftp_fgetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.fstat.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.fstat.len=openfile->handle.name.len;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;
    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	reply_VFS_error(f_request, EINTR);
	return;

    }

    /* send fstat cause a handle is available */

    if (send_sftp_fstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);
	error=0;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {
		struct sftp_attr_s attr;
		struct inode_s *inode=openfile->inode;

		memset(&attr, 0, sizeof(struct sftp_attr_s));
		read_sftp_attributes_ctx(interface, &reply->response.attr, &attr);
		fill_inode_attr_sftp(interface, &inode->st, &attr);
		_fs_common_getattr(get_root_context(context), f_request, inode);
		get_current_time(&inode->stim);
		free(reply->response.attr.buff);
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

}

int _fs_sftp_getattr_root(struct context_interface_s *interface, void *ptr)
{
    struct sftp_attr_s *attr=(struct sftp_attr_s *) ptr;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct pathinfo_s pathinfo={rootpath, strlen(rootpath), 0, 0};
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo.len);
    char path[pathlen];
    int cache_size=0;

    logoutput("_fs_sftp_getattr_root");

    pathinfo.len += (* interface->backend.sftp.complete_path)(interface, path, &pathinfo);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.lstat.path=(unsigned char *) pathinfo.path;
    sftp_r.call.lstat.len=pathinfo.len;
    sftp_r.ptr=NULL;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    /* send lstat cause not interested in target when dealing with symlink */

    if (send_sftp_lstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);
	error=0;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {

		if (attr) read_sftp_attributes_ctx(interface, &reply->response.attr, attr);
		cache_size=reply->response.attr.size;
		free(reply->response.attr.buff);
		return cache_size;

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
    logoutput("_fs_sftp_getattr_root: error %i (%s)", error, strerror(error));
    return cache_size;

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

