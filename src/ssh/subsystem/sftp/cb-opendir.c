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
#include <sys/vfs.h>
#include <pwd.h>
#include <sys/syscall.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "init.h"
#include "attributes-write.h"
#include "send.h"
#include "handle.h"
#include "path.h"

#define SFTP_READDIR_NAMES_SIZE		1024

struct linux_dirent64 {
    ino64_t			d_ino;
    off64_t			d_off;
    unsigned short		d_reclen;
    unsigned char		d_type;
    char			d_name[];
};


static void _sftp_op_readdir(struct sftp_dirhandle_s *dirhandle, struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=dirhandle->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;
    unsigned int valid=SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_MODIFYTIME | SSH_FILEXFER_ATTR_SUBSECOND_TIMES | SSH_FILEXFER_ATTR_CTIME;
    unsigned int error=0;
    char buffer[SFTP_READDIR_NAMES_SIZE];
    unsigned int pos=0;
    unsigned int count=0;
    struct linux_dirent64 *de=NULL;
    unsigned char eof=0;
    int left=0;

    logoutput("_sftp_op_readdir (%i)", (int) gettid());

    if (dirhandle->buffer==NULL) {

	dirhandle->buffer=malloc(dirhandle->size);
	if (dirhandle->buffer==NULL) {

	    status=SSH_FX_FAILURE;
	    goto out;

	}

	dirhandle->pos=0;

    }

    if (dirhandle->read==0) {
	int read=0;

	/* read a batch of dirents */

	read =syscall(SYS_getdents64, dirhandle->handle.fd, (struct linux_dirent64 *) dirhandle->buffer, dirhandle->size);

	if (read==-1) {

	    status=SSH_FX_FAILURE;
	    goto out;

	} else if (read==0) {

	    eof=1;
	    goto finish;

	}

	dirhandle->read=(unsigned int) read;
	left=dirhandle->read;

    }

    getdent:

    if (left>0) {
	struct stat st;

	de=(struct linux_dirent64 *) &dirhandle->buffer[dirhandle->pos];

	logoutput("sftp_op_readdir: found %s", de->d_name);

	if (fstatat(dirhandle->handle.fd, de->d_name, &st, AT_SYMLINK_NOFOLLOW)==0) {

	    /* does it fit? */

	    error=0;
	    pos+=write_readdir_attr(sftp, &buffer[pos], SFTP_READDIR_NAMES_SIZE - pos, de->d_name, strlen(de->d_name), &st, valid, &error);

	    if (error==0) {

		count++;
		dirhandle->pos += de->d_reclen;
		left -= de->d_reclen;
		if (dirhandle->pos < dirhandle->read) goto getdent;
		dirhandle->pos=0;
		dirhandle->read=0;

	    }

	} else {

	    /* fstatat did not find this entry */

	    dirhandle->pos += de->d_reclen;
	    goto getdent;

	}

    } else {

	eof=1;

    }

    finish:

    logoutput("sftp_op_readdir: reply count %i pos %i", count, pos);
    if (reply_sftp_names(sftp, payload->id, count, buffer, pos, eof)==-1) logoutput_warning("sftp_op_readdir: error sending readdir names");

    return;

    out:

    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}

static int send_sftp_dirhandle(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, struct sftp_dirhandle_s *dirhandle)
{
    char bytes[SFTP_HANDLE_SIZE];

    logoutput_info("send_sftp_dirhandle");
    write_sftp_dirhandle(dirhandle, bytes);
    return reply_sftp_handle(sftp, payload->id, bytes, SFTP_HANDLE_SIZE);
}

static void _sftp_op_opendir(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, char *path)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;
    unsigned int error=0;
    struct stat st;
    struct sftp_dirhandle_s *dirhandle=NULL;

    int fd=-1;

    logoutput("_sftp_op_opendir: path %s", path);

    memset(&st, 0, sizeof(struct stat));

    if (lstat(path, &st)==-1) {

	error=errno;

	if (error==ENOENT) {

	    status=SSH_FX_NO_SUCH_FILE;

	} else {

	    status=SSH_FX_FAILURE;

	}

	goto error;

    }

    if (! S_ISDIR(st.st_mode)) {

	status=SSH_FX_NOT_A_DIRECTORY;
	goto error;

    }

    logoutput("_sftp_op_opendir: insert dirhandle");

    dirhandle=insert_sftp_dirhandle(sftp, st.st_dev, st.st_ino, 0);

    if (dirhandle==NULL) {

	status=SSH_FX_FAILURE;
	goto error;

    }

    fd=open(path, O_DIRECTORY);
    error=errno;

    if (fd==-1) {

	logoutput("_sftp_op_opendir: error %i open (%s)", error, strerror(error));
	status=SSH_FX_FAILURE;
	goto error;

    }

    dirhandle->handle.fd=(unsigned int) fd;
    dirhandle->size=SFTP_READDIR_NAMES_SIZE;
    dirhandle->flags=0;
    dirhandle->readdir=_sftp_op_readdir;

    if (send_sftp_dirhandle(sftp, payload, dirhandle)==-1) logoutput("_sftp_op_opendir: error sending handle reply");
    return;

    error:

    logoutput("_sftp_op_opendir: status %i", status);
    if (dirhandle) {
	struct commonhandle_s *handle=&dirhandle->handle;

	(* dirhandle->handle.free)(&handle);

    }

    reply_sftp_status_simple(sftp, payload->id, status);

}

/* SSH_FXP_OPENDIR
    message has the form:
    - byte 				SSH_FXP_OPENDIR
    - uint32				id
    - string				path
    */

void sftp_op_opendir(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_opendir (%i)", (int) gettid());

    /* message should at least have 4 bytes for the path string
	note an empty path is possible */

    if (payload->len>=4) {
	char *buffer=payload->data;
	unsigned int pos=0;
	unsigned int len=0;

	len=get_uint32(&buffer[pos]);
	pos+=4;

	/* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero) */

	if (payload->len >= len + 4) {
	    struct sftp_identity_s *user=&sftp->identity;
	    unsigned int size=get_fullpath_len(user, len, &buffer[pos]); /* get size of buffer for path */
	    char path[size+1];
	    unsigned int error=0;

	    get_fullpath(user, len, &buffer[pos], path);
	    pos+=len;

	    _sftp_op_opendir(sftp, payload, path);
	    return;

	}

    }

    error:

    logoutput("sftp_op_opendir: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

/* SSH_FXP_READDIR
    message has the form:
    - byte 				SSH_FXP_READDIR
    - uint32				id
    - string				handle
    */

void sftp_op_readdir(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_readdir (%i)", (int) gettid());

    /* handle is 16 bytes long, so message is 4 + 16 = 20 bytes */

    if (payload->len >= 4 + SFTP_HANDLE_SIZE) {
	unsigned int len=0;
	char *data=payload->data;

	len=get_uint32(&data[0]);

	if (len==SFTP_HANDLE_SIZE) {
	    unsigned int error=0;
	    struct sftp_dirhandle_s *dirhandle=find_sftp_dirhandle_buffer(sftp, &data[4]);

	    if (dirhandle) {

		(* dirhandle->readdir)(dirhandle, payload);
		return;

	    } else {

		logoutput("sftp_op_readdir: handle not found");

		if (error==EPERM) {

		    /* serious error: client wants to use a handle he has no permissions for */

		    logoutput("sftp_op_readdir: client has no permissions to use handle");
		    goto disconnect;

		}

		status=SSH_FX_INVALID_HANDLE;

	    }

	}

    }

    out:
    reply_sftp_status_simple(sftp, payload->id, status);

    disconnect:
    finish_sftp_subsystem(sftp);

}
