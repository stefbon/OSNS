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

#include <math.h>

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"

#include "path.h"
#include "open.h"
#include "location.h"

static unsigned int get_fieldsize_uint(unsigned int value)
{
    return (unsigned int)(log10((double) value) + 1);
}

unsigned int append_location_path_get_required_size(struct fs_location_path_s *path, const unsigned char type, void *ptr)
{
    unsigned int len=0;

    switch (type) {

	case 'c' : {
	    char *name=(char *) ptr;

	    len=path->len + 2 + strlen(name);
	    break;

	}

	case 'p' : {
	    struct fs_location_path_s *other=(struct fs_location_path_s *) ptr;

	    len=path->len + 2 + other->len;
	    break;

	}

	case 's' : {
	    struct ssh_string_s *other=(struct ssh_string_s *) ptr;

	    len=path->len + 2 + other->len;
	    break;

	}

	case 'n' : {
	    struct name_string_s *other=(struct name_string_s *) ptr;

	    len=path->len + 2 + other->len;
	    break;

	}

	case 'u' : {
	    unsigned int tmp=*((unsigned int *) ptr);

	    len=path->len + 2 + get_fieldsize_uint(tmp);
	    break;

	}


    }

    return len;

}

void assign_buffer_location_path(struct fs_location_path_s *path, char *buffer, unsigned int len)
{
    path->flags=0;
    path->ptr=buffer;
    path->len=0;
    path->size=len;
}

void set_location_path(struct fs_location_path_s *path, const unsigned char type, void *ptr)
{

    switch (type) {

	case 's': {

	    struct ssh_string_s *tmp=(struct ssh_string_s *) ptr;

	    path->flags=0;
	    path->ptr=tmp->ptr;
	    path->len=tmp->len;
	    path->size=tmp->len;

	    break;

	}

	case 'p': {

	    struct fs_location_path_s *tmp=(struct fs_location_path_s *) ptr;

	    path->flags=0;
	    path->ptr=tmp->ptr;
	    path->len=tmp->len;
	    path->size=tmp->size;

	    break;

	}

	case 'c': {
	    char *buffer=(char *) ptr;

	    path->flags=0;
	    path->ptr=buffer;
	    path->len=strlen(buffer);
	    path->size=path->len;

	    break;

	}

	case 'n': {

	    struct name_string_s *tmp=(struct name_string_s *) ptr;

	    path->flags=0;
	    path->ptr=tmp->ptr;
	    path->len=tmp->len;
	    path->size=tmp->len;

	    break;

	}

	case 'z': {

	    path->flags=0;
	    path->ptr=NULL;
	    path->len=0;
	    path->size=0;

	    break;

	}

    }

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
    int size=0;
    char *tmp=NULL;
    unsigned int len=0;

    memset(result->ptr, 0, result->size);

    switch (type) {

	case 'c' : {

	    tmp=(char *) ptr;
	    len=strlen(tmp);
	    break;

	}

	case 's' : {
	    struct ssh_string_s *other=(struct ssh_string_s *) ptr;

	    tmp=other->ptr;
	    len=other->len;
	    break;

	}

	case 'p' : {
	    struct fs_location_path_s *other=(struct fs_location_path_s *) ptr;

	    tmp=other->ptr;
	    len=other->len;
	    break;

	}

	case 'n' : {
	    struct name_string_s *other=(struct name_string_s *) ptr;

	    tmp=other->ptr;
	    len=other->len;
	    break;


	}


    }

    if (tmp) {

#ifdef __linux__

	size=snprintf(result->ptr, result->size, "%.*s/%.*s", path->len, path->ptr, len, tmp);

	if (size<0) {

	    logoutput_error("combine_location_path: error %i appending (%s)", errno, strerror(errno));
	    result=0;

	}

	result->len=size;

#endif

    }

    return size;

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
	memcpy(&buffer[pos], tmp, len);
	result=len+1;

    } else if (left>0) {
	char tmp2[len + 1]; /* create a temp buffer to ease the copy */

	tmp2[0]='/';
	memcpy(&tmp2[1], tmp, len);
	memcpy(&buffer[pos], tmp2, left);
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

	case 'n' : {
	    struct name_string_s *n=(struct name_string_s *) ptr;

#ifdef __linux__

	    result=append_bytes_location_path(path, n->ptr, n->len);

#endif

	    break;

	}

	case 'u' : {
	    unsigned int value=*((unsigned int *) ptr);
	    unsigned int tmp=get_fieldsize_uint(value);
	    char buffer[tmp+1];

	    memset(buffer, 0, tmp+1);

#ifdef __linux__

	    /* use snprintf to convert an unsigned int to char buffer
		(note the - flag here: make it left alligned) */

	    if (snprintf(buffer, tmp+1, "%-u", value)>0) {

		result=append_bytes_location_path(path, buffer, strlen(buffer));

	    }

#endif

	    break;

	}

    }

    return result;

}

unsigned int get_unix_location_path_length(struct fs_location_path_s *path)
{

    logoutput_debug("get_unix_location_path_length: path size %u len %u", path->size, path->len);

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

    logoutput_debug("copy_unix_location_path: buffer %.*s (path len %u)", size, buffer, path->len);

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

unsigned char test_location_path_absolute(struct fs_location_path_s *path)
{

#ifdef __linux__

    return ((path->ptr[0]=='/') ? 1 : 0);

#else

    return 0;

#endif

}

unsigned char test_location_path_subdirectory(struct fs_location_path_s *path, const unsigned char type, void *ptr, struct fs_location_path_s *sub)
{
    unsigned char result=0;
    char *tmp=NULL;
    unsigned int len=0;

    switch (type) {

	case 's' : {
	    struct ssh_string_s *prefix=(struct ssh_string_s *) ptr;

	    len=prefix->len;
	    tmp=prefix->ptr;
	    break;

	}

	case 'p' : {
	    struct fs_location_path_s *prefix=(struct fs_location_path_s *) ptr;

	    len=prefix->len;
	    tmp=prefix->ptr;
	    break;

	}

	case 'c' : {

	    tmp=(char *) ptr;
	    len=strlen(tmp);
	    break;

	}

    }

    if (tmp) {

	if ((path->len > len) && (strncmp(path->ptr, tmp, len)==0) && (path->ptr[len]=='/')) {

	    result=1;

	    if (sub) {

		sub->ptr=(char *)(path->ptr + len);
		sub->len=path->len - len;

	    }

	}

    }

    return result;

}

#define HANDLE_PATH_FLAG_FINISH					1

#define HANDLE_PATH_ACTION_APPEND				0
#define HANDLE_PATH_ACTION_IGNORE				1
#define HANDLE_PATH_ACTION_UP					2

struct path_element_s {
    unsigned char	flags;
    unsigned char	action;
    unsigned int 	len;
};

static void handle_path_element(char *pos, unsigned int len, struct path_element_s *pa)
{

    /* default: append ... override in special cases */

    pa->action=HANDLE_PATH_ACTION_APPEND;
    pa->len=len;

    if (len>=3) {

	if (len>=4 && strncmp(pos, "/../", 4)==0) {

	    pa->action=HANDLE_PATH_ACTION_UP;
	    pa->len=4;

	} else if (len==3 && (strncmp(pos, "/..", 3)==0 || strncmp(pos, "../", 3)==0)) {

	    pa->action=HANDLE_PATH_ACTION_UP;
	    pa->len=3;

	} else if (strncmp(pos, "/./", 3)==0 || strncmp(pos, "///", 3)==0) {

	    pa->action=HANDLE_PATH_ACTION_IGNORE;
	    pa->len=3;

	} else if (strncmp(pos, "//", 2)==0 || strncmp(pos, "/.", 2)==0) {

	    pa->action=HANDLE_PATH_ACTION_IGNORE;
	    pa->len=2;

	}

    } else if (len==2) {

	if (strncmp(pos, "/.", 2)==0 || strncmp(pos, "//", 2)==0) {

	    pa->action=HANDLE_PATH_ACTION_IGNORE;
	    pa->len=2;

	}

    } else if (len==1) {

	if (strncmp(pos, ".", 1)==0) {

	    pa->action=HANDLE_PATH_ACTION_IGNORE;
	    pa->len=1;

	}

    }

}

unsigned int remove_unneeded_path_elements(struct fs_location_path_s *path)
{
    char *slash[2];
    char *pos=path->ptr;
    int left=(int) path->len;
    unsigned int len=0;
    int result=-1;
    struct path_element_s pa;

    logoutput_debug("remove_unneeded_path_elements: path %.*s", path->len, path->ptr);

    slash[0]=NULL;
    slash[1]=NULL;

    pa.flags=0;
    pa.action=0;
    pa.len=0;

    while (left>0 && (pa.flags & HANDLE_PATH_FLAG_FINISH)==0) {

	slash[1]=memchr(pos, '/', left);
	pa.len=0;
	pa.action=0;

	if (slash[1]==NULL) {

	    handle_path_element(pos, left, &pa);

	} else {

	    if (slash[1] > path->ptr) {

		handle_path_element(pos, (unsigned int)(slash[1] - pos + 1), &pa);

	    } else {

		pa.action=HANDLE_PATH_ACTION_APPEND;
		pa.len=1;

	    }

	}

	if (pa.action==HANDLE_PATH_ACTION_APPEND) {

	    pos+=pa.len;
	    left-=pa.len;
	    len += pa.len;

	    if (slash[1]) slash[0]=slash[1];

	} else if (pa.action==HANDLE_PATH_ACTION_IGNORE) {
	    char *new=pos + pa.len;

	    memmove(pos, new, left - pa.len);
	    pos+=pa.len;
	    left-=pa.len;

	} else if (pa.action==HANDLE_PATH_ACTION_UP) {

	    if (slash[0]) {
		char *new=pos + pa.len;
		unsigned int tmp=(unsigned int)(pos - slash[0]);

		pos=slash[0] + 1;

		/* remove the previous path element -> len decreases by the length of this element */

		memmove(pos, new, left - pa.len);
		left-= pa.len;

	    } else {

		/* no slash[0] -> no obvious previous path element -> two possible reasons:
		    - must be the start of the path (which has no slash)
		    - or it's the first element */

		char *new=pos + pa.len;
		unsigned int tmp=(unsigned int)(pos - path->ptr);

		pos=path->ptr;
		memmove(pos, new, left - pa.len);
		left-= pa.len;

		path->back++;

	    }

	}

    }

    out:

    if (len < path->len) {

	pos=(char *)(path->ptr + len);
	left=(unsigned int)(path->len - len);
	memset(pos, 0, left);

    }

    return len;

}
