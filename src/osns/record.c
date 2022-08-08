/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-lock.h"

#include "osns-protocol.h"

unsigned int read_osns_record(char *buffer, unsigned int size, struct osns_record_s *r)
{
    struct osns_record_s dummy={0, NULL};

    if (r==NULL) r=&dummy;

    r->len=0;
    r->data=NULL;

    if (size >= 2) {

	r->len=get_uint16(buffer);

	if (size >= 2 + r->len) {

	    r->data=(char *) (buffer + 2);
	    return (2 + r->len);

	}

    }

    return 0;

}

unsigned int write_osns_record(char *buffer, unsigned int size, const unsigned char type, void *ptr)
{
    char *pos=NULL;

    switch (type) {

	case 'c' : {
	    char *data=(char *) ptr;
	    unsigned int len=strlen(data);

	    if (buffer) {
		char *pos=buffer;

		store_uint16(pos, len);
		pos+=2;
		memcpy(pos, data, len);

	    }

	    return (2 + len);
	    break;
	}

	case 'r' : {
	    struct osns_record_s *r=(struct osns_record_s *) ptr;

	    if (buffer) {
		char *pos=buffer;

		store_uint16(pos, r->len);
		pos+=2;
		memcpy(pos, r->data, r->len);


	    }

	    return (2 + r->len);
	    break;
	}

    }

    return 0;

}


int compare_osns_record(struct osns_record_s *r, const unsigned char type, void *ptr)
{
    switch (type) {

	case 'c' : {
	    char *data=(char *) ptr;
	    unsigned int len=strlen(data);

	    return ((len==r->len) && memcmp(data, r->data, len)==0) ? 0 : -1;
	    }

	case 'r' : {
	    struct osns_record_s *s=(struct osns_record_s *) ptr;
	    return ((s->len==r->len) && memcmp(s->data, r->data, r->len)==0) ? 0 : -1;
	    }

    }

    return -1;

}

int process_osns_records(char *data, unsigned int size, int (* cb)(struct osns_record_s *r, unsigned int count, unsigned int index, void *ptr), void *ptr)
{
    unsigned int count=0;
    unsigned int pos=0;

    if (size<4) goto errorout;
    count=get_uint32(&data[pos]);
    pos+=4;

    if (pos + (2 * count) > size) goto errorout;

    for (unsigned int i=0; i<count; i++) {
	struct osns_record_s r;
	unsigned int tmp=read_osns_record(&data[pos], (size-pos), &r);

	if (tmp>0) {

	    if ((* cb)(&r, count, i, ptr)==-1) goto errorout;
	    pos+=tmp;

	} else {

	    goto errorout;

	}

    }

    return (int) count;

    errorout:

    logoutput_debug("process_osns_records: failed to read osns records");
    return 0;

}
