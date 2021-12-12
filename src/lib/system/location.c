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

#include "log.h"
#include "misc.h"
#include "datatypes.h"

#include "open.h"
#include "location.h"
#include "stat.h"

#ifdef __linux__

int get_target_unix_symlink(char *path, unsigned int len, unsigned int extra, struct fs_location_path_s *target)
{
    char tmp[len + 1];
    char *buffer=NULL;
    unsigned int size=512;
    ssize_t bytesread=0;
    int result=0;

    memcpy(tmp, path, len);
    tmp[len]='\0';

    realloc:

    buffer=realloc(buffer, size);
    if (buffer==NULL) {

	result=ENOMEM;
	logoutput_warning("get_target_unix_symlink: error %i allocating %i bytes (%s)", errno, size, strerror(errno));
	goto error;

    }

    bytesread=readlink(tmp, buffer, size);

    if (bytesread==-1) {

	result=errno;
	logoutput_warning("get_target_unix_symlink: error %i reading path %s (%s)", errno, tmp, strerror(errno));
	goto error;

    } else if (bytesread + extra >= size) {

	/* possible the path is truncated .... */

	size+=512;
	goto realloc;

    }

    target->ptr=buffer;
    target->size=size;
    target->len=(unsigned int) bytesread;
    target->flags |= FS_LOCATION_PATH_FLAG_PTR_ALLOC;
    return 0;

    error:

    logoutput_warning("get_target_unix_symlink: fialed to get target");
    if (buffer) free(buffer);
    return -result;

}

#else

int get_target_unix_symlink(char *path, unsigned int len, unsigned int extra, struct fs_location_path_s *result)
{
    return -ENOSYS;
}

#endif

int get_target_fs_socket(struct fs_socket_s *socket, char *name, struct fs_location_path_s *result)
{
    unsigned int extra=(name) ? strlen(name) : 0;
    int tmp=-1;

#ifdef __linux__

    char procpath[64]; /* more than enough */

    tmp=snprintf(procpath, 64, "/proc/%i/fd/%i", get_unix_pid_fs_socket(socket), get_unix_fd_fs_socket(socket));

    if (tmp>0) {

	if (get_target_unix_symlink(procpath, (unsigned int) tmp, extra + 2, result)==0) {

	    if (name) {
		unsigned int len=append_location_path(result, 'c', name);

	    }

	    tmp=(int) result->len;

	}

    }

#endif

    return tmp;

}

int get_realpath_fs_location_path(struct fs_location_path_s *result, struct fs_location_path_s *path)
{

#ifdef __linux__

    char *target=NULL;
    char tmp[path->len + 1];

    memcpy(tmp, path->ptr, path->len);
    tmp[path->len]='\0';

    if (realpath(tmp, &target)) {

	logoutput("get_realpath_fs_location_path: path %s realpath %s", tmp, target);

	result->ptr=target;
	result->len=strlen(target);
	result->size=result->len+1;
	result->flags=FS_LOCATION_PATH_FLAG_PTR_ALLOC;
	return 0;

    }

#endif

    return -1;

}

static int compare_devinos(struct fs_location_s *a, struct fs_location_s *b)
{
    int equal=-1;

    if (((a->name && b->name) && (strcmp(a->name, b->name)==0)) || (a->name==NULL && b->name==0)) {

	equal=(a->type.devino.dev==b->type.devino.dev && a->type.devino.ino==b->type.devino.ino) ? 0 : -1;

    }

    return equal;
}

static int compare_devino_location_path(struct fs_location_path_s *path, struct fs_location_devino_s *devino)
{
    struct system_stat_s stat;
    int differ=1;

    if (system_getstat(path, SYSTEM_STAT_MNTID | SYSTEM_STAT_INO, &stat)==0) {
	struct system_dev_s dev;

	get_dev_system_stat(&stat, &dev);

	if (get_unique_system_dev(&dev)==devino->dev && get_ino_system_stat(&stat)==devino->ino) differ=0;

    }

    return differ;
}

int compare_fs_socket_location(struct fs_location_s *a, struct fs_location_s *b)
{
    int equal=-1;

    if (b->flags & FS_LOCATION_FLAG_PATH) {
	struct fs_location_path_s result=FS_LOCATION_PATH_INIT;

	if (get_target_fs_socket(&a->type.socket, a->name, &result)==0) {

	    equal=compare_location_path(&b->type.path, b->name, 'p', (void *) &result);

	}

	clear_location_path(&result);

    } else if (b->flags & FS_LOCATION_FLAG_DEVINO) {
	struct fs_location_devino_s *devino=&b->type.devino;

	if (b->name && a->name) {

	    if (strcmp(b->name, a->name)==0) {
		struct fs_location_path_s result=FS_LOCATION_PATH_INIT;

		if (get_target_fs_socket(&a->type.socket, NULL, &result)==0) {

		    equal=compare_devino_location_path(&result, devino);

		}

		clear_location_path(&result);

	    }

	} else if (b->name) {
	    struct fs_location_path_s result=FS_LOCATION_PATH_INIT;

	    if (get_target_fs_socket(&a->type.socket, NULL, &result)==0) {
		struct ssh_string_s filename=SSH_STRING_INIT;

		detach_filename_location_path(&result, &filename);

		if (compare_ssh_string(&filename, 'c', b->name)==0) {

		    equal=compare_devino_location_path(&result, devino);

		}

	    }

	    clear_location_path(&result);

	} else {
	    struct fs_location_path_s result=FS_LOCATION_PATH_INIT;

	    if (get_target_fs_socket(&a->type.socket, a->name, &result)==0) {

		equal=compare_devino_location_path(&result, devino);

	    }

	    clear_location_path(&result);

	}

    } else if (b->flags & FS_LOCATION_FLAG_SOCKET) {
	struct fs_location_path_s tmp1=FS_LOCATION_PATH_INIT;
	struct fs_location_path_s tmp2=FS_LOCATION_PATH_INIT;

	if (get_target_fs_socket(&a->type.socket, a->name, &tmp1)==0 && get_target_fs_socket(&b->type.socket, b->name, &tmp2)==0) {

	    if (compare_location_paths(&tmp1, &tmp2)==0) equal=0;

	}

	clear_location_path(&tmp1);
	clear_location_path(&tmp2);

    }

    out:
    return equal;

}

static int compare_fs_devino_location_path(struct fs_location_s *b, struct fs_location_s *a)
{
    int equal=-1;

    if (a->name && b->name) {

	if (strcmp(a->name, b->name)==0 && compare_devino_location_path(&a->type.path, &b->type.devino)==0) equal=0;

    } else if (b->name) {
	struct ssh_string_s filename=SSH_STRING_INIT;

	detach_filename_location_path(&a->type.path, &filename);

	if (compare_ssh_string(&filename, 'c', b->name)==0) {

	    equal=compare_devino_location_path(&a->type.path, &b->type.devino);
	    append_location_path(&a->type.path, 's', &filename);

	}

    } else if (a->name) {
	unsigned int len=append_location_path_get_required_size(&a->type.path, 'c', (void *) a->name);
	char buffer[len];
	struct fs_location_path_s tmp=FS_LOCATION_PATH_INIT;

	assign_buffer_location_path(&tmp, buffer, len);
	append_location_path(&tmp, 'p', (void *) &a->type.path);
	append_location_path(&tmp, 'c', a->name);

	equal=compare_devino_location_path(&tmp, &b->type.devino);

    } else {

	equal=compare_devino_location_path(&a->type.path, &b->type.devino);

    }

    return equal;

}

int compare_fs_locations(struct fs_location_s *a, struct fs_location_s *b)
{
    int equal=-1;

    if (a->flags & FS_LOCATION_FLAG_PATH) {

	if (b->flags & FS_LOCATION_FLAG_PATH) {

	    if (a->name && b->name) {

		if (strcmp(a->name, b->name)==0) {

		    equal=compare_location_paths(&a->type.path, &b->type.path);

		}

	    } else if (a->name) {

		equal=compare_location_path(&a->type.path, a->name, 'p', (void *) &b->type.path);

	    } else if (b->name) {

		equal=compare_location_path(&b->type.path, b->name, 'p', (void *) &a->type.path);

	    } else {

		equal=compare_location_paths(&b->type.path, &a->type.path);

	    }

	} else if (b->flags & FS_LOCATION_FLAG_DEVINO) {

	    equal=compare_fs_devino_location_path(b, a);

	} else if (b->flags & FS_LOCATION_FLAG_SOCKET) {

	    equal=compare_fs_socket_location(b, a);

	}

    } else if (a->flags & FS_LOCATION_FLAG_DEVINO) {

	if (b->flags & FS_LOCATION_FLAG_PATH) {

	    equal=compare_fs_devino_location_path(a, b);

	} else if (b->flags & FS_LOCATION_FLAG_DEVINO) {

	    equal=compare_devinos(a, b);

	} else if (b->flags & FS_LOCATION_FLAG_SOCKET) {

	    equal=compare_fs_socket_location(b, a);

	}

    } else if (a->flags & FS_LOCATION_FLAG_SOCKET) {

	equal=compare_fs_socket_location(a, b);

    }

    return equal;

}
