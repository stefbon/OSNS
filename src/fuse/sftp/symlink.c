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

#include "main.h"
#include "log.h"
#include "misc.h"
#include "options.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"
#include "datatypes/ssh-uint.h"

extern struct fs_options_s fs_options;

unsigned int check_target_symlink_sftp_client(struct context_interface_s *interface, struct fs_location_path_s *syml, struct fs_location_path_s *sub)
{
    unsigned int result=EIO; /* default */

    if ((syml->flags & FS_LOCATION_PATH_FLAG_RELATIVE)==0) {

	/* absolute path (on server) */

	if (syml->back>0) {

	    result=EACCES;

	} else if (issubdirectory_prefix_sftp_client(interface, &syml, sub)) {

	    result=0;

	} else {

	    /* target does not start with the prefix
		note again that this check does not belong here but on the server */

	    result=EXDEV;

	}

    } else {

	/* relative 
	    assume symlink has been checked on double dots which go one level higher */

	if (syml->back>0) {

	    /* no matter what going one (or more) level higher is not allowed ...
		also when the result is still a subdirectory of the shared directory like:

		take the following example:
		prefix: /home/guest with inside a directory doc
		and the relative target is ../guest/doc will result in the directory /home/guest/doc, which is ok
		but here is chosen that no going higher at the prefix is allowed in --> any <-- case
		*/

	    result=EXDEV;

	} else {

	    if (sub) {

		sub->ptr=syml->ptr;
		sub->len=syml->len;

	    }

	    result=0;

	}

    }

    return result;

}



/* READLINK */

void _fs_sftp_readlink(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo)
{
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    char origpath[pathinfo->len + 1];
    unsigned int origpath_len=pathinfo->len;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char pathinfobuffer[pathlen];

    if (fs_options.sftp.flags & _OPTIONS_SFTP_FLAG_SYMLINKS_DISABLE) {

	reply_VFS_error(f_request, ENOENT);
	return;

    }

    logoutput("_fs_sftp_readlink");

    strcpy(origpath, pathinfo->path);
    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, pathinfobuffer, pathinfo);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.readlink.path=(unsigned char *) pathinfo->path;
    sftp_r.call.readlink.len=pathinfo->len;

    if ((inode->flags & INODE_FLAG_REMOTECHANGED)==0) {

	/* try cached target symlink if there is any */

	struct fuse_symlink_s *syml=get_inode_fuse_cache_symlink(inode);

	if (syml) {

	    reply_VFS_data(f_request, syml->path.ptr, syml->path.len);
	    unset_fuse_request_flags_cb(f_request);
	    return;

	}

    }

    if (send_sftp_readlink_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    unset_fuse_request_flags_cb(f_request);

	    if (reply->type==SSH_FXP_NAME) {
		struct name_response_s *names=&reply->response.names;
		struct ssh_string_s tmp=SSH_STRING_INIT;
		int result=0;

		if (read_ssh_string(names->buff, names->size, &tmp)>0) {
		    struct fs_location_path_s target=FS_LOCATION_PATH_INIT;

		    set_location_path(&target, 's', &tmp);

		    if (test_location_path_absolute(&target)==0) target.flags |= FS_LOCATION_PATH_FLAG_RELATIVE;
		    result=remove_unneeded_path_elements(&target);

		    if (result>0) {

			logoutput("_fs_sftp_readlink: %s target %.*s", pathinfo->path, target.len, target.ptr);
			error=check_target_symlink_sftp_client(interface, &target, NULL);

			if (error==0) {
			    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

			    reply_VFS_data(f_request, target.ptr, target.len);
			    set_inode_fuse_cache_symlink(workspace, inode, &target, FUSE_SYMLINK_FLAG_PATH_ALLOC);

			    free(names->buff);
			    names->buff=NULL;
			    names->size=0;
			    return;

			}

		    }

		} else {

		    error=EPROTO;

		}

		free(names->buff);
		names->buff=NULL;
		names->size=0;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;

	    } else {

		error=EPROTO;

	    }

	}

    }

    out:
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/* SYMLINK */

void _fs_sftp_symlink(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, struct fs_location_path_s *target)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct service_context_s *rootcontext=get_root_context(context);
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.symlink.path=(unsigned char *) pathinfo->path;
    sftp_r.call.symlink.len=pathinfo->len;
    sftp_r.call.symlink.target_path=(unsigned char *) target->ptr;
    sftp_r.call.symlink.target_len=target->len;

    if (send_sftp_symlink_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(&context->interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    reply_VFS_error(f_request, 0);
		    unset_fuse_request_flags_cb(f_request);
		    return;

		}

		error=reply->response.status.linux_error;

	    } else {

		error=EIO;

	    }

	}

    }

    out:

    queue_inode_2forget(workspace, get_ino_system_stat(&entry->inode->stat), 0, 0);
    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}

/*
    test the symlink pointing to target is valid
    - a symlink is valid when it stays inside the "root" directory of the shared map: target is a subdirectory of the root
*/

int _fs_sftp_symlink_validate(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, struct fs_location_path_s *sub)
{
    struct fs_location_path_s tmp=FS_LOCATION_PATH_INIT;
    int result=0;
    unsigned int error=EIO;

    set_location_path(&tmp, 'c', target);
    if (test_location_path_absolute(&tmp)==0) tmp.flags |= FS_LOCATION_PATH_FLAG_RELATIVE;

    result=remove_unneeded_path_elements(&tmp);
    if (result==0 || tmp.back>0) return -1;

    error=check_target_symlink_sftp_client(&context->interface, &tmp, sub);

    if (error) {

	logoutput_debug("_fs_sftp_symlink_validate: error %i comparing %s with sftp prefix (%s)", error, target, strerror(error));
	return -1;

    }

    return 0;

}

void _fs_sftp_readlink_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_symlink_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, const char *target)
{
    reply_VFS_error(f_request, ENOTCONN);
}

int _fs_sftp_symlink_validate_disconnected(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, struct fs_location_path_s *sub)
{
    return -1;
}
