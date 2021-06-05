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

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "attributes-write.h"
#include "send.h"
#include "path.h"
#include "handle.h"
#include "init.h"

/* SSH_FXP_(L)STAT/
    message has the form:
    - byte 				SSH_FXP_STAT or SSH_FXP_LSTAT
    - uint32				id
    - string				path
    - uint32				flags
    */

static void sftp_op_stat_generic(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, int (* cb_stat)(const char *path, struct stat *st))
{
    unsigned int status=SSH_FX_BAD_MESSAGE;
    char *data=payload->data;

    /* message should at least have 4 bytes for the path string, and 4 for the flags
	note an empty path is possible */

    if (payload->len>=8) {
	char *data=payload->data;
	struct sftp_identity_s *user=&sftp->identity;
	unsigned int pos=0;
	unsigned int len=0;
	unsigned int valid=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len + 8 == payload->len) {
	    struct stat st;
	    int result=0;
	    unsigned int error=0;

	    if (len==0) {

		/* empty path resolves to the users home */

		logoutput_debug("sftp_op_stat_generic: path %s", user->pwd.pw_dir);
		result=cb_stat(user->pwd.pw_dir, &st);
		error=errno;

	    } else if (data[pos]=='/') {
		char path[len+1];

		/* absolute path */
		memcpy(path, &data[pos], len);
		path[len]='\0';
		logoutput("sftp_op_stat_generic: path %s", path);
		result=cb_stat(path, &st);
		error=errno;

	    } else {
		char path[user->len_home + len + 2];
		unsigned int index=0;

		/* relative to users homedirectory */

		memcpy(&path[index], user->pwd.pw_dir, user->len_home);
		index+=user->len_home;
		path[index]='/';
		index++;
		memcpy(&path[index], &data[pos], len);
		index+=len;
		path[index]='\0';
		logoutput("sftp_op_stat_generic: path %s", path);

		result=cb_stat(path, &st);
		error=errno;

	    }

	    pos+=len;
	    valid=get_uint32(&data[pos]);

	    logoutput("sftp_op_stat: result %i valid %i", result, valid);

	    if (result==0) {
		struct sftp_attr_s attr;
		unsigned int size=write_attributes_len(sftp, &attr, &st, valid);
		char buffer[size];

		size=write_attributes(sftp, buffer, size, &attr, valid);

		if (reply_sftp_attrs(sftp, payload->id, buffer, size)==-1) logoutput_warning("sftp_op_stat: error sending attr");
		return;

	    } else {

		status=SSH_FX_FAILURE;
		if (error==0) error=EIO;

		if (error==ENOENT) {

		    status=SSH_FX_NO_SUCH_FILE;

		} else if (error==ENOTDIR) {

		    status=SSH_FX_NO_SUCH_PATH;

		} else if (error==EACCES) {

		    status=SSH_FX_PERMISSION_DENIED;

		}

	    }

	}

    }

    logoutput("sftp_op_stat_generic: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

void sftp_op_stat(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    sftp_op_stat_generic(sftp, payload, stat);
}

void sftp_op_lstat(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    sftp_op_stat_generic(sftp, payload, lstat);
}

/* SSH_FXP_FSTAT/
    message has the form:
    - byte 				SSH_FXP_FSTAT
    - uint32				id
    - string				handle
    - uint32				flags
    */

void sftp_op_fstat(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_fstat");

    /* message should at least have 4 bytes for the path string, and 4 for the flags
	note an empty path is possible */

    if (payload->len>=8) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;
	unsigned int valid=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len + 8 == payload->len && len==SFTP_HANDLE_SIZE) {
	    int result=0;
	    unsigned int error=0;
	    struct sftp_filehandle_s *filehandle=find_sftp_filehandle_buffer(sftp, &data[pos]);

	    if (filehandle) {
		struct sftp_identity_s *user=&sftp->identity;
		struct stat st;

		result=fstat(filehandle->handle.fd, &st);

		pos+=len;
		valid=get_uint32(&data[pos]);

		logoutput("sftp_op_fstat: result %i valid %i", result, valid);

		if (result==0) {
		    struct sftp_attr_s attr;
		    unsigned int size=write_attributes_len(sftp, &attr, &st, valid);
		    char buffer[size];

		    size=write_attributes(sftp, buffer, size, &attr, valid);

		    if (reply_sftp_attrs(sftp, payload->id, buffer, size)==-1) logoutput_warning("sftp_op_fstat: error sending attr");

		    // decrease_filehandle(&filehandle);
		    return;

		} else {

		    status=SSH_FX_FAILURE;
		    if (error==0) error=EIO;

		    if (error==ENOENT) {

			status=SSH_FX_NO_SUCH_FILE;

		    } else if (error==ENOTDIR) {

			status=SSH_FX_NO_SUCH_PATH;

		    } else if (error==EBADF) {

			status=SSH_FX_INVALID_HANDLE;

		    } else if (error==EACCES) {

			status=SSH_FX_PERMISSION_DENIED;

		    }

		}

		//decrease_filehandle(&filehandle);

	    } else {

		if (error==EPERM) {

		    /* serious error: client wants to use a handle he has no permissions for */

		    logoutput("sftp_op_fstat: client has no permissions to use handle");
		    goto disconnect;

		}

		status=SSH_FX_INVALID_HANDLE;

	    }

	}

    }

    logoutput("sftp_op_fstat: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}
