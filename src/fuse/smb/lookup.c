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

#include "interface/smb-signal.h"
#include "interface/smb.h"
#include "interface/smb-wait-response.h"

#include "smb/send/stat.h"

/*
    common functions to do a
    LOOKUP
    of a name on sftp map
*/

static unsigned int _smb_lookup_cb_cache_size(struct create_entry_s *ce)
{
    struct context_interface_s *interface=&ce->context->interface;

    fill_inode_attr_smb(interface, &ce->cache.stat, ce->cache.link.link.ptr);
    return 0;
}

static void _smb_lookup_cb_created(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct fuse_request_s *r=(struct fuse_request_s *) ce->ptr;
    struct inode_s *inode=entry->inode;
    struct entry_s *parent=get_parent_entry(entry);

    fill_inode_stat(inode, &ce->cache.st); /* from ce cache */
    inode->st.st_mode=ce->cache.st.st_mode;
    inode->st.st_size=ce->cache.st.st_size;
    inode->nlookup=1;
    inode->st.st_nlink=1;

    add_inode_context(context, inode);
    get_current_time(&inode->stim);

    if (S_ISDIR(inode->st.st_mode)) {

	inode->st.st_nlink++;
	parent->inode->st.st_nlink++;
	logoutput_debug("_smb_lookup_cb_created: dir name %s ino %li", entry->name.name, inode->st.st_ino);
	set_directory_dump(inode, get_dummy_directory());

    } else {

	logoutput_debug("_smb_lookup_cb_created: nondir name %s ino %li", entry->name.name, inode->st.st_ino);

    }

    memcpy(&parent->inode->st.st_ctim, &inode->stim, sizeof(struct timespec));
    memcpy(&parent->inode->st.st_mtim, &inode->stim, sizeof(struct timespec));
    _fs_common_cached_lookup(context, r, inode); /* reply FUSE/VFS */
    adjust_pathmax(workspace, ce->pathlen);

}

static void _smb_lookup_cb_found(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct fuse_request_s *r=(struct fuse_request_s *) ce->ptr;
    struct inode_s *inode=entry->inode;
    struct timespec mtim;

    memcpy(&mtim, &inode->st.st_mtim, sizeof(struct timespec));

    fill_inode_stat(inode, &ce->cache.st); /* from ce cache */
    inode->st.st_mode=ce->cache.st.st_mode;
    inode->st.st_size=ce->cache.st.st_size;

    if (inode->st.st_mtim.tv_sec>mtim.tv_sec || (inode->st.st_mtim.tv_sec==mtim.tv_sec && inode->st.st_mtim.tv_nsec>mtim.tv_nsec)) {

	inode->alias->flags |= _ENTRY_FLAG_REMOTECHANGED;

    }

    inode->nlookup++;
    get_current_time(&inode->stim);
    _fs_common_cached_lookup(context, r, inode); /* reply FUSE/VFS*/

    if (inode->nlookup==1) {
	struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

	adjust_pathmax(workspace, ce->pathlen);

    }

}

static void _smb_lookup_cb_error(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int error)
{
    struct fuse_request_s *r=(struct fuse_request_s *) ce->ptr;
    reply_VFS_error(r, error); /* reply FUSE/VFS */
}

void _fs_smb_lookup_new(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo)
{
    struct context_interface_s *interface=&context->interface;
    struct smb_request_s smb_r;
    struct smb_data_s *data=NULL;
    unsigned int error=EIO;
    unsigned int size=get_size_buffer_smb_stat();

    data=malloc(sizeof(struct smb_data_s) + size);

    if (data==NULL) {

	error=ENOMEM;
	goto out;

    }

    logoutput("_fs_smb_lookup_new: (%li) %i %s", inode->st.st_ino, pathinfo->len, pathinfo->path);

    init_smb_request(&smb_r, interface, f_request);

    if (send_smb_stat_ctx(interface, &smb_r, pathinfo->path, data)==0) {
	struct timespec timeout;

	get_smb_request_timeout_ctx(interface, &timeout);
	data->id=get_smb_unique_id(interface);
	smb_r.id=data->id;
	error=0;

	if (wait_smb_response_ctx(interface, &smb_r, &timeout)==1) {
	    struct entry_s *entry=NULL;
	    struct create_entry_s ce;

	    init_create_entry(&ce, xname, inode->alias, NULL, NULL, context, NULL, (void *) f_request);

	    ce.cache.link.link.ptr=(void *) data->buffer;

	    ce.pathlen=pathinfo->len;
	    ce.cb_created=_smb_lookup_cb_created;
	    ce.cb_found=_smb_lookup_cb_found;
	    ce.cb_error=_smb_lookup_cb_error;
	    ce.cb_cache_size=_smb_lookup_cb_cache_size;

	    entry=create_entry_extended(&ce);
	    unset_fuse_request_flags_cb(f_request);
	    free(data);

	    return;

	}

	/* put smb data on special list */

	add_smb_list_pending_requests_ctx(interface, &data->list);
	error=(smb_r.error) ? smb_r.error : EPROTO;

    }

    out:

    unset_fuse_request_flags_cb(f_request);
    reply_VFS_error(f_request, error);
    if (data) free(data);

}

void _fs_smb_lookup_existing(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo)
{
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct service_context_s *rootcontext=get_root_context(context);
    struct smb_request_s smb_r;
    struct smb_data_s *data=NULL;
    unsigned int error=EIO;
    unsigned int size=get_size_buffer_smb_stat();

    data=malloc(sizeof(struct smb_data_s) + size);

    if (data==NULL) {

	error=ENOMEM;
	goto out;

    }


    logoutput("_fs_smb_lookup_existing: (ino=%li) %i %s", entry->inode->st.st_ino, pathinfo->len, pathinfo->path);

    init_smb_request(&smb_r, interface, f_request);

    if (send_smb_stat_ctx(interface, &smb_r, pathinfo->path, data)==0) {
	struct timespec timeout;

	get_smb_request_timeout_ctx(interface, &timeout);
	data->id=get_smb_unique_id(interface);
	smb_r.id=data->id;
	error=0;

	if (wait_smb_response_ctx(interface, &smb_r, &timeout)==1) {
	    struct timespec mtim;
	    struct inode_s *inode=entry->inode;

	    memcpy(&mtim, &inode->st.st_mtim, sizeof(struct timespec));

	    fill_inode_attr_smb(interface, &inode->st, data->buffer);

	    if (inode->st.st_mtim.tv_sec>mtim.tv_sec || (inode->st.st_mtim.tv_sec==mtim.tv_sec && inode->st.st_mtim.tv_nsec>mtim.tv_nsec)) {

		inode->alias->flags |= _ENTRY_FLAG_REMOTECHANGED;

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
	    free(data);
	    return;

	}

	/* put smb data on special list */

	add_smb_list_pending_requests_ctx(interface, &data->list);
	error=(smb_r.error) ? smb_r.error : EPROTO;

    }

    if (error==ENOENT) {
	struct inode_s *inode=entry->inode;

	queue_inode_2forget(workspace, inode->st.st_ino, 0, 0);

    }

    out:
    reply_VFS_error(f_request, error);

}

void _fs_smb_lookup_existing_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo)
{
    struct inode_s *inode=entry->inode;

    inode->nlookup++;
    get_current_time(&inode->stim);
    _fs_common_cached_lookup(context, f_request, inode);
}

void _fs_smb_lookup_new_disconnected(struct service_context_s *context, struct fuse_request_s *f_request, struct inode_s *inode, struct name_s *xname, struct pathinfo_s *pathinfo)
{
    reply_VFS_error(f_request, ENOENT);
}
