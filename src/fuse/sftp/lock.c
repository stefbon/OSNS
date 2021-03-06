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
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"
#include "inode-stat.h"

static void _fs_sftp_flock_lock(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned char type)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    /* emulate file locks */

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.block.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.block.len=openfile->handle.name.len;
    sftp_r.call.block.offset=0;
    sftp_r.call.block.size=0;
    sftp_r.call.block.type=type;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	reply_VFS_error(f_request, EINTR);
	return;

    }

    if (send_sftp_block_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    openfile->flock=type; /* lock successfull */
		    reply_VFS_error(f_request, 0);
		    return;

		}

		logoutput("_fs_sftp_flock: status code %i", reply->response.status.code);
		error=reply->response.status.linux_error;

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

}

static void _fs_sftp_flock_unlock(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    /* emulate file locks */

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.unblock.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.unblock.len=openfile->handle.name.len;
    sftp_r.call.unblock.offset=0;
    sftp_r.call.unblock.size=0;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	reply_VFS_error(f_request, EINTR);
	return;

    }

    if (send_sftp_unblock_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    openfile->flock=0; /* lock removed */
		    reply_VFS_error(f_request, 0);
		    return;

		}

		logoutput("_fs_sftp_funlock: status code %i", reply->response.status.code);
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

void _fs_sftp_flock(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned char type)
{

    if (type & LOCK_UN) {

	_fs_sftp_flock_unlock(openfile, f_request);

    } else if (type & (LOCK_SH | LOCK_EX)) {

	_fs_sftp_flock_lock(openfile, f_request, type);

    } else {

	reply_VFS_error(f_request, EINVAL);

    }

}

void _fs_sftp_getlock(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct flock *flock)
{
    reply_VFS_error(f_request, ENOSYS);
}

void _fs_sftp_setlock(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct flock *flock)
{
    reply_VFS_error(f_request, ENOSYS);
}

void _fs_sftp_setlockw(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct flock *flock)
{
    reply_VFS_error(f_request, ENOSYS);
}

void _fs_sftp_flock_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, unsigned char type)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_getlock_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct flock *flock)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_setlock_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct flock *flock)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_setlockw_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct flock *flock)
{
    reply_VFS_error(f_request, ENOTCONN);
}
