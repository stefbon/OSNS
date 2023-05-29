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

#include <linux/kdev_t.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-system.h"

#include "protocol.h"
#include "osns_sftp_subsystem.h"
#include "send.h"

#ifndef SSH_FXF_BLOCK_READ

#define SSH_FXF_BLOCK_READ              	0x00000040
#define SSH_FXF_BLOCK_WRITE             	0x00000080
#define SSH_FXF_BLOCK_DELETE            	0x00000100
#define SSH_FXF_BLOCK_ADVISORY			0x00000200
#define SSH_FXF_DELETE_ON_CLOSE         	0x00000800

#endif

#define SFTP_DIRHANDLE_FLAG_NEGOTIATED				1
#define SFTP_DIRHANDLE_FLAG_KEEP_DENTRY				2

struct sftp_handle_data_s {
    int							access;
    unsigned int					flags;
    struct sftp_valid_s					valid;
};

static unsigned int translate_sftp_flags_2_fs_handle(unsigned int sftpflags)
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

	result |= FS_HANDLE_BLOCK_READ;

    }

    if (sftpflags & SSH_FXF_BLOCK_WRITE) {

	result |= FS_HANDLE_BLOCK_WRITE;

    }

    if (sftpflags & (SSH_FXF_BLOCK_DELETE | SSH_FXF_DELETE_ON_CLOSE)) {

	result |= FS_HANDLE_BLOCK_DELETE;

    }

    return result;
}

static unsigned int translate_sftp_access_2_fs_handle(unsigned int sftpaccess)
{
    return sftpaccess; /* same for protocol 6 at least (see: ... )*/
}

/* FIND */

struct fs_handle_s *sftp_find_fs_handle(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *p_count)
{
    return get_fs_handle(sftp->connection.unique, buffer, size, p_count);
}



/* functions to get and set the flags when opening a file
    these flags indicate the file is created or not, the kind of locking, append or not */


void set_sftp_handle_flags(struct fs_handle_s *handle, unsigned int flags)
{
    struct sftp_handle_data_s *data=(struct sftp_handle_data_s *) handle->buffer;
    data->flags=flags;
}

unsigned int get_sftp_handle_flags(struct fs_handle_s *handle)
{
    struct sftp_handle_data_s *data=(struct sftp_handle_data_s *) handle->buffer;
    return data->flags;
}

static unsigned int _get_flags_sftp(struct fs_handle_s *handle)
{
    unsigned int flags=get_sftp_handle_flags(handle);
    return translate_sftp_flags_2_fs_handle(flags);
}

/*
    functions to get/set the access bits when opening a file
    these bits indicate what has to be opened/accessed like:

    - read/write contents of the file
    - append to file
    - attributes
    - named attributes
    - acl

*/

void set_sftp_handle_access(struct fs_handle_s *handle, unsigned int access)
{
    struct sftp_handle_data_s *data=(struct sftp_handle_data_s *) handle->buffer;
    data->access=access;
}

int get_sftp_handle_access(struct fs_handle_s *handle)
{
    struct sftp_handle_data_s *data=(struct sftp_handle_data_s *) handle->buffer;
    return data->access;
}

static unsigned int _get_access_sftp(struct fs_handle_s *handle)
{
    unsigned int access=get_sftp_handle_access(handle);
    return translate_sftp_access_2_fs_handle(access);
}

/* CREATE HANDLE */

struct fs_handle_s *sftp_create_fs_handle(struct sftp_subsystem_s *sftp, dev_t dev, uint64_t ino, unsigned int flags, unsigned int access, const char *what)
{
    unsigned int type=(strcmp(what, "dir")==0) ? FS_HANDLE_TYPE_DIR : FS_HANDLE_TYPE_FILE;
    struct fs_handle_s *handle=create_fs_handle(type, sizeof(struct sftp_handle_data_s));

    if (handle) {
	struct sftp_handle_data_s *data=(struct sftp_handle_data_s *) handle->buffer;

	handle->subsystem=HANDLE_SUBSYSTEM_TYPE_SFTP;
	handle->get_access=_get_access_sftp;
	handle->get_flags=_get_flags_sftp;

	data->flags=flags;
	data->access=access;

	/* insert
	    TODO: check for conflicts
	    - lock conflicts -> access flags do not allow each other */

        insert_fs_handle(handle, sftp->connection.unique, dev, ino);


    }

    return handle;

}

/* SFTP valid */

struct sftp_valid_s *sftp_get_valid_fs_handle(struct fs_handle_s *handle)
{
    struct sftp_handle_data_s *data=(struct sftp_handle_data_s *) handle->buffer;
    return &data->valid;
}

int send_sftp_handle(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, struct fs_handle_s *handle)
{
    unsigned int size=(unsigned int) get_fs_handle_buffer_size();
    char buffer[size];

    size=(* handle->write_handle)(handle, buffer, size);

    return reply_sftp_handle(sftp, inh->id, buffer, size);
}
