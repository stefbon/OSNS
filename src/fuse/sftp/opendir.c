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
#include "libosns-threads.h"
#include "libosns-context.h"
#include "libosns-fuse-public.h"
#include "libosns-sftp.h"

#include "interface/sftp.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

#include "inode-stat.h"
#include "symlink.h"
#include "path.h"
#include "handle.h"
#include "getattr.h"
#include "setattr.h"
#include "lock.h"

#include <linux/fuse.h>

static void clear_attr_buffer(struct attr_buffer_s *abuff)
{
    set_attr_buffer_read(abuff, NULL, 0);
}

static void _fs_sftp_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    return _fs_common_readdir(opendir, r, size, offset);
}

static void _fs_sftp_fsyncdir(struct fuse_open_header_s *oh, struct fuse_request_s *r, unsigned int flags)
{
    reply_VFS_error(r, 0);
}

static void _fs_sftp_flushdir(struct fuse_open_header_s *oh, struct fuse_request_s *r, uint64_t lo)
{
    reply_VFS_error(r, 0);
}

static void _fs_sftp_releasedir(struct fuse_open_header_s *oh, struct fuse_request_s *request, unsigned int flags, uint64_t lo)
{
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) oh;
    struct service_context_s *ctx=oh->ctx;
    struct workspace_mount_s *w=get_workspace_mount_ctx(ctx);
    struct context_interface_s *ie=&ctx->interface;
    struct osns_lock_s wlock;
    struct directory_s *directory=NULL;

    logoutput("_fs_sftp_releasedir");

    reply_VFS_error(request, 0);

    signal_lock(opendir->signal);
    opendir->flags |= FUSE_OPENDIR_FLAG_RELEASE;
    signal_unlock(opendir->signal);

    if (opendir->header.handle) release_fuse_handle(opendir->header.handle); /* only set the handle to be released ... it is released if the last user releases it */

    /* wait for the the background thread to finish (if it isn't that already) */

    if (signal_wait_flag_unset(opendir->signal, &opendir->flags, FUSE_OPENDIR_FLAG_THREAD, NULL)==0) {
        struct directory_s *d=get_directory(ctx, opendir->header.inode, 0);

	clear_attr_buffer(&opendir->data.abuff);

	if ((opendir->flags & (FUSE_OPENDIR_FLAG_INCOMPLETE | FUSE_OPENDIR_FLAG_ERROR))==0) {

	    opendir->header.inode->flags &= ~INODE_FLAG_REMOTECHANGED;
	    _fs_common_remove_nonsynced_dentries(opendir);

	}

        if (d) release_cached_path(d);

    }

}

static void _cb_created(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *ctx=ce->context;
    struct inode_s *inode=entry->inode;
    struct fuse_opendir_s *fo=ce->tree.opendir;
    struct directory_s *directory=(* ce->get_directory)(ce);
    struct attr_buffer_s *abuff=ce->cache.abuff;
    struct attr_context_s *attrctx=(struct attr_context_s *) ce->ptr;
    struct context_interface_s *interface=&ctx->interface;
    struct system_stat_s *stat=&inode->stat;

    set_sftp_stat_defaults(interface, stat);
    read_sftp_attributes(attrctx, abuff, stat);
    set_nlink_system_stat(stat, 1);
    inode->nlookup=0;

    copy_system_time(&inode->stime, &directory->synctime);
    add_inode_context(get_root_context(ctx), inode);
    (* directory->inode->fs->type.dir.use_fs)(ctx, inode);

    if (system_stat_test_ISDIR(stat)) {

	set_nlink_system_stat(stat, 2);
	increase_nlink_system_stat(&directory->inode->stat, 1);
	set_ctime_system_stat(&directory->inode->stat, &directory->synctime); /* set the change time of parent since attribute nlink changed */
	assign_directory_inode(inode);

    } else if (system_stat_test_ISLNK(stat)) {

        queue_fuse_symlink(fo, entry);

    }

    /* no matter what type: the mtime of the parent changes */
    set_mtime_system_stat(&directory->inode->stat, &directory->synctime);
    fo->count_created++;

}

static void _cb_found(struct entry_s *entry, struct create_entry_s *ce)
{
    struct inode_s *inode=entry->inode;
    struct fuse_opendir_s *opendir=ce->tree.opendir;
    struct directory_s *directory=NULL;
    struct system_timespec_s mtime=SYSTEM_TIME_INIT;
    struct attr_buffer_s *abuff=ce->cache.abuff;
    struct attr_context_s *attrctx=(struct attr_context_s *) ce->ptr;
    struct system_stat_s *stat=&inode->stat;

    get_mtime_system_stat(stat, &mtime);    /* keep track if the file has been changed on the remote side for caching purposes */
    read_sftp_attributes(attrctx, abuff, stat);
    if (test_remote_file_modified(stat, &mtime)==1) inode->flags |= INODE_FLAG_REMOTECHANGED;

    directory=(* ce->get_directory)(ce);
    opendir->count_found++;
    copy_system_time(&inode->stime, &directory->synctime);

}

static void _cb_error(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int error)
{
    logoutput_warning("_cb_error: error %i:%s creating %s", error, strerror(error), xname->name);
    ce->error=error;
}

struct _cb_readdir_hlpr_s {
    struct fuse_opendir_s 			*opendir;
    unsigned int				error;
    unsigned int				count;
};

static void _cb_success_readdir(struct fuse_handle_s *handle, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_readdir_hlpr_s *hlpr=(struct _cb_readdir_hlpr_s *) ptr;
    struct fuse_opendir_s *opendir=hlpr->opendir;
    struct attr_buffer_s *abuff=&opendir->data.abuff;
    struct name_response_s *name=&reply->response.name;

    /* take the data read from sftp to attr buffer */
    hlpr->count=name->count;
    set_attr_buffer_read(abuff, reply->data, reply->size);
    set_attr_buffer_count(abuff, name->count);

    if (name->flags & SFTP_RESPONSE_FLAG_EOF_SUPPORTED) {
	unsigned int flags = ATTR_BUFFER_FLAG_EOF_SUPPORTED;

	if (name->flags & SFTP_RESPONSE_FLAG_EOF) flags |= ATTR_BUFFER_FLAG_EOF;
	set_attr_buffer_flags(abuff, flags);

    }

    reply->data = NULL;
    reply->size = 0;

}

static void _cb_error_readdir(struct fuse_handle_s *handle, unsigned int errcode, void *ptr)
{
    struct _cb_readdir_hlpr_s *hlpr=(struct _cb_readdir_hlpr_s *) ptr;
    hlpr->error=errcode;
}

static unsigned char _cb_interrupted_readdir(void *ptr)
{
    struct _cb_readdir_hlpr_s *hlpr=(struct _cb_readdir_hlpr_s *) ptr;
    struct fuse_opendir_s *opendir=hlpr->opendir;
    return ((opendir->flags & FUSE_OPENDIR_FLAG_FINISH ) ? 1 : 0);
}

static int _sftp_readdir(struct fuse_opendir_s *opendir)
{
    struct attr_buffer_s *abuff=&opendir->data.abuff;
    struct _cb_readdir_hlpr_s hlpr;

    clear_attr_buffer(abuff);

    hlpr.opendir=opendir;
    hlpr.count=0;
    hlpr.error=0;

    _sftp_handle_readdir(opendir->header.handle, 0, 0, _cb_success_readdir, _cb_error_readdir, _cb_interrupted_readdir, (void *)&hlpr);
    return ((hlpr.count) ? hlpr.count : -hlpr.error);

}

static void skip_sftp_attributes(struct attr_context_s *attrctx, struct attr_buffer_s *abuff)
{
    struct system_stat_s stat;

    read_sftp_attributes(attrctx, abuff, &stat);
}

static void do_sftp_readdir_background(void *ptr)
{
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) ptr;
    struct attr_buffer_s *abuff=&opendir->data.abuff;
    struct service_context_s *ctx=opendir->header.ctx;
    struct attr_context_s *attrctx=get_sftp_attr_context(&ctx->interface);
    struct name_s xname;
    struct entry_s *entry=NULL;
    struct inode_s *inode=NULL;
    struct create_entry_s ce;

    logoutput_debug("do_sftp_readdir_background");

    signal_lock(opendir->signal);

    if (opendir->flags & (FUSE_OPENDIR_FLAG_THREAD | FUSE_OPENDIR_FLAG_EOD | FUSE_OPENDIR_FLAG_FINISH)) {

	signal_unlock(opendir->signal);
	return;

    }

    opendir->flags |= FUSE_OPENDIR_FLAG_THREAD;
    signal_unlock(opendir->signal);

    use_fuse_handle(opendir->header.handle); /* make sure the handle is not released during the period used here */

    init_create_entry(&ce, &xname, NULL, NULL, opendir, ctx, NULL, (void *)attrctx);
    ce.cb_created=_cb_created;
    ce.cb_found=_cb_found;
    ce.cb_error=_cb_error;
    ce.cache.abuff=abuff;
    init_name(&xname);

    while ((opendir->flags & (FUSE_OPENDIR_FLAG_EOD | FUSE_OPENDIR_FLAG_FINISH))==0) {

	if (abuff->count<=0) {
	    int result=0;

	    /* if no dentries in buffer get from server */

	    result=_sftp_readdir(opendir);

	    if (result<=0) {
		unsigned int flag=0;

		/* some error */

		result=abs(result);

		logoutput_debug("do_sftp_readdir_background: error result %i", result);

		if (result==EINTR || result==ETIMEDOUT) {

		    flag|=FUSE_OPENDIR_FLAG_INCOMPLETE;

		} else if (result==ENODATA || result==0) {

		    flag|=FUSE_OPENDIR_FLAG_EOD;

		} else {

		    flag|=FUSE_OPENDIR_FLAG_ERROR;

		}

		set_flag_fuse_opendir(opendir, flag);
		break;

	    }

	}

	while (abuff->count>0) {
	    struct ssh_string_s name=SSH_STRING_INIT;

	    /* extract name and attributes from names
		only get the name, do the attr later */

	    (* attrctx->ops.read_name_name_response)(attrctx, abuff, &name);

	    if ((name.len==0 || name.ptr==NULL) || (name.len==1 && strncmp(name.ptr, ".", 1)==0) || (name.len==2 && strncmp(name.ptr, "..", 2)==0)) {
	        struct system_stat_s stat;

		/* skip the . and .. entries */
                read_sftp_attributes(attrctx, abuff, &stat);
                logoutput_debug("do_sftp_readdir_background: ignore name %.*s", name.len, name.ptr);

	    } else {

                if ((memcmp(name.ptr, ".", 1)==0) && (opendir->flags & FUSE_OPENDIR_FLAG_IGNORE_DOTFILES)) {
                    struct system_stat_s stat;

                    logoutput_debug("do_sftp_readdir_background: ignore dotfile name %.*s", name.len, name.ptr);
                    read_sftp_attributes(attrctx, abuff, &stat);
                    goto next;

                } else {

                    logoutput_debug("do_sftp_readdir_background: queue name %.*s", name.len, name.ptr);

                }

		xname.name=name.ptr;
		xname.len=name.len;
		calculate_nameindex(&xname);

		/* create the entry (if not exist already)
		    create a direntry queue for the (later) readdir to fill the buffer to send to the VFS */

		entry=create_entry_extended(&ce);
		if (entry) queue_fuse_direntry(opendir, entry);

	    }

	    next:
	    abuff->count--;

	}

	if (abuff->flags & ATTR_BUFFER_FLAG_EOF_SUPPORTED) {

	    if ((abuff->count <= 0) && abuff->left>0) {
		unsigned char eof= *(abuff->buffer + abuff->pos);

		if (eof) {

		    abuff->flags |= ATTR_BUFFER_FLAG_EOF;
		    set_flag_fuse_opendir(opendir, FUSE_OPENDIR_FLAG_EOD);
		    break;

		}

	    }

	}

    }

    if (opendir->symlinks.count>0) {
        struct directory_s *d=get_directory(ctx, opendir->header.inode, 0);
        struct list_element_s *list=remove_list_head(&opendir->symlinks);
        struct service_context_s *rootctx=get_root_context(ctx);
        unsigned int pathlen=get_pathmax(rootctx) + 1;
        char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
        struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
        struct shared_signal_s *signal=opendir->signal;

        while (list) {
            struct fuse_direntry_s *direntry=(struct fuse_direntry_s *) list;
            struct entry_s *entry=direntry->entry;
            struct fuse_symlink_s *fcs=get_inode_fuse_cache_symlink(entry->inode);

            logoutput_debug("do_sftp_readdir_background: symlink %.*s", entry->name.len, entry->name.name);

            if ((fcs==NULL) || (entry->inode->flags & INODE_FLAG_REMOTECHANGED)) {

                /* get the target .... */

                init_fuse_path(fpath, pathlen + 1);
	        append_name_fpath(fpath, &entry->name);
	        get_path_root_context(d, fpath);

                unsigned int errcode=_fs_sftp_getlink(ctx, NULL, opendir, entry->inode, fpath);

            }

            list=remove_list_head(&opendir->symlinks);

        }

    }

    signal_lock(opendir->signal);
    opendir->flags &= ~FUSE_OPENDIR_FLAG_THREAD;
    opendir->flags |= FUSE_OPENDIR_FLAG_EOD;
    signal_unlock(opendir->signal);

    post_fuse_handle(opendir->header.handle, 0);

}

void start_sftp_readdir_background_thread(struct fuse_opendir_s *opendir)
{
    work_workerthread(NULL, 0, do_sftp_readdir_background, (void *) opendir);
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

struct _cb_opendir_hlpr_s {
    struct fuse_request_s 			*request;
    struct fuse_opendir_s 			*opendir;
    struct fuse_path_s				*fpath;
};

static void _cb_success_opendir(struct service_context_s *ctx, struct sftp_reply_s *reply, void *ptr)
{
    struct _cb_opendir_hlpr_s *hlpr=(struct _cb_opendir_hlpr_s *) ptr;
    struct fuse_opendir_s *opendir=hlpr->opendir;
    struct fuse_path_s *fpath=hlpr->fpath;
    struct fuse_open_out out;
    struct inode_s *inode=opendir->header.inode;
    struct directory_s *d=get_directory(ctx, inode, 0);
    struct fuse_handle_s *handle=create_fuse_handle(ctx, get_ino_system_stat(&inode->stat), FUSE_HANDLE_FLAG_OPENDIR, (char *) reply->data, reply->size, 0);

    if (handle==NULL) {

	reply_VFS_error(hlpr->request, ENOMEM);
	opendir->error=ENOMEM;
	return;

    }

    memset(&out, 0, sizeof(struct fuse_open_out));
    out.fh=(uint64_t) opendir;
    out.open_flags = (FOPEN_KEEP_CACHE | FOPEN_CACHE_DIR);
    reply_VFS_data(hlpr->request, (char *) &out, sizeof(struct fuse_open_out));

    opendir->header.handle=handle;

    /* find out which operations on the (directory) handle are suppported
        - by default fgetstat and fsetstat (default operations of sftp xfer)
        - fsyncdir is an optional extension by openssh.com
        - fstatat is an optional extension by osns.net
    */

    handle->flags |= (FUSE_HANDLE_FLAG_FGETSTAT | FUSE_HANDLE_FLAG_FSETSTAT);
    if (get_index_sftp_extension_fsync(&ctx->interface)>0) handle->flags |= FUSE_HANDLE_FLAG_FSYNC;
    if (get_index_sftp_extension_fstatat(&ctx->interface)>0) handle->flags |= FUSE_HANDLE_FLAG_FSTATAT;

    handle->pathlen=(unsigned int)(fpath->path + fpath->len - fpath->pathstart); /* to determine the buffer is still big enough to hold every path on the workspace */
    handle->cb.release=_sftp_handle_release;

    start_sftp_readdir_background_thread(opendir); /* start a thread to get entries in the background */

    get_current_time_system_time(&d->synctime);

    opendir->readdir=_fs_sftp_readdir;
    opendir->header.fgetattr=_fs_sftp_fgetattr;
    opendir->header.fsetattr=_fs_sftp_fsetattr;
    opendir->header.flush=_fs_sftp_flushdir;
    opendir->header.fsync=_fs_sftp_fsyncdir;
    opendir->header.release=_fs_sftp_releasedir;
    opendir->header.getlock=_fs_sftp_getlock;
    opendir->header.setlock=_fs_sftp_setlock;
    opendir->header.flock=_fs_sftp_flock;

    // if ((handle->flags & FUSE_HANDLE_FLAG_FSTATAT)==0) {

	/* only cache the path when handle calls and espacially the fstatat call are not available
	    if this is available the path (and thus the cached path) is not used for lookups etc */

	cache_fuse_path(d, hlpr->fpath);

    // }

}

static void _cb_error_opendir(struct service_context_s *ctx, unsigned int errcode, void *ptr)
{
    struct _cb_opendir_hlpr_s *hlpr=(struct _cb_opendir_hlpr_s *) ptr;
    reply_VFS_error(hlpr->request, errcode);
    hlpr->opendir->error=errcode;
}

static unsigned char _cb_interrupted_opendir(void *ptr)
{
    struct _cb_opendir_hlpr_s *hlpr=(struct _cb_opendir_hlpr_s *) ptr;
    return ((hlpr->request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) ? 1 : 0);
}

void _fs_sftp_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *request, struct fuse_path_s *fpath, unsigned int flags)
{
    struct _cb_opendir_hlpr_s hlpr;

    opendir->flags |= (FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS | FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS | FUSE_OPENDIR_FLAG_IGNORE_DOTFILES); /* sane flags */

    hlpr.request=request;
    hlpr.opendir=opendir;
    hlpr.fpath=fpath;

    _sftp_path_open(opendir->header.ctx, fpath, NULL, flags, "opendir", _cb_success_opendir, _cb_error_opendir, _cb_interrupted_opendir, (void *) &hlpr);

}
