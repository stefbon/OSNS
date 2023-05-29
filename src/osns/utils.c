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
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-connection.h"

#include "osns-protocol.h"
#include "osns/osns.h"
#include "utils.h"

struct _rw_osns_uint8_s {
    uint8_t			value;
};

struct _rw_osns_uint16_s {
    uint16_t			value;
};

struct _rw_osns_uint32_s {
    uint32_t			value;
};

struct _rw_osns_uint64_s {
    uint64_t			value;
};

struct _rw_osns_int64_s {
    int64_t			value;
};

struct _rw_osns_timespec_s {
    int64_t			ts_sec;
    uint32_t			ts_nsec;
};

unsigned int osns_size_uint8()
{
    return sizeof(struct _rw_osns_uint8_s);
}

unsigned int osns_size_uint16()
{
    return sizeof(struct _rw_osns_uint16_s);
}

unsigned int osns_size_uint32()
{
    return sizeof(struct _rw_osns_uint32_s);
}

unsigned int osns_size_uint64()
{
    return sizeof(struct _rw_osns_uint64_s);
}

unsigned int osns_size_int64()
{
    return sizeof(struct _rw_osns_int64_s);
}

unsigned int osns_size_timespec()
{
    return sizeof(struct _rw_osns_timespec_s);
}

static void nowrite_uint8(struct osns_buffer_s *b, uint8_t v)
{
    b->pos += sizeof(struct _rw_osns_uint8_s);
}

static void nowrite_uint16(struct osns_buffer_s *b, uint16_t v)
{
    b->pos += sizeof(struct _rw_osns_uint16_s);
}

static void nowrite_uint32(struct osns_buffer_s *b, uint32_t v)
{
    b->pos += sizeof(struct _rw_osns_uint32_s);
}

static void nowrite_uint64(struct osns_buffer_s *b, uint64_t v)
{
    b->pos += sizeof(struct _rw_osns_uint64_s);
}

static void nowrite_int64(struct osns_buffer_s *b, int64_t v)
{
    b->pos += sizeof(struct _rw_osns_int64_s);
}

static void nowrite_timespec(struct osns_buffer_s *b, struct system_timespec_s *t)
{
    b->pos += sizeof(struct _rw_osns_timespec_s);
}

static void nowrite_data(struct osns_buffer_s *b, char *data, unsigned int len)
{
    b->pos += len;
}

static void copy_osns_data_buffer(struct osns_buffer_s *b, char *data, unsigned int size)
{
    char *buffer=b->buffer;
    memcpy(&buffer[b->pos], data, size);
    b->pos += size;
}

static void write_uint8(struct osns_buffer_s *b, uint8_t v)
{
    struct _rw_osns_uint8_s tmp={v};

    // logoutput_debug("write_uint8: pos %u field size %u value %u", b->pos, sizeof(struct _rw_osns_uint8_s), v);
    copy_osns_data_buffer(b, (char *) &tmp, sizeof(struct _rw_osns_uint8_s));
}

static void write_uint16(struct osns_buffer_s *b, uint16_t v)
{
    struct _rw_osns_uint16_s tmp={v};

    // logoutput_debug("write_uint16: pos %u field size %u value %u", b->pos, sizeof(struct _rw_osns_uint16_s), v);
    copy_osns_data_buffer(b, (char *) &tmp, sizeof(struct _rw_osns_uint16_s));
}

static void write_uint32(struct osns_buffer_s *b, uint32_t v)
{
    struct _rw_osns_uint32_s tmp={v};

    // logoutput_debug("write_uint32: pos %u field size %u value %u", b->pos, sizeof(struct _rw_osns_uint32_s), v);
    copy_osns_data_buffer(b, (char *) &tmp, sizeof(struct _rw_osns_uint32_s));
}

static void write_uint64(struct osns_buffer_s *b, uint64_t v)
{
    struct _rw_osns_uint64_s tmp={v};

    // logoutput_debug("write_uint64: pos %u field size %u value %lu", b->pos, sizeof(struct _rw_osns_uint64_s), v);
    copy_osns_data_buffer(b, (char *) &tmp, sizeof(struct _rw_osns_uint64_s));
}

static void write_int64(struct osns_buffer_s *b, int64_t v)
{
    struct _rw_osns_int64_s tmp={v};

    // logoutput_debug("write_int64: pos %u field size %u value %li", b->pos, sizeof(struct _rw_osns_uint64_s), v);
    copy_osns_data_buffer(b, (char *) &tmp, sizeof(struct _rw_osns_int64_s));
}

static void write_timespec(struct osns_buffer_s *b, struct system_timespec_s *t)
{
    struct _rw_osns_timespec_s tmp={t->st_sec, t->st_nsec};
    copy_osns_data_buffer(b, (char *) &tmp, sizeof(struct _rw_osns_timespec_s));
}

static void write_data(struct osns_buffer_s *b, char *data, unsigned int len)
{
    char *buffer=b->buffer;

    // logoutput_debug("write_data: pos %u field size %u value %.*s", b->pos, len, len, data);
    memcpy(&buffer[b->pos], data, len);
    b->pos += len;
}

static uint8_t read_uint8(struct osns_buffer_s *b)
{
    char *buffer=b->buffer;
    struct _rw_osns_uint8_s *tmp=(struct _rw_osns_uint8_s *) &buffer[b->pos];

    b->pos += sizeof(struct _rw_osns_uint8_s);
    return tmp->value;
}

static uint16_t read_uint16(struct osns_buffer_s *b)
{
    char *buffer=b->buffer;
    struct _rw_osns_uint16_s *tmp=(struct _rw_osns_uint16_s *) &buffer[b->pos];

    b->pos += sizeof(struct _rw_osns_uint16_s);
    return tmp->value;
}

static uint32_t read_uint32(struct osns_buffer_s *b)
{
    char *buffer=b->buffer;
    struct _rw_osns_uint32_s *tmp=(struct _rw_osns_uint32_s *) &buffer[b->pos];

    b->pos += sizeof(struct _rw_osns_uint32_s);
    return tmp->value;
}

static uint64_t read_uint64(struct osns_buffer_s *b)
{
    char *buffer=b->buffer;
    struct _rw_osns_uint64_s *tmp=(struct _rw_osns_uint64_s *) &buffer[b->pos];

    b->pos += sizeof(struct _rw_osns_uint64_s);
    return tmp->value;
}

static int64_t read_int64(struct osns_buffer_s *b)
{
    char *buffer=b->buffer;
    struct _rw_osns_int64_s *tmp=(struct _rw_osns_int64_s *) &buffer[b->pos];

    b->pos += sizeof(struct _rw_osns_int64_s);
    return tmp->value;
}

static void read_timespec(struct osns_buffer_s *b, struct system_timespec_s *t)
{
    char *buffer=b->buffer;
    struct _rw_osns_timespec_s *tmp=(struct _rw_osns_timespec_s *) &buffer[b->pos];

    t->st_sec=tmp->ts_sec;
    t->st_nsec=tmp->ts_nsec;
    b->pos += sizeof(struct _rw_osns_timespec_s);
}

void init_osns_buffer_read(struct osns_buffer_s *b, char *buffer, unsigned int size)
{

    b->flags = OSNS_BUFFER_FLAG_READ;
    b->buffer = buffer;
    b->len = size;
    b->pos = 0;
    b->count = 0;
    b->ops.r.read_uint8=read_uint8;
    b->ops.r.read_uint16=read_uint16;
    b->ops.r.read_uint32=read_uint32;
    b->ops.r.read_uint64=read_uint64;
    b->ops.r.read_int64=read_int64;
    b->ops.r.read_timespec=read_timespec;

}

static void _set_osns_buffer_ops_write(struct osns_buffer_s *b)
{
    b->ops.w.write_uint8=write_uint8;
    b->ops.w.write_uint16=write_uint16;
    b->ops.w.write_uint32=write_uint32;
    b->ops.w.write_uint64=write_uint64;
    b->ops.w.write_int64=write_int64;
    b->ops.w.write_timespec=write_timespec;
    b->ops.w.write_data=write_data;
}

static void _set_osns_buffer_ops_nowrite(struct osns_buffer_s *b)
{
    b->ops.w.write_uint8=nowrite_uint8;
    b->ops.w.write_uint16=nowrite_uint16;
    b->ops.w.write_uint32=nowrite_uint32;
    b->ops.w.write_uint64=nowrite_uint64;
    b->ops.w.write_int64=nowrite_int64;
    b->ops.w.write_timespec=nowrite_timespec;
    b->ops.w.write_data=nowrite_data;
}

void init_osns_buffer_write(struct osns_buffer_s *b, char *buffer, unsigned int size)
{

    b->buffer = buffer;

    if (buffer) {

	b->flags = OSNS_BUFFER_FLAG_WRITE;
	_set_osns_buffer_ops_write(b);

    } else {

	b->flags = OSNS_BUFFER_FLAG_NOWRITE;
	_set_osns_buffer_ops_nowrite(b);

    }

    b->len = size;
    b->pos = 0;
    b->count = 0;

}

struct osns_buffer_s default_osns_buffer_nowrite = {
    .flags=OSNS_BUFFER_FLAG_NOWRITE,
    .len=0,
    .buffer=NULL,
    .pos=0,
    .count=0,
    .ops.w.write_uint8=nowrite_uint8,
    .ops.w.write_uint16=nowrite_uint16,
    .ops.w.write_uint32=nowrite_uint32,
    .ops.w.write_uint64=nowrite_uint64,
    .ops.w.write_int64=nowrite_int64,
    .ops.w.write_timespec=nowrite_timespec,
    .ops.w.write_data=nowrite_data,
};

void set_osns_buffer(struct osns_buffer_s *b, char *buffer, unsigned int size)
{

    b->buffer=buffer;
    b->len=size;
    b->pos=0;

    if (b->flags & OSNS_BUFFER_FLAG_WRITE) {

	if (buffer==NULL) {

	    b->flags &= ~OSNS_BUFFER_FLAG_WRITE;
	    b->flags |= OSNS_BUFFER_FLAG_NOWRITE;
	    _set_osns_buffer_ops_nowrite(b);

	}

    } else if (b->flags & OSNS_BUFFER_FLAG_NOWRITE) {

	if (buffer) {

	    b->flags &= ~OSNS_BUFFER_FLAG_NOWRITE;
	    b->flags |= OSNS_BUFFER_FLAG_WRITE;
	    _set_osns_buffer_ops_write(b);

	}

    }

}

void set_connection_cap(struct osns_connection_s *oc, unsigned int version, unsigned int sr, unsigned int sp)
{
    unsigned int osnsmajor=get_osns_major(version);

    /* set the parameters for this connection:
	- cap: what is requested by client and what does server offer
	- c_version: the version agreed by client and server */

    oc->protocol.version=version;

    if (osnsmajor==1) {

	oc->protocol.level.one.sr=sr;
	oc->protocol.level.one.sp=sp;

    }

    // oc->flags|=flags;

}

unsigned int create_osns_version(unsigned int major, unsigned int minor)
{

    if (((major & 0x00FF) != major) || ((minor & 0x00FF) != minor)) {

	logoutput_warning("create_osns_version: error ... major and/or minor out of range");
	return 0;

    }

    return (unsigned int) ((major << 16) + minor);
}

unsigned int get_osns_major(unsigned int version)
{
    return ((version >> 16) & 0x00FF);
}

unsigned int get_osns_minor(unsigned int version)
{
    return (version & 0x00FF);
}

unsigned int get_osns_protocol_info(struct osns_connection_s *oc, const char *what)
{
    unsigned int major=get_osns_major(oc->protocol.version);
    unsigned int result=0;

    if (strcmp(what, "sr")==0) {

	if (major==1) result=oc->protocol.level.one.sr;

    } else if (strcmp(what, "sp")==0) {

	if (major==1) result=oc->protocol.level.one.sp;

    } else if (strcmp(what, "major")==0) {

	result=major;

    } else if (strcmp(what, "minor")==0) {

	result=get_osns_minor(oc->protocol.version);

    }

    return result;
}

void parse_options_commalist(char *list, unsigned int size, void (* cb)(char *entry, void *ptr), void *ptr)
{
    int left = (size>0) ? size : strlen(list);
    char *sep=NULL;
    char *start=list;

    finditem:

    sep=memchr(start, ',', left);
    if (sep) *sep='\0';

    (* cb)(start, ptr);

    if (sep) {

	*sep=',';
	left-=(sep - start);
	start=sep+1;
	if (left>0) goto finditem;

    }

}
