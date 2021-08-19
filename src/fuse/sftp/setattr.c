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

#include "log.h"
#include "main.h"
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

static void set_local_attributes(struct inode_s *inode, struct sftp_attr_s *attr)
{

    if (attr->asked & SFTP_ATTR_SIZE) inode->st.st_size=attr->size;
    if (attr->asked & SFTP_ATTR_USER) inode->st.st_uid=attr->user.uid;
    if (attr->asked & SFTP_ATTR_GROUP) inode->st.st_gid=attr->group.gid;
    if (attr->asked & SFTP_ATTR_PERMISSIONS) inode->st.st_mode=(attr->permissions & attr->type);

    if (attr->asked & SFTP_ATTR_ATIME) {

	inode->st.st_atim.tv_sec=attr->atime;
	inode->st.st_atim.tv_nsec=attr->atime_n;

    }

    if (attr->asked & SFTP_ATTR_MTIME) {

	inode->st.st_mtim.tv_sec=attr->mtime;
	inode->st.st_mtim.tv_nsec=attr->mtime_n;

    }

    if (attr->asked & SFTP_ATTR_CTIME) {

	inode->st.st_ctim.tv_sec=attr->ctime;
	inode->st.st_ctim.tv_nsec=attr->ctime_n;

    }

}

/* SETATTR */

void _fs_sftp_setattr(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo, struct stat *st, unsigned int set)
{
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct sftp_attr_s attr;
    struct rw_attr_result_s r;
    unsigned int size=get_attr_buffer_size(interface, st, set, &r, &attr, 0);
    char buffer[size];
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);
    write_attributes_ctx(interface, buffer, size, &r, &attr);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.setstat.path=(unsigned char *)pathinfo->path;
    sftp_r.call.setstat.len=pathinfo->len;
    sftp_r.call.setstat.size=size;
    sftp_r.call.setstat.buff=(unsigned char *)buffer;

    if (send_sftp_setstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		logoutput("_fs_sftp_setattr: reply %i", reply->response.status.code);

		if (reply->response.status.code==0) {

		    /* TODO: do a getattr to the server to check which attributes are set
			now is assumed that this status code == 0 means that everythis is set as asked */

		    set_local_attributes(inode, &attr);
		    _fs_common_getattr(get_root_context(context), f_request, inode);
		    unset_fuse_request_flags_cb(f_request);
		    return;

		} else {

		    error=reply->response.status.linux_error;

		}

	    } else {

		error=EPROTO;

	    }

	}

    } else {

	error=sftp_r.reply.error;

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/* FSETATTR */

void _fs_sftp_fsetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct stat *st, unsigned int set)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct sftp_attr_s attr;
    struct rw_attr_result_s r;
    unsigned int size=get_attr_buffer_size(interface, st, set, &r, &attr, 0);
    char buffer[size];

    write_attributes_ctx(interface, buffer, size, &r, &attr);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.fsetstat.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.fsetstat.len=openfile->handle.name.len;
    sftp_r.call.fsetstat.size=size;
    sftp_r.call.fsetstat.buff=(unsigned char *)buffer;

    /* send fsetstat cause a handle is available */

    if (send_sftp_fsetstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    /* TODO: do a getattr to the server to check which attributes are set */

		    set_local_attributes(openfile->inode, &attr);
		    _fs_common_getattr(get_root_context(context), f_request, openfile->inode);
		    unset_fuse_request_flags_cb(f_request);
		    return;

		}

		error=reply->response.status.linux_error;
		logoutput("_fs_sftp_fsetattr: reply %i", reply->response.status.code);

	    } else {

		error=EPROTO;

	    }

	}

    } else {

	error=sftp_r.reply.error;

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

void _fs_sftp_setattr_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo, struct stat *st, unsigned int set)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_fsetattr_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct stat *st, unsigned int set)
{
    reply_VFS_error(f_request, ENOTCONN);
}

