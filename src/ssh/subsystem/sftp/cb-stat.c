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

static void sftp_op_stat_generic(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, int (* cb_stat)(char *path, unsigned int mask, struct system_stat_s *s))
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
	char *path=NULL;

	len=get_uint32(&data[pos]);
	pos+=4;
	path=&data[pos];
	pos+=len;
	valid=get_uint32(&data[pos]);

	if (len + 8 == payload->len) {
	    struct system_stat_s st;
	    int result=0;
	    unsigned int error=0;
	    unsigned int mask=translate_sftp_valid_2_statx_mask(valid);

	    if (len==0) {

		/* empty path resolves to the users home */

		logoutput_debug("sftp_op_stat_generic: path %s", user->pwd.pw_dir);
		result=(* cb_stat)(user->pwd.pw_dir, mask, &st);
		error=errno;

	    } else if (path[0]=='/') {
		char tmp[len+1];

		/* absolute path */
		memcpy(tmp, &path[0], len);
		tmp[len]='\0';
		logoutput("sftp_op_stat_generic: path %s", tmp);
		result=(* cb_stat)(tmp, mask, &st);
		error=errno;

	    } else {
		char tmp[user->len_home + len + 2];
		unsigned int index=0;

		/* relative to users homedirectory */

		memcpy(&tmp[index], user->pwd.pw_dir, user->len_home);
		index+=user->len_home;
		tmp[index]='/';
		index++;
		memcpy(&tmp[index], &path[0], len);
		index+=len;
		tmp[index]='\0';
		logoutput("sftp_op_stat_generic: path %s", tmp);

		result=(* cb_stat)(tmp, mask, &stx);
		error=errno;

	    }

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
    sftp_op_stat_generic(sftp, payload, system_getstat);
}

void sftp_op_lstat(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    sftp_op_stat_generic(sftp, payload, system_getlstat);
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

    /* message should at least have 4 bytes for the handle string, and 4 for the flags */

    if (payload->len>8) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;
	unsigned int valid=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len + 8 <= payload->len && len==BUFFER_HANDLE_SIZE) {
	    int result=0;
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle_buffer(sftp, &data[pos], &count);

	    if (handle) {
		struct sftp_identity_s *user=&sftp->identity;
		struct statx stx;
		unsigned int mask=0;

		pos+=count;
		valid=get_uint32(&data[pos]);
		mask=translate_sftp_valid_2_statx_mask(valid);

		result=(* handle->type.filehandle.fstat)(handle, mask, &stx);

		logoutput("sftp_op_fstat: result %i valid %i", result, valid);

		if (result==0) {
		    struct sftp_attr_s attr;
		    unsigned int size=write_attributes_len(sftp, &attr, &stx, valid);
		    char buffer[size];

		    size=write_attributes(sftp, buffer, size, &attr, valid);
		    if (reply_sftp_attrs(sftp, payload->id, buffer, size)==-1) logoutput_warning("sftp_op_fstat: error sending attr");

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

int sftp_read_setstat_attr(struct sftp_subsystem_s *sftp, char *buffer, unsigned int left, struct sftp_attr_s *attr, struct stat *st)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;

    /* read the attr */

    if (read_attributes_v06(sftp, buffer, left, attr)==0) {

	status=SSH_FX_INVALID_PARAMETER;
	logoutput("sftp_read_setstat_attr: error reading attributes");
	goto error;

    }

    if (attr->valid[SFTP_ATTR_INDEX_USERGROUP]==1) {

	/* check the user and/or group are reckognized
	    and if set to the connecting user they do not have to be set
	    since they are set automatically */

	if (attr->flags & SFTP_ATTR_FLAG_USERNOTFOUND) {

	    status=SSH_FX_OWNER_INVALID;
	    logoutput("sftp_read_setstat_attr: error user not found");
	    goto error;

	} else if (attr->flags & SFTP_ATTR_FLAG_VALIDUSER) {

	    if (attr->uid==user->pwd.pw_uid) attr->flags -= SFTP_ATTR_FLAG_VALIDUSER;

	}

	if (attr->flags & SFTP_ATTR_FLAG_GROUPNOTFOUND) {

	    status=SSH_FX_GROUP_INVALID;
	    logoutput("sftp_read_setstat_attr: error group not found");
	    goto error;

	} else if (attr->flags & SFTP_ATTR_FLAG_VALIDGROUP) {

	    if (attr->gid==user->pwd.pw_gid) attr->flags -= SFTP_ATTR_FLAG_VALIDGROUP;

	}

	if (!(attr->flags & (SFTP_ATTR_FLAG_VALIDGROUP | SFTP_ATTR_FLAG_VALIDUSER))) attr->valid[SFTP_ATTR_INDEX_USERGROUP]=0;

    }

    if (attr->valid[SFTP_ATTR_INDEX_PERMISSIONS]) {

	st.st_mode=attr.type | attr.permissions;
	attr.valid[SFTP_ATTR_INDEX_PERMISSIONS]=0; /* not needed futher afterwards by the setstat */

	if (! S_ISREG(st.st_mode)) {

	    status=(st.st_mode>0) ? SSH_FX_FILE_IS_A_DIRECTORY : SSH_FX_PERMISSION_DENIED;
	    logoutput("sftp_read_setstat_attr: error type not file (%i)", (int) st.st_mode);
	    goto error;

	}

    }

/* SSH_FXP_FSETSTAT/
    message has the form:
    - byte 				SSH_FXP_FSETSTAT
    - uint32				id
    - string				handle
    - ATTR				attrs
    */

void sftp_op_fsetstat(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_fsetstat");

    /* message should at least have 4 bytes for the handle string, and 4 for the attr */

    if (payload->len>8) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;
	unsigned int valid=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len + 8 <= payload->len && len==BUFFER_HANDLE_SIZE) {
	    int result=0;
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle_buffer(sftp, &data[pos], &count);

	    if (handle) {
		struct sftp_identity_s *user=&sftp->identity;
		struct system_stat_s st;
		unsigned int mask=0;
		struct sftp_attr_s attr;

		pos+=count;

		memset(&attr, 0, sizeof(struct sftp_attr_s));
		memset(&st, 0, sizeof(struct system_stat_s));

		/* read attributes */

		if ((* sftp->attr_ops.read_attributes)(sftp, &data[pos], payload->len - pos, &attr)==0) {

		    status=SSH_FX_INVALID_PARAMETER;
		    logoutput("sftp_op_fsetstat: error reading attributes");
		    goto out;

		}


		mask=translate_sftp_valid_2_statx_mask(valid);
		result=(* handle->type.filehandle.fstat)(handle, mask, &stx);

		logoutput("sftp_op_fstat: result %i valid %i", result, valid);

		if (result==0) {
		    struct sftp_attr_s attr;
		    unsigned int size=write_attributes_len(sftp, &attr, &stx, valid);
		    char buffer[size];

		    size=write_attributes(sftp, buffer, size, &attr, valid);
		    if (reply_sftp_attrs(sftp, payload->id, buffer, size)==-1) logoutput_warning("sftp_op_fstat: error sending attr");

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

    out:

    logoutput("sftp_op_fstat: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}
