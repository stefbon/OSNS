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
#include "threads.h"
#include "options.h"

#include "interface/smb-signal.h"
#include "interface/smb.h"
#include "interface/smb-wait-response.h"

#include "smb/send/opendir.h"
#include "smb/send/readlink.h"

extern struct fs_options_s fs_options;

extern const char *dotdotname;
extern const char *dotname;
extern const char *rootpath;

static unsigned int _cb_cache_size(struct create_entry_s *ce)
{
    struct context_interface_s *interface=&ce->context->interface;

    fill_inode_attr_smb(interface, &ce->cache.st, ce->cache.link.link.ptr);
    return 0;
}

static void _cb_created(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct fuse_buffer_s *buffer=ce->cache.buffer;
    struct entry_s *parent=get_parent_entry(entry);
    struct inode_s *inode=entry->inode;
    struct fuse_opendir_s *fo=ce->tree.opendir;
    struct directory_s *directory=(* ce->get_directory)(ce);

    fill_inode_stat(inode, &ce->cache.st); /* from ce cache */
    inode->st.st_mode=ce->cache.st.st_mode;
    inode->st.st_size=ce->cache.st.st_size;
    inode->st.st_nlink=1;
    inode->nlookup=1;
    fo->count_created++; /* count the numbers added */

    memcpy(&inode->stim, &directory->synctime, sizeof(struct timespec));
    add_inode_context(context, inode);

    if (S_ISDIR(inode->st.st_mode)) {

	inode->st.st_nlink=2;
	parent->inode->st.st_nlink++;
	assign_directory_inode(inode);

    } else if (S_ISLNK(inode->st.st_mode)) {

	/* symlink is cached */

	if (ce->cache.link.type==DATA_LINK_TYPE_SYMLINK) {

	    inode->link.type=DATA_LINK_TYPE_SYMLINK;
	    inode->link.link.ptr=ce->cache.link.link.ptr;

	    ce->cache.link.link.ptr=NULL;
	    ce->cache.link.type=0;

	}

    }

    memcpy(&directory->inode->st.st_ctim, &directory->synctime, sizeof(struct timespec));
    memcpy(&directory->inode->st.st_mtim, &directory->synctime, sizeof(struct timespec));
    set_entry_ops(entry);

}

static void _cb_found(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct fuse_buffer_s *buffer=ce->cache.buffer;
    struct inode_s *inode=NULL;
    struct fuse_opendir_s *opendir=ce->tree.opendir;
    struct directory_s *directory=NULL;
    struct timespec mtim;

    logoutput("_cb_found: (entry %s)", (entry) ? "exists" : "NULL");

    inode=entry->inode;
    directory=(* ce->get_directory)(ce);

    memcpy(&mtim, &inode->st.st_mtim, sizeof(struct timespec));
    fill_inode_stat(inode, &ce->cache.st);
    inode->st.st_mode=ce->cache.st.st_mode;
    inode->st.st_size=ce->cache.st.st_size;
    inode->flags |= INODE_FLAG_CACHED;

    if (inode->st.st_mtim.tv_sec>mtim.tv_sec || (inode->st.st_mtim.tv_sec==mtim.tv_sec && inode->st.st_mtim.tv_nsec>mtim.tv_nsec)) {

	inode->flags |= INODE_FLAG_REMOTECHANGED;

    }

    opendir->count_found++;
    inode->nlookup++;
    memcpy(&inode->stim, &directory->synctime, sizeof(struct timespec));

    if (S_ISLNK(inode->st.st_mode)) {

	/* symlink is cached */

	if (ce->cache.link.type==DATA_LINK_TYPE_SYMLINK) {

	    if (inode->link.type==DATA_LINK_TYPE_SYMLINK) {

		if (strcmp(inode->link.link.ptr, ce->cache.link.link.ptr) !=0) {

		    if ((inode->flags & INODE_FLAG_REMOTECHANGED)==0) {

			logoutput_warning("_cb_found: symbolic link changed but attribute mtime did not");

		    }

		    free(inode->link.link.ptr);
		    inode->link.link.ptr=ce->cache.link.link.ptr;
		    ce->cache.link.link.ptr=NULL;
		    ce->cache.link.type=0;

		}

	    } else if (inode->link.type==0) {

		inode->link.type=DATA_LINK_TYPE_SYMLINK;
		inode->link.link.ptr=ce->cache.link.link.ptr;
		ce->cache.link.link.ptr=NULL;
		ce->cache.link.type=0;

	    } else {

		logoutput_warning("_cb_found: symbolic link has different link type (%i)", inode->link.type);

	    }

	}

    } else if (S_ISDIR(inode->st.st_mode)) {
	struct getpath_s *getpath=directory->getpath;

	if (getpath==NULL) set_directory_pathcache(context, directory, NULL);

    }
}

static void _cb_error(struct entry_s *parent, struct name_s *xname, struct create_entry_s *ce, unsigned int error)
{
    logoutput_warning("_cb_error: error %i:%s creating %s", error, strerror(error), xname->name);
    ce->error=error;
}

/* special thread to read the smb direntries and translate them into FUSE dentries */

static void get_smb_readdir_thread(void *ptr)
{
    struct fuse_opendir_s *opendir=(struct fuse_opendir_s *) ptr;
    struct service_context_s *context=opendir->context;
    struct name_s xname;
    struct entry_s *entry=NULL;
    struct inode_s *inode=NULL;
    struct create_entry_s ce;
    unsigned int error=0;
    unsigned char dofree=0;

    logoutput("get_smb_readdir_thread");

    signal_lock(opendir->signal);

    if (opendir->flags & _FUSE_OPENDIR_FLAG_THREAD) {

	signal_unlock(opendir->signal);
	return;

    }

    opendir->flags |= _FUSE_OPENDIR_FLAG_THREAD;
    signal_unlock(opendir->signal);

    init_create_entry(&ce, NULL, NULL, NULL, opendir, context, NULL, NULL);

    ce.cb_cache_size=_cb_cache_size;
    ce.cb_created=_cb_created;
    ce.cb_found=_cb_found;
    ce.cb_error=_cb_error;

    xname.name=NULL;
    xname.len=0;
    xname.index=0;

    ce.name=&xname;

    while ((opendir->flags & _FUSE_OPENDIR_FLAG_READDIR_FINISH)==0) {
	char *name=NULL;
	char *buffer=NULL;

	if (get_smb_direntry(&context->interface, opendir->handle.ptr, &name, &buffer)) {

	    xname.name=name;
	    xname.len=strlen(name);
	    calculate_nameindex(&xname);

	    init_fuse_buffer(ce.cache.buffer, buffer, get_size_buffer_smb_opendir(), 1);

	    /* create the entry (if not exist already) */

	    entry=create_entry_extended_batch(&ce);

	    if (! entry) {

		logoutput_warning("get_smb_readdir_thread: entry %s not created", name);

	    } else {

		if ((* opendir->hidefile)(opendir, entry)==0) {

		    /* create a direntry queue for the (later) readdir to fill the buffer to send to the VFS */

		    queue_fuse_direntry(opendir, entry);

		}

	    }

	    if (ce.cache.link.type==DATA_LINK_TYPE_SYMLINK) {

		free(ce.cache.link.link.ptr);
		ce.cache.link.link.ptr=NULL;
		ce.cache.link.type=0;

	    }

	} else {

	    finish_get_fuse_direntry(opendir);
	    break;

	}

    }

    signal_lock(opendir->signal);
    opendir->flags &= ~_FUSE_OPENDIR_FLAG_THREAD;
    if (opendir->flags & _FUSE_OPENDIR_FLAG_RELEASE) dofree=1;
    signal_unlock(opendir->signal);

    if (dofree) free(opendir);

}

void start_get_smb_readdir_thread(struct fuse_opendir_s *opendir)
{
    work_workerthread(NULL, 0, get_smb_readdir_thread, (void *) opendir, NULL);
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

void _fs_smb_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, unsigned int flags)
{
    struct service_context_s *context=(struct service_context_s *) opendir->context;
    struct context_interface_s *interface=&context->interface;
    struct smb_request_s smb_r;
    struct smb_data_s *data=NULL;
    unsigned int size=get_size_buffer_smb_opendir();
    unsigned int error=EIO;
    struct directory_s *directory=get_directory(opendir->inode);

    /* test a full opendir/readdir is required: test entries are deleted and/or created */

    if (directory && directory->synctime.tv_sec>0) {

	if ((opendir->inode->flags & INODE_FLAG_REMOTECHANGED)==0) {

	    /* no entries added and deleted: no need to read all entries again: use cache */

	    logoutput("_fs_smb_opendir: remote directory has not changed since last visit: no full opendir is required");

	    opendir->readdir=_fs_common_virtual_readdir;
	    opendir->readdirplus=_fs_common_virtual_readdirplus;
	    opendir->fsyncdir=_fs_common_virtual_fsyncdir;
	    opendir->releasedir=_fs_common_virtual_releasedir;
	    set_get_fuse_direntry_common(opendir);
	    _fs_common_virtual_opendir(opendir, f_request, flags);
	    return;

	}

    }

    logoutput("_fs_smb_opendir: send opendir %s", pathinfo->path);

    data=malloc(sizeof(struct smb_data_s) + size);

    if (data==NULL) {

	error=ENOMEM;
	goto out;

    }

    memset(data, 0, sizeof(struct smb_data_s) + size);

    init_smb_request(&smb_r, interface, f_request);

    if (send_smb_opendir_ctx(interface, &smb_r, pathinfo->path, data)>0) {
	struct timespec timeout;

	/* it's possible to let this threat do things here since
	    it goes into a wait state to receive the response from the server
	    and this goes over the network so that gives a little time (interesting: how much ?)*/

	get_smb_request_timeout_ctx(interface, &timeout);

	 /* sane flags for symlinks:
	    - no symlinks crossing filesystems
	    - no broken symlinks
	     */

	opendir->flags |= (_FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS | _FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS) ;

	 /* sane flags for hiding files:
	    - hide files starting with a dot */

	if (fs_options.sftp.hideflags & _OPTIONS_SFTP_HIDE_FLAG_DOTFILE) opendir->flags |= _FUSE_OPENDIR_FLAG_HIDE_DOTFILES;

	if (wait_smb_response_ctx(interface, &smb_r, &timeout)==1) {
	    struct smb_data_s *data=smb_r.data; /* smb request is pointing to data by */
	    struct fuse_open_out open_out;

	    /* store the pointer to smb2dir */

	    opendir->handle.ptr=data->ptr;

	    /* here start a thread to translate smb direntries into FUSE ones */

	    start_get_smb_readdir_thread(opendir);

	    memset(&open_out, 0, sizeof(struct fuse_open_out));
	    open_out.fh=(uint64_t) opendir;

	    reply_VFS_data(f_request, (char *) &open_out, sizeof(struct fuse_open_out));
	    get_current_time(&directory->synctime);
	    unset_fuse_request_flags_cb(f_request);
	    return;

	}

	error=(smb_r.error) ? smb_r.error : EPROTO;

	/* put smb data on special list */

	add_smb_list_pending_requests_ctx(interface, &data->list);

    }

    out:

    if (error==EOPNOTSUPP) {

	logoutput("_fs_smb_opendir_common: not supported, switching to virtual");

	opendir->readdir=_fs_common_virtual_readdir;
	opendir->fsyncdir=_fs_common_virtual_fsyncdir;
	opendir->releasedir=_fs_common_virtual_releasedir;
	opendir->get_fuse_direntry=get_fuse_direntry_virtual;
	_fs_common_virtual_opendir(opendir, f_request, flags);
	unset_fuse_request_flags_cb(f_request);

    }

    opendir->error=error;
    if (error==EINTR || error==ETIMEDOUT) {

	set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_INCOMPLETE);

    } else {

	set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_ERROR);

    }

    reply_VFS_error(f_request, error);
    unset_fuse_request_flags_cb(f_request);
    if (data) free(data);

}
void _fs_smb_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    return _fs_common_virtual_readdir(opendir, r, size, offset);
}

void _fs_smb_readdirplus(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    reply_VFS_error(r, EOPNOTSUPP);
}

void _fs_smb_fsyncdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, unsigned char datasync)
{
    reply_VFS_error(r, 0);
}

void _fs_smb_releasedir(struct fuse_opendir_s *opendir, struct fuse_request_s *f_request)
{
    struct service_context_s *context=opendir->context;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct context_interface_s *interface=&context->interface;

    logoutput("_fs_smb_releasedir");

    set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_FINISH);

    if (opendir->handle.ptr) {

	smb_closedir_ctx(interface, opendir->handle.ptr);
	opendir->handle.ptr=NULL;

    }

    out:

    reply_VFS_error(f_request, 0);

    if ((opendir->flags & _FUSE_OPENDIR_FLAG_READDIR_INCOMPLETE)==0) {

	if (opendir->inode->flags & INODE_FLAG_REMOTECHANGED) opendir->inode->flags-=INODE_FLAG_REMOTECHANGED;
	_fs_common_remove_nonsynced_dentries(opendir);

    }

    unset_fuse_request_flags_cb(f_request);

}

void _fs_smb_opendir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, struct pathinfo_s *pathinfo, unsigned int flags)
{
    _fs_common_virtual_opendir(opendir, r, flags);
}

void _fs_smb_readdir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    _fs_common_virtual_readdir(opendir, r, size, offset);
}

void _fs_smb_readdirplus_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    _fs_common_virtual_readdirplus(opendir, r, size, offset);
}

void _fs_smb_fsyncdir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, unsigned char datasync)
{
    reply_VFS_error(r, 0);
}

void _fs_smb_releasedir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r)
{
    _fs_common_virtual_releasedir(opendir, r);
}

