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
#include "misc.h"
#include "main.h"

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

/*
    common functions to do a
    LOOKUP
    of a name on sftp map
*/

static unsigned int _sftp_cb_cache_size(struct create_entry_s *ce)
{
    struct attr_response_s *attr=(struct attr_response_s *) ce->cache.link.link.ptr;
    return attr->size;
}

static void _sftp_lookup_cb_created(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct attr_response_s *response=(struct attr_response_s *) ce->cache.link.link.ptr;
    struct fuse_request_s *r=(struct fuse_request_s *) ce->ptr;
    struct inode_s *inode=entry->inode;
    struct entry_s *parent=get_parent_entry(entry);
    struct attr_buffer_s abuff;

    set_sftp_inode_stat_defaults(interface, inode);

    /* read attributes */

    set_attr_buffer_read_attr_response(&abuff, response);
    read_sftp_attributes_ctx(interface, &abuff, &inode->stat);

    inode->nlookup=1;
    set_nlink_system_stat(&inode->stat, 1);
    add_inode_context(context, inode);
    get_current_time_system_time(&inode->stime);

    if (S_ISDIR(inode->stat.sst_mode)) {

	increase_nlink_system_stat(&inode->stat, 1);
	increase_nlink_system_stat(&parent->inode->stat, 1);
	set_ctime_system_stat(&parent->inode->stat, &inode->stime); /* attribute of parent changed */
	logoutput("_sftp_lookup_cb_created: dir name %s ino %li", entry->name.name, inode->stat.sst_ino);
	set_directory_dump(inode, get_dummy_directory());

    } else {

	logoutput("_sftp_lookup_cb_created: nondir name %s ino %li", entry->name.name, inode->stat.sst_ino);

    }

    set_mtime_system_stat(&parent->inode->stat, &inode->stime); /* entry added: parent directory is modified */
    _fs_common_cached_lookup(context, r, inode); /* reply FUSE/VFS */
    adjust_pathmax(workspace, ce->pathlen);

    check_create_inodecache(inode, response->size, (char *)response->buff, INODECACHE_FLAG_STAT);
    inode->flags|=INODE_FLAG_CACHED;
    set_entry_ops(entry);

}

static void _sftp_lookup_cb_found(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct attr_response_s *response=(struct attr_response_s *) ce->cache.link.link.ptr;
    struct fuse_request_s *r=(struct fuse_request_s *) ce->ptr;
    struct inode_s *inode=entry->inode;

    if (check_create_inodecache(inode, response->size, (char *)response->buff, INODECACHE_FLAG_STAT)==1) {
	struct system_timespec_s mtime;
	struct attr_buffer_s abuff;
	struct system_stat_s *stat=&inode->stat;

	get_mtime_system_stat(stat, &mtime);
	set_attr_buffer_read_attr_response(&abuff, (struct attr_response_s *) ce->cache.link.link.ptr);
	read_sftp_attributes_ctx(&context->interface, &abuff, stat);

	if (stat->sst_mtime.tv_sec > mtime.tv_sec ||
	    (stat->sst_mtime.tv_sec==mtime.tv_sec && stat->sst_mtime.tv_nsec>mtime.tv_nsec)) {

	    inode->flags |= INODE_FLAG_REMOTECHANGED;

	}

	inode->flags|=INODE_FLAG_CACHED;

    }

    inode->nlookup++;
    get_current_time_system_time(&inode->stime);
    _fs_common_cached_lookup(context, r, inode); /* reply FUSE/VFS*/

    if (inode->nlookup==1) adjust_pathmax(workspace, ce->pathlen); /* only adjust the maximal path length the first lookup */

    if (S_ISDIR(inode->stat.sst_mode)) {
	struct directory_s *d=(* ce->get_directory)(ce);
	struct getpath_s *getpath=d->getpath;

	if (getpath==NULL) set_directory_pathcache(context, d, NULL);

    }

}

static void _sftp_lookup_cb_error(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int error)
{
    struct fuse_request_s *r=(struct fuse_request_s *) ce->ptr;
    reply_VFS_error(r, error); /* reply FUSE/VFS */
}

void _fs_sftp_lookup_new(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo)
{
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    logoutput("_fs_sftp_lookup_new: (%li) %i %s", get_ino_system_stat(&inode->stat), pathinfo->len, pathinfo->path);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.lstat.path=(unsigned char *) pathinfo->path;
    sftp_r.call.lstat.len=pathinfo->len;

    /* send lstat cause not interested in target when dealing with symlink */

    if (send_sftp_lstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);
	error=0;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {
		struct entry_s *entry=NULL;
		struct create_entry_s ce;

		init_create_entry(&ce, xname, inode->alias, NULL, NULL, context, NULL, (void *) f_request);
		ce.cache.link.link.ptr=(void *) &reply->response.attr;
		ce.cache.link.type=DATA_LINK_TYPE_CACHE; /* not really required */
		ce.pathlen=pathinfo->len;
		ce.cb_created=_sftp_lookup_cb_created;
		ce.cb_found=_sftp_lookup_cb_found;
		ce.cb_error=_sftp_lookup_cb_error;
		ce.cb_cache_size=_sftp_cb_cache_size;
		entry=create_entry_extended(&ce);

		// logoutput("_fs_sftp_lookup_new: %i %s", pathinfo->len, pathinfo->path);

		free(reply->response.attr.buff);
		reply->response.attr.buff=NULL;
		unset_fuse_request_flags_cb(f_request);
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

void _fs_sftp_lookup_existing(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct service_context_s *rootcontext=get_root_context(context);
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    logoutput("_fs_sftp_lookup_existing: (ino=%li) %i %s", get_ino_system_stat(&entry->inode->stat), pathinfo->len, pathinfo->path);

    init_sftp_request(&sftp_r, interface, f_request);

    sftp_r.call.lstat.path=(unsigned char *) pathinfo->path;
    sftp_r.call.lstat.len=pathinfo->len;

    /* send lstat cause not interested in target when dealing with symlink */

    if (send_sftp_lstat_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);
	error=0;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_ATTRS) {
		struct attr_response_s *response=&reply->response.attr;
		struct inode_s *inode=entry->inode;

		if (check_create_inodecache(inode, response->size, (char *)response->buff, INODECACHE_FLAG_STAT)==1) {
		    struct attr_buffer_s abuff;
		    struct system_timespec_s mtime;
		    struct system_stat_s *stat=&inode->stat;

		    get_mtime_system_stat(stat, &mtime);

		    logoutput("_fs_sftp_lookup_existing: cache differs");

		    set_attr_buffer_read_attr_response(&abuff, response);
		    read_sftp_attributes_ctx(interface, &abuff, stat);

		    if (stat->sst_mtime.tv_sec > mtime.tv_sec ||
			(stat->sst_mtime.tv_sec==mtime.tv_sec && stat->sst_mtime.tv_nsec>mtime.tv_nsec)) {

			inode->flags |= INODE_FLAG_REMOTECHANGED;

		    }

		    inode->flags|=INODE_FLAG_CACHED;

		} else {

		    logoutput("_fs_sftp_lookup_existing: cache the same");

		}

		get_current_time_system_time(&inode->stime);
		inode->nlookup++;

		if (inode->nlookup==1) {

		    adjust_pathmax(workspace, pathinfo->len);
		    add_inode_context(context, inode);

		}

		_fs_common_cached_lookup(context, f_request, inode); /* reply FUSE/VFS*/
		free(reply->response.attr.buff);
		reply->response.attr.buff=NULL;
		unset_fuse_request_flags_cb(f_request);
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;
		if (error==ENOENT) {
		    struct inode_s *inode=entry->inode;

		    queue_inode_2forget(workspace, get_ino_system_stat(&inode->stat), 0, 0);

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
    unset_fuse_request_flags_cb(f_request);

}

void _fs_sftp_lookup_existing_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo)
{
    struct inode_s *inode=entry->inode;

    inode->nlookup++;
    get_current_time_system_time(&inode->stime);
    _fs_common_cached_lookup(context, f_request, inode);
}

void _fs_sftp_lookup_new_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo)
{
    reply_VFS_error(f_request, ENOENT);
}
