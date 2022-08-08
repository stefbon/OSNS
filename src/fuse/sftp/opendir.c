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

#include "inode-stat.h"
#include "symlink.h"

#include <linux/fuse.h>

extern const char *dotdotname;
extern const char *dotname;
extern const char *rootpath;

void _fs_sftp_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    return _fs_common_readdir(opendir, r, size, offset);
}

void _fs_sftp_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, unsigned char datasync)
{
    reply_VFS_error(r, 0);
}

void _fs_sftp_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *f_request)
{
    struct service_context_s *context=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct osns_lock_s wlock;
    struct directory_s *directory=NULL;

    logoutput("_fs_sftp_releasedir");

    set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_FINISH);

    init_sftp_request(&sftp_r, interface, f_request);
    sftp_r.call.close.handle=(unsigned char *) opendir->handle->name;
    sftp_r.call.close.len=opendir->handle->len;

    if (send_sftp_close_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		/* send ok reply to VFS no matter what the ssh server reports */

		error=0;
		if (reply->response.status.code>0) logoutput_notice("_fs_sftp_releasedir: got reply %i:%s when closing dir", reply->response.status.linux_error, strerror(reply->response.status.linux_error));

	    } else {

		error=EPROTO;

	    }

	}

    }

    out:

    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

    signal_lock(opendir->signal);

    if ((opendir->flags & FUSE_OPENDIR_FLAG_THREAD)==0) {

	/* free opendir handle */

	if (opendir->handle) {

	    release_fuse_handle(opendir->handle);
	    opendir->handle=NULL;

	}

	/* free cached data */

	if (opendir->data.abuff.buffer) {

	    free(opendir->data.abuff.buffer);
	    set_attr_buffer_read(&opendir->data.abuff, NULL, 0);

	}

    }

    signal_unlock(opendir->signal);

    if ((opendir->flags & FUSE_OPENDIR_FLAG_INCOMPLETE)==0) {

	opendir->inode->flags &= ~INODE_FLAG_REMOTECHANGED;

	/* remove local entries not found on server */
	_fs_common_remove_nonsynced_dentries(opendir);

    }

}

static void _cb_created(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct inode_s *inode=entry->inode;
    struct fuse_opendir_s *fo=ce->tree.opendir;
    struct directory_s *directory=(* ce->get_directory)(ce);
    struct attr_buffer_s *abuff=ce->cache.abuff;
    struct context_interface_s *interface=&context->interface;
    struct system_stat_s *stat=&inode->stat;

    set_sftp_inode_stat_defaults(interface, inode);

    read_sftp_attributes_ctx(interface, abuff, stat);
    set_nlink_system_stat(stat, 1);
    inode->nlookup=0;
    fo->count_created++; /* count the numbers added */

    copy_system_time(&inode->stime, &directory->synctime);
    add_inode_context(context, inode);
    (* directory->inode->fs->type.dir.use_fs)(context, inode);

    log_inode_information(inode, INODE_INFORMATION_OWNER | INODE_INFORMATION_SIZE | INODE_INFORMATION_MODE);

    if (system_stat_test_ISDIR(stat)) {
	struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);

	set_nlink_system_stat(stat, 2);
	increase_nlink_system_stat(&directory->inode->stat, 1);
	set_ctime_system_stat(&directory->inode->stat, &directory->synctime); /* set the change time of parent since attribute nlink changed */
	assign_directory_inode(workspace, inode);

    }

    /* no matter what type: the mtime of the parent changes */
    set_mtime_system_stat(&directory->inode->stat, &directory->synctime);

}

static void _cb_found(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct inode_s *inode=entry->inode;
    struct fuse_opendir_s *opendir=ce->tree.opendir;
    struct directory_s *directory=NULL;
    struct system_timespec_s mtime=SYSTEM_TIME_INIT;
    struct attr_buffer_s *abuff=ce->cache.abuff;
    struct context_interface_s *interface=&context->interface;
    struct system_stat_s *stat=&inode->stat;

    get_mtime_system_stat(stat, &mtime);
    read_sftp_attributes_ctx(interface, abuff, stat);

    /* keep track if the file has been changed on the remote side for caching purposes */

    if (test_remote_file_changed(stat, &mtime)==1) inode->flags |= INODE_FLAG_REMOTECHANGED;

    directory=(* ce->get_directory)(ce);
    opendir->count_found++;
    copy_system_time(&inode->stime, &directory->synctime);

}

static void _cb_error(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int error)
{
    logoutput_warning("_cb_error: error %i:%s creating %s", error, strerror(error), xname->name);
    ce->error=error;
}

static unsigned int take_sftp_readdir_names(struct sftp_reply_s *reply, struct attr_buffer_s *abuff)
{
    struct name_response_s *names=&reply->response.names;
    int count=names->count;

    set_attr_buffer_read(abuff, names->buff, names->size);
    set_attr_buffer_count(abuff, names->count);

    if (names->flags & SFTP_RESPONSE_FLAG_EOF_SUPPORTED) {
	unsigned int flags = ATTR_BUFFER_FLAG_EOF_SUPPORTED;

	if (names->flags & SFTP_RESPONSE_FLAG_EOF) flags |= ATTR_BUFFER_FLAG_EOF;
	set_attr_buffer_flags(abuff, flags);

    }

    names->buff = NULL;
    names->size = 0;
    return (unsigned int) count;

}

/* send readdir to server to get list of names
    return
    > 0: positive amount of entries
    = 0: no more entries
    < 0: error
    */

static int _sftp_get_readdir_names(struct fuse_opendir_s *opendir, unsigned int *error)
{
    struct service_context_s *context=opendir->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    int result=-1;

    if (opendir->data.abuff.buffer) {

	free(opendir->data.abuff.buffer);
	set_attr_buffer_read(&opendir->data.abuff, NULL, 0);

    }

    logoutput("_sftp_get_readdir_names");

    init_sftp_request_minimal(&sftp_r, interface);

    sftp_r.call.readdir.handle=(unsigned char *) opendir->handle->name;
    sftp_r.call.readdir.len=opendir->handle->len;

    if (send_sftp_readdir_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_NAME) {
		struct attr_buffer_s *abuff=&opendir->data.abuff;

		result=take_sftp_readdir_names(reply, abuff);
		logoutput("_sftp_get_readdir_names: names size: %i count %i", abuff->len, abuff->count);

	    } else if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		if (status->linux_error==ENODATA) {

		    *error=0;
		    result=0;

		} else {

		    *error=status->linux_error;
		    result=-1;

		}

		logoutput("_sftp_get_readdir_names: reply status: %i result %i", status->linux_error, result);

	    } else {

		*error=EPROTO;
		result=-1;

	    }

	}

    }

    out:
    return result;

}

static void get_sftp_readdir_thread(void *ptr)
{
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) ptr;
    struct attr_buffer_s *abuff=&opendir->data.abuff;
    struct service_context_s *context=opendir->context;
    struct name_s xname;
    struct entry_s *entry=NULL;
    struct inode_s *inode=NULL;
    struct create_entry_s ce;
    unsigned int error=0;
    unsigned char dofree=0;

    logoutput("get_sftp_readdir_thread");

    signal_lock(opendir->signal);

    if (opendir->flags & FUSE_OPENDIR_FLAG_THREAD) {

	signal_unlock(opendir->signal);
	return;

    }

    opendir->flags |= FUSE_OPENDIR_FLAG_THREAD;
    signal_unlock(opendir->signal);

    init_create_entry(&ce, &xname, NULL, NULL, opendir, context, NULL, NULL);

    ce.cb_created=_cb_created;
    ce.cb_found=_cb_found;
    ce.cb_error=_cb_error;
    ce.cache.abuff=abuff;
    // enable_ce_extended_adjust_pathmax(&ce); /* set pathlen from somewhere .... */

    init_name(&xname);

    while ((opendir->flags & FUSE_OPENDIR_FLAG_FINISH)==0) {
	unsigned int total=0;

	if (abuff->count<=0) {
	    int result=0;

	    /* if no dentries in buffer get from server */

	    result=_sftp_get_readdir_names(opendir, &error);

	    if (result==-1) {

		/* some error */

		if (error==EINTR || error==ETIMEDOUT) {

		    set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_INCOMPLETE);

		} else {

		    set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_ERROR);

		}

		set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_FINISH);
		break;

	    } else if (result==0) {

		/* no more names from server */

		finish_get_fuse_direntry(opendir);
		set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_FINISH);
		break;

	    }

	    total=(unsigned int) result;

	}

	while (abuff->count>0) {
	    struct ssh_string_s name=SSH_STRING_INIT;

	    /* extract name and attributes from names
		only get the name, do the attr later */

	    read_name_name_response_ctx(&context->interface, abuff, &name);
	    logoutput("get_sftp_readdir_thread: got name %.*s ctr %i", name.len, name.ptr, (total - abuff->count));

	    if ((name.len==1 && strncmp(name.ptr, ".", 1)==0) || (name.len==2 && strncmp(name.ptr, "..", 2)==0)) {
		struct system_stat_s stat;

		/* skip the . and .. entries */

		read_sftp_attributes_ctx(&context->interface, abuff, &stat);

	    } else {

		set_name_from(&xname, 's', (void *) &name);
		calculate_nameindex(&xname);

		/* create the entry (if not exist already) */

		entry=create_entry_extended(&ce);

		if (entry) {

		    if ((* opendir->hidefile)(opendir, entry)==0) {

			/* create a direntry queue for the (later) readdir to fill the buffer to send to the VFS */

			queue_fuse_direntry(opendir, entry);

		    }

		} else {

		    logoutput_warning("get_sftp_readdir_thread: entry %.*s not created", name.len, name.ptr);

		}

	    }

	    next:

	    abuff->count--;

	}

	if (abuff->flags & ATTR_BUFFER_FLAG_EOF_SUPPORTED) {

	    if ((abuff->count <= 0) && abuff->left>0) {
		unsigned char eof= *(abuff->buffer + abuff->pos);

		if (eof) {

		    abuff->flags |= ATTR_BUFFER_FLAG_EOF;
		    break;

		}

	    }

	}

    }

    finish_get_fuse_direntry(opendir);

}

void start_get_sftp_readdir_thread(struct fuse_opendir_s *opendir)
{
    work_workerthread(NULL, 0, get_sftp_readdir_thread, (void *) opendir, NULL);
}

/* OPEN a directory */

/*
    TODO:
    1.
    - 	when using the "normal" readdir (not readdirplus) it's possible to first send a getattr, and test there is a change in mtim
	if there is continue the normal way by sending an sftp open message
	if there isn't a change, just list the already cached entries ib this client
    2.
    -	use readdirplus
*/

void _fs_sftp_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *f_request, struct fuse_path_s *fpath, unsigned int flags)
{
    struct service_context_s *ctx=(struct service_context_s *) opendir->context;
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct context_interface_s *i=&ctx->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct directory_s *directory=get_directory(w, opendir->inode, 0);
    unsigned int pathlen=sftp_get_complete_pathlen(i, fpath);
    unsigned int size=sftp_get_required_buffer_size_l2p(i, pathlen);
    char buffer[size];
    int result=0;

    memset(buffer, 0, size);
    result=sftp_convert_path_l2p(i, buffer, size, fpath->pathstart, pathlen);

    if (result==-1) {

	logoutput_debug("_fs_sftp_opendir: error converting local path");
	goto out;

    }

    opendir->flags |= (FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS | FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS | FUSE_OPENDIR_FLAG_HIDE_DOTFILES) ; /* sane flag */

    /* test a full opendir/readdir is required: test entries are deleted and/or created */

    if (directory && get_system_time_sec(&directory->synctime)>0) {;

	if ((opendir->inode->flags & INODE_FLAG_REMOTECHANGED)==0) {

	    /* no entries added and deleted: no need to read all entries again: use cache */

	    logoutput("_fs_sftp_opendir_common: remote directory has not changed since last visit: no full opendir is required");

	    opendir->readdir=_fs_common_virtual_readdir;
	    opendir->fsyncdir=_fs_common_virtual_fsyncdir;
	    opendir->releasedir=_fs_common_virtual_releasedir;

	    _fs_common_virtual_opendir(opendir, f_request, flags);
	    return;

	}

    }

    logoutput("_fs_sftp_opendir_common: send opendir %s", fpath->pathstart);

    init_sftp_request(&sftp_r, i, f_request);
    sftp_r.call.opendir.path=(unsigned char *) buffer;
    sftp_r.call.opendir.len=(unsigned int) size;

    if (send_sftp_opendir_ctx(i, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	/* it's possible to let this threat do things here since
	    it goes into a wait state to receive the response from the server
	    and this goes over the network so that gives a little time (interesting: how much ?)*/

	get_sftp_request_timeout_ctx(i, &timeout);

	if (wait_sftp_response_ctx(i, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {
		struct fuse_open_out open_out;
		struct inode_s *inode=opendir->inode;
		struct fuse_handle_s *handle=create_fuse_handle(ctx, get_ino_system_stat(&inode->stat), FUSE_HANDLE_FLAG_OPENDIR, (char *) reply->response.handle.name, reply->response.handle.len, 0);

		if (handle==NULL) {

		    error=ENOMEM;
		    goto out;

		}

		opendir->handle=handle;

		/* here start a thread which gets readdir data from server */
		start_get_sftp_readdir_thread(opendir);

		open_out.fh=(uint64_t) opendir;
		open_out.open_flags=0;
		open_out.padding=0;

		reply_VFS_data(f_request, (char *) &open_out, sizeof(open_out));
		get_current_time_system_time(&directory->synctime);
		unset_fuse_request_flags_cb(f_request);

		opendir->readdir=_fs_sftp_readdir;
		opendir->fsyncdir=_fs_sftp_fsyncdir;
		opendir->releasedir=_fs_sftp_releasedir;

		//  cache_fuse_path(struct directory_s *directory, struct fuse_path_s *fpath)

		return;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;
		logoutput("_fs_sftp_opendir_common: error %i:%s", error, strerror(error));

	    } else {

		error=EPROTO;

	    }

	}

    }

    out:

    if (error==EOPNOTSUPP) {

	logoutput("_fs_sftp_opendir_common: not supported, switching to virtual");

	opendir->readdir=_fs_common_virtual_readdir;
	opendir->fsyncdir=_fs_common_virtual_fsyncdir;
	opendir->releasedir=_fs_common_virtual_releasedir;
	_fs_common_virtual_opendir(opendir, f_request, flags);
	unset_fuse_request_flags_cb(f_request);

    }

    opendir->error=error;
    if (error==EINTR || error==ETIMEDOUT) {

	set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_INCOMPLETE);

    } else {

	set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_ERROR);

    }

    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);

}


void _fs_sftp_opendir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, struct fuse_path_s *fpath, unsigned int flags)
{
    _fs_common_virtual_opendir(opendir, r, flags);
}

void _fs_sftp_readdir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    _fs_common_virtual_readdir(opendir, r, size, offset);
}

void _fs_sftp_fsyncdir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, unsigned char datasync)
{
    reply_VFS_error(r, 0);
}

void _fs_sftp_releasedir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r)
{
    _fs_common_virtual_releasedir(opendir, r);
}

