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

struct handle_result_s {
    unsigned char		type;
    union {
	struct sftp_filehandle_s *file;
	struct sftp_dirhandle_s  *dir;
    } handle;
    unsigned int		error;
};

/* generic function to match a filehandle or a dirhandle to a set of parameters forming the handle */

static int find_sftp_handle(struct sftp_subsystem_s *sftp, dev_t dev, uint64_t ino, unsigned int pid, unsigned int fd, struct handle_result_s *findresult)
{
    int result=-1;
    struct commonhandle_s *handle=NULL;
    unsigned int hashvalue=0;
    void *index=NULL;
    struct simple_lock_s lock;

    hashvalue=calculate_ino_hash(ino);
    readlock_commonhandles(&lock);
    handle=(struct commonhandle_s *) get_next_commonhandle(&index, hashvalue);

    while (handle) {

	if (handle->ino==ino && handle->dev==dev && handle->pid==pid && handle->fd==fd) {

	    if (handle->type==COMMONHANDLE_TYPE_FILE) {
		struct sftp_filehandle_s *filehandle=(struct sftp_filehandle_s *)(((char *) handle) - offsetof(struct sftp_filehandle_s, handle));

		if (filehandle->sftp==sftp) {

		    findresult->type=COMMONHANDLE_TYPE_FILE;
		    findresult->handle.file=filehandle;
		    findresult->error=0;
		    result=0;

		} else {

		    findresult->error=EPERM;

		}

		break;

	    } else if (handle->type==COMMONHANDLE_TYPE_DIR) {
		struct sftp_dirhandle_s *dirhandle=(struct sftp_dirhandle_s *)(((char *) handle) - offsetof(struct sftp_dirhandle_s, handle));

		if (dirhandle->sftp==sftp) {

		    findresult->type=COMMONHANDLE_TYPE_DIR;
		    findresult->handle.dir=dirhandle;
		    findresult->error=0;
		    result=0;

		} else {

		    findresult->error=EPERM;

		}

		break;

	    }

	}

	handle=(struct commonhandle_s *) get_next_commonhandle(&index, hashvalue);

    }

    unlock_commonhandles(&lock);
    return result;

}

/*
    FILEHANDLE
		*/

static void close_filehandle(struct commonhandle_s *handle)
{
    if (handle->fd>0) {

	close(handle->fd);
	handle->fd=0;

    }
}

static void free_filehandle(struct commonhandle_s **p_handle)
{
    struct commonhandle_s *handle=*p_handle;

    close_filehandle(handle);
    free(handle);
    *p_handle=NULL;

}

struct sftp_filehandle_s *insert_sftp_filehandle(struct sftp_subsystem_s *sftp, dev_t dev, uint64_t ino, unsigned int fd)
{
    struct sftp_filehandle_s *filehandle=NULL;
    struct commonhandle_s *handle=NULL;

    logoutput("insert_filehandle: dev %i ino %i");

    filehandle=malloc(sizeof(struct sftp_filehandle_s));
    if (filehandle==NULL) return NULL;
    handle=&filehandle->handle;

    memset(filehandle, 0, sizeof(struct sftp_filehandle_s));

    handle->type=COMMONHANDLE_TYPE_FILE;
    handle->dev=dev;
    handle->ino=ino;
    handle->pid=getpid();
    handle->fd=fd;
    handle->close=close_filehandle;
    handle->free=free_filehandle;

    filehandle->sftp=sftp;

    insert_commonhandle_hash(handle);
    return filehandle;

}

struct sftp_filehandle_s *find_sftp_filehandle_buffer(struct sftp_subsystem_s *sftp, char *buffer)
{
    unsigned char pos=0;
    dev_t dev=0;
    uint64_t ino=0;
    unsigned int pid=0;
    unsigned int fd=0;
    unsigned char type=0;
    struct handle_result_s findresult;
    struct sftp_filehandle_s *filehandle=NULL;

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

    memset(&findresult, 0, sizeof(struct handle_result_s));

    if (pid != getpid()) {

	logoutput_warning("find_sftp_filehandle_buffer: handle has unknown pid %i", pid);
	return NULL;

    }

    if (find_sftp_handle(sftp, dev, ino, pid, fd, &findresult)==0 && findresult.type==COMMONHANDLE_TYPE_FILE) filehandle=findresult.handle.file;

    return filehandle;

}

unsigned char write_sftp_filehandle(struct sftp_filehandle_s *filehandle, char *buffer)
{
    if (buffer==NULL) return SFTP_HANDLE_SIZE;
    return write_commonhandle(&filehandle->handle, buffer);
}

int release_sftp_handle_buffer(char *buffer, struct sftp_subsystem_s *sftp)
{
    unsigned char pos=0;
    dev_t dev=0;
    uint64_t ino=0;
    unsigned int pid=0;
    unsigned int fd=0;
    unsigned char type=0;
    struct handle_result_s findresult;
    int result=-1;

    /* read the buffer (it must be at least 21 bytes) */

    dev=get_uint32(&buffer[pos]);
    pos+=4;
    ino=get_uint64(&buffer[pos]);
    pos+=8;
    pid=get_uint32(&buffer[pos]);
    pos+=4;
    fd=get_uint32(&buffer[pos]);
    pos+=4;
    type=(unsigned char) buffer[pos];

    memset(&findresult, 0, sizeof(struct handle_result_s));

    if (find_sftp_handle(sftp, dev, ino, pid, fd, &findresult)==0) {

	if (findresult.type==type) {

	    if (type==COMMONHANDLE_TYPE_FILE) {
		struct sftp_filehandle_s *filehandle=findresult.handle.file;

		(* filehandle->handle.close)(&filehandle->handle);
		result=0;

	    } else if (type==COMMONHANDLE_TYPE_DIR) {
		struct sftp_dirhandle_s *dirhandle=findresult.handle.dir;

		(* dirhandle->handle.close)(&dirhandle->handle);
		result=0;

	    }

	}

    }

    return result;

}

/*
    DIRHANDLE
		*/

unsigned char write_sftp_dirhandle(struct sftp_dirhandle_s *dirhandle, char *buffer)
{
    if (buffer==NULL) return SFTP_HANDLE_SIZE;
    return write_commonhandle(&dirhandle->handle, buffer);
}

static void close_dirhandle(struct commonhandle_s *handle)
{

    if (handle->fd>0) {

	close(handle->fd);
	handle->fd=0;

    }

}

static void free_dirhandle(struct commonhandle_s **p_handle)
{
    struct commonhandle_s *handle=*p_handle;
    struct sftp_dirhandle_s *dirhandle=(struct sftp_dirhandle_s *) ( ((char *) handle) - offsetof(struct sftp_dirhandle_s, handle));

    close_dirhandle(handle);

    if (dirhandle->buffer) {

	free(dirhandle->buffer);
	dirhandle->buffer=NULL;

    }

    free(handle);
    *p_handle=NULL;

}

struct sftp_dirhandle_s *insert_sftp_dirhandle(struct sftp_subsystem_s *sftp, dev_t dev, uint64_t ino, unsigned int fd)
{
    struct sftp_dirhandle_s *dirhandle=NULL;
    struct commonhandle_s *handle=NULL;

    logoutput("insert_dirhandle: dev %i ino %i");

    dirhandle=malloc(sizeof(struct sftp_dirhandle_s));
    if (dirhandle==NULL) return NULL;
    handle=&dirhandle->handle;

    memset(dirhandle, 0, sizeof(struct sftp_dirhandle_s));

    handle->type=COMMONHANDLE_TYPE_DIR;
    handle->dev=dev;
    handle->ino=ino;
    handle->pid=getpid();
    handle->fd=fd;
    handle->close=close_dirhandle;
    handle->free=free_dirhandle;

    dirhandle->sftp=sftp;
    dirhandle->size=0;
    dirhandle->buffer=NULL;
    dirhandle->pos=0;
    dirhandle->read=0;

    insert_commonhandle_hash(handle);
    return dirhandle;

}

struct sftp_dirhandle_s *find_sftp_dirhandle_buffer(struct sftp_subsystem_s *sftp, char *buffer)
{
    unsigned char pos=0;
    dev_t dev=0;
    uint64_t ino=0;
    unsigned int pid=0;
    unsigned int fd=0;
    unsigned char type=0;
    struct handle_result_s findresult;
    struct sftp_dirhandle_s *dirhandle=NULL;

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

    memset(&findresult, 0, sizeof(struct handle_result_s));

    if (pid != getpid()) {

	logoutput_warning("find_sftp_dirhandle_buffer: handle has unknown pid %i", pid);
	return NULL;

    }

    if (find_sftp_handle(sftp, dev, ino, pid, fd, &findresult)==0 && findresult.type==COMMONHANDLE_TYPE_DIR) dirhandle=findresult.handle.dir;

    return dirhandle;

}
