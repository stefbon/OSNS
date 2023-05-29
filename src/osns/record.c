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
#include "record.h"

struct read_uint16_hlpr_s {
    uint16_t		len;
};

struct read_uint32_hlpr_s {
    uint32_t		count;
};

static unsigned int read_size_uint16=sizeof(struct read_uint16_hlpr_s);
static unsigned int read_size_uint32=sizeof(struct read_uint32_hlpr_s);

unsigned int read_osns_record(char *buffer, unsigned int size, struct osns_record_s *r)
{
    struct osns_record_s dummy={0, NULL};

    if (r==NULL) r=&dummy;

    r->len=0;
    r->data=NULL;

    if (size >= read_size_uint16) {
	struct read_uint16_hlpr_s *hlpr=(struct read_uint16_hlpr_s *) buffer;

	r->len=hlpr->len;

	if (r->len>0) {

	    if (size >= read_size_uint16 + r->len) {

		r->data=(char *) (buffer + read_size_uint16);
		return (read_size_uint16 + r->len);

	    }

	}

	return (read_size_uint16);

    }

    return 0;

}

static void write_osns_record_hlpr(char *buffer, unsigned int size, char *data, unsigned int len)
{
    struct read_uint16_hlpr_s *hlpr=(struct read_uint16_hlpr_s *)buffer;

    hlpr->len=len;
    if (len>0) memcpy((char *)(buffer + read_size_uint16), data, len);
}

unsigned int write_osns_record(char *buffer, unsigned int size, const unsigned char type, void *ptr)
{
    unsigned int bytes=0;

    switch (type) {

	case 'c' : {
	    char *data=(char *) ptr;
	    unsigned int len=((data) ? strlen(data) : 0);

	    if (buffer) write_osns_record_hlpr(buffer, size, data, len);
	    bytes=(read_size_uint16 + len);
	}

	case 'r' : {
	    struct osns_record_s tmp={0, NULL};
	    struct osns_record_s *r=((ptr) ? (struct osns_record_s *) ptr : &tmp);

	    if (buffer) write_osns_record_hlpr(buffer, size, r->data, r->len);
	    bytes=(read_size_uint16 + r->len);

	}

    }

    // logoutput_debug("write_osns_record: bytes %u", bytes);
    return bytes;

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
    int result=-1;

    logoutput_debug("process_osns_records: size %u osns_records_hlpr size %u", size, sizeof(struct osns_records_hlpr_s));

    if (size >= sizeof(struct osns_records_hlpr_s)) {
	struct osns_records_hlpr_s *hlpr=(struct osns_records_hlpr_s *) data;
	unsigned int pos = sizeof(struct osns_records_hlpr_s);

	logoutput_debug("process_osns_records: hlpr size %u count %u", hlpr->size, hlpr->count);

	logoutput_base64encoded(NULL, data, size, 1);

	if ((hlpr->size + sizeof(struct osns_records_hlpr_s)) <= size) {

	    for (unsigned int i=0; i<hlpr->count; i++) {
		struct osns_record_s r;
		unsigned int tmp=read_osns_record(&data[pos], (size-pos), &r);

		logoutput_debug("process_osns_records: read %u rec len %u", tmp, r.len);

		if (tmp>0 && r.len<(size-pos)) {

		    if ((* cb)(&r, hlpr->count, i, ptr)==-1) goto errorout;
		    pos+=tmp;

		} else {

		    goto errorout;

		}

	    }

	}

	result=(int) hlpr->count;

    }

    return result;

    errorout:
    logoutput_debug("process_osns_records: failed to read osns records");
    return -1;

}
