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

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"

#include "fspath.h"

void fs_path_assign_buffer(struct fs_path_s *path, char *buffer, unsigned int size)
{
    path->flags=0;
    path->back=0;
    path->size=size;
    path->start=0;
    path->len=0;
    path->buffer=buffer;
}

void fs_path_set(struct fs_path_s *path, const unsigned char type, void *ptr)
{

    switch (type) {

	case 's': {
	    struct ssh_string_s *tmp=(struct ssh_string_s *) ptr;
            fs_path_assign_buffer(path, tmp->ptr, tmp->len);
	    break;
	}

	case 'p': {
	    struct fs_path_s *tmp=(struct fs_path_s *) ptr;
	    memcpy(tmp, path, sizeof(struct fs_path_s));
	    break;
	}

	case 'c': {
	    char *data=(char *) ptr;
	    fs_path_assign_buffer(path, data, strlen(data));
	    break;

	}

	case 'n': {
	    struct name_string_s *tmp=(struct name_string_s *) ptr;
	    fs_path_assign_buffer(path, tmp->ptr, tmp->len);
	    break;

	}

	case 'z': {
	    fs_path_assign_buffer(path, NULL, 0);
	    break;

	}

    }

}

void fs_path_clear(struct fs_path_s *path)
{

    if (path->flags & FS_PATH_FLAG_BUFFER_ALLOC) {

	free(path->buffer);
	path->flags &= ~FS_PATH_FLAG_BUFFER_ALLOC;

    }

    fs_path_assign_buffer(path, NULL, 0);

}

