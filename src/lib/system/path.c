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

#include "path.h"
#include "open.h"
#include "location.h"

unsigned int append_location_path_get_required_size(struct fs_location_path_s *path, const unsigned char type, void *ptr)
{
    unsigned int len=0;

#ifdef __linux__

    switch (type) {

	case 'c' : {
	    char *name=(char *) ptr;

	    len=path->len + 2 + strlen(name);

	}

    }

#endif

    return len;

}

void set_buffer_location_path(struct fs_location_path_s *path, char *buffer, unsigned int size, unsigned int flags)
{
    path->flags=flags;
    path->ptr=buffer;
    path->size=size;
    path->len=0;
}

void clear_location_path(struct fs_location_path_s *path)
{

    if (path->flags & FS_LOCATION_PATH_FLAG_PTR_ALLOC) {

	free(path->ptr);
	path->flags &= ~FS_LOCATION_PATH_FLAG_PTR_ALLOC;

    }

    path->ptr=NULL;
    path->size=0;
    path->len=0;

}

unsigned int combine_location_path(struct fs_location_path_s *result, struct fs_location_path_s *path, const unsigned char type, void *ptr)
{
    int tmp=0;

    memset(result->ptr, 0, result->size);

    switch (type) {

	case 'c' : {
	    char *name=(char *) ptr;

#ifdef __linux__

	    tmp=snprintf(result->ptr, result->size, "%.*s/%s", path->len, path->ptr, name);

	    if (tmp<0) {

		logoutput("combine_location_path: error %i appending %s (%s)", errno, name, strerror(errno));
		return 0;

	    }
#endif

	}

    }

    result->len=(unsigned int) tmp;
    return result->len;

}

static int compare_paths_with_extra(char *a, unsigned int lena, char *b, unsigned int lenb, char *c, unsigned int lenc)
{
    unsigned int len=lena + lenb + 2;
    char d[len];

    if (len - 1 == lenc) {

	if (snprintf(d, len, "%.*s/%.*s", lena, a, lenb, b)>0) {

	    return memcmp(d, c, len);

	}

    }

    return -1;
}

int compare_location_paths(struct fs_location_path_s *a, struct fs_location_path_s *b)
{
    int result=-1;

#ifdef __linux__

    if (a->len==b->len && ((a->ptr==b->ptr) || memcmp(a->ptr, b->ptr, a->len)==0)) result=0;

#endif

    return result;

}


int compare_location_path(struct fs_location_path_s *path, char *extra, const unsigned char type, void *ptr)
{
    int result=-1;

#ifdef __linux__

    switch (type) {

	case 'c' : {
	    char *name=(char *) ptr;

	    if (extra) {

		result=compare_paths_with_extra(path->ptr, path->len, extra, strlen(extra), name, strlen(name));

	    } else {

		if (strlen(name)==path->len && strncmp(path->ptr, name, path->len)) result=0;

	    }

	}

	case 'p' : {
	    struct fs_location_path_s *tmp=(struct fs_location_path_s *) ptr;

	    if (extra) {

		result=compare_paths_with_extra(path->ptr, path->len, extra, strlen(extra), tmp->ptr, tmp->len);

	    } else {

		if (tmp->len==path->len && memcmp(tmp->ptr, path->ptr, path->len)) result=0;

	    }

	}

    }

#endif

    return result;

}

static unsigned int append_bytes_location_path(struct fs_location_path_s *path, char *ptr, unsigned int len)
{
    unsigned int result=0;
    char *buffer=path->ptr;
    unsigned int pos=path->len;
    int left=path->size - pos;
    char *tmp=NULL;

    if (ptr==NULL || len==0) return 0;

    /* skip starting slashes in ptr */

    tmp=ptr;
    while (*tmp=='/' && len>0) {

	tmp++;
	len--;

    }

    memset(&buffer[pos], 0, left);

    if (len + 1 <= left) {

	buffer[pos]='/';
	pos++;
	memcpy(&buffer[pos], ptr, len);
	result=len+1;

    } else if (left>0) {
	char tmp[len + 1]; /* create a temp buffer to ease the copy */

	tmp[0]='/';
	memcpy(&tmp[1], ptr, len);
	memcpy(&buffer[pos], tmp, left);

	result=left;

    }

    path->len+=result;
    return result;
}

unsigned int append_location_path(struct fs_location_path_s *path, const unsigned char type, void *ptr)
{
    unsigned int result=0;

    switch (type) {

	case 'c' : {
	    char *name=(char *) ptr;

#ifdef __linux__

	    result=append_bytes_location_path(path, name, strlen(name));

#endif

	    break;

	}

	case 's' : {
	    struct ssh_string_s *s=(struct ssh_string_s *) ptr;

#ifdef __linux__

	    result=append_bytes_location_path(path, s->ptr, s->len);

#endif

	    break;

	}

	case 'p' : {
	    struct fs_location_path_s *other=(struct fs_location_path_s *) ptr;

#ifdef __linux__

	    result=append_bytes_location_path(path, other->ptr, other->len);

#endif

	}


    }

    return result;

}

unsigned int get_unix_location_path_length(struct fs_location_path_s *path)
{
#ifdef __linux__
    return path->len;
#else
    return 0;
#endif
}

unsigned int copy_unix_location_path(struct fs_location_path_s *path, char *buffer, unsigned int size)
{
    unsigned int len=0;

#ifdef __linux__

    len=((path->len < size) ? path->len : size);
    memcpy(buffer, path->ptr, len);

#endif

    return len;

}

char *get_filename_location_path(struct fs_location_path_s *path)
{
    char *sep=memrchr(path->ptr, '/', path->len);

    return (sep ? sep + 1 : path->ptr);
}

void detach_filename_location_path(struct fs_location_path_s *path, struct ssh_string_s *filename)
{
    char *sep=memrchr(path->ptr, '/', path->len);

    if (sep) {

	*sep='\0';
	filename->ptr=(char *) (sep+1);
	filename->len=(unsigned int)(path->ptr + path->len - sep - 1);
	path->len = (unsigned int)(sep - path->ptr);


    } else {

	filename->ptr=path->ptr;
	filename->len=path->len;

    }

}
