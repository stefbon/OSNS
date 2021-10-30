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

void filter_setting_attributes(struct inode_s *inode, struct system_stat_s *stat)
{

    if (stat->mask & SYSTEM_STAT_MODE) {

	if (get_mode_system_stat(&inode->stat) == get_mode_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_MODE;

    }

    if (stat->mask & SYSTEM_STAT_SIZE) {

	if (get_size_system_stat(&inode->stat) == get_size_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_SIZE;

    }

    if (stat->mask & SYSTEM_STAT_UID) {

	if (get_uid_system_stat(&inode->stat) == get_uid_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_UID;

    }

    if (stat->mask & SYSTEM_STAT_GID) {

	if (get_gid_system_stat(&inode->stat) == get_gid_system_stat(stat)) stat->mask &= ~SYSTEM_STAT_GID;

    }

    if (stat->mask & SYSTEM_STAT_ATIME) {

	if ((get_atime_sec_system_stat(&inode->stat) == get_atime_sec_system_stat(stat)) && (get_atime_nsec_system_stat(&inode->stat) == get_atime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_ATIME;

    }

    if (stat->mask & SYSTEM_STAT_MTIME) {

	if ((get_mtime_sec_system_stat(&inode->stat) == get_mtime_sec_system_stat(stat)) && (get_mtime_nsec_system_stat(&inode->stat) == get_mtime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_MTIME;

    }

    if (stat->mask & SYSTEM_STAT_CTIME) {

	if ((get_ctime_sec_system_stat(&inode->stat) == get_ctime_sec_system_stat(stat)) && (get_ctime_nsec_system_stat(&inode->stat) == get_ctime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_CTIME;

    }

    if (stat->mask & SYSTEM_STAT_BTIME) {

	if ((get_btime_sec_system_stat(&inode->stat) == get_btime_sec_system_stat(stat)) && (get_btime_nsec_system_stat(&inode->stat) == get_btime_nsec_system_stat(stat))) stat->mask &= ~SYSTEM_STAT_BTIME;

    }

}

/* SETATTR */

void _fs_sftp_setattr(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo, struct system_stat_s *stat)
{
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa=GSSA_INIT;
    unsigned int size=get_attr_buffer_size(interface, &r, stat, &gssa) + 5;
    char buffer[size];
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];
    struct attr_buffer_s abuff;

    /* test attributes really differ from the current */

    filter_setting_attributes(inode, stat);
    if (stat->mask==0) {

	error=0;
	goto out;

    }

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    set_attr_buffer_write(&abuff, buffer, size);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid);
    write_attributes_ctx(interface, &abuff, &r, stat, gssa.valid);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.setstat.path=(unsigned char *) pathinfo->path;
    sftp_r.call.setstat.len=pathinfo->len;
    sftp_r.call.setstat.size=(unsigned int)(abuff.pos - abuff.buffer);
    sftp_r.call.setstat.buff=(unsigned char *)abuff.buffer;

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

		    set_local_attributes(interface, inode, stat);
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

void _fs_sftp_fsetattr(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct system_stat_s *stat)
{
    struct service_context_s *context=(struct service_context_s *) openfile->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
    struct get_supported_sftp_attr_s gssa=GSSA_INIT;
    unsigned int size=get_attr_buffer_size(interface, &r, stat, &gssa) + 5;
    char buffer[size];
    struct attr_buffer_s abuff;

    /* test attributes really differ from the current */

    filter_setting_attributes(openfile->inode, stat);
    if (stat->mask==0) {

	error=0;
	goto out;

    }

    set_attr_buffer_write(&abuff, buffer, size);
    (* abuff.ops->rw.write.write_uint32)(&abuff, gssa.valid);
    write_attributes_ctx(interface, &abuff, &r, stat, gssa.valid);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.fsetstat.handle=(unsigned char *) openfile->handle.name.name;
    sftp_r.call.fsetstat.len=openfile->handle.name.len;
    sftp_r.call.fsetstat.size=(unsigned int)(abuff.pos - abuff.buffer);
    sftp_r.call.fsetstat.buff=(unsigned char *) abuff.buffer;

    /* send fsetstat cause a handle is available */

    if (send_sftp_fsetstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    /* TODO: do a getattr to the server to check which attributes are set */

		    set_local_attributes(interface, openfile->inode, stat);
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

void _fs_sftp_setattr_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct pathinfo_s *pathinfo, struct system_stat_s *stat)
{
    reply_VFS_error(f_request, ENOTCONN);
}

void _fs_sftp_fsetattr_disconnected(struct fuse_openfile_s *openfile, struct fuse_request_s *f_request, struct system_stat_s *stat)
{
    reply_VFS_error(f_request, ENOTCONN);
}
