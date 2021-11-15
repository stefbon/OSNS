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

#include "fshandle.h"
#include "open.h"

static int _open_error(struct filehandle_s *handle, struct fs_location_s *location, unsigned int flags, struct fs_init_s *init)
{
    logoutput_warning("open_error: handle already open");
    return -1;
}

static int _pread_error(struct filehandle_s *handle, char *data, unsigned int size, off_t off)
{
    logoutput_warning("_pread_error: handle not open");
    return -1;
}

static int _pwrite_error(struct filehandle_s *handle, char *data, unsigned int size, off_t off)
{
    logoutput_warning("_pwrite_error: handle not open");
    return -1;
}

static int _fsync_error(struct filehandle_s *handle, unsigned int flags)
{
    logoutput_warning("_fsync_error: handle not open");
    return -1;
}

static int _flush_error(struct filehandle_s *handle)
{
    logoutput_warning("_flush_error: handle not open");
    return -1;
}

static int _fgetstat_error(struct filehandle_s *handle, unsigned int mask, struct system_stat_s *st)
{
    logoutput_warning("_fgetstat_error: handle not open");
    return -1;
}

static int _fsetstat_error(struct filehandle_s *handle, unsigned int mask, struct system_stat_s *st)
{
    logoutput_warning("_fsetstat_error: handle not open");
    return -1;
}

static off_t _lseek_error(struct filehandle_s *handle, off_t off, int whence)
{
    logoutput_warning("_lseek_error: handle not open");
    return -1;
}

static void _close_error(struct filehandle_s *handle)
{
    logoutput_warning("_close_error: handle not open");
}

#ifdef __linux__

static int _pread_handle(struct filehandle_s *handle, char *data, unsigned int size, off_t off)
{
    return system_pread(&handle->socket, data, size, off);
}

static int _pwrite_handle(struct filehandle_s *handle, char *data, unsigned int size, off_t off)
{
    return system_pwrite(&handle->socket, data, size, off);
}

static int _fsync_handle(struct filehandle_s *handle, unsigned int flags)
{
    return ((flags & FSYNC_FLAG_DATASYNC) ? system_fdatasync(&handle->socket) : system_fsync(&handle->socket));
}

static int _flush_handle(struct filehandle_s *handle)
{
    /* what to do here ? */
    return 0;
}

static off_t _lseek_handle(struct filehandle_s *handle, off_t off, int whence)
{
    return system_lseek(&handle->socket, off, whence);
}

static void _close_handle(struct filehandle_s *handle)
{
    system_close(&handle->socket);
}

static int _fgetstat_handle(struct filehandle_s *handle, unsigned int mask, struct system_stat_s *stat)
{
    return system_fgetstat(&handle->socket, mask, stat);
}

static int _fsetstat_handle(struct filehandle_s *handle, unsigned int mask, struct system_stat_s *stat)
{
    return system_fsetstat(&handle->socket, mask, stat);
}

static int _open_handle(struct filehandle_s *fh, struct fs_location_s *location, unsigned int flags, struct fs_init_s *init)
{
    int result=-1;

    if (fh->flags & FILEHANDLE_FLAG_CREATE) {

	if (location->name==NULL) {

	    logoutput("_open_handle: creating a file but name is not specified");
	    goto error;

	}

	if (init==NULL) {

	    logoutput("_open_handle: creating a file but init is not specified");
	    goto error;

	}

    }

    /* creating file depends on via a path or a filedescriptor */

    if (fh->flags & FILEHANDLE_FLAG_CREATE) {

	if (location->flags & FS_LOCATION_FLAG_PATH) {
	    unsigned int len=append_location_path_get_required_size(&location->type.path, 'c', location->name);
	    char buffer[len];
	    struct fs_location_path_s tmp;

	    set_buffer_location_path(&tmp, buffer, len, 0);

	    if (combine_location_path(&tmp, &location->type.path, 'c', location->name)>0) {

		result = system_create(&tmp, flags, init, &fh->socket);

	    }

	} else if (location->flags & FS_LOCATION_FLAG_AT) {

	    result = system_creatat(&location->type.socket, location->name, flags, init, &fh->socket);

	}

    } else {

	if (location->flags & FS_LOCATION_FLAG_PATH) {

	    result = system_open(&location->type.path, flags, &fh->socket);

	} else if (location->flags & FS_LOCATION_FLAG_AT) {

	    result = system_openat(&location->type.socket, location->name, flags, &fh->socket);

	}

    }

    if (result>=0) {

	/* success */

	fh->open=_open_error; /* already open so another open call is an error */
	fh->close=_close_handle;
	fh->pwrite=_pwrite_handle;
	fh->pread=_pread_handle;
	fh->flush=_flush_handle;
	fh->fsync=_fsync_handle;
	fh->fgetstat=_fgetstat_handle;
	fh->fsetstat=_fsetstat_handle;
	fh->lseek=_lseek_handle;
	return 0;

    }

    error:

    logoutput_warning("_open_handle: unable to open");
    _close_handle(fh);
    return -1;

}

#endif

void init_filehandle(struct filehandle_s *fh)
{

    fh->flags=0;
    fh->open=_open_error;
    fh->pwrite=_pwrite_error;
    fh->pread=_pread_error;
    fh->flush=_flush_error;
    fh->fsync=_fsync_error;
    fh->fsetstat=_fsetstat_error;
    fh->fgetstat=_fgetstat_error;
    fh->lseek=_lseek_error;
    fh->close=_close_error;
    init_fs_socket(&fh->socket);

}

void enable_filehandle(struct filehandle_s *fh)
{
    fh->flags |= FILEHANDLE_FLAG_ENABLED;
    fh->open=_open_handle;
}

void free_filehandle(struct filehandle_s *fh)
{
    /* nothing to free */
}
