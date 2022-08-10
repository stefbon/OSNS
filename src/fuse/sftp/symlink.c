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

static int check_target_symlink_sftp_client(struct workspace_mount_s *w, struct context_interface_s *i, struct fuse_request_s *request, struct inode_s *inode, char *link, unsigned int len)
{
    int result=-EIO; /* default */
    unsigned int size=sftp_get_required_buffer_size_p2l(i, len);
    char buffer[size+1];
    struct fs_location_path_s path=FS_LOCATION_PATH_INIT;
    int tmp=0;

    memset(buffer, 0, size+1);
    tmp=sftp_convert_path_p2l(i, buffer, size, link, len);

    if (tmp==-1) {

	logoutput_debug("check_target_symlink_sftp_client: error converting local path");
	return -EIO;

    }

    set_location_path(&path, 'c', (void *) buffer);

    if (remove_unneeded_path_elements(&path)>=0) {

	if (path.back>0) {

	    /* no matter what going one (or more) level higher is not allowed ...
		also when the result is still a subdirectory of the shared directory like:

		take the following example:
		prefix: /home/guest with inside a directory doc
		and the relative target is ../guest/doc will result in the directory /home/guest/doc, which is ok
		but here is chosen that going higher/up at the prefix is not allowed in --> any <-- case
		*/

	    result=-EXDEV;
	    logoutput_debug("check_target_symlink_sftp_client: path has too many backslashes");

	} else if (buffer[0]=="/") {

	    size=strlen(buffer);
	    tmp=sftp_compare_path(i, buffer, size, SFTP_COMPARE_PATH_PREFIX_SUBDIR);

	    if (tmp>=0) {

		/* target of link is a subdirectory of (remote) prefix */

		result=tmp;
		reply_VFS_data(request, &buffer[result], (unsigned int)(size - result));

		tmp=set_inode_fuse_cache_symlink(w, inode, &buffer[result]);
		if (tmp>0) logoutput_debug("check_target_symlink_sftp_client: errcode %i saving link", tmp);

	    } else {

		result=-EXDEV;

	    }

	} else {

	    result=0;
	    size=strlen(buffer);
	    reply_VFS_data(request, buffer, size);

	}

    }

    return result;

}

/* READLINK */

void _fs_sftp_readlink(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct fuse_path_s *fpath)
{
    struct context_interface_s *i=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    if ((inode->flags & INODE_FLAG_REMOTECHANGED)==0) {

	/* try cached target symlink if there is any */

	struct fuse_symlink_s *cs=get_inode_fuse_cache_symlink(inode);

	if (cs) {

	    reply_VFS_data(f_request, cs->path, cs->len);
	    unset_fuse_request_flags_cb(f_request);
	    return;

	}

    }

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);

    if (result==-1) {

	logoutput_debug("_fs_sftp_readlink: error converting local path");
	goto out;

    }

    logoutput("_fs_sftp_readlink: %s", fpath->pathstart);

    init_sftp_request(&sftp_r, i, f_request);
    sftp_r.call.readlink.path=(unsigned char *) buffer;
    sftp_r.call.readlink.len=(unsigned int) result;

    if (send_sftp_readlink_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    unset_fuse_request_flags_cb(f_request);

	    if (reply->type==SSH_FXP_NAME) {
		struct workspace_mount_s *w=get_workspace_mount_ctx(context);

		if (reply->response.names.size>4) {
		    unsigned int len=0;

		    len=get_uint32(reply->response.names.buff);

		    if (len + 4 < reply->response.names.size) {
			char *link=&reply->response.names.buff[4];

			logoutput_debug("_fs_sftp_readlink: received target %.*s", len, link);

			result=check_target_symlink_sftp_client(w, i, f_request, inode, link, len);
			free(reply->response.names.buff);
			reply->response.names.buff=NULL;
			reply->response.names.size=0;
			if (result>=0) return;
			error=abs(result);

		    }

		}

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

    reply_VFS_error(f_request, ENOSYS);
    unset_fuse_request_flags_cb(f_request);

}

/*
    test the symlink pointing to target is valid
    - a symlink is valid when it stays inside the "root" directory of the shared map: target is a subdirectory of the root
*/

int _fs_sftp_symlink_validate(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, struct fs_location_path_s *sub)
{

    return -1;

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
