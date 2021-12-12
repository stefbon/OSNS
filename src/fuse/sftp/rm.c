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

static void _remove_entry_common(struct entry_s **pentry)
{
    struct entry_s *entry=*pentry;
    struct directory_s *directory=get_upper_directory_entry(entry);

    if (directory) {
	unsigned int error=0;
        struct simple_lock_s wlock;

        /* remove entry from directory */

        if (wlock_directory(directory, &wlock)==0) {

    	    remove_entry_batch(directory, entry, &error);
    	    unlock_directory(directory, &wlock);

	}

    }

    entry->inode->alias=NULL;
    entry->inode=NULL;
    destroy_entry(entry);
    *pentry=NULL;
}

/* REMOVE a file and a directory */

void _fs_sftp_unlink(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s **pentry, struct pathinfo_s *pathinfo)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    logoutput("_fs_sftp_unlink: remove %.*s", pathinfo->len, pathinfo->path);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.remove.path=(unsigned char *) pathinfo->path;
    sftp_r.call.remove.len=pathinfo->len;

    if (send_sftp_remove_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		logoutput("_fs_sftp_remove: status code %i", reply->response.status.code);

		if (reply->response.status.code==0) {

		    _remove_entry_common(pentry);

		    reply_VFS_error(f_request, 0);
		    unset_fuse_request_flags_cb(f_request);
		    return;

		} else {

		    error=reply->response.status.linux_error;

		}

	    } else {

		error=EPROTO;

	    }

	}

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

void _fs_sftp_rmdir(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s **pentry, struct pathinfo_s *pathinfo)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.rmdir.path=(unsigned char *) pathinfo->path;
    sftp_r.call.rmdir.len=pathinfo->len;

    if (send_sftp_rmdir_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    _remove_entry_common(pentry);

		    reply_VFS_error(f_request, 0);
		    unset_fuse_request_flags_cb(f_request);
		    return;

		} else {

		    error=reply->response.status.linux_error;
		    logoutput("_fs_sftp_rmdir: status code %i", reply->response.status.code);

		}

	    } else {

		error=EPROTO;

	    }

	}

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

void _fs_sftp_unlink_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s **pentry, struct pathinfo_s *pathinfo)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_rmdir_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s **pentry, struct pathinfo_s *pathinfo)
{
    reply_VFS_error(f_request, ENOTCONN);
}
