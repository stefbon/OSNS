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

#include "sftp/common-protocol.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

#include "inode-stat.h"
#include "symlink.h"

extern struct fs_options_s fs_options;

extern const char *dotdotname;
extern const char *dotname;
extern const char *rootpath;

static unsigned int _cb_cache_size(struct create_entry_s *ce)
{
    /* unfortunatly with a name response (=readdir response) there is no
	other way to determine the size than to process the ATTR and than compare the new position
	in the buffer with the old one .... */

    struct fuse_buffer_s *buffer=ce->cache.buffer;
    struct context_interface_s *interface=&ce->context->interface;
    char *pos=buffer->pos;
    struct sftp_attr_s attr;
    unsigned int size=0;

    // logoutput("_cb_cache_size: pos %i", (unsigned int)(buffer->pos - buffer->data));

    memset(&attr, 0, sizeof(struct sftp_attr_s));
    read_attr_nameresponse_ctx(interface, buffer, &attr);
    fill_inode_attr_sftp(interface, &ce->cache.st, &attr);
    logoutput("_cb_cache_size: attr type %i permissions %i", attr.type, attr.permissions);
    return (unsigned int)(buffer->pos - pos);
}

static void strdup_target_path(char *target, char **p_path, unsigned int *error)
{
    char *path=strdup(target);

    *error=ENOMEM;

    if (path) {

	*p_path=path;
	*error=0;

    }

}

static unsigned int send_check_target_symlink_valid(struct create_entry_s *ce, struct pathinfo_s *pathinfo, char **p_path)
{
    struct fuse_opendir_s *opendir=ce->tree.opendir;
    struct context_interface_s *interface=&ce->context->interface;
    struct sftp_request_s sftp_r;
    unsigned int result=EIO;
    char origpath[pathinfo->len + 1];
    unsigned int origpath_len=pathinfo->len;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char pathinfobuffer[pathlen];

    strcpy(origpath, pathinfo->path);
    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, pathinfobuffer, pathinfo);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.readlink.path=(unsigned char *) pathinfo->path;
    sftp_r.call.readlink.len=pathinfo->len;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    if (send_sftp_readlink_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_NAME) {
		struct name_response_s *names=&reply->response.names;
		struct ssh_string_s tmp=SSH_STRING_INIT;

		if (read_ssh_string(names->buff, names->size, &tmp)>0) {
		    char target[tmp.len+1];
		    unsigned int error=0;

		    memcpy(target, tmp.ptr, tmp.len);
		    target[tmp.len]='\0';
		    logoutput("_fs_sftp_readlink_common: %s target %s", pathinfo->path, target);

		    error=check_valid_target_symlink(interface, origpath, target, tmp.len);

		    logoutput("_fs_sftp_readlink_common: target %s result %i:%s", target, error, strerror(error));

		    switch (error) {

			case EXDEV:

			    if (opendir->flags & _FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS) {

				result=EXDEV;

			    } else {

				strdup_target_path(target, p_path, &result);

			    }

			    break;

			case EINVAL:

			    if (opendir->flags & _FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS) {

				result=EINVAL;

			    } else {

				strdup_target_path(target, p_path, &result);

			    }

			    break;

			default:

			    strdup_target_path(target, p_path, &result);

		    }

		} else {

		    result=EPROTO;

		}

		free(names->buff);
		names->buff=NULL;
		names->size=0;

	    } else if (reply->type==SSH_FXP_STATUS) {

		result=reply->response.status.linux_error;

	    } else {

		result=EPROTO;

	    }

	}

    }

    logoutput("send_check_target_symlink_valid: result %i:%s", result, strerror(result));
    return result;
}


static int _cb_check(struct create_entry_s *ce)
{
    int result=0;

    /* when a symlink check it's a valid one */

    if (S_ISLNK(ce->cache.st.st_mode)) {
	struct fuse_opendir_s *opendir=ce->tree.opendir;
	struct service_context_s *context=ce->context;
	struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
	struct directory_s *directory=(* ce->get_directory)(ce);
	struct pathinfo_s pathinfo=PATHINFO_INIT;
	unsigned int pathlen=get_pathmax(workspace);
	char buffer[sizeof(struct fuse_path_s) + pathlen + 1];
	struct fuse_path_s *fpath=(struct fuse_path_s *) buffer;
	struct service_fs_s *fs=NULL;
	unsigned int error=0;
	char *target=NULL;

	/* check symlink */
	logoutput("_cb_check: %.*s", ce->name->len, ce->name->name);

	init_fuse_path(fpath, pathlen + 1);
	append_name_fpath(fpath, ce->name);
	get_service_context_path(context, directory, fpath);
	pathinfo.path=get_pathinfo_fpath(fpath, &pathinfo.len);

	error=send_check_target_symlink_valid(ce, &pathinfo, &target);

	if (error==0) {

	    if (target) {

		ce->cache.link.type=DATA_LINK_TYPE_SYMLINK;
		ce->cache.link.link.ptr=target;

	    }

	} else {

	    result=-1;

	}

    }

    return result;

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
	set_directory_dump(inode, get_dummy_directory());

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
    check_create_inodecache(inode, ce->cache_size, buffer->pos - ce->cache_size, INODECACHE_FLAG_READDIR);
    inode->flags |= INODE_FLAG_CACHED;
    set_entry_ops(entry);

}

static void _cb_found(struct entry_s *entry, struct create_entry_s *ce)
{
    struct service_context_s *context=ce->context;
    struct fuse_buffer_s *buffer=ce->cache.buffer;
    struct inode_s *inode=entry->inode;
    struct fuse_opendir_s *opendir=ce->tree.opendir;
    struct directory_s *directory=(* ce->get_directory)(ce);

    logoutput("_cb_found");

    if (check_create_inodecache(inode, ce->cache_size, buffer->pos - ce->cache_size, INODECACHE_FLAG_READDIR)==1) {
	struct timespec mtim;

	memcpy(&mtim, &inode->st.st_mtim, sizeof(struct timespec));

	fill_inode_stat(inode, &ce->cache.st);
	inode->st.st_mode=ce->cache.st.st_mode;
	inode->st.st_size=ce->cache.st.st_size;
	inode->flags |= INODE_FLAG_CACHED;

	if (inode->st.st_mtim.tv_sec>mtim.tv_sec || (inode->st.st_mtim.tv_sec==mtim.tv_sec && inode->st.st_mtim.tv_nsec>mtim.tv_nsec)) {

	    entry->flags |= _ENTRY_FLAG_REMOTECHANGED;

	}

    }

    opendir->count_found++;
    inode->nlookup++;
    memcpy(&inode->stim, &directory->synctime, sizeof(struct timespec));

    if (S_ISLNK(inode->st.st_mode)) {

	/* symlink is cached */

	if (ce->cache.link.type==DATA_LINK_TYPE_SYMLINK) {

	    if (inode->link.type==DATA_LINK_TYPE_SYMLINK) {

		if (strcmp(inode->link.link.ptr, ce->cache.link.link.ptr) !=0) {

		    if ((entry->flags & _ENTRY_FLAG_REMOTECHANGED)==0) {

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

    clear_fuse_buffer(&opendir->data.buffer);

    logoutput("_sftp_get_readdir_names");

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.readdir.handle=(unsigned char *) opendir->handle.name.name;
    sftp_r.call.readdir.len=opendir->handle.name.len;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    if (send_sftp_readdir_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_NAME) {
		struct name_response_s *names=&reply->response.names;

		/* copy the pointers to the names, not the names (and attr) self */

		init_fuse_buffer(&opendir->data.buffer, (char *) names->buff, names->size, names->count, names->eof);
		result=(names->buff && names->size>0) ? names->count : 0;
		names->buff = NULL;
		names->size = 0;

	    } else if (reply->type==SSH_FXP_STATUS) {
		struct status_response_s *status=&reply->response.status;

		logoutput("_sftp_get_readdir_names: reply status: %i", status->linux_error);

		if (status->linux_error==ENODATA) {

		    // set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_FINISH);
		    *error=0;
		    result=0;

		} else {

		    // set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_ERROR);
		    *error=status->linux_error;
		    result=-1;

		}

	    } else {

		// set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_ERROR);
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
    struct fuse_buffer_s *buffer=&opendir->data.buffer;
    struct service_context_s *context=opendir->context;
    struct stat st;
    size_t pos=0, dirent_size=0;
    struct name_s xname;
    struct entry_s *entry=NULL;
    struct inode_s *inode=NULL;
    struct create_entry_s ce;
    unsigned int error=0;
    unsigned char dofree=0;

    logoutput("get_sftp_readdir_thread");

    pthread_mutex_lock(opendir->mutex);

    if (opendir->flags & _FUSE_OPENDIR_FLAG_THREAD) {

	pthread_mutex_unlock(opendir->mutex);
	return;

    }

    opendir->flags |= _FUSE_OPENDIR_FLAG_THREAD;
    pthread_mutex_unlock(opendir->mutex);

    init_create_entry(&ce, NULL, NULL, NULL, opendir, context, NULL, NULL);

    ce.cb_cache_size=_cb_cache_size;
    ce.cb_check=_cb_check;
    ce.cb_created=_cb_created;
    ce.cb_found=_cb_found;
    ce.cb_error=_cb_error;

    ce.cache.buffer=buffer;

    memset(&st, 0, sizeof(struct stat));
    memset(&xname, 0, sizeof(struct name_s));

    xname.name=NULL;
    xname.len=0;
    xname.index=0;

    while ((opendir->flags & _FUSE_OPENDIR_FLAG_READDIR_FINISH)==0) {

	if (buffer->left <= 0) {
	    int result=0;

	    result=_sftp_get_readdir_names(opendir, &error);

	    if (result==-1) {

		/* some error */

		if (error==EINTR || error==ETIMEDOUT) {

		    set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_INCOMPLETE);

		} else {

		    set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_ERROR);

		}

		break;

	    } else if (result==0) {

		/* no more names from server */

		finish_get_fuse_direntry(opendir);
		break;

	    }

	}

	while (buffer->left>0) {
	    struct ssh_string_s name=SSH_STRING_INIT;

	    /* extract name and attributes from names
		only get the name, do the attr later */

	    read_name_nameresponse_ctx(&context->interface, buffer, &name);

	    logoutput("get_sftp_readdir_thread: got name %.*s", name.len, name.ptr);

	    if ((name.len==1 && strncmp(name.ptr, ".", 1)==0) || (name.len==2 && strncmp(name.ptr, "..", 2)==0)) {
		struct sftp_attr_s attr;

		/* skip the . and .. entries */

		read_attr_nameresponse_ctx(&context->interface, buffer, &attr);

	    } else {

		xname.name=name.ptr;
		xname.len=name.len;
		calculate_nameindex(&xname);
		ce.name=&xname;

		/* create the entry (if not exist already) */

		entry=create_entry_extended_batch(&ce);

		if (! entry) {

		    logoutput_warning("_process_sftp_readdir_preread: entry %.*s not created", name.len, name.ptr);

		} else {

		    if ((* opendir->hidefile)(opendir, entry)==0) {

			/* create a direntry queue for the (later) readdir to fill the buffer to send to the VFS */

			queue_fuse_direntry(opendir, entry);

		    }

		}

	    }

	    next:

	    if (ce.cache.link.type==DATA_LINK_TYPE_SYMLINK) {

		free(ce.cache.link.link.ptr);
		ce.cache.link.link.ptr=NULL;
		ce.cache.link.type=0;

	    }

	}

	/* all names read: check the "eof" boolean */

	if (buffer->eof) finish_get_fuse_direntry(opendir);

    }

    pthread_mutex_lock(opendir->mutex);
    opendir->flags &= ~_FUSE_OPENDIR_FLAG_THREAD;
    if (opendir->flags & _FUSE_OPENDIR_FLAG_RELEASE) dofree=1;
    pthread_mutex_unlock(opendir->mutex);

    if (dofree) {

	free(opendir);

    }

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

void _fs_sftp_opendir(struct fuse_opendir_s *opendir, struct fuse_request_s *f_request, struct pathinfo_s *pathinfo, unsigned int flags)
{
    struct service_context_s *context=(struct service_context_s *) opendir->context;
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    struct directory_s *directory=get_directory(opendir->inode);
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo->len);
    char path[pathlen];

    /* test a full opendir/readdir is required: test entries are deleted and/or created */

    if (directory && directory->synctime.tv_sec>0) {
	struct entry_s *entry=opendir->inode->alias;

	if ((entry->flags & _ENTRY_FLAG_REMOTECHANGED)==0) {

	    /* no entries added and deleted: no need to read all entries again: use cache */

	    logoutput("_fs_sftp_opendir_common: remote directory has not changed since last visit: no full opendir is required");

	    opendir->readdir=_fs_common_virtual_readdir;
	    opendir->readdirplus=_fs_common_virtual_readdirplus;
	    opendir->fsyncdir=_fs_common_virtual_fsyncdir;
	    opendir->releasedir=_fs_common_virtual_releasedir;
	    set_get_fuse_direntry_common(opendir);
	    _fs_common_virtual_opendir(opendir, f_request, flags);
	    return;

	}

    }

    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, path, pathinfo);

    logoutput("_fs_sftp_opendir_common: send opendir %i %s", pathinfo->len, pathinfo->path);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.opendir.path=(unsigned char *) pathinfo->path;
    sftp_r.call.opendir.len=pathinfo->len;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	error=EINTR;
	set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_FINISH);
	goto out;

    }

    if (send_sftp_opendir_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	/* it's possible to let this threat do things here since
	    it goes into a wait state to receive the response from the server
	    and this goes over the network so that gives a little time (interesting: how much ?)*/

	get_sftp_request_timeout_ctx(interface, &timeout);

	opendir->flags |= (_FUSE_OPENDIR_FLAG_IGNORE_XDEV_SYMLINKS | _FUSE_OPENDIR_FLAG_IGNORE_BROKEN_SYMLINKS) ; /* sane flag */
	if (fs_options.sftp.hideflags & _OPTIONS_SFTP_HIDE_FLAG_DOTFILE) opendir->flags |= _FUSE_OPENDIR_FLAG_HIDE_DOTFILES;

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {
		struct fuse_open_out open_out;

		logoutput_base64encoded("_fs_sftp_opendir_common:", (char *) reply->response.handle.name, reply->response.handle.len);

		/* take over handle */
		opendir->handle.name.name=(char *) reply->response.handle.name;
		opendir->handle.name.len=reply->response.handle.len;
		reply->response.handle.name=NULL;
		reply->response.handle.len=0;

		/* here start a thread which gets readdir data from server */

		start_get_sftp_readdir_thread(opendir);

		open_out.fh=(uint64_t) opendir;
		open_out.open_flags=0;
		open_out.padding=0;

		reply_VFS_data(f_request, (char *) &open_out, sizeof(open_out));
		get_current_time(&directory->synctime);
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
	opendir->get_fuse_direntry=get_fuse_direntry_virtual;
	_fs_common_virtual_opendir(opendir, f_request, flags);
	return;

    }

    opendir->error=error;
    if (error==EINTR || error==ETIMEDOUT) {

	set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_INCOMPLETE);

    } else {

	set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_ERROR);

    }

    reply_VFS_error(f_request, error);

}
void _fs_sftp_readdir(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    return _fs_common_virtual_readdir(opendir, r, size, offset);
}

void _fs_sftp_readdirplus(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    reply_VFS_error(r, EOPNOTSUPP);
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
    struct entry_s *entry=opendir->inode->alias;
    struct simple_lock_s wlock;
    struct directory_s *directory=NULL;

    logoutput("_fs_sftp_releasedir");

    set_flag_fuse_opendir(opendir, _FUSE_OPENDIR_FLAG_READDIR_FINISH);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.close.handle=(unsigned char *) opendir->handle.name.name;
    sftp_r.call.close.len=opendir->handle.name.len;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	reply_VFS_error(f_request, EINTR);
	return;

    }

    if (send_sftp_close_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

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

    /* free opendir handle */

    free(opendir->handle.name.name);
    opendir->handle.name.name=NULL;
    opendir->handle.name.len=0;

    /* free cached data */

    clear_fuse_buffer(&opendir->data.buffer);

    if ((opendir->flags & _FUSE_OPENDIR_FLAG_READDIR_INCOMPLETE)==0) {

	if (entry->flags & _ENTRY_FLAG_REMOTECHANGED) entry->flags-=_ENTRY_FLAG_REMOTECHANGED;

	/* remove local entries not found on server */

	directory=get_directory(opendir->inode);

	if (wlock_directory(directory, &wlock)==0) {

	/*
		only check when there are deleted entries:
		- the entries found on server (opendir->created plus the already found opendir->count)
		is not equal to the number of entries in this local directory
	*/

	    if (opendir->count_created + opendir->count_found != get_directory_count(directory)) {
		struct sl_skiplist_s *sl=(struct sl_skiplist_s *) directory->buffer;
		struct inode_s *inode=NULL;
		struct entry_s *entry=NULL;
		struct list_element_s *next=NULL;
		struct list_element_s *list=get_list_head(&sl->header, 0);

		while (list) {

		    next=get_next_element(list);
		    entry=(struct entry_s *)((char *) list - offsetof(struct entry_s, list));

		    inode=entry->inode;
		    if (check_entry_special(inode)) goto next;

		    if (inode->stim.tv_sec < directory->synctime.tv_sec || (inode->stim.tv_sec == directory->synctime.tv_sec && inode->stim.tv_nsec < directory->synctime.tv_nsec)) {

			logoutput("_fs_sftp_releasedir: remove inode %li", inode->st.st_ino);
			queue_inode_2forget(workspace, inode->st.st_ino, FORGET_INODE_FLAG_DELETED, 0);

		    }

		    next:
		    list=next;

		}

	    }

	    unlock_directory(directory, &wlock);

	}

    }

}

void _fs_sftp_opendir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, struct pathinfo_s *pathinfo, unsigned int flags)
{
    _fs_common_virtual_opendir(opendir, r, flags);
}

void _fs_sftp_readdir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    _fs_common_virtual_readdir(opendir, r, size, offset);
}

void _fs_sftp_readdirplus_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, size_t size, off_t offset)
{
    _fs_common_virtual_readdirplus(opendir, r, size, offset);
}

void _fs_sftp_fsyncdir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r, unsigned char datasync)
{
    reply_VFS_error(r, 0);
}

void _fs_sftp_releasedir_disconnected(struct fuse_opendir_s *opendir, struct fuse_request_s *r)
{
    _fs_common_virtual_releasedir(opendir, r);
}

static int _fs_sftp_opendir_root(struct fuse_opendir_s *opendir, struct context_interface_s *interface)
{
    struct pathinfo_s pathinfo={rootpath, strlen(rootpath), 0, 0};
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;
    unsigned int pathlen=(* interface->backend.sftp.get_complete_pathlen)(interface, pathinfo.len);
    char path[pathlen];
    int result=-1;

    logoutput("_fs_sftp_opendir_root: send opendir %i %s", pathinfo.len, pathinfo.path);

    pathinfo.len += (* interface->backend.sftp.complete_path)(interface, path, &pathinfo);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.opendir.path=(unsigned char *) pathinfo.path;
    sftp_r.call.opendir.len=pathinfo.len;
    sftp_r.ptr=NULL;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    if (send_sftp_opendir_ctx(interface, &sftp_r)==0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_HANDLE) {
		struct fuse_open_out open_out;

		/* take over handle */
		opendir->handle.name.name	=	(char *) reply->response.handle.name;
		opendir->handle.name.len	=	reply->response.handle.len;
		reply->response.handle.name	=	NULL;
		reply->response.handle.len	=	0;

		opendir->data.buffer.data=NULL;
		opendir->data.buffer.pos=NULL;
		opendir->data.buffer.size=0;
		opendir->data.buffer.left=0;
		opendir->data.buffer.eof=0;
		opendir->data.buffer.count=0;

		open_out.fh=(uint64_t) opendir;
		open_out.open_flags=0;
		open_out.padding=0;

		result=0;

	    } else if (reply->type==SSH_FXP_STATUS) {

		error=reply->response.status.linux_error;

	    } else {

		error=EPROTO;

	    }

	}

    }

    out:

    opendir->error=error;
    return result;
}

static int _sftp_get_readdir_names_root(struct fuse_opendir_s *opendir, struct context_interface_s *interface, unsigned int *error)
{
    struct sftp_request_s sftp_r;
    int result=-1;

    logoutput("_sftp_get_readdir_names_root");

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.readdir.handle=(unsigned char *) opendir->handle.name.name;
    sftp_r.call.readdir.len=opendir->handle.name.len;
    sftp_r.ptr=NULL;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    clear_fuse_buffer(&opendir->data.buffer);

    if (send_sftp_readdir_ctx(interface, &sftp_r)>0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_NAME) {
		struct name_response_s *names=&reply->response.names;

		logoutput("_sftp_get_readdir_names: reply name");

		/* take the response from server to read the entries from */
		init_fuse_buffer(&opendir->data.buffer, (char *) names->buff, names->size, names->count, names->eof);

		result=(names->buff && names->size>0) ? names->count : 0;
		names->buff = NULL;
		names->size = 0;

	    } else if (reply->type==SSH_FXP_STATUS) {

		logoutput("_sftp_get_readdir_names: reply status");

		if (reply->response.status.linux_error==ENODATA) {

		    *error=0;
		    result=0;

		} else {

		    *error=reply->response.status.linux_error;
		    result=-1;

		}

	    } else {

		*error=EPROTO;
		result=-1;

	    }

	}

    }

    logoutput("_sftp_get_readdir_names: result %i", result);
    return result;

}

static unsigned int _fs_sftp_readdir_root(struct fuse_opendir_s *opendir, struct context_interface_s *interface, size_t size, off_t offset, struct sftp_attr_s *attr, unsigned int *tmp)
{
    unsigned int valid=0;
    size_t pos=0;
    struct name_s xname={NULL, 0, 0};
    char data[size];
    unsigned int error=EIO;
    struct direntry_buffer_s direntries;

    direntries.data=data;
    direntries.pos=data;
    direntries.size=size;
    direntries.left=size;
    direntries.offset=offset;

    while (direntries.left>0) {
	struct fuse_buffer_s *buffer=&opendir->data.buffer;
	struct ssh_string_s name=SSH_STRING_INIT;

	sftp_readdir:

	if (buffer->left <= 0) {
	    int result=0;

	    result=_sftp_get_readdir_names_root(opendir, interface, &error);

	    if (result==-1) {

		/* some error */

		opendir->flags |= _FUSE_OPENDIR_FLAG_READDIR_FINISH;
		break;

	    } else if (result==0) {

		/* no more names from server */

		opendir->flags |= _FUSE_OPENDIR_FLAG_READDIR_FINISH;
		break;

	    }

	}

	read_name_nameresponse_ctx(interface, buffer, &name);

	if (tmp && *tmp==0) {

	    /* read the first longname to determine the length of the first field: the permissions */

	    if (buffer->left>4) {
		char *sep=NULL;

		buffer->pos+=4;
		buffer->left-=4;
		sep=memrchr(buffer->pos, ' ', buffer->left);

		if (sep) *tmp=(unsigned int)(sep - buffer->pos);

		/* test the length of the longname */

		logoutput("_fs_sftp_readdir_common: found length readdir longname %i", *tmp);

	    }

	    read_attr_nameresponse_ctx(interface, buffer, attr);
	    valid=attr->received;

	}

    }

    return valid;

}

void _fs_sftp_releasedir_root(struct fuse_opendir_s *opendir, struct context_interface_s *interface)
{
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    logoutput("_fs_sftp_releasedir_root");

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));

    sftp_r.id=0;
    sftp_r.call.close.handle=(unsigned char *) opendir->handle.name.name;
    sftp_r.call.close.len=opendir->handle.name.len;
    sftp_r.ptr=NULL;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    if (send_sftp_close_ctx(interface, &sftp_r)==0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		/* send ok reply to VFS no matter what the ssh server reports */

		error=0;
		if (reply->response.status.code>0) logoutput_notice("_fs_sftp_releasedir_root: got reply %i:%s when closing dir", reply->response.status.linux_error, strerror(reply->response.status.linux_error));

	    } else {

		error=EPROTO;

	    }

	}

    }

    out:

    /* free opendir handle */

    free(opendir->handle.name.name);
    opendir->handle.name.name=NULL;
    opendir->handle.name.len=0;

    /* free cached data */

    clear_fuse_buffer(&opendir->data.buffer);

}

int test_valid_sftp_readdir(struct context_interface_s *interface, void *ptr, unsigned int *len)
{
    struct fuse_opendir_s opendir;
    int result=0;
    struct sftp_attr_s *attr=(struct sftp_attr_s *) ptr;

    logoutput("test_valid_sftp_readdir");

    memset(&opendir, 0, sizeof(struct fuse_opendir_s));

    if (_fs_sftp_opendir_root(&opendir, interface)==0) {
	unsigned int valid=0;

	readdir:

	valid=_fs_sftp_readdir_root(&opendir, interface, 1024, 0, attr, len);
	if ((opendir.flags & _FUSE_OPENDIR_FLAG_READDIR_FINISH)==0) goto readdir;
	_fs_sftp_releasedir_root(&opendir, interface);
	result=0;

    }

    return result;
}
