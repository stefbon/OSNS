/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "ssh-uint.h"
#include "name-string.h"

void init_name_string(struct name_string_s *s)
{

    if (s) {

	memset(s, 0, sizeof(struct name_string_s));
	s->ptr=NULL;
	s->len=0;

    }

}

void set_name_string(struct name_string_s *s, const unsigned char type, char *ptr)
{

    if (ptr) {

	if (type=='c') {

	    s->len=strlen(ptr);
	    s->ptr=ptr;

	} else if (type=='n') {
	    struct name_string_s *tmp=(struct name_string_s *) ptr;

	    s->len=tmp->len;
	    s->ptr=tmp->ptr;

	}

    } else {

	init_name_string(s);

    }

}

int compare_name_string(struct name_string_s *t, const unsigned char type, void *ptr)
{
    switch (type) {

	case 'c' : {
	    char *data=(char *) ptr;
	    unsigned int len=strlen(data);

	    return ((len==t->len) && memcmp(data, t->ptr, t->len)==0) ? 0 : -1;
	    }

	case 'n' : {
	    struct name_string_s *s=(struct name_string_s *) ptr;
	    return ((s->len==t->len) && memcmp(s->ptr, t->ptr, t->len)==0) ? 0 : -1;
	    }

    }

    return -1;

}

unsigned int read_name_string(char *buffer, unsigned int size, struct name_string_s *s)
{
    struct name_string_s dummy=NAME_STRING_INIT;

    if (s==NULL) s=&dummy;

    s->len=0;
    s->ptr=NULL;

    if (size >= 1) {

	s->len=(unsigned char)buffer[0];

	if (size >= 1 + s->len) {

	    s->ptr=(char *) (buffer + 1);
	    return (1 + s->len);

	}

    }

    return 0;

}

unsigned int write_name_string(char *buffer, unsigned int size, const unsigned char type, void *ptr)
{
    char *pos=NULL;

    switch (type) {

	case 'c' : {
	    char *data=(char *) ptr;
	    unsigned int len=strlen(data);

	    if (buffer) {
		char *pos=buffer;

		pos[0]=(unsigned char) len;
		memcpy(&pos[1], data, len);

	    }

	    return (1 + len);
	    break;
	}

	case 'n' : {
	    struct name_string_s *s=(struct name_string_s *) ptr;

	    if (buffer) {
		char *pos=buffer;

		pos[0]=(unsigned char) (s->len);
		memcpy(&pos[1], s->ptr, s->len);

	    }

	    return (1 + s->len);
	    break;
	}

    }

    return 0;

}

static unsigned int copy_name_string_hlpr(struct name_string_s *s, char *data, unsigned int size, unsigned int flags)
{
    unsigned int bytescopied=0;

    if (size <= s->len) {

	memcpy(s->ptr, data, size);
	bytescopied=size;

    } else if (flags & COPY_NAME_STRING_FLAG_ALLOW_TRUNCATE) {

	memcpy(s->ptr, data, s->len);
	bytescopied=s->len;

    }

    return bytescopied;
}

unsigned int copy_name_string(struct name_string_s *s, const unsigned char type, void *ptr, unsigned int flags)
{
    unsigned int bytescopied=0;

    switch (type) {

	case 'c' : {
	    char *data=(char *) ptr;
	    unsigned int len=strlen(data);

	    bytescopied=copy_name_string_hlpr(s, data, len, flags);
	    break;

	}

	case 'n' : {
	    struct name_string_s *o=(struct name_string_s *) ptr;

	    bytescopied=copy_name_string_hlpr(s, o->ptr, o->len, flags);
	    break;

	}

    }

    return bytescopied;
}
