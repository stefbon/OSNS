/*
  2018 Stef Bon <stefbon@gmail.com>

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

#include "ssh-string.h"
#include "name.h"

#define NAMEINDEX_ROOT1						92			/* number of valid chars*/
#define NAMEINDEX_ROOT2						8464			/* 92 ^ 2 */
#define NAMEINDEX_ROOT3						778688			/* 92 ^ 3 */
#define NAMEINDEX_ROOT4						71639296		/* 92 ^ 4 */
#define NAMEINDEX_ROOT5						6590815232		/* 92 ^ 5 */

void init_name(struct name_s *a)
{
    a->name=NULL;
    a->len=0;
    a->index=0;
}

void set_name(struct name_s *a, char *s, unsigned int len)
{

    if (a && s) {

	a->name=s;
	a->len=len;

    }

}

void set_name_from(struct name_s *a, unsigned char type, void *ptr)
{

    if (type=='s') {
	struct ssh_string_s *s=(struct ssh_string_s *) ptr;

	a->name=s->ptr;
	a->len=s->len;

    }

}

int compare_names(struct name_s *a, struct name_s *b)
{
    int result=0;

    if (a->index==b->index) {

	if (b->len > 6) {

	    result=(a->len > 6) ? strcmp(a->name + 6, b->name + 6) : -1;

	} else if (b->len==6) {

	    result=(a->len>6) ? 1 : 0;

	}

    } else {

	result=(a->index > b->index) ? 1 : -1;

    }

    return result;

}

int compare_name(struct name_s *a, const unsigned char type, void *ptr)
{

    switch (type) {

	case 'n': {

	    struct name_s *b=(struct name_s *) ptr;
	    return compare_names(a, b);

	}

	case 'c': {

	    char *tmp=(char *) ptr;
	    struct name_s b={tmp, strlen(tmp), 0};

	    calculate_nameindex(&b);
	    return compare_names(a, &b);

	}

    }

    return -1;

}

uint64_t calculate_nameindex_hlp(char* buffer)
{
    unsigned char firstletter		= buffer[0] - 32;
    unsigned char secondletter		= buffer[1] - 32;
    unsigned char thirdletter		= buffer[2] - 32;
    unsigned char fourthletter		= buffer[3] - 32;
    unsigned char fifthletter		= buffer[4] - 32;
    unsigned char sixthletter		= buffer[5] - 32;

    return (firstletter * NAMEINDEX_ROOT5) + (secondletter * NAMEINDEX_ROOT4) + (thirdletter * NAMEINDEX_ROOT3) + (fourthletter * NAMEINDEX_ROOT2) + (fifthletter * NAMEINDEX_ROOT1) + sixthletter;
}

void calculate_nameindex(struct name_s *name)
{
    char buffer[6];
    unsigned char count=(name->len > 5) ? 6 : name->len;

    memset(buffer, 32, 6);
    memcpy(buffer, name->name, count);
    name->index=calculate_nameindex_hlp(buffer);

}
