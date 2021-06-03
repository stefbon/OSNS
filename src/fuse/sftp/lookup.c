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
    struct sftp_attr_s attr;
    struct fuse_request_s *r=(struct fuse_request_s *) ce->ptr;
    struct inode_s *inode=entry->inode;
    struct entry_s *parent=get_parent_entry(entry);

    memset(&attr, 0, sizeof(struct sftp_attr_s));
    read_sftp_attributes_ctx(interface, response, &attr);
    fill_inode_attr_sftp(interface, &inode->st, &attr);
    inode->nlookup=1;
    inode->st.st_nlink=1;

    add_inode_context(context, inode);
    get_current_time(&inode->stim);

    if (S_ISDIR(inode->st.st_mode)) {

	inode->st.st_nlink++;
	parent->inode->st.st_nlink++;
	logoutput("_sftp_lookup_cb_created: dir name %s ino %li", entry->name.name, inode->st.st_ino);
	set_directory_dump(inode, get_dummy_directory());

    } else {

	logoutput("_sftp_lookup_cb_created: nondir name %s ino %li", entry->name.name, inode->st.st_ino);

    }

    memcpy(&parent->inode->st.st_ctim, &inode->stim, sizeof(struct timespec));
    memcpy(&parent->inode->st.st_mtim, &inode->stim, sizeof(struct timespec));
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

    // logoutput("_sftp_lookup_cb_found: name %s ino %li", entry->name.name, inode->st.st_ino);

    if (check_create_inodecache(inode, response->size, (char *)response->buff, INODECACHE_FLAG_STAT)==1) {
	struct sftp_attr_s attr;
	struct timespec mtim;

	/* do this only when there is a difference, it's quite an intensive task */

	memset(&attr, 0, sizeof(struct sftp_attr_s));
	read_sftp_attributes_ctx(&context->interface, response, &attr);
	memcpy(&mtim, &inode->st.st_mtim, sizeof(struct timespec));
	fill_inode_attr_sftp(&context->interface, &inode->st, &attr);

	/*
	    keep track the remote entry has a newer mtim
	    - file: the remote file is changed
	    - directory: an entry is added and/or removed
	*/

	if (inode->st.st_mtim.tv_sec>mtim.tv_sec || (inode->st.st_mtim.tv_sec==mtim.tv_sec && inode->st.st_mtim.tv_nsec>mtim.tv_nsec)) {

	    inode->alias->flags |= _ENTRY_FLAG_REMOTECHANGED;

	}

	inode->flags|=INODE_FLAG_CACHED;

    }

    inode->nlookup++;
    get_current_time(&inode->stim);
    _fs_common_cached_lookup(context, r, inode); /* reply FUSE/VFS*/
    if (inode->nlookup==1) adjust_pathmax(workspace, ce->pathlen);

    if (S_ISDIR(inode->st.st_mode)) {
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

    // logoutput("_fs_sftp_lookup_new");

    // if (get_sftp_version_ctx(context->interface.ptr)<=3) {

	/* versions up to 3 do not support full lookup */

	// reply_VFS_error(f_request, ENOENT);
	// return;

    //}


    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    logoutput("_fs_sftp_lookup_new: (%li) %i %s", inode->st.st_ino, pathinfo->len, pathinfo->path);

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

    logoutput("_fs_sftp_lookup_existing: (ino=%li) %i %s", entry->inode->st.st_ino, pathinfo->len, pathinfo->path);

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
		struct attr_response_s *response=&reply->response.attr;
		struct inode_s *inode=entry->inode;

		/* do this different: let this to the cb's */

		if (check_create_inodecache(inode, response->size, (char *)response->buff, INODECACHE_FLAG_STAT)==1) {
		    struct sftp_attr_s attr;

		    logoutput("_fs_sftp_lookup_existing: cache differs");

		    memset(&attr, 0, sizeof(struct sftp_attr_s));
		    read_sftp_attributes_ctx(interface, response, &attr);
		    fill_inode_attr_sftp(interface, &inode->st, &attr);
		    inode->flags|=INODE_FLAG_CACHED;

		} else {

		    logoutput("_fs_sftp_lookup_existing: cache the same");

		}

		get_current_time(&inode->stim);

		if (inode->nlookup==0) {

		    inode->nlookup=1;
		    adjust_pathmax(workspace, pathinfo->len);
		    add_inode_context(context, inode);

		} else {

		    inode->nlookup++;

		}

		_fs_common_cached_lookup(context, f_request, inode); /* reply FUSE/VFS*/
		free(reply->response.attr.buff);
		reply->response.attr.buff=NULL;
		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;
		if (error==ENOENT) {
		    struct inode_s *inode=entry->inode;

		    queue_inode_2forget(workspace, inode->st.st_ino, 0, 0);

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

}

void _fs_sftp_lookup_existing_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo)
{
    struct inode_s *inode=entry->inode;

    inode->nlookup++;
    get_current_time(&inode->stim);
    _fs_common_cached_lookup(context, f_request, inode);
}

void _fs_sftp_lookup_new_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo)
{
    reply_VFS_error(f_request, ENOENT);
}
