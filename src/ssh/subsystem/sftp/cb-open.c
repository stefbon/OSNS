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
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"

#include "init.h"
#include "attr.h"
#include "send.h"
#include "handle.h"
#include "path.h"

struct local_openmode_s {
    unsigned int			posix_flags;
};

#define LOCAL_OPENMODE_INIT		{0}

struct sftp_openmode_s {
    unsigned int			flags;
    unsigned int			access;
};

#define SFTP_OPENMODE_INIT		{0, 0}

/* translate the access and flags sftp parameters into posix
    do also some sane checking (write access is required for append etc) */

static int translate_sftp2local(struct sftp_openmode_s *openmode, struct local_openmode_s *local, unsigned int *error)
{

    if (openmode->access & ACE4_APPEND_DATA) {

	/* flags must have a bit about how to append */

	if ((openmode->flags & (SSH_FXF_APPEND_DATA | SSH_FXF_APPEND_DATA_ATOMIC))==0) {

	    logoutput_debug("translate_sftp2local: append data without append flags");
	    goto errorinval;

	}

	if ((openmode->access & ACE4_WRITE_DATA)==0) {

	    logoutput_debug("translate_sftp2local: append data without write access");
	    goto errorinval;

	}

	local->posix_flags |= O_APPEND;
	openmode->access &= ~ACE4_APPEND_DATA;
	openmode->flags &= ~(SSH_FXF_APPEND_DATA | SSH_FXF_APPEND_DATA_ATOMIC);

    }

    if ((openmode->access & (ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) ==
	(ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) {

	local->posix_flags |= O_RDWR;
	openmode->access &= ~(ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES);

    } else if ((openmode->access & (ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) ==
	(ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES)) {

	local->posix_flags |= O_WRONLY;
	openmode->access &= ~(ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES);

    } else if ((openmode->access & (ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) ==
	(ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) {

	local->posix_flags |= O_RDONLY;
	openmode->access &= ~(ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES);

    } else {

	logoutput_debug("translate_sftp2local: not enough WRITE or READ access flags");
	goto errorinval;

    }

    if ((openmode->flags & SSH_FXF_ACCESS_DISPOSITION)==0) {

	logoutput_debug("translate_sftp2local: no access flags set");
	goto errorinval;

    } else if (openmode->flags & SSH_FXF_CREATE_TRUNCATE) {

	local->posix_flags |= (O_CREAT | O_TRUNC);
	openmode->flags &= ~SSH_FXF_CREATE_TRUNCATE;

    } else if (openmode->flags & SSH_FXF_CREATE_NEW) {

	local->posix_flags |= (O_CREAT | O_EXCL);
	openmode->flags &= ~SSH_FXF_CREATE_NEW;

    } else if (openmode->flags & SSH_FXF_OPEN_OR_CREATE) {

	local->posix_flags |= (O_CREAT);
	openmode->flags &= ~SSH_FXF_OPEN_OR_CREATE;

    } else if (openmode->flags & SSH_FXF_TRUNCATE_EXISTING) {

	local->posix_flags |= (O_TRUNC);
	openmode->flags &= ~SSH_FXF_TRUNCATE_EXISTING;

    }

    if (openmode->flags>0) logoutput("translate_sftp2local: sftp flags not supported %i", openmode->flags);
    if (openmode->access>0) logoutput("translate_sftp2local: sftp access not supported %i", openmode->access);

    logoutput("translate_sftp2local: posix %i", local->posix_flags);
    return 0;

    errorinval:
    logoutput("translate_sftp2local: received incompatible/incomplete open access and flags (access %i flags %i)", openmode->access, openmode->flags);
    return -1;

}

static unsigned int translate_open_error(unsigned int error)
{
    unsigned int status=SSH_FX_FAILURE;

    switch (error) {

	case ENOENT:

	    status=SSH_FX_NO_SUCH_FILE;
	    break;


	case ENOTDIR:

	    status=SSH_FX_NO_SUCH_PATH;
	    break;

	case EACCES:
	case EPERM:

	    status=SSH_FX_PERMISSION_DENIED;
	    break;

	case EEXIST:

	    status=SSH_FX_FILE_ALREADY_EXISTS;
	    break;

    }

    return status;

}

static unsigned int translate_read_error(unsigned int error)
{
    unsigned int status=SSH_FX_FAILURE;

    switch (error) {

	case EBADF:

	    status=SSH_FX_INVALID_PARAMETER;
	    break;

    }

    return status;
}

static unsigned int translate_write_error(unsigned int error)
{
    unsigned int status=SSH_FX_FAILURE;

    switch (error) {

	case EBADF:

	    status=SSH_FX_PERMISSION_DENIED;
	    break;

	case EDQUOT:

	    status=SSH_FX_QUOTA_EXCEEDED;
	    break;

	case ENOSPC:

	    status=SSH_FX_NO_SPACE_ON_FILESYSTEM;
	    break;

	case EINVAL:

	    status=SSH_FX_WRITE_PROTECT;
	    break;

	case EFBIG:

	    status=SSH_FX_INVALID_PARAMETER;
	    break;

    }

    return status;
}

static void sftp_op_open_existing(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, struct fs_location_s *location, struct sftp_openmode_s *openmode, struct local_openmode_s *local)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;
    int result=0;
    struct system_stat_s st;
    struct system_dev_s dev;
    struct commonhandle_s *handle=NULL;

    logoutput("sftp_op_open_existing: path %.*s", location->type.path.len, location->type.path.ptr);

    memset(&st, 0, sizeof(struct stat));

    /* file must exist, otherwise error */

    result=system_getstat(&location->type.path, SYSTEM_STAT_SIZE, &st);

    if (result<0) {

	result=abs(result);

	if (result==ENOENT) {

	    status=SSH_FX_NO_SUCH_FILE;

	} else {

	    status=SSH_FX_FAILURE;

	}

	goto error;

    }

    if (S_ISDIR(st.sst_mode)) {

	status=SSH_FX_FILE_IS_A_DIRECTORY;
	goto error;

    }

    get_dev_system_stat(&st, &dev);
    handle=create_sftp_filehandle(sftp, INSERTHANDLE_TYPE_OPEN, get_unique_system_dev(&dev), get_ino_system_stat(&st), NULL, openmode->flags, openmode->access);

    if (handle==NULL) {

	goto error;

    } else {
	struct filehandle_s *fh=&handle->type.file;

	if ((* fh->open)(fh, location, local->posix_flags, NULL)==-1) {

	    status=SSH_FX_FAILURE;
	    goto error;

	}

	if (send_sftp_handle(sftp, payload, handle)==-1) logoutput("_sftp_op_opendir: error sending handle reply");
	return;

    }

    error:

    free_commonhandle(&handle);
    if (status==0) status=SSH_FX_FAILURE;
    reply_sftp_status_simple(sftp, payload->id, status);
    logoutput("sftp_op_error_existing: status %i", status);

}

/* SSH_FXP_OPEN
    message has the form:
    - byte 				SSH_FXP_OPEN
    - uint32				id
    - string				path
    - uint32				access
    - uint32				flags
    - ATTRS				attrs
    */

void sftp_op_open(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_open (%i)", (int) gettid());

    /* message should at least have 4 bytes for the path string, and 4 for the access and 4 for the flags
	note an empty path is possible */

    /* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero)
	    - 4       ... access
	    - 4       ... flags
	    - x       ... ATTR (optional, only required when file is created) */

    if (payload->len>=12) {
	char *data=payload->data;
	unsigned int pos=0;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	if (payload->len >= path.len + 12) {
	    struct sftp_identity_s *user=&sftp->identity;
	    struct fs_location_s location;
	    struct convert_sftp_path_s convert=CONVERT_PATH_INIT;
	    unsigned int size=get_fullpath_size(user, &path, &convert); /* get size of buffer for path */
	    char tmp[size+1];
	    struct sftp_openmode_s openmode=SFTP_OPENMODE_INIT;
	    struct local_openmode_s local=LOCAL_OPENMODE_INIT;
	    unsigned int error=0;

	    openmode.access=get_uint32(&data[pos]);
	    pos+=4;
	    openmode.flags=get_uint32(&data[pos]);
	    pos+=4;

	    memset(&location, 0, sizeof(struct fs_location_s));
	    location.flags=FS_LOCATION_FLAG_PATH;
	    set_buffer_location_path(&location.type.path, tmp, size+1, 0);
	    (* convert.complete)(user, &path, &location.type.path);

	    if (translate_sftp2local(&openmode, &local, &error)==-1) {

		status=SSH_FX_PERMISSION_DENIED;
		logoutput("sftp_op_open: error %i translating sftp to posix (%s)", error, strerror(error));
		goto error;

	    }

	    logoutput("sftp_op_open: path %.*s", location.type.path.len, location.type.path.ptr);

	    /* TODO:
	    */

	    if ((local.posix_flags & (O_CREAT | O_EXCL))==(O_CREAT | O_EXCL)) {

		logoutput("sftp_op_open: creating file is not supported");
		status=SSH_FX_OP_UNSUPPORTED;
		goto error;

	    } else {

		sftp_op_open_existing(sftp, payload, &location, &openmode, &local);

	    }

	    return;

	}

    }

    error:

    logoutput("sftp_op_open: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

/* SSH_FXP_READ
    message has the form:
    - byte 				SSH_FXP_READ
    - uint32				id
    - string				handle
    - uint64				offset
    - uint32				length
    */

void sftp_op_read(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_read (%i)", (int) gettid());

    /* message is 4 + 8 + handle size + 4 =  hs + 16 bytes */

    if (payload->len>=get_sftp_handle_size() + 16) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;

	/* read handle: formatted as ssh string */

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_sftp_handle_size()) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, &data[pos], len, &count);
	    struct sftp_subsystem_s *tmp=NULL;
	    uint64_t offset=0;
	    uint32_t size=0;

	    pos+=count;

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle not found");
		goto error;

	    }

	    offset=get_uint64(&data[pos]);
	    pos+=8;
	    size=get_uint32(&data[pos]);
	    pos+=4;

	    /* TODO: handle max-read-size (2K?) */

	    tmp=get_sftp_subsystem_commonhandle(handle);

	    if ((handle->flags & COMMONHANDLE_FLAG_FILE)==0) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle not a file handle");
		goto error;

	    } else if (tmp==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle is not a sftp handle");
		goto error;

	    } else if (tmp != sftp) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle does belong by other sftp server");
		goto error;

	    } else {
		struct filehandle_s *fh=&handle->type.file;
		char buffer[size];
		int bytesread=0;
		unsigned int error=0;

		bytesread=(* fh->pread)(fh, buffer, size, offset);
		error=errno;

		if (bytesread>=0) {
		    unsigned char eof=0;

		    /* when less bytes read then requested check the offset + size is past end of file
			this is usefull for passing the eof parameter to the client */

		    if (bytesread<size) {
			struct system_stat_s st;

			if ((* fh->fgetstat)(fh, SYSTEM_STAT_SIZE, &st)==0) {
			    uint64_t check=get_size_system_stat(&st);

			    if ((offset + size > check) && (offset + bytesread == check)) eof=1;

			}

		    }

		    logoutput("sftp_op_read: size %i off %i read %i bytes ", (int) size, (int) offset, bytesread);
		    if (reply_sftp_data(sftp, payload->id, buffer, bytesread, eof)==-1) logoutput("sftp_op_read: error sending data");
		    return;

		}

		logoutput("sftp_op_read: error %i reading data (%s)", error, strerror(error));
		status=translate_read_error(error);

	    }

	} else {

	    logoutput_warning("sftp_op_read: invalid file handle size (len=%i)", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_read: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}

/* SSH_FXP_WRITE
    message has the form:
    - byte 				SSH_FXP_WRITE
    - uint32				id
    - string				handle
    - uint64				offset
    - string				data
    */

void sftp_op_write(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_write (%i)", (int) gettid());

    /* message is minimal 4 + handle size + 8 + 4 =  hs + 16 bytes */

    if (payload->len>=get_sftp_handle_size() + 16) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_sftp_handle_size()) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, &data[pos], len, &count);
	    struct sftp_subsystem_s *tmp=NULL;
	    uint64_t offset=0;
	    uint32_t size=0;

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_write: handle not found");
		goto error;

	    }

	    pos+=count;

	    offset=get_uint64(&data[pos]);
	    pos+=8;
	    size=get_uint32(&data[pos]);
	    pos+=4;

	    if (size==0) {

		status=SSH_FX_INVALID_PARAMETER;
		goto error;

	    }

	    /* check the size parameter: add a max write size */

	    tmp=get_sftp_subsystem_commonhandle(handle);

	    if ((handle->flags & COMMONHANDLE_FLAG_FILE)==0) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_write: handle not a file handle");
		goto error;

	    } else if (tmp==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_write: handle is not a sftp handle");
		goto error;

	    } else if (tmp != sftp) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_write: handle does belong by other sftp server");
		goto error;

	    } else {
		struct filehandle_s *fh=&handle->type.file;
		int byteswritten=0;

		byteswritten=(* fh->pwrite)(fh, &data[pos], size, offset);
		error=errno;

		/* here: */

		if (byteswritten>0) {

		    reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
		    return;

		}

		if (error==0) error=EIO;
		logoutput("sftp_op_write: error %i writing %i bytes (%s)", error, size, strerror(error));
		status=translate_write_error(error);

	    }

	} else {

	    logoutput_warning("sftp_op_write: invalid file handle size (len=%i)", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_write: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:
    finish_sftp_subsystem(sftp);

}

