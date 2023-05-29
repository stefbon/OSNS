/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "utils.h"
#include "replaceandskip.h"

void replace_cntrl_char(char *buffer, unsigned int size, unsigned char flags)
{
    unsigned char tmp=((flags & REPLACE_CNTRL_FLAG_UNDERSCORE) ? '_' : ' ');

    for (unsigned int i=0; i<size; i++) {

	if (flags & REPLACE_CNTRL_FLAG_BINARY) {

	    if (iscntrl(buffer[i])) buffer[i]=tmp;

	} else if (flags & REPLACE_CNTRL_FLAG_TEXT) {

	    if (! isalnum(buffer[i]) && ! ispunct(buffer[i])) {

		// logoutput("replace_cntrl_char: replace %i", 1, buffer[i]);
		buffer[i]=tmp;

	    }

	}

    }

}

void replace_slash_char(char *buffer, unsigned int size)
{
    for (unsigned int i=0; i<size; i++) if (buffer[i]=='/') buffer[i]=' ';
}

void replace_newline_char(char *ptr, unsigned int size)
{
    char *sep=NULL;

    sep=memchr(ptr, 13, size);
    if (sep) *sep='\0';

}

unsigned int skip_trailing_spaces(char *ptr, unsigned int size, unsigned int flags)
{
    unsigned int len=size;

    skipspace:

    if (len > 0 && isspace(ptr[len-1])) {

	if (flags & SKIPSPACE_FLAG_REPLACEBYZERO) ptr[len-1]='\0';
	len--;
	goto skipspace;

    }

    return (size - len);

}

unsigned int skip_heading_spaces(char *ptr, unsigned int size)
{
    unsigned int pos=0;

    skipspace:

    if (pos < size && isspace(ptr[pos])) {

	pos++;
	goto skipspace;

    }

    if (pos>0) {

	memmove(ptr, &ptr[pos], size - pos);
	memset(&ptr[size - pos], '\0', pos);

    }

    return pos;

}
