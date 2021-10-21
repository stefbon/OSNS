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
#include "connect.h"
#include "attributes-write.h"
#include "attributes-read.h"
#include "send.h"
#include "handle.h"
#include "path.h"

#include "cb-utils.h"

#define _SFTP_OPENACCESS_READ				( ACE4_READ_DATA | ACE4_READ_ATTRIBUTES )
#define _SFTP_OPENACCESS_WRITE				( ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES )
#define _SFTP_OPENACCESS_READWRITE			( ACE4_READ_DATA | ACE4_READ_ATTRIBUTES | ACE4_WRITE_DATA | ACE4_WRITE_ATTRIBUTES )
#define _SFTP_OPENACCESS_APPEND				( ACE4_APPEND_DATA )

/* translate the access and flags sftp parameters into posix
    do also some sane checking (write access is required for append etc) */

static int translate_sftp2posix(unsigned int access, unsigned int flags, unsigned int *posix, unsigned int *error)
{
    int result=0;

    if ((access & _SFTP_OPENACCESS_READ) && (access & _SFTP_OPENACCESS_WRITE)) {

	*posix|=O_RDWR;

	if (access & _SFTP_OPENACCESS_APPEND) {

	    if (flags & (SSH_FXF_APPEND_DATA | SSH_FXF_APPEND_DATA_ATOMIC)) {

		*posix|=O_APPEND;

	    } else {

		*error=EINVAL;
		goto error;

	    }

	}

    } else if (access & _SFTP_OPENACCESS_WRITE) {

	*posix|=O_WRONLY;

	if (access & _SFTP_OPENACCESS_APPEND) {

	    if (flags & (SSH_FXF_APPEND_DATA | SSH_FXF_APPEND_DATA_ATOMIC)) {

		*posix|=O_APPEND;

	    } else {

		*error=EINVAL;
		goto error;

	    }

	}

    } else if (access & _SFTP_OPENACCESS_READ) {
	unsigned int openflags=flags & SSH_FXF_ACCESS_DISPOSITION;

	*posix|=O_RDONLY;

	if (access & _SFTP_OPENACCESS_APPEND) {

	    *error=EINVAL;
	    goto error;

	}

	if (openflags != SSH_FXF_OPEN_EXISTING) {

	    *error=EINVAL;
	    goto error;

	}

    } else {

	*error=EINVAL;
	goto error;

    }

    if (flags & SSH_FXF_ACCESS_DISPOSITION) {

	if (flags & SSH_FXF_CREATE_NEW) {

	    *posix |= (O_EXCL | O_CREAT);

	} else if (flags & SSH_FXF_CREATE_TRUNCATE) {

	    *posix |= (O_CREAT | O_TRUNC);

	} else if (flags & SSH_FXF_OPEN_EXISTING) {

	    /* no additional posix flags ... */

	} else if (flags & SSH_FXF_OPEN_OR_CREATE) {

	    *posix |= O_CREAT;

	} else if (flags & SSH_FXF_TRUNCATE_EXISTING) {

	    *posix |= O_TRUNC;

	}

    } else {

	*error=EINVAL;

    }

    out:
    logoutput("translate_sftp2posix: access %i flags %i posix %i", access, flags, *posix);
    return 0;

    error:
    logoutput("translate_sftp2posix: received incompatible/incomplete open access and flags");
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


    }

    return status;
}

static int send_sftp_handle(struct sftp_subsystem_s *sftp, uint32_t id, struct sftp_filehandle_s *filehandle)
{
    unsigned int size=(unsigned int) get_sftp_handlesize();
    char bytes[size];
    write_sftp_commonhandle(filehandle, bytes, size);
    return reply_sftp_handle(sftp, id, bytes, SFTP_HANDLE_SIZE);
}

/* open of not existing file: create */

static void sftp_op_open_new(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, char *buffer, unsigned int left, char *path, unsigned int sftpaccess, unsigned int sftpflags, unsigned int posix)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;
    unsigned int valid=0;
    unsigned int error=0;
    struct stat st;
    struct sftp_attr_s attr;
    struct commonhandle_s *handle=NULL;
    struct insert_filehandle_s result;
    int fd=0;
    dev_t dev=0;
    uint64_t ino=0;
    char *sep=NULL;
    char *name=NULL;

    sep=memrchr(path, '/', strlen(path));
    if (sep) {

	*sep='\0';
	name=sep+1;

	if (stat(path, &st)==0 && S_ISDIR(st.st_mode)) {

	    dev=st.st_dev;
	    ino=st.st_ino;
	    *sep='/';

	} else {

	    /* set status to NO_SUCH_FILE_OR_DIRECTORY */
	    *sep='/';
	    goto error;

	}

    } else {

	goto error;

    }

    memset(&result, 0, sizeof(struct insert_filehandle_s));
    memset(&attr, 0, sizeof(struct sftp_attr_s));
    memset(&st, 0, sizeof(struct stat));

    st.st_uid=user->pwd.pw_uid; /* sane default */
    st.st_gid=user->pwd.pw_gid; /* sane default */
    st.st_mode=S_IFREG | (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); /* default file with perm 0666 -> make this configurable */
    st.st_size=0; /* default zero size */

    /* read the attr */

    if (read_attributes_v06(sftp, buffer, left, &attr)==0) {

	status=SSH_FX_INVALID_PARAMETER;
	logoutput("sftp_op_open_new: error reading attributes");
	goto error;

    }

    if (attr.valid[SFTP_ATTR_INDEX_USERGROUP]==1) {

	/* check the user and/or group are reckognized
	    and if set to the connecting user they do not have to be set
	    since they are set automatically */

	if (attr.flags & SFTP_ATTR_FLAG_USERNOTFOUND) {

	    status=SSH_FX_OWNER_INVALID;
	    logoutput("sftp_op_open_new: error user not found");
	    goto error;

	} else if (attr.flags & SFTP_ATTR_FLAG_VALIDUSER) {

	    if (attr.uid==user->pwd.pw_uid) attr.flags -= SFTP_ATTR_FLAG_VALIDUSER;

	}

	if (attr.flags & SFTP_ATTR_FLAG_GROUPNOTFOUND) {

	    status=SSH_FX_GROUP_INVALID;
	    logoutput("sftp_op_open_new: error group not found");
	    goto error;

	} else if (attr.flags & SFTP_ATTR_FLAG_VALIDGROUP) {

	    if (attr.gid==user->pwd.pw_gid) attr.flags -= SFTP_ATTR_FLAG_VALIDGROUP;

	}

	if (!(attr.flags & (SFTP_ATTR_FLAG_VALIDGROUP | SFTP_ATTR_FLAG_VALIDUSER))) attr.valid[SFTP_ATTR_INDEX_USERGROUP]=0;

    }

    if (attr.valid[SFTP_ATTR_INDEX_PERMISSIONS]) {

	st.st_mode=attr.type | attr.permissions;
	attr.valid[SFTP_ATTR_INDEX_PERMISSIONS]=0; /* not needed futher afterwards by the setstat */

	if (! S_ISREG(st.st_mode)) {

	    status=(st.st_mode>0) ? SSH_FX_FILE_IS_A_DIRECTORY : SSH_FX_PERMISSION_DENIED;
	    logoutput("sftp_op_open_new: error type not file (%i)", (int) st.st_mode);
	    goto error;

	}

    }

    handle=init_sftp_filehandle(sftp, INSERTHANDLE_TYPE_CREATE, dev, ino, name);
    if (handle==NULL) {

	status=result.status; /* possibly additional information in result about locking for example */
	goto error;

    }

    set_sftp_handle_access(sftp, sftpaccess);
    set_sftp_handle_flags(sftp, sftpflags);

    if (insert_sftp_filehandle(handle, &result)==-1) {

	/* TODO: more info in result */
	logoutput("sftp_op_open_new: unable to insert filehandle ...");
	goto error'

    }

    fd=(* handle->open)(handle, path, posix, st.st_mode);
    error=errno;

    if (fd<0) {

	logoutput("sftp_op_open_new: unable to create %s error %i (%s)", path, errno, strerror(errno));
	goto error'

    }

    /* when size is zero ignore it */
    if (attr.valid[SFTP_ATTR_INDEX_SIZE]==1 && attr.size==0) attr.valid[SFTP_ATTR_INDEX_SIZE]=0;

    if (attr.valid[SFTP_ATTR_INDEX_PERMISSIONS] | attr.valid[SFTP_ATTR_INDEX_SIZE] | attr.valid[SFTP_ATTR_INDEX_USERGROUP] | 
	attr.valid[SFTP_ATTR_INDEX_ATIME] | attr.valid[SFTP_ATTR_INDEX_MTIME]) {

	/* set stat values: some protection to make it atomic... TODO */

	if (_setstat_fd(fd, &attr, &error)==-1) {

	    /* remove the file: protection? */

	    logoutput("sftp_op_open: error setting stat values");
		close(fd);
		unlink(path);
		status=SSH_FX_FAILURE;
		goto error;

	    }

	}

	memset(&test, 0, sizeof(struct stat));

	if (fstat(fd, &test)==0) {

	    complete_create_filehandle(filehandle, test.st_dev, test.st_ino, fd);

	} else {

	    error=errno;
	    status=SSH_FX_FAILURE;
	    status=translate_open_error(error);
	    goto error;

	}

    } else {

	status=SSH_FX_FAILURE;
	if (error==0) error=EIO;
	status=translate_open_error(error);
	goto error;

    }

    if (send_sftp_handle(sftp, payload->id, filehandle)==-1) logoutput("sftp_op_open_new: error sending handle reply");
    return;
    

    error:

    if (fd>=0) {

	if (handle) {

	    (* hande->close)(handle);

	} else {

	    close(fd);

	}

    }

    remove_filehandle(&handle);
    if (status==0) status=SSH_FX_FAILURE;
    logoutput("sftp_op_error_new: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

void sftp_op_open_existing(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, char *path, unsigned int sftp_access, unsigned int sftp_flags, unsigned int posix)
{
    struct sftp_identity_s *user=&sftp->identity;
    unsigned int status=0;
    unsigned int valid=0;
    unsigned int error=0;
    struct stat st;
    int fd=0;
    struct sftp_filehandle_s *filehandle=NULL;
    struct insert_filehandle_s result;

    memset(&result, 0, sizeof(struct insert_filehandle_s));
    memset(&st, 0, sizeof(struct stat));

    /* file must exist, otherwise error */

    if (lstat(path, &st)==-1) {

	error=errno;

	if (error==ENOENT) {

	    status=SSH_FX_NO_SUCH_FILE;

	} else {

	    status=SSH_FX_FAILURE;

	}

	goto error;

    }

    if (S_ISDIR(st.st_mode)) {

	status=SSH_FX_FILE_IS_A_DIRECTORY;
	goto error;

    }

    result.server.sftp.access=sftp_access;
    result.server.sftp.flags=sftp_flags;

    filehandle=start_insert_filehandle(sftp, st.st_dev, st.st_ino, &result);

    if (filehandle==NULL) {

	status=result.status; /* possibly additional information in result about locking for example */
	goto error;

    }

    logoutput("sftp_op_open_existing: open path %s", path);

    fd=open(path, posix);
    error=errno;

    if (fd==-1) {

	status=translate_open_error(error);
	goto error;

    }

    filehandle->handle.fd=(unsigned int) fd;
    logoutput("sftp_op_open_existing: fd %i", fd);
    if (send_sftp_handle(sftp, payload, filehandle)==-1) logoutput("sftp_op_open_existing: error sending handle reply");
    return;

    error:

    remove_filehandle(&filehandle);
    if (status==0) status=SSH_FX_FAILURE;
    logoutput("sftp_op_error_existing: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

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

    if (payload->len>=12) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	/* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero)
	    - 4       ... access
	    - 4       ... flags
	    - x       ... ATTR (optional, only required when file is created) */

	if (payload->len >= len + 12) {
	    struct sftp_identity_s *user=&sftp->identity;
	    unsigned int size=get_fullpath_len(user, len, &data[pos]); /* get size of buffer for path */
	    char path[size+1];
	    unsigned int sftp_access=0;
	    unsigned int sftp_flags=0;
	    unsigned int posix=0;
	    unsigned int error=0;

	    get_fullpath(user, len, &data[pos], path);
	    pos+=len;
	    sftp_access=get_uint32(&data[pos]);
	    pos+=4;
	    sftp_flags=get_uint32(&data[pos]);
	    pos+=4;

	    if (translate_sftp2posix(sftp_access, sftp_flags, &posix, &error)==-1) {

		status=SSH_FX_PERMISSION_DENIED;
		logoutput("sftp_op_open: error %i translating sftp to posix (%s)", error, strerror(error));
		goto error;

	    }

	    /* TODO:
	    */

	    if (posix & O_CREAT) {

		sftp_op_open_new(sftp, payload, &data[pos], payload->len - pos, path, sftp_access, sftp_flags, posix);

	    } else {

		sftp_op_open_existing(sftp, payload, path, sftp_access, sftp_flags, posix);

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

    if (payload->len>=SFTP_HANDLE_SIZE + 16) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;

	/* read handle: formatted as ssh string */

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==SFTP_HANDLE_SIZE) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, &data[pos], &count);
	    uint64_t offset=0;
	    uint32_t size=0;

	    pos+=count;

	    offset=get_uint64(&data[pos]);
	    pos+=8;
	    size=get_uint32(&data[pos]);
	    pos+=4;

	    /* TODO: handle max-read-size (2K?) */

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle not found");

	    } else if ((handle->flags & COMMONHANDLE_FLAG_FILE)==0) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle not a file handle");

	    /* TODO: additional checks about handle, does is belong to this sftp subsystem and pid ?*/

	    } else if (handle->pid != getpid()) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_read: handle owned by another process");

	    } else {
		char buffer[size];
		int bytesread=0;
		unsigned int error=0;

		bytesread=(*handle->type.filehandle.pread)(handle, buffer, size, offset);
		error=errno;

		if (bytesread>=0) {

		    logoutput("sftp_op_read: size %i off %i read %i bytes ", (int) size, (int) offset, bytesread);

		    if (reply_sftp_data(sftp, payload->id, buffer, bytesread, 0)==-1) logoutput("sftp_op_read: error sending data");
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

    out:

    logoutput("sftp_op_read: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:

    finish_session(sftp);

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

    if (payload->len>=SFTP_HANDLE_SIZE + 16) {
	char *data=payload->data;
	unsigned int pos=0;
	unsigned int len=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==SFTP_HANDLE_SIZE) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, &data[pos], &count);
	    uint64_t offset=0;
	    uint32_t size=0;

	    pos+=16;

	    offset=get_uint64(&buffer[pos]);
	    pos+=8;
	    size=get_uint32(&buffer[pos]);
	    pos+=4;

	    /* check the size parameter: add a max write size */

	    if (size==0) {

		status=SSH_FX_INVALID_PARAMETER;
		goto error;

	    }

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_write: handle not found");

	    } else if ((handle->flags & COMMONHANDLE_FLAG_FILE)==0) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_write: handle not a file handle");

	    /* TODO: additional checks about handle, does is belong to this sftp subsystem and pid ?*/

	    } else if (handle->pid != getpid()) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_write: handle owned by another process");

	    } else {
		int byteswritten=0;

		byteswritten=(* handle->type.filehandle.pwrite)(filehandle->handle.fd, &data[pos], size, offset);
		error=errno;

		if (count>0) {

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
    finish_session(sftp);

}

