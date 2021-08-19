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
#include "interface/sftp-extensions.h"
#include "interface/sftp-wait-response.h"
#include "inode-stat.h"

/* OPEN a file */

void _fs_sftp_open(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, unsigned int flags)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    logoutput("_fs_sftp_open");

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    init_sftp_request(&sftp_r, interface, f_request);
    sftp_r.call.open.path=(unsigned char *) pathinfo->path;
    sftp_r.call.open.len=pathinfo->len;
    sftp_r.call.open.posix_flags=flags;

    if (send_sftp_open_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {
		struct fuse_open_out open_out;
		struct entry_s *entry=openfile->inode->alias;

		/* handle name is defined in sftp_r.response.handle.name: take it "over" */

		openfile->handle.name.name=(char *) reply->response.handle.name;
		openfile->handle.name.len=reply->response.handle.len;
		reply->response.handle.name=NULL;
		reply->response.handle.len=0;

		open_out.fh=(uint64_t) openfile;

		if (entry->flags & _ENTRY_FLAG_REMOTECHANGED) {

		    /* VFS will free any cached data for this file */

		    open_out.open_flags=0;
		    entry->flags -= _ENTRY_FLAG_REMOTECHANGED;

		} else {

		    /* if there is a local cache it's uptodate */

		    open_out.open_flags=FOPEN_KEEP_CACHE;

		}

		open_out.padding=0;
		reply_VFS_data(f_request, (char *) &open_out, sizeof(open_out));
		unset_fuse_request_flags_cb(f_request);
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

void _fs_sftp_create(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, struct stat *st, unsigned int flags)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct sftp_attr_s attr;
    struct rw_attr_result_s r;
    unsigned int size=get_attr_buffer_size(interface, st, FATTR_MODE | FATTR_SIZE | FATTR_UID | FATTR_GID, &r, &attr, 0); /* uid and gid by server ?*/
    char buffer[size];
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    logoutput("_fs_sftp_create: path %s len %i", pathinfo->path, pathinfo->len);

    write_attributes_ctx(interface, buffer, size, &r, &attr);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.create.path=(unsigned char *) pathinfo->path;
    sftp_r.call.create.len=pathinfo->len;
    sftp_r.call.create.posix_flags=flags;
    sftp_r.call.create.size=size;
    sftp_r.call.create.buff=(unsigned char *)buffer;

    if (send_sftp_create_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {

		/* handle name is defined in sftp_r.response.handle.name: take it "over" */

		openfile->handle.name.name=(char *) reply->response.handle.name;
		openfile->handle.name.len=reply->response.handle.len;
		reply->response.handle.name=NULL;
		reply->response.handle.len=0;
		fill_inode_attr_sftp(interface, &openfile->inode->st, &attr);
		add_inode_context(context, openfile->inode);
		set_directory_dump(openfile->inode, get_dummy_directory());

		/* note: how the entry is created on the remote server does not have to be the same .... */

		_fs_common_cached_create(context, f_request, openfile);
		unset_fuse_request_flags_cb(f_request);
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

/* READ a file */

void _fs_sftp_read(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, size_t size, off_t off, unsigned int flags, uint64_t lock_owner)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.read.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.read.len=openfile->handle.name.len;
    sftp_r.call.read.offset=(uint64_t) off;
    sftp_r.call.read.size=(uint64_t) size;

    /* ignore flags and lockowner */

    if (send_sftp_read_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

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
    sftp_r.call.write.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.write.len=openfile->handle.name.len;
    sftp_r.call.write.offset=(uint64_t) off;
    sftp_r.call.write.size=(uint64_t) size;
    sftp_r.call.write.data=(char *)buff;

    if (send_sftp_write_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

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

void _fs_sftp_fsync(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned char datasync)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);
    sftp_r.call.fsync.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.fsync.len=openfile->handle.name.len;

    /* TODO: add f_request */

    if (send_sftp_fsync_ctx(interface, &sftp_r, &error)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		/* send ok reply to VFS no matter what the sftp server reports */

		reply_VFS_error(f_request, 0);

		if (reply->response.status.linux_error==EOPNOTSUPP) {

		    logoutput_warning("_fs_sftp_fsync: server response fsync not supported");
		    set_sftp_fsync_unsupp_ctx(interface);

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

    /* no support for flush */
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

    sftp_r.call.close.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.close.len=openfile->handle.name.len;

    /*
	TODO:
	- handle flush?
	- unlock when lock set (flock)
    */

    if (send_sftp_close_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {
		struct entry_s *entry=openfile->inode->alias;

		/* send ok reply to VFS no matter what the sftp server reports */

		reply_VFS_error(f_request, 0);

		free(openfile->handle.name.name);
		openfile->handle.name.name=NULL;
		openfile->handle.name.len=0;

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

    free(openfile->handle.name.name);
    openfile->handle.name.name=NULL;
    openfile->handle.name.len=0;
    unset_fuse_request_flags_cb(f_request);

}

void _fs_sftp_open_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, unsigned int flags)
{
    reply_VFS_error(f_request, ENOTCONN);
}
void _fs_sftp_create_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, struct stat *st, unsigned int flags)
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
