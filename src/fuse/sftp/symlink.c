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
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"
#include "datatypes/ssh-uint.h"

extern struct fs_options_s fs_options;
static char *nullpath='\0';

static unsigned int get_sftp_prefix(struct context_interface_s *interface, struct pathinfo_s *prefix)
{
    unsigned int tmp=0;

    if (interface->backend.sftp.prefix.type==INTERFACE_BACKEND_SFTP_PREFIX_HOME ||
	interface->backend.sftp.prefix.type==INTERFACE_BACKEND_SFTP_PREFIX_CUSTOM) {

	tmp=interface->backend.sftp.prefix.len;
	prefix->path=interface->backend.sftp.prefix.path;
	prefix->len=interface->backend.sftp.prefix.len;

    } else if (interface->backend.sftp.prefix.type==INTERFACE_BACKEND_SFTP_PREFIX_ROOT) {

	prefix->path=NULL;
	prefix->len=0;
	tmp=0;

    }

    return tmp;
}

static void reply_symlink_target(struct fuse_request_s *f_request, char *path, char *target)
{
    char *start_p=path + 1;
    unsigned int len_p=strlen(start_p);
    char *start_t=target + 1;
    unsigned int len_t=strlen(start_t);
    char *sep_p=memchr(start_p, '/', len_p);
    char *sep_t=memchr(start_t, '/', len_t);

    /* look for the common part */

    while (sep_p && sep_t) {

	if ((unsigned int)(sep_p - start_p) != (unsigned int)(sep_t - start_t) || memcmp(start_p, start_t, (unsigned int)(sep_p - start_p))!=0) break;

	/* found common path entry */
	len_p-=(sep_p + 1 - start_p);
	start_p=sep_p+1;
	len_t-=(sep_t + 1 - start_t);
	start_t=sep_t+1;
	sep_p=memchr(start_p, '/', len_p);
	sep_t=memchr(start_t, '/', len_t);

    }

    if (sep_p==NULL && sep_t==NULL) {

	/* both are NULL: basepaths are equal, so in the same directory */

	reply_VFS_data(f_request, start_t, len_t);

    } else if (sep_p && sep_t==NULL) {
	char symlink[2 * len_p  + len_t + 1]; /* big enough */
	char *pos=symlink;

	/* situation:
	    A/B/C -> A/D
	    plan:

	    - count the slashes to reply:
		../D */

	while (sep_p) {

	    memcpy(pos, "../", 3);
	    pos+=3;
	    len_p-=(sep_p + 1 - start_p);
	    start_p=sep_p+1;
	    sep_p=memchr(start_p, '/', len_p);

	}

	/* append the entry name */

	memcpy(pos, start_t, len_t);
	pos+=len_t;
	*pos='\0';
	reply_VFS_data(f_request, symlink, strlen(symlink));

    } else { /* sep_p==NULL && sep_t */

	/* situation:
	    A/B -> A/C/D

	    so the not common part of the target is already the relative symlink:
		C/D */

	reply_VFS_data(f_request, start_t, len_t);

    }

}

/* create the full path (mountpoint + path to share + path to file relative to share 
    to canonicalize and finaly make a relative symlink of it */

static char *check_reply_symlink_target(struct service_context_s *context, char *path, char *target, unsigned char relative, char *resolved)
{
    unsigned int len_p=strlen(path) ;
    unsigned int len_t=strlen(target);
    struct workspace_mount_s *workspace=context->workspace;
    unsigned int pathmax=workspace->pathmax;
    unsigned int len=workspace->mountpoint.len + pathmax + len_p + len_t + 1;
    char fullpath[len]; /* for the full path including fuse mountpoint, network path to share, path and target */
    char fusepath[pathmax]; /* for determing the network path to share */
    struct fuse_path_s fpath;
    char *sep=NULL;
    char *real=NULL;
    unsigned int written=0;

    /* fuse path from mountpoint to the entry where this remote directory is ""mounted"*/

    init_fuse_path(&fpath, fusepath, pathmax);
    fpath.len=get_path_root(context->service.filesystem.inode, &fpath);

    sep=(relative) ? memrchr(path, '/', len_p) : NULL;

    if (sep && (sep>path)) {

	*sep='\0';
	written=snprintf(fullpath, len, "%.*s%.*s%s/%s", workspace->mountpoint.len, workspace->mountpoint.path, fpath.len, fpath.pathstart, path, target);

    } else {

	/* no slash found or absolute path: direct in root of share */
	written=snprintf(fullpath, len, "%.*s%.*s/%s", workspace->mountpoint.len, workspace->mountpoint.path, fpath.len, fpath.pathstart, target);

    }

    logoutput("check_reply_symlink_target: path %s", fullpath);

    /* canonicalize the full path */

    real=realpath(fullpath, resolved);

    logoutput("check_reply_symlink_target: resolved path %s", resolved);

    if (real) {
	unsigned int len_r=strlen(real);

	/* test the path is a subdirectory of the shared directory */

	written=snprintf(fullpath, len, "%.*s%.*s/", workspace->mountpoint.len, workspace->mountpoint.path, fpath.len, fpath.pathstart);

	if (len_r > written && memcmp(real, fullpath, written)==0) {

	    /* move data to beginning */

	    memmove(real, &real[written], len_r - written);
	    memset(&real[len_r - written], '\0', written);
	    errno=0;

	} else {

	    errno=EPERM;
	    free(real);
	    real=NULL;

	}

    } else {

	logoutput("check_reply_symlink_target: realpath failed");
	errno=ENOMEM;

    }

    return real;

}

static unsigned int get_pathmax_system()
{
#ifdef PATH_MAX

    return PATH_MAX;

#else

    int pathmax=pathconf(path, _PC_PATH_MAX);

    if (pathmax <= 0) pathmax=4096;
    return (unsigned int) pathmax;

#endif
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

    // logoutput("_fs_sftp_readlink_common");

    strcpy(origpath, pathinfo->path);
    pathinfo->len += (* interface->backend.sftp.complete_path)(interface, pathinfobuffer, pathinfo);

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.readlink.path=(unsigned char *) pathinfo->path;
    sftp_r.call.readlink.len=pathinfo->len;
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	reply_VFS_error(f_request, EINTR);
	return;

    }

    if (send_sftp_readlink_ctx(interface, &sftp_r)==0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_NAME) {
		struct name_response_s *names=&reply->response.names;
		unsigned int len=get_uint32(names->buff);
		char target[len+1];

		memcpy(target, names->buff + 4, len);
		target[len]='\0';
		free(reply->response.names.buff);
		reply->response.names.buff=NULL;

		logoutput("_fs_sftp_readlink_common: %s target %s", pathinfo->path, target);

		/* server replies with the target of the symlink. Some things are important to note:
		    - if the reply path starts with a slash (=absolute path) this is an absolute path
		    on the server, not on the client. So howto interpret this reply? Only accept the
		    result if it has the same prefix (like the remote homedirectory). Disallowing "outside" shared
		    sftp directories is not upto the client but the server.
		    - if the reply path does not start with a slash (=relative path) check it's not pointing
		    outside the shared directory
		    */

		if (target[0] == '/') {
		    struct pathinfo_s prefix;
		    unsigned int tmp=get_sftp_prefix(interface, &prefix);

		    /* target is absolute path on server: this is not suitable, needed is the path relative to the prefix
			so remove the prefix (if the first part of the target is the prefix) */

		    if (tmp==0 || (tmp<len && strncmp(target, prefix.path, tmp)==0 && target[tmp]=='/')) {
			unsigned int pathmax=get_pathmax_system();
			char resolved[pathmax];

			memset(resolved, 0, pathmax);

			if (check_reply_symlink_target(context, origpath, &target[tmp], 0, resolved)) {

			    reply_symlink_target(f_request, origpath, resolved);
			    return;

			}

			error=errno;

		    } else {

			/* target does not start with the prefix
			    note again that this check does not belong here but on the server */

			reply_VFS_error(f_request, EPERM);

		    }

		} else {
		    unsigned int pathmax=get_pathmax_system();
		    char resolved[pathmax];

		    memset(resolved, 0, pathmax);

		    /* relative to path */

		    if (check_reply_symlink_target(context, origpath, target, 1, resolved)) {

			reply_symlink_target(f_request, origpath, resolved);
			return;

		    }

		    error=errno;

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

}

/* SYMLINK */

void _fs_sftp_symlink(struct service_context_s *context, struct fuse_request_s *f_request, struct entry_s *entry, struct pathinfo_s *pathinfo, const char *target)
{
    struct service_context_s *rootcontext=get_root_context(context);
    struct context_interface_s *interface=&context->interface;
    struct sftp_request_s sftp_r;
    unsigned int error=EIO;

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.symlink.path=(unsigned char *) pathinfo->path;
    sftp_r.call.symlink.len=pathinfo->len;
    sftp_r.call.symlink.target_path=(unsigned char *) target;
    sftp_r.call.symlink.target_len=strlen(target);
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    set_sftp_request_fuse(&sftp_r, f_request);

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {

	reply_VFS_error(f_request, EINTR);
	return;

    }

    if (send_sftp_symlink_ctx(interface, &sftp_r)==0) {
	struct timespec timeout;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(&context->interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_STATUS) {

		if (reply->response.status.code==0) {

		    reply_VFS_error(f_request, 0);
		    return;

		}

		error=reply->response.status.linux_error;

	    } else {

		error=EIO;

	    }

	}

    }

    out:

    queue_inode_2forget(context->workspace, entry->inode->st.st_ino, 0, 0);
    reply_VFS_error(f_request, error);

}

/*
    test the symlink pointing to target is valid
    - a symlink is valid when it stays inside the "root" directory of the shared map: target is a subdirectory of the root
*/

int _fs_sftp_symlink_validate(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, char **p_real)
{
    int result=-1;
    unsigned int error=EIO;

    if (target[0] == '/') {
	struct pathinfo_s prefix;
	unsigned int tmp=get_sftp_prefix(&context->interface, &prefix);
	unsigned int len=strlen(target);

	/* target is absolute path on server: this is not suitable, needed is the path relative to the prefix
	    so remove the prefix (if the first part of the target is the prefix) */

	if (tmp==0 || (tmp<len && strncmp(target, prefix.path, tmp)==0 && target[tmp]=='/')) {
	    unsigned int pathmax=get_pathmax_system();
	    char resolved[pathmax];

	    memset(resolved, 0, pathmax);

	    if (check_reply_symlink_target(context, pathinfo->path, &target[tmp], 0, resolved)) {

		*p_real=strdup(resolved);
		result=0;

	    }

	    error=errno;

	}

    } else {
	unsigned int pathmax=get_pathmax_system();
	char resolved[pathmax];

	memset(resolved, 0, pathmax);

	/* relative to path */

	if (check_reply_symlink_target(context, pathinfo->path, target, 1, resolved)) {

	    *p_real=strdup(resolved);
	    result=0;

	}

	error=errno;

    }

    if (result==-1) {

	logoutput("_fs_sftp_symlink_validate: error %i testing target (%s)", errno, strerror(error));
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

int _fs_sftp_symlink_validate_disconnected(struct service_context_s *context, struct pathinfo_s *pathinfo, char *target, char **remote_target)
{
    return -1;
}
