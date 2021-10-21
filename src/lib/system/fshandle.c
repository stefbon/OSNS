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

#include "fshandle.h"
#include "file.h"
#include "directory.h"
#include "hash.h"

void free_commonhandle(struct commonhandle_s **p_handle)
{
    struct commonhandle_s *handle=(p_handle) ? *p_handle : NULL;
    unsigned int flags=0;
    struct simple_lock_s lock;

    if (handle==NULL) return;
    flags=handle->flags;

    writelock_commonhandles(&lock);
    remove_commonhandle_hash(handle);
    unlock_commonhandles(&lock);

    if (flags & COMMONHANDLE_FLAG_FILE) {

	free_filehandle(&handle->type.file);

    } else if (flags & COMMONHANDLE_FLAG_DIR) {

	free_dirhandle(&handle->type.dir);
    }

    if (handle->location.name && (flags & COMMONHANDLE_FLAG_NAME_ALLOC)) {

	free(handle->location.name);
	handle->location.name=NULL;

    }

    (* handle->clear_buffer)(handle);
    memset(handle, 0, sizeof(struct commonhandle_s));

    if (flags & COMMONHANDLE_FLAG_ALLOC) {

	free(handle);
	*p_handle=NULL;

    }

}

static void _clear_buffer_default(struct commonhandle_s *handle)
{
    memset(handle->buffer, 0, handle->size);
}

static unsigned int _get_access_default(struct commonhandle_s *handle)
{
    return ((handle->flags & COMMONHANDLE_FLAG_DIR) ? 1 : 0);
}

static unsigned int _get_flags_default(struct commonhandle_s *handle)
{
    return 0;
}

struct commonhandle_s *create_commonhandle(unsigned char type, struct fs_location_s *location, unsigned int size)
{
    struct commonhandle_s *handle=NULL;

    if (type != COMMONHANDLE_TYPE_DIR && type != COMMONHANDLE_TYPE_FILE) return NULL;

    handle=malloc(sizeof(struct commonhandle_s) + size);

    if (handle) {

	memset(handle, 0, sizeof(struct commonhandle_s) + size);
	handle->flags=COMMONHANDLE_FLAG_ALLOC;
	handle->clear_buffer=_clear_buffer_default;
	handle->get_access=_get_access_default;
	handle->get_flags=_get_flags_default;

	if (location) {

	    memcpy(&handle->location, location, sizeof(struct fs_location_s));

	    if (location->name) {

		handle->location.name=strdup(location->name);
		if (handle->location.name) {

		    handle->flags |= COMMONHANDLE_FLAG_NAME_ALLOC;

		} else {

		    logoutput("create_commonhandle: failed to allocate size for %s", location->name);
		    goto error;

		}

	    }

	}

	handle->size=size;

	switch (type) {

	    case COMMONHANDLE_TYPE_DIR:

		handle->flags |= COMMONHANDLE_FLAG_DIR;
		init_dirhandle(&handle->type.dir);
		break;

	    case COMMONHANDLE_TYPE_FILE:

		handle->flags |= COMMONHANDLE_FLAG_FILE;
		init_filehandle(&handle->type.file);
		break;

	}

    }

    return handle;

    error:
    free_commonhandle(&handle);
    return NULL;

}
pid_t get_pid_commonhandle(struct commonhandle_s *handle)
{

    if (handle->flags & COMMONHANDLE_FLAG_DIR) {

	return get_unix_pid_fs_socket(&handle->type.dir.socket);

    } else if (handle->flags & COMMONHANDLE_FLAG_FILE) {

	return get_unix_pid_fs_socket(&handle->type.file.socket);

    }

    return 0;
}

int get_fd_commonhandle(struct commonhandle_s *handle)
{

    if (handle->flags & COMMONHANDLE_FLAG_DIR) {

	return get_unix_fd_fs_socket(&handle->type.dir.socket);

    } else if (handle->flags & COMMONHANDLE_FLAG_FILE) {

	return get_unix_fd_fs_socket(&handle->type.file.socket);

    }

    return -1;
}
