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
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

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

    } else if ((openmode->access & (ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) ==
	(ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES)) {

	local->posix_flags |= O_WRONLY;

    } else if ((openmode->access & (ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) ==
	(ACE4_READ_DATA | ACE4_READ_ATTRIBUTES)) {

	local->posix_flags |= O_RDONLY;

    } else {

	logoutput_debug("translate_sftp2local: not enough WRITE or READ access flags");
	goto errorinval;

    }

    openmode->access &= ~(ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES | ACE4_READ_DATA | ACE4_READ_ATTRIBUTES);

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

    if (openmode->flags>0) logoutput_debug("translate_sftp2local: sftp flags not supported %i", openmode->flags);
    if (openmode->access>0) logoutput_debug("translate_sftp2local: sftp access not supported %i", openmode->access);
    logoutput_debug("translate_sftp2local: posix %i", local->posix_flags);
    return 0;

    errorinval:
    logoutput_debug("translate_sftp2local: received incompatible/incomplete open access and flags (access %i flags %i)", openmode->access, openmode->flags);
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

static void sftp_op_open_existing(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data, struct fs_path_s *path, struct sftp_openmode_s *openmode, struct local_openmode_s *local)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;
    int result=0;
    struct system_stat_s st;
    struct system_dev_s dev;
    struct fs_handle_s *handle=NULL;
    struct fs_socket_s *sock=NULL;

    logoutput_debug("sftp_op_open_existing: path %.*s", fs_path_get_length(path), fs_path_get_start(path));
    memset(&st, 0, sizeof(struct stat));

    /* file must exist, otherwise error */

    result=system_getstat(path, SYSTEM_STAT_SIZE, &st);

    if (result<0) {

	result=abs(result);

	if (result==ENOENT) {

	    status=SSH_FX_NO_SUCH_FILE;

	} else {

	    status=SSH_FX_FAILURE;

	}

	goto error;

    }

    if (system_stat_test_ISDIR(&st)) {

	status=SSH_FX_FILE_IS_A_DIRECTORY;
	goto error;

    }

    get_dev_system_stat(&st, &dev);
    handle=sftp_create_fs_handle(sftp, get_unique_system_dev(&dev), get_ino_system_stat(&st), openmode->flags, openmode->access, "file");

    if (handle==NULL) {

        status=SSH_FX_FAILURE;
        goto error;

    }

    sock=&handle->socket;

    if ((* sock->ops.open)(NULL, path, sock, local->posix_flags, NULL)==-1) {

	status=SSH_FX_FAILURE;
	goto error;

    }

    if (send_sftp_handle(sftp, inh, handle)==-1) logoutput_warning("_sftp_op_open_existing: error sending handle reply");
    return;

    error:

    free_fs_handle(&handle);
    if (status==0) status=SSH_FX_FAILURE;
    reply_sftp_status_simple(sftp, inh->id, status);
    logoutput_debug("sftp_op_open_existing: status %i", status);

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

void sftp_op_open(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput_debug("sftp_op_open: (tid %u)", gettid());

    /* message should at least have 4 bytes for the path string, and 4 for the access and 4 for the flags
	note an empty path is possible */

    /* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero)
	    - 4       ... access
	    - 4       ... flags
	    - x       ... ATTR (optional, only required when file is created) */

    if (inh->len>=12) {
	unsigned int pos=0;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	if (inh->len >= path.len + 12) {
	    struct fs_path_s location=FS_PATH_INIT;
	    struct convert_sftp_path_s convert=CONVERT_PATH_INIT;
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert); /* get size of buffer for path */
	    char tmp[size+1];
	    struct sftp_openmode_s openmode=SFTP_OPENMODE_INIT;
	    struct local_openmode_s local=LOCAL_OPENMODE_INIT;
	    unsigned int error=0;

	    openmode.access=get_uint32(&data[pos]);
	    pos+=4;
	    openmode.flags=get_uint32(&data[pos]);
	    pos+=4;

	    fs_path_assign_buffer(&location, tmp, size+1);
	    (* convert.complete)(sftp, &path, &location);

	    if (translate_sftp2local(&openmode, &local, &error)==-1) {

		status=SSH_FX_PERMISSION_DENIED;
		logoutput_debug("sftp_op_open: error %i translating sftp to posix (%s)", error, strerror(error));
		goto error;

	    }

	    if ((local.posix_flags & (O_CREAT | O_EXCL))==(O_CREAT | O_EXCL)) {

		logoutput_debug("sftp_op_open: creating file is not supported");
		status=SSH_FX_OP_UNSUPPORTED;
		goto error;

	    } else {

		sftp_op_open_existing(sftp, inh, data, &location, &openmode, &local);

	    }

	    return;

	}

    }

    error:

    logoutput_debug("sftp_op_open: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);

}

/* SSH_FXP_READ
    message has the form:
    - byte 				SSH_FXP_READ
    - uint32				id
    - string				handle
    - uint64				offset
    - uint32				length
    */

void sftp_op_read(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput_debug("sftp_op_read (tid %u)", gettid());

    /* message is 4 + 8 + handle buffer size + 4 =  handle buffer size + 16 bytes */

    if (inh->len>=get_fs_handle_buffer_size() + 16) {
	unsigned int pos=0;
	unsigned int len=0;

	/* read handle: formatted as ssh string */

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_fs_handle_buffer_size()) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct fs_handle_s *handle=NULL;
	    struct sftp_subsystem_s *tmp=NULL;
	    uint64_t offset=0;
	    uint32_t size=0;

            handle=get_fs_handle(sftp->connection.unique, &data[pos], len, &count);

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle not found");
		goto error;

	    }

	    pos+=count;
	    offset=get_uint64(&data[pos]);
	    pos+=8;
	    size=get_uint32(&data[pos]);
	    pos+=4;

	    /* TODO: handle max-read-size from sftp config */

            if (size <= 16384) {
                struct fs_socket_s *sock=&handle->socket;
		char buffer[size];
		int bytesread=0;
		unsigned int errcode=0;

		bytesread=(* sock->ops.type.file.pread)(sock, buffer, size, offset);
		errcode=errno;

		if (bytesread>=0) {
		    unsigned char eof=0;

		    /* when less bytes read then requested check the offset + size is past end of file
			this is usefull for passing the eof parameter to the client */

		    if (bytesread<size) {
			struct system_stat_s st;

			if ((* sock->ops.fgetstat)(sock, SYSTEM_STAT_SIZE, &st)==0) {
			    uint64_t filesize=get_size_system_stat(&st);

			    if ((offset + size > filesize) && (offset + bytesread == filesize)) eof=1;

			}

		    }

		    if (reply_sftp_data(sftp, inh->id, buffer, bytesread, eof)==-1) {

                        logoutput_warning("sftp_op_read: error sending %u bytes (offset %lu size %lu)", bytesread, offset, size);

                    } else {

		        logoutput_debug("sftp_op_read: send %lu bytes offset %lu size %lu", bytesread, offset, size);

                    }

		    return;

		}

		logoutput_debug("sftp_op_read: error %i reading data (%s)", errcode, strerror(errcode));
		status=translate_read_error(errcode);

	    }

	} else {

	    logoutput_warning("sftp_op_read: invalid file handle size (len=%i)", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_read: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);
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

void sftp_op_write(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput_debug("sftp_op_write (%i)", (int) gettid());

    /* message is minimal 4 + handle size + 8 + 4 =  hs + 16 bytes */

    if (inh->len>=get_fs_handle_buffer_size() + 16) {
	unsigned int pos=0;
	unsigned int len=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_fs_handle_buffer_size()) {
	    unsigned int count=0;
	    struct fs_handle_s *handle=NULL;
	    uint64_t offset=0;
	    uint32_t size=0;

            handle=get_fs_handle(sftp->connection.unique, &data[pos], len, &count);

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

	    } else {
		struct fs_socket_s *sock=&handle->socket;
		int byteswritten=0;
	        unsigned int errcode=0;

		byteswritten=(* sock->ops.type.file.pwrite)(sock, &data[pos], size, offset);
		errcode=errno;

		/* here: */

		if (byteswritten>0) {

		    reply_sftp_status_simple(sftp, inh->id, SSH_FX_OK);
		    return;

		}

		if (errcode==0) errcode=EIO;
		logoutput_debug("sftp_op_write: error %i writing %i bytes (%s)", errcode, size, strerror(errcode));
		status=translate_write_error(errcode);

	    }

	} else {

	    logoutput_warning("sftp_op_write: invalid file handle size (len=%i)", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_write: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);
    return;

    disconnect:
    finish_sftp_subsystem(sftp);

}

