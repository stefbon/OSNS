/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Stef Bon <stefbon@gmail.com>

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
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"
#include "misc.h"
#include "datatypes.h"

#include "fshandle.h"

#define HANDLE_READDIR_BUFFER_SIZE		1024

static int _open_error(struct dirhandle_s *dh, struct fs_location_s *l, unsigned int flags)
{
    logoutput_warning("_open_error: handle already open");
    return -1;
}

static struct fs_dentry_s *_readdentry_error(struct dirhandle_s *dh)
{
    logoutput_warning("_readentry_error: handle not open");
    return NULL;
}

static void _set_keep_current_dentry_error(struct dirhandle_s *dh)
{
}

static int _fstatat_error(struct dirhandle_s *dh, char *name, unsigned int mask, struct system_stat_s *st)
{
    logoutput_warning("_fstatat_error: handle not open");
    return -1;
}

static int _rmat_error(struct dirhandle_s *dh, const char *name)
{
    logoutput_warning("_rmat_error: handle not open");
    return -1;
}

static void _fsyncdir_error(struct dirhandle_s *dh, unsigned int flags)
{
    logoutput_warning("_fsyncdir_error: handle not open");
}

static void _close_error(struct dirhandle_s *dh)
{
    logoutput_warning("_close_error: handle not open");
}


#ifdef __linux__

struct linux_dirent64 {
    ino64_t			d_ino;
    off64_t			d_off;
    unsigned short		d_reclen;
    unsigned char		d_type;
    char			d_name[];
};

static struct fs_dentry_s *_readdentry_handle(struct dirhandle_s *dh)
{
    struct linux_dirent64 *dirent=NULL;
    struct fs_dentry_s *dentry=&dh->dentry;
    char *pos=NULL;
    unsigned int len=0;

    if (dh->flags & DIRHANDLE_FLAG_KEEP_DENTRY) {

	dh->flags &= ~DIRHANDLE_FLAG_KEEP_DENTRY;
	return dentry;

    }

    readdentry:

    if (dh->pos >= dh->read) {
	int result=-1;
	int fd=get_unix_fd_fs_socket(&dh->socket);

	/* read a batch of dirents */

	result=syscall(SYS_getdents64, fd, (struct linux_dirent64 *) dh->buffer, dh->size);

	if (result>0) {

	    dh->pos=0;
	    dh->read=(unsigned int) result;

	} else if (result==0) {

	    /* see man getdents: 0 means End Of Data */
	    dh->flags |= DIRHANDLE_FLAG_EOD;
	    return NULL;

	} else {

	    dh->flags |= DIRHANDLE_FLAG_ERROR;
	    return NULL;

	}

    }

    dirent=(struct linux_dirent64 *) (dh->buffer + dh->pos);
    dh->pos += dirent->d_reclen;

    /* TODO: a server side filter based on name,
	if this filters jump to the next */

    dentry->type=DTTOIF(dirent->d_type);
    dentry->ino=dirent->d_ino;
    dentry->name=dirent->d_name;
    len=dirent->d_reclen - offsetof(struct linux_dirent64, d_name) - 1; /* minus the type byte */
    /* use rawmemchr ?? dangerous .... no check if there is no zero byte found */
    pos=memchr(dentry->name, '\0', len);
    dentry->len=(pos) ? (unsigned int)(pos - dentry->name) : len;
    return dentry;

}

static void _set_keep_current_dentry_handle(struct dirhandle_s *dh)
{
    dh->flags |= DIRHANDLE_FLAG_KEEP_DENTRY;
}

static int _fstatat_handle(struct dirhandle_s *dh, char *name, unsigned int mask, struct system_stat_s *st)
{
    return system_fgetstatat(&dh->socket, name, mask, st);
}

static int _unlinkat_handle(struct dirhandle_s *dh, const char *name)
{
    return system_unlinkat(&dh->socket, name);
}

static int _rmdirat_handle(struct dirhandle_s *dh, const char *name)
{
    return system_rmdirat(&dh->socket, name);
}

static void _fsyncdir_handle(struct dirhandle_s *dh, unsigned int flags)
{
    int fd=get_unix_fd_fs_socket(&dh->socket);

    if (flags & FSYNC_FLAG_DATASYNC) {

	fdatasync(fd);

    } else {

	fsync(fd);

    }

}

static void _close_handle(struct dirhandle_s *dh)
{
    int fd=get_unix_fd_fs_socket(&dh->socket);

    if (fd>=0) {

	close(fd);
	set_unix_fd_fs_socket(&dh->socket, -1);

    }

}

static int _open_handle(struct dirhandle_s *dh, struct fs_location_s *location, unsigned int flags)
{
    int fd=-1;

    if (location->flags & FS_LOCATION_FLAG_PATH) {

	fd=system_open(&location->type.path, O_DIRECTORY | O_RDONLY, &dh->socket);

    } else if ((location->flags & FS_LOCATION_FLAG_AT) && (location->flags & FS_LOCATION_FLAG_SOCKET) && location->name) {

	fd=system_openat(&location->type.socket, location->name, O_DIRECTORY | O_RDONLY, &dh->socket);

    }

    if (fd==-1) {

	logoutput_warning("open_handle: error %i open (%s)", errno, strerror(errno));
	return -1;

    }

    set_unix_fd_fs_socket(&dh->socket, fd);

    /* create a buffer for the getdents call to store result of getting direntries*/

    if (dh->buffer==NULL) {

	if (dh->size==0) dh->size=HANDLE_READDIR_BUFFER_SIZE;
	dh->buffer=malloc(dh->size);

	if (dh->buffer==NULL) {

	    logoutput_warning("open_handle: error %i allocating %i bytes", errno, dh->size, strerror(errno));
	    goto error;

	}

    }

    /* success */

    dh->open=_open_error;
    dh->close=_close_handle;
    dh->readdentry=_readdentry_handle;
    dh->set_keep_dentry=_set_keep_current_dentry_handle;
    dh->fstatat=_fstatat_handle;
    dh->unlinkat=_unlinkat_handle;
    dh->rmdirat=_rmdirat_handle;
    dh->fsyncdir=_fsyncdir_handle;

    return fd;

    error:

    fd=get_unix_fd_fs_socket(&dh->socket);

    if (fd>=0) {

	close(fd);
	set_unix_fd_fs_socket(&dh->socket, -1);

    }

    if (dh->buffer) {

	free(dh->buffer);
	dh->buffer=NULL;
	dh->size=0;

    }

    return -1;

}

#endif

void init_dirhandle(struct dirhandle_s *dh)
{

    memset(dh, 0, sizeof(struct dirhandle_s));

    dh->flags=0;

    dh->open=_open_error;
    dh->close=_close_error;
    dh->readdentry=_readdentry_error;
    dh->set_keep_dentry=_set_keep_current_dentry_error;
    dh->fstatat=_fstatat_error;
    dh->unlinkat=_rmat_error;
    dh->rmdirat=_rmat_error;
    dh->fsyncdir=_fsyncdir_error;

    dh->buffer=NULL;
    dh->size=0;
    dh->read=0;
    dh->pos=0;

    init_fs_socket(&dh->socket);

    dh->dentry.flags=0;
    dh->dentry.type=0;
    dh->dentry.ino=0;
    dh->dentry.len=0;
    dh->dentry.name=NULL;

}

void enable_dirhandle(struct dirhandle_s *dh)
{

    dh->flags |= DIRHANDLE_FLAG_ENABLED;

#ifdef __linux__

    dh->open=_open_handle;

#endif

}

int system_remove_dir(struct fs_location_path_s *path)
{

#ifdef __linux__

    char tmp[path->len + 1];

    memcpy(tmp, path->ptr, path->len);
    tmp[path->len]='\0';
    return rmdir(tmp);

#else

    return -1;

#endif

}

int system_create_dir(struct fs_location_path_s *path, struct fs_init_s *init)
{

#ifdef __linux__

    char tmp[path->len + 1];

    memcpy(tmp, path->ptr, path->len);
    tmp[path->len]='\0';
    return mkdir(tmp, init->mode);

#else

    return -1;

#endif

}



void free_dirhandle(struct dirhandle_s *dh)
{

    if (dh->buffer) {

	free(dh->buffer);
	dh->buffer=NULL;

    }

    init_dirhandle(dh);

}
