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
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"
#include "misc.h"
#include "datatypes.h"

#include "open.h"

int get_unix_fd_fs_socket(struct fs_socket_s *s)
{

#ifdef __linux__

    return s->fd;

#else

    return -1;

#endif

}

unsigned int get_unix_pid_fs_socket(struct fs_socket_s *s)
{

#ifdef __linux__

    return s->pid;

#else

    return 0;

#endif

}

void set_unix_fd_fs_socket(struct fs_socket_s *s, int fd)
{

#ifdef __linux__

    s->fd=fd;

#endif

}

void init_fs_socket(struct fs_socket_s *s)
{
    memset(s, 0, sizeof(struct fs_socket_s));
    s->pid=getpid();
    set_unix_fd_fs_socket(s, -1);
}

int compare_fs_socket(struct fs_socket_s *s, struct fs_socket_s *t)
{

#ifdef __linux__

    return ((s->fd==t->fd) && (s->pid==t->pid)) ? 0 : -1;

#else

    return -1;

#endif

}


int system_pread(struct fs_socket_s *s, char *data, unsigned int size, off_t off)
{

#ifdef __linux__

    return pread(s->fd, data, size, off);

#else

    return -1;

#endif

}

int system_pwrite(struct fs_socket_s *s, char *data, unsigned int size, off_t off)
{

#ifdef __linux__

    return pwrite(s->fd, data, size, off);

#else

    return -1;

#endif
}

int system_fsync(struct fs_socket_s *s)
{

#ifdef __linux__

    return fsync(s->fd);

#else

    return -1;

#endif

}

int system_fdatasync(struct fs_socket_s *s)
{

#ifdef __linux__

    return fdatasync(s->fd);

#else

    return -1;

#endif

}

int system_flush(struct fs_socket_s *socket, unsigned int flags)
{

    /* what to do here ?? */
    return 0;
}

off_t system_lseek(struct fs_socket_s *s, off_t off, int whence)
{

#ifdef __linux__

    return lseek(s->fd, off, whence);

#endif

}

void system_close(struct fs_socket_s *s)
{

#ifdef __linux__

    close(s->fd);
    s->fd=-1;

#endif

}

int system_open(struct fs_location_path_s *path, unsigned int flags, struct fs_socket_s *sock)
{
    int fd=-1;

#ifdef __linux__

    unsigned int size=get_unix_location_path_length(path);
    char tmp[size+1];

    memset(tmp, 0, size+1);
    size=copy_unix_location_path(path, tmp, size);

    fd=open(tmp, flags);

    if (fd==-1) {

	logoutput_warning("system_open: error %i open (%s)", errno, strerror(errno));

    } else {

	sock->fd=fd;

    }

#endif

    return fd;

}

int system_create(struct fs_location_path_s *path, unsigned int flags, struct fs_init_s *init, struct fs_socket_s *sock)
{
    int fd=-1;

#ifdef __linux__

    unsigned int size=get_unix_location_path_length(path);
    char tmp[size+1];
    mode_t mode=init->mode;

    memset(tmp, 0, size+1);
    size=copy_unix_location_path(path, tmp, size); 

    fd=open(tmp, flags, mode);

    if (fd==-1) {

	logoutput_warning("system_create: error %i open (%s)", errno, strerror(errno));

    } else {

	sock->fd=fd;

    }

#endif

    return fd;

}

int system_openat(struct fs_socket_s *ref, const char *name, unsigned int flags, struct fs_socket_s *sock)
{
    int fd=-1;

#ifdef __linux__

    fd=openat(ref->fd, name, flags);

    if (fd==-1) {

	logoutput_warning("system_openat: error %i open (%s)", errno, strerror(errno));

    } else {

	sock->fd=fd;

    }

#endif

    return fd;

}

int system_creatat(struct fs_socket_s *ref, const char *name, unsigned int flags, struct fs_init_s *init, struct fs_socket_s *sock)
{
    int fd=-1;

#ifdef __linux__
    mode_t mode=init->mode;

    fd=openat(ref->fd, name, flags, mode);

    if (fd==-1) {

	logoutput_warning("system_creatat: error %i open (%s)", errno, strerror(errno));

    } else {

	sock->fd=fd;

    }

#endif

    return fd;

}

int system_unlinkat(struct fs_socket_s *ref, const char *name)
{
#ifdef __linux__

    return unlinkat(ref->fd, name, 0);

#else

    return -1;

#endif

}

int system_rmdirat(struct fs_socket_s *ref, const char *name)
{
#ifdef __linux__

    return unlinkat(ref->fd, name, AT_REMOVEDIR);

#else

    return -1;

#endif

}

int system_readlinkat(struct fs_socket_s *ref, const char *name, struct fs_location_path_s *target)
{

#ifdef __linux__

    int len=-1;
    unsigned int size=512;

    target->ptr=realloc(target->ptr, size);
    if (target->ptr==NULL) return -1;
    target->len=0;
    target->size=size;
    target->flags=FS_LOCATION_PATH_FLAG_PTR_ALLOC;

    while (size < 4096) {

	len=readlinkat(ref->fd, name, target->ptr, size);

	if (len==-1) {

	    return -errno;

	} else if (len<size) {

	    target->len=len;
	    target->ptr[len]='\0';
	    break;

	}

	size+=512;

    }

    return len;

#else

    return -1;

#endif

}
