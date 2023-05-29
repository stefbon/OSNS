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
#include "libosns-fspath.h"

static unsigned int get_fieldsize_uint(unsigned int value)
{
    return (unsigned int)(log10((double) value) + 1);
}

unsigned int fs_path_get_append_required_size(struct fs_path_s *path, const unsigned char type, void *ptr)
{
    unsigned int len=0;

    switch (type) {

	case 'c' : {
	    char *name=(char *) ptr;

	    len=path->len + 2 + strlen(name);
	    break;

	}

	case 'p' : {
	    struct fs_path_s *other=(struct fs_path_s *) ptr;

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

unsigned int fs_path_append_raw(struct fs_path_s *path, char *pstart2, unsigned int plen2)
{
    char *pstart1=&path->buffer[path->start + path->len];
    unsigned int pleft1=(path->size - path->start - path->len);
    unsigned int result=0;

    if (plen2 >= pleft1) {
        unsigned int extra=plen2 + 1;

        /* not enough space in buffer ... only enlarge when
            buffer is allocated before */

        if ((path->flags & FS_PATH_FLAG_BUFFER_ALLOC)==0) return 0;

        path->buffer=realloc(path->buffer, path->size + extra);
        if (path->buffer==NULL) return 0;
        memset(&path->buffer[path->size], 0, extra);
        path->size += extra;

    }

    memcpy(pstart1, pstart2, plen2);
    result=plen2;
    path->len += plen2;

    return result;
}

unsigned int fs_path_append(struct fs_path_s *path, const unsigned char type, void *ptr)
{
    char *pstart2=NULL;
    unsigned int plen2=0;
    unsigned int result=0;

    if (path->buffer==NULL) return 0;

    switch (type) {

	case 'c' : {
	    char *name=(char *) ptr;

            pstart2=name;
            plen2=strlen(name);
	    break;

	}

	case 's' : {
	    struct ssh_string_s *s=(struct ssh_string_s *) ptr;

	    pstart2=s->ptr;
	    plen2=s->len;
	    break;

	}

	case 'p' : {
	    struct fs_path_s *other=(struct fs_path_s *) ptr;

	    if (other->buffer) {

	        pstart2=&other->buffer[other->start];
	        plen2=other->len;

            }
	    break;

        }

	case 'n' : {
	    struct name_string_s *n=(struct name_string_s *) ptr;

            pstart2=n->ptr;
            plen2=n->len;
	    break;

	}

    }

    if (pstart2) result=fs_path_append_raw(path, pstart2, plen2);
    return result;

}

void fs_path_prepend_init(struct fs_path_s *path)
{
    char *pstart1=NULL;

    /* init: start at the end to append backwards */

    path->start=path->size - 1;

    /* trailing zero */

    pstart1=&path->buffer[path->start];
    *pstart1='\0';
    path->len=1;

}

void fs_path_prepend_raw(struct fs_path_s *path, char *pstart2, unsigned int plen2)
{
    char *pstart1=NULL;

    path->start -= plen2;
    pstart1=&path->buffer[path->start];
    memcpy(pstart1, pstart2, plen2);
    path->len += plen2;

}
