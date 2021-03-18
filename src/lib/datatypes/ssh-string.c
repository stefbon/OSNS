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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ssh-uint.h"
#include "ssh-string.h"
#include "log.h"

struct ssh_string_s *create_ssh_string(struct ssh_string_s **p_s, unsigned int len, char *data, unsigned char flags)
{
    struct ssh_string_s *s=(p_s) ? *p_s : NULL;

    // logoutput("create_ssh_string: len %i flags %i", len, flags);

    if (s==NULL) {

	s=malloc(sizeof(struct ssh_string_s) + len);

	if (s) {

	    s->flags=0;
	    s->len=len;
	    s->size=len;
	    s->ptr=s->buffer;
	    if (len>0 && data) memcpy(s->ptr, data, len);

	}

	*p_s=s;

    } else {

	if (s->size==0) {

	    if (flags & SSH_STRING_FLAG_ALLOC) {

		s->ptr=malloc(len);

		if (s->ptr) {

		    if (data) memcpy(s->ptr, data, len);
		    s->len=len;
		    s->flags |= SSH_STRING_FLAG_ALLOC;

		} else {

		    s=NULL; /* failed */

		}

	    } else {

		if (s->flags & SSH_STRING_FLAG_ALLOC) {

		    if (s->ptr) free(s->ptr);
		    s->flags -= SSH_STRING_FLAG_ALLOC;

		}

		s->ptr=data;
		s->len=len;

	    }

	} else if (s->size>0) {

	    if (len==s->len) {

		memcpy(s->buffer, data, len);

	    } else {

		s=realloc(s, sizeof(struct ssh_string_s) + len);

		if (s) {

		    if (data) memcpy(s->buffer, data, len);
		    s->len=len;
		    s->size=len;
		    s->ptr=s->buffer; /* possibly changed*/

		}

		*p_s=s;

	    }

	} else {

	    /* serious error */

	}

    }

    return s;
}

void init_ssh_string(struct ssh_string_s *s)
{

    if (s) {

	memset(s, 0, sizeof(struct ssh_string_s));
	s->flags=0;
	s->ptr=NULL;
	s->len=0;
	s->size=0;

    }

}

void set_ssh_string(struct ssh_string_s *s, const unsigned char type, char *ptr)
{
    if (s->size>0) return;

    if (ptr) {

	if (type=='c') {

	    s->len=strlen(ptr);
	    s->ptr=ptr;

	} else if (type=='s') {
	    struct ssh_string_s *tmp=(struct ssh_string_s *) ptr;

	    s->len=tmp->len;
	    s->ptr=tmp->ptr;

	}

    } else {

	init_ssh_string(s);

    }

}

void clear_ssh_string(struct ssh_string_s *s)
{

    if (s) {

	if (s->size==0) {

	    if (s->flags & SSH_STRING_FLAG_ALLOC) {

		if (s->ptr) free(s->ptr);
		s->flags -= SSH_STRING_FLAG_ALLOC;

	    }

	    s->ptr=NULL;

	} else {

	    memset(s->buffer, 0, s->size);

	}

    }

}

void free_ssh_string(struct ssh_string_s **p_s)
{
    struct ssh_string_s *s=(p_s) ? *p_s : NULL;

    if (s) {

	clear_ssh_string(s);

	if (s->size>0) {

	    free(s);
	    *p_s=NULL;

	}

    }

}

unsigned char ssh_string_isempty(struct ssh_string_s *s)
{
    return (s->ptr ? 0 : 1);
}

int create_copy_ssh_string(struct ssh_string_s **p_t, struct ssh_string_s *s)
{
    struct ssh_string_s *t=(p_t) ? *p_t : NULL;
    int result=0;

    if (s==NULL) return -1;

    if (t==NULL) {

	if (s->size==0) {

	    if (s->flags & SSH_STRING_FLAG_ALLOC) {

		char *data=malloc(s->len);
		t=malloc(sizeof(struct ssh_string_s));

		if (t && data) {

		    t->flags=s->flags;
		    t->len=s->len;
		    t->size=0;
		    t->ptr=data;
		    memcpy(data, s->ptr, s->len);

		} else {

		    if (t) free(t);
		    if (data) free(data);
		    t=NULL;
		    result=-1;

		}

		*p_t=t;

	    } else {

		t=malloc(sizeof(struct ssh_string_s));

		if (t) {

		    t->flags=s->flags;
		    t->len=s->len;
		    t->size=0;
		    t->ptr=s->ptr;

		} else {

		    result=-1;

		}

		*p_t=t;

	    }

	} else if (s->size>0) {

	    /* dealing with a "data" inluded ssh string */

	    t=malloc(sizeof(struct ssh_string_s) + s->size);

	    if (t) {

		t->flags=s->flags;
		t->len=s->len;
		t->size=s->size;
		memcpy(t->buffer, s->buffer, s->len);
		t->ptr=t->buffer;

	    } else {

		result=-1;

	    }

	    *p_t=t;

	}

    } else {

	if (s->size==0) {

	    if (t->size>0) {

		if (t->len == s->len) {

		    memcpy(t->buffer, s->ptr, s->len);

		} else {

		    t=realloc(t, sizeof(struct ssh_string_s) + s->len);

		    if (t) {

			t->ptr=t->buffer;
			t->len=s->len;
			t->size=s->len;
			memcpy(t->buffer, s->ptr, s->len);

		    } else {

			result=-1;

		    }

		    *p_t=t;

		}

	    } else if (t->size==0) {

		if (t->len == s->len) {

		    memcpy(t->ptr, s->ptr, s->len);

		} else {

		    t->ptr=realloc(t->ptr, s->len);

		    if (t->ptr) {

			t->len=s->len;
			memcpy(t->ptr, s->ptr, s->len);

		    } else {

			result=-1;

		    }

		}

	    }

	} else if (s->size>0) {

	    if (t->size>0) {

		if (t->size==s->size) {

		    t->flags=s->flags;
		    memcpy(t->buffer, s->buffer, s->len);

		} else {

		    t=realloc(t, sizeof(struct ssh_string_s) + s->len);

		    if (t) {

			t->len=s->len;
			t->size=t->len;
			t->flags=s->flags;
			memcpy(t->buffer, s->buffer, s->len);
			t->ptr=t->buffer;

		    } else {

			result=-1;

		    }

		    *p_t=t;

		}

	    } else if (t->size==0) {

		if (s->len==t->len) {

		    memcpy(t->ptr, s->buffer, s->len);

		} else {

		    t->ptr=realloc(t->ptr, s->len);

		    if (t->ptr) {

			memcpy(t->ptr, s->buffer, s->len);
			t->len=s->len;

		    } else {

			result=-1;

		    }

		}

	    }

	}

    }

    return result;

}

int compare_ssh_string(struct ssh_string_s *t, const unsigned char type, void *ptr)
{
    switch (type) {

    case 's' : {
	struct ssh_string_s *s=(struct ssh_string_s *) ptr;
	return ((s->len==t->len) && memcmp(s->ptr, t->ptr, t->len)==0) ? 0 : -1;
	}
    case 'c' : {
	char *data=(char *) ptr;
	unsigned int len=strlen(data);

	return ((len==t->len) && memcmp(data, t->ptr, t->len)==0) ? 0 : -1;
	}
    }

    return -1;

}

int ssh_string_compare(struct ssh_string_s *s, const unsigned char type, void *ptr)
{
    return compare_ssh_string(s, type, ptr);
}

unsigned int get_ssh_string_length(struct ssh_string_s *s, unsigned int flags)
{
    unsigned int len=0;
    if (flags & SSH_STRING_FLAG_HEADER) len+=4;
    if (flags & SSH_STRING_FLAG_DATA) len+=s->len;
    return len;
}

unsigned int read_ssh_string_header(char *buffer, unsigned int size, unsigned int *len)
{

    if (size >= 4) {

	if (len) *len=get_uint32(buffer);
	return 4;

    }

    return 0;

}

unsigned int write_ssh_string_header(char *buffer, unsigned int size, unsigned int len)
{

    if (size >= 4 + len) {

	store_uint32(buffer, len);
	return 4;
    }

    return 0;
}

unsigned int read_ssh_string(char *buffer, unsigned int size, struct ssh_string_s *s)
{
    struct ssh_string_s dummy=SSH_STRING_INIT;

    if (s==NULL) s=&dummy;

    if (s->size==0) {

	s->len=0;
	s->ptr=NULL;

	if (size >= 4) {

	    s->len=get_uint32(buffer);

	    if (size >= 4 + s->len) {

		s->ptr=(char *) (buffer + 4);
		return (4 + s->len);

	    }

	}

    }

    return 0;

}

unsigned int write_ssh_string(char *buffer, unsigned int size, const unsigned char type, void *ptr)
{
    char *pos=NULL;

    switch (type) {

    case 's' : {
	struct ssh_string_s *s=(struct ssh_string_s *) ptr;

	if (buffer) {
	    char *pos=buffer;

	    store_uint32(pos, s->len);
	    pos+=4;
	    memcpy(pos, s->ptr, s->len);

	}

	return (4 + s->len);
	break;
    }
    case 'c' : {
	char *data=(char *) ptr;
	unsigned int len=strlen(data);

	if (buffer) {
	    char *pos=buffer;

	    store_uint32(pos, len);
	    pos+=4;
	    memcpy(pos, data, len);

	}

	return (4 + len);
	break;
    }
    case 'l' : {
	unsigned int len=(ptr) ? *((unsigned int *) ptr) : 0;

	if (buffer) store_uint32(buffer, len);
	return 4;
    }
    default :

	break;

    }

    return 0;

}

void move_ssh_string(struct ssh_string_s *a, struct ssh_string_s *b, unsigned int flags)
{

    /* move only possible when both strings do not have "inside" data */

    if (a->size==0 && b->size==0) {

	a->ptr=b->ptr;
	a->len=b->len;
	a->flags=b->flags;

	if ((flags & SSH_STRING_FLAG_COPY)==0) {

	    /* it's a move: b releases the data */

	    init_ssh_string(b);

	}

    }
}

unsigned int buffer_count_strings(char *buffer, unsigned int size, unsigned int max)
{
    unsigned int count=0;
    unsigned int pos=0;
    unsigned int len=0;

    readstring:

    len=get_uint32(&buffer[pos]);

    if (pos + 4 + len <= size) {

	pos += 4 + len;
	count++;
	if (max>0 && count==max) goto out;
	if (pos < size) goto readstring;

    }

    out:

    return count;

}
