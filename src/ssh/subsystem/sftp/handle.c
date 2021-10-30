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
#include <grp.h>

#include <linux/kdev_t.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"
#include "system.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "handle.h"

/*
    every handle has the form:
    dev || inode || pid || fd || type

    - 4 bytes				dev
    - 8 bytes				inode
    - 4 bytes				pid
    - 4 bytes				fd
    - 1 byte				type

    ---------
    21 bytes

    - encrypt and decrypt? or hash with some key?

    better:

    - 1 byte				length excluding this byte self
    - 1 byte				version
    - 1 byte				padding [n]
    - 4 bytes				dev
    - 8 bytes				ino
    - 4 bytes				pid
    - 4 bytes				fd
    - 1 byte				type
    - n bytes				random 

*/

#define BUFFER_HANDLE_SIZE				21

#ifndef SSH_FXF_BLOCK_READ

#define SSH_FXF_BLOCK_READ              	0x00000040
#define SSH_FXF_BLOCK_WRITE             	0x00000080
#define SSH_FXF_BLOCK_DELETE            	0x00000100
#define SSH_FXF_BLOCK_ADVISORY			0x00000200
#define SSH_FXF_DELETE_ON_CLOSE         	0x00000800

#endif

struct sftp_handle_s {
    struct sftp_subsystem_s					*sftp;
    union _sftp_type_u {
	struct sftp_filehandle_s {
	    int							access;
	    int							flags;
	} filehandle;
	struct sftp_dirhandle_s {
	    unsigned int					valid;
	} dirhandle;
    } type;
};

static unsigned int translate_sftp_flags_2_commonshared(unsigned int sftpflags)
{
    unsigned int result=0;

    /* translate the sftp flags into shared ones
	- only the block flags for now, possible candidates are:
	    DELETE_ON_CLOSE
	    APPEND_DATA (atomic)
	    TEXTMODE
	    TRUNCATE
	    CREATE
	    */

    if (sftpflags & SSH_FXF_BLOCK_READ) {

	result |= FILEHANDLE_BLOCK_READ;

    }

    if (sftpflags & SSH_FXF_BLOCK_WRITE) {

	result |= FILEHANDLE_BLOCK_WRITE;

    }

    if (sftpflags & (SSH_FXF_BLOCK_DELETE | SSH_FXF_DELETE_ON_CLOSE)) {

	result |= FILEHANDLE_BLOCK_DELETE;

    }

    return result;
}

static unsigned int translate_sftp_access_2_commonshared(unsigned int sftpaccess)
{
    return sftpaccess; /* same for protocol 6 at least (see: ... )*/
}

/* FIND */

struct sftp_subsystem_s *get_sftp_subsystem_commonhandle(struct commonhandle_s *handle)
{

    logoutput_debug("get_sftp_subsystem_commonhandle: handle flags %i", handle->flags);

    if (handle->flags & COMMONHANDLE_FLAG_SFTP) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	logoutput_debug("get_sftp_subsystem_commonhandle: is sftp");

	return sftp_handle->sftp;

    }

    return NULL;
}

static int compare_subsystem_sftp(struct commonhandle_s *handle, void *ptr)
{
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *) ptr;
    return (get_sftp_subsystem_commonhandle(handle)==sftp) ? 0 : -1;
}

struct commonhandle_s *find_sftp_commonhandle(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *p_count)
{
    unsigned char pos=0; /* unsigned char is enough for buffer size */
    dev_t dev=0;
    uint64_t ino=0;
    unsigned int pid=0;
    unsigned int fd=0;
    unsigned char type=0;

    logoutput_debug("find_commonhandle_buffer: size %i", size);

    if (size < BUFFER_HANDLE_SIZE) return NULL;

    /* read the buffer, assume it's big enough */

    dev=get_uint32(&buffer[pos]);
    pos+=4;
    ino=get_uint64(&buffer[pos]);
    pos+=8;
    pid=get_uint32(&buffer[pos]);
    pos+=4;
    fd=get_uint32(&buffer[pos]);
    pos+=4;
    type=(unsigned char) buffer[pos];
    pos++;

    if (p_count) *p_count+=pos;
    if (pid != getpid()) logoutput_warning("find_commonhandle_buffer: handle has unknown pid %i", pid);
    return find_commonhandle(dev, ino, pid, fd, type, compare_subsystem_sftp, (void *) sftp);

}

unsigned char write_sftp_commonhandle(struct commonhandle_s *handle, char *buffer, unsigned int size)
{
    unsigned char pos=0;
    pid_t pid=get_pid_commonhandle(handle);
    int fd=get_fd_commonhandle(handle);

    if (size < BUFFER_HANDLE_SIZE) return 0;

    store_uint32(&buffer[pos], handle->location.type.devino.dev);
    pos+=4;
    store_uint64(&buffer[pos], handle->location.type.devino.ino);
    pos+=8;
    store_uint32(&buffer[pos], pid);
    pos+=4;
    store_uint32(&buffer[pos], fd);
    pos+=4;

    if (handle->flags & COMMONHANDLE_FLAG_DIR) {

	buffer[pos]=COMMONHANDLE_TYPE_DIR;

    } else if (handle->flags & COMMONHANDLE_FLAG_FILE) {

	buffer[pos]=COMMONHANDLE_TYPE_FILE;

    } else {

	buffer[pos]=0;

    }

    pos++;
    return pos;
}

unsigned char get_sftp_handle_size()
{
    return (unsigned char) BUFFER_HANDLE_SIZE;
}

/* CREATE a filehandle for open/create a file */

/* */

void set_sftp_handle_flags(struct commonhandle_s *handle, unsigned int flags)
{

    if ((handle->flags & COMMONHANDLE_FLAG_FILE) && (handle->flags & COMMONHANDLE_FLAG_SFTP)) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	sftp_handle->type.filehandle.flags=flags;

    }

}

unsigned int get_sftp_handle_flags(struct commonhandle_s *handle)
{

    if ((handle->flags & COMMONHANDLE_FLAG_FILE) && (handle->flags & COMMONHANDLE_FLAG_SFTP)) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	return sftp_handle->type.filehandle.flags;

    }

    return 0;

}

static unsigned int _get_flags_sftp(struct commonhandle_s *handle)
{
    unsigned int flags=get_sftp_handle_flags(handle);
    return translate_sftp_flags_2_commonshared(flags);
}

void set_sftp_handle_access(struct commonhandle_s *handle, unsigned int access)
{

    if ((handle->flags & COMMONHANDLE_FLAG_FILE) && (handle->flags & COMMONHANDLE_FLAG_SFTP)) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	sftp_handle->type.filehandle.access=access;

    }

}

int get_sftp_handle_access(struct commonhandle_s *handle)
{

    if (handle->flags & COMMONHANDLE_FLAG_FILE) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	return sftp_handle->type.filehandle.access;

    }

    return 0;
}

static unsigned int _get_access_sftp(struct commonhandle_s *handle)
{
    unsigned int access=get_sftp_handle_access(handle);
    return translate_sftp_access_2_commonshared(access);
}

static struct commonhandle_s *find_sftp_handle_common(struct sftp_subsystem_s *sftp, unsigned int flag, char *buffer, unsigned int size, unsigned int *p_count)
{
    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, buffer, size, p_count);

    if (handle) {

	if (flag>0 && (handle->flags & flag)==0) {

	    logoutput_warning("find_sftp_handle_common: handle found of wrong type");
	    return NULL;

	} else if (get_pid_commonhandle(handle) != getpid()) {

	    logoutput_warning("find_sftp_filehandle: handle found is of different process");
	    return NULL;

	}

    }

    return handle;

}


/* FILEHANDLE */

struct commonhandle_s *create_sftp_filehandle(struct sftp_subsystem_s *sftp, unsigned int inserttype, dev_t dev, uint64_t ino, char *name, unsigned int flags, unsigned int access)
{
    struct fs_location_s location;
    struct commonhandle_s *handle=NULL;

    memset(&location, 0, sizeof(struct fs_location_s));

    location.flags=FS_LOCATION_FLAG_DEVINO;
    location.type.devino.dev=dev;
    location.type.devino.ino=ino;

    if (inserttype==INSERTHANDLE_TYPE_CREATE) {

	if (name==NULL) {

	    logoutput_warning("create_sftp_filehandle: filehandle for create requires a name (is not defined)");
	    return NULL;

	}

	location.name=name;
	location.flags|=FS_LOCATION_FLAG_NAME;

    }

    handle=create_commonhandle(COMMONHANDLE_TYPE_FILE, &location, sizeof(struct sftp_handle_s));

    if (handle) {
	struct sftp_handle_s *sftp_handle=NULL;
	struct insert_filehandle_s insert;

	handle->flags |= COMMONHANDLE_FLAG_SFTP;
	handle->get_access=_get_access_sftp;
	handle->get_flags=_get_flags_sftp;

	sftp_handle=(struct sftp_handle_s *) handle->buffer;

	sftp_handle->sftp=sftp;
	sftp_handle->type.filehandle.flags=flags;
	sftp_handle->type.filehandle.access=access;

	/* insert to check for conflicts */

	memset(&insert, 0, sizeof(struct insert_filehandle_s));
	insert.type=inserttype;

	if (start_insert_filehandle(handle, &insert)==0) {

	    logoutput("create_sftp_filehandle: created file handle");

	} else {

	    logoutput("create_sftp_filehandle: failed to insert the file handle (..)");
	    goto failed;

	}

    }

    return NULL;

    failed:

    if (handle) free_commonhandle(&handle);
    return NULL;

}

/* DIRHANDLE */

struct commonhandle_s *create_sftp_dirhandle(struct sftp_subsystem_s *sftp, struct fs_location_devino_s *devino)
{
    struct fs_location_s location;
    struct commonhandle_s *handle=NULL;

    memset(&location, 0, sizeof(struct fs_location_s));

    location.flags=FS_LOCATION_FLAG_DEVINO;
    location.type.devino.dev=devino->dev;
    location.type.devino.ino=devino->ino;

    handle=create_commonhandle(COMMONHANDLE_TYPE_DIR, &location, sizeof(struct sftp_handle_s));

    if (handle) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	sftp_handle->sftp=sftp;

	/* take a default for the sftp attributes the server sends to the client per dentry next to the name and the type (which are always send)
	    TODO:
	    - make this configurable
	    - use a "new" opendir where the client can ask for the set of attributes: an extension or a new protocol version */

	sftp_handle->type.dirhandle.valid=SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_MODIFYTIME | SSH_FILEXFER_ATTR_SUBSECOND_TIMES | SSH_FILEXFER_ATTR_CTIME;
	insert_dirhandle(handle);

    }

    return handle;

}

struct commonhandle_s *find_sftp_handle(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *p_count)
{
    return find_sftp_handle_common(sftp, 0, buffer, size, p_count);
}

void release_sftp_handle(struct commonhandle_s **p_handle)
{
    struct commonhandle_s *handle=*p_handle;

    if (handle) {

	if (handle->flags & COMMONHANDLE_FLAG_DIR) {
	    struct dirhandle_s *dh=&handle->type.dir;

	    (* dh->close)(dh);

	} else if (handle->flags & COMMONHANDLE_FLAG_FILE) {
	    struct filehandle_s *fh=&handle->type.file;

	    (* fh->close)(fh);

	}

	free_commonhandle(p_handle);

    }

}

uint32_t get_valid_sftp_dirhandle(struct commonhandle_s *handle)
{
    struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;
    return sftp_handle->type.dirhandle.valid;
}
