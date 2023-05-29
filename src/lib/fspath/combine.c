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

unsigned int fs_path_combine(struct fs_path_s *result, struct fs_path_s *path, const unsigned char type, void *ptr)
{
    int size=0;
    char *pstart2=NULL;
    unsigned int plen2=0;

    if ((result->buffer==NULL) || (result->size==0)) return 0;

    switch (type) {

	case 'c' : {

	    pstart2=(char *) ptr;
	    plen2=strlen(pstart2);
	    break;

	}

	case 's' : {
	    struct ssh_string_s *other=(struct ssh_string_s *) ptr;

	    pstart2=other->ptr;
	    plen2=other->len;
	    break;

	}

	case 'p' : {
	    struct fs_path_s *other=(struct fs_path_s *) ptr;

	    pstart2=&other->buffer[other->start];
	    plen2=other->len;
	    break;

	}

	case 'n' : {
	    struct name_string_s *other=(struct name_string_s *) ptr;

	    pstart2=other->ptr;
	    plen2=other->len;
	    break;

	}

    }

    if (pstart2) {
        char *pstart1=&path->buffer[path->start];
        unsigned int plen1=path->len;

#ifdef __linux__

        memset(result->buffer, 0, result->size);
	size=snprintf(result->buffer, result->size, "%.*s/%.*s", plen1, pstart1, plen2, pstart2);

	if (size<0) {

	    logoutput_error("fs_path_combine: error %i appending (%s)", errno, strerror(errno));
	    size=0;

	} else {

	    result->start=0;
	    result->len=size;

        }

#endif

    }

    return size;

}

