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

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "ssh/subsystem/commonhandle.h"
#include "handle.h"

struct sftp_handle_s {
    struct sftp_subsystem_s					*sftp;
    union _sftp_type_u {
	struct sftp_filehandle_s {
	    int							access;
	    int							flags;
	} filehandle;
	struct sftp_dirhandle_s {
	    unsigned int					valid;
	    char						*attr;
	    int 						(* get_direntry)(struct commonhandle_s *handle, struct commondentry_s *dentry, char *attr);
	} dirhandle;
    } type;
};

static unsigned int translate_sftp_flags_2_commonshared(unsigned int sftpflags)
{
    unsigned int result=0;

    if (sftpflags & SSH_FXP_BLOCK_READ) {

	result |= FILEHANDLE_BLOCK_READ;

    }

    if (sftpflags & SSH_FXP_BLOCK_WRITE) {

	result |= FILEHANDLE_BLOCK_WRITE;

    }

    if (sftpflags & (SSH_FXP_BLOCK_DELETE | SSH_FXP_DELETE_ON_CLOSE)) {

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
    struct sftp_subsystem_s *sftp=NULL;

    if (handle->flag & COMMONHANDLE_FLAG_SFTP) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	sftp=sftp_handle->sftp;

    }

    return sftp;

}

static int compare_subsystem_sftp(struct commonhandle_s *handle, void *ctx)
{
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *) ctx;
    return (get_sftp_subsystem_commonhandle(handle)==sftp) ? 0 : -1;
}

struct commonhandle_s *find_sftp_commonhandle_buffer(struct sftp_subsystem_s *sftp, char *buffer, unsigned int *p_count)
{
    unsigned char pos=0;
    dev_t dev=0;
    uint64_t ino=0;
    unsigned int pid=0;
    unsigned int fd=0;
    unsigned char type=0;
    struct commonhandle_s *commonhandle=NULL;

    /* read the buffer */

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

/* CREATE a filehandle for open/create a file */

void set_sftp_handle_flags(struct commonhandle_s *handle, unsigned int flags)
{
    if (handle->flag & COMMONHANDLE_FLAG_FILE) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	sftp_handle->type.filehandle.flags=flags;

    }
}

static unsigned int get_sftp_handle_flags(struct commonhandle_s *handle)
{

    if (handle->flag & COMMONHANDLE_FLAG_FILE) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	return translate_sftp_flags_2_commonshared(sftp_handle->type.filehandle.flags);

    }

    return 0;

}

void set_sftp_handle_access(struct commonhandle_s *handle, unsigned int access)
{

    if (handle->flag & COMMONHANDLE_FLAG_FILE) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	sftp_handle->type.filehandle.access=access;

    }

}

static int get_sftp_handle_access(struct commonhandle_s *handle)
{

    if (handle->flag & COMMONHANDLE_FLAG_FILE) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	return translate_sftp_access_2_commonshared(sftp_handle->type.filehandle.access);

    }

    return 0;
}

static const char *get_name_sftp_handle(void *ctx, struct commonhandle_s *handle)
{

    if (handle->flag & COMMONHANDLE_FLAG_FILE) {

	return "SFTP file handle";

    } else if (handle->flag & COMMONHANDLE_FLAG_DIR) {

	return "SFTP dir handle";

    }

    return "";

}

static unsigned int get_buffer_size_sftp(void *ctx, const char *what)
{
    return sizeof(struct sftp_handle_s);
}

static void close_sftp_handle(struct commonhandle_s *handle)
{
    /* nothing to close ... */
}

static void free_sftp_handle(struct commonhandle_s *handle)
{

    if (handle->flag & COMMONHANDLE_FLAG_DIR) {
	struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;

	if (sftp_handle->type.dirhandle.attr) {

	    free(sftp_handle->type.dirhandle.attr);
	    sftp_handle->type.dirhandle.attr=NULL;

	}

    }

}

static void init_buffer_sftp(void *ctx, struct commonhandle_s *handle)
{
    struct sftp_handle_s *sftp_handle=(struct sftp_handle_s *) handle->buffer;
    struct sftp_subsystem_s *sftp=(struct sftp_subsystem_s *) ctx;

    sftp_handle->sftp=sftp;

    /* close and free the custom data for sftp */

    handle->close=close_sfto_handle;
    handle->free=free_sftp_handle;

    if (handle->flag & COMMONHANDLE_FLAG_FILE) {

	sftp_handle->type.filehandle.access=0;
	sftp_handle->type.filehandle.flags=0;

	handle->type.filehandle.check_lock_flags=check_lock_flags_sftp;
	handle->type.filehandle.get_access_flags=get_access_flags_sftp;

    } else if (handle->flag & COMMONHANDLE_FLAG_DIR) {

	handle->type.dirhandle.valid=0;

    }

}

static clear_buffer_sftp(struct commonhandle_s *handle)
{
    free_sftp_handle(handle);
}

static commonhandle_ops_s sftp_handle_ops = {
    .name					= get_name_sftp_handle,
    .get_buffer_size				= get_buffer_size_sftp,
    .init					= init_buffer_sftp,
    .clear					= clear_buffer_sftp,
};

/*
    FILEHANDLE
		*/


struct commonhandle_s *init_sftp_filehandle(struct sftp_subsystem_s *sftp, unsigned int inserttype, dev_t dev, uint64_t ino, char *name)
{
    struct _fs_location_s location;
    struct commonhandle_s *handle=NULL;

    if (inserttype==INSERTHANDLE_TYPE_CREATE) {

	if (name==NULL) {

	    logoutput_warning("init_sftp_filehandle: filehandle for create requires a name (is not defined)");
	    return NULL;

	}

    }

    location.flags=FS_LOCATION_FLAG_DEVINO;
    location.type.devino.dev=dev;
    location.type.devino.ino=ino;

    if (inserttype==INSERTHANDLE_TYPE_CREATE) {

	location.name=name;
	location.flags|=FS_LOCATION_FLAG_NAME;

    }

    handle=create_commonhandle((void *) sftp, "file", &location, &sftp_handle_ops);

    if (handle) {

	handle->flags |= COMMONHANDLE_FLAG_SFTP;
	return handle;

    }

    return NULL;

}

int insert_sftp_filehandle(struct commonhandle_s *new, struct insert_filehandle_s *insert)
{
    if (commonhandle & COMMONHANDLE_FLAG_SFTP) return start_insert_filehandle(new, insert);
    return -1;
}

void complete_create_sftp_filehandle(struct commonhandle_s *new, dev_t dev, uint64_t ino, unsigned int fd)
{
    if (commonhandle & COMMONHANDLE_FLAG_SFTP) complete_create_filehandle(new, dev, ino, fd);
}

void release_sftp_handle_buffer(struct sftp_subsystem_s *sftp, char *buffer)
{
    struct commonhandle_s *handle=find_sftp_commonhandle_buffer(sftp, buffer);
    if (handle) (* handle->close)(handle);

}

/* DIRHANDLE */

struct commonhandle_s *create_sftp_dirhandle(struct sftp_subsystem_s *sftp, dev_t dev, uint64_t ino)
{
    logoutput("create_sftp_dirhandle: dev %i ino %i");
    return create_dirhandle((void *) sftp, dev, ino, &sftp_handle_ops);
}
