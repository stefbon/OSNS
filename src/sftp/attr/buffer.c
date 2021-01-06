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
#include "list.h"
#include "sftp/common-protocol.h"
#include "logging.h"

static void nowrite_uchar(struct attr_buffer_s *ab, unsigned char b)
{
    ab->pos++;
    ab->left--;
}

static void write_uchar(struct attr_buffer_s *ab, unsigned char b)
{
    *(ab->pos)=b;
    ab->pos++;
    ab->left--;
}

static void nowrite_uchars(struct attr_buffer_s *ab, unsigned char *bytes, unsigned int len)
{
    ab->pos+=len;
    ab->left-=len;
}

static void write_uchars(struct attr_buffer_s *ab, unsigned char *bytes, unsigned int len)
{
    memcpy(ab->pos, bytes, len);
    ab->pos+=len;
    ab->left-=len;
}

static void nowrite_uint32(struct attr_buffer_s *ab, uint32_t value)
{
    ab->pos += 4;
    ab->left -= 4;
}

static void write_uint32(struct attr_buffer_s *ab, uint32_t value)
{
    unsigned char *four=(unsigned char *) ab->pos;

    four[0] = (value >> 24) & 0xFF;
    four[1] = (value >> 16) & 0xFF;
    four[2] = (value >> 8) & 0xFF;
    four[3] = value & 0xFF;
    ab->pos += 4;
    ab->left -= 4;
}

static void nowrite_uint64(struct attr_buffer_s *ab, uint64_t value)
{
    ab->pos += 8;
    ab->left -= 8;
}

static void write_uint64(struct attr_buffer_s *ab, uint64_t value)
{
    unsigned char *eight=(unsigned char *) ab->pos;

    eight[0] = (value >> 56) & 0xFF;
    eight[1] = (value >> 48) & 0xFF;
    eight[2] = (value >> 40) & 0xFF;
    eight[3] = (value >> 32) & 0xFF;
    eight[4] = (value >> 24) & 0xFF;
    eight[5] = (value >> 16) & 0xFF;
    eight[6] = (value >> 8) & 0xFF;
    eight[7] = value & 0xFF;
    ab->pos += 8;
    ab->left -= 8;
}

static void nowrite_uint16(struct attr_buffer_s *ab, uint16_t value)
{
    ab->pos += 2;
    ab->left -= 2;
}

static void write_uint16(struct attr_buffer_s *ab, uint16_t value)
{
    unsigned char *two=(unsigned char *) ab->pos;

    two[0] = (value >> 8) & 0xFF;
    two[1] = value & 0xFF;
    ab->pos += 2;
    ab->left -= 2;
}

static void write_string(struct attr_buffer_s *ab, struct ssh_string_s *s)
{
    (* ab->ops->rw.write.write_uint32)(ab, s->len);
    (* ab->ops->rw.write.write_uchars)(ab, (unsigned char *) s->ptr, s->len);
}

static void write_skip(struct attr_buffer_s *ab, unsigned int len)
{
    ab->pos += len;
    ab->left -= len;
}

static uint8_t read_uchar(struct attr_buffer_s *ab)
{
    uint8_t u=(uint8_t) *ab->pos;
    ab->pos++;
    ab->left--;
    return u;
}

static uint32_t read_string(struct attr_buffer_s *ab, struct ssh_string_s *s, void (* cb)(struct attr_buffer_s *ab, struct ssh_string_s *s, void *ptr), void *ptr)
{
    s->len=(* ab->ops->rw.read.read_uint32)(ab);

    logoutput("read_string");

    if (s->len>0 && ab->left>= s->len) {

	s->ptr=(char *) ab->pos;
	ab->pos+=s->len;
	ab->left-=s->len;
	(* cb)(ab, s, ptr);
	return (s->len + 4);

    }

    return 0;

}

static uint16_t read_uint16(struct attr_buffer_s *ab)
{
    unsigned char *two=(unsigned char *) ab->pos;
    ab->pos+=2;
    ab->left-=2;
    return (uint32_t) ((two[0] << 8)  | two[1]);
}

static uint32_t read_uint32(struct attr_buffer_s *ab)
{
    unsigned char *four=(unsigned char *) ab->pos;
    ab->pos+=4;
    ab->left-=4;
    return (uint32_t) ((four[0] << 24)  | (four[1] << 16) | (four[2] << 8) | four[3]);
}

static uint64_t read_uint64(struct attr_buffer_s *ab)
{
    unsigned char *eight=(unsigned char *) ab->pos;
    ab->pos+=8;
    ab->left-=8;
    return (uint64_t) ((((uint64_t) eight[0]) << 56)  | (((uint64_t) eight[1]) << 48) | (((uint64_t) eight[2]) << 40) | (((uint64_t) eight[3]) << 32) | (((uint64_t) eight[4]) << 24)  | (((uint64_t) eight[5]) << 16) | (((uint64_t) eight[6]) << 8) | eight[7] );
}

static struct attr_buffer_ops_s ab_nowrite = {
    .rw.write.write_uchar		= nowrite_uchar,
    .rw.write.write_uchars		= nowrite_uchars,
    .rw.write.write_uint32		= nowrite_uint32,
    .rw.write.write_uint64		= nowrite_uint64,
    .rw.write.write_uint16		= nowrite_uint16,
    .rw.write.write_string		= write_string,
    .rw.write.write_skip		= write_skip,
};

static struct attr_buffer_ops_s ab_write = {
    .rw.write.write_uchar		= write_uchar,
    .rw.write.write_uchars		= write_uchars,
    .rw.write.write_uint32		= write_uint32,
    .rw.write.write_uint64		= write_uint64,
    .rw.write.write_uint16		= write_uint16,
    .rw.write.write_string		= write_string,
    .rw.write.write_skip		= write_skip,
};

static struct attr_buffer_ops_s ab_read = {
    .rw.read.read_uchar			= read_uchar,
    .rw.read.read_string		= read_string,
    .rw.read.read_uint32		= read_uint32,
    .rw.read.read_uint64		= read_uint64,
    .rw.read.read_uint16		= read_uint16,
};

void set_attr_buffer_read(struct attr_buffer_s *ab, char *buffer, unsigned int len)
{
    // logoutput("set_attr_buffer_read");
    ab->buffer=(unsigned char *) buffer;
    ab->pos=ab->buffer;
    ab->left=(int) len;
    ab->len=len;
    ab->ops=&ab_read;
}

void set_attr_buffer_read_attr_response(struct attr_buffer_s *ab, struct attr_response_s *response)
{
    ab->buffer=response->buff;
    ab->pos=ab->buffer;
    ab->left=(int) response->size;
    ab->len=(unsigned int) response->size;
    ab->ops=&ab_read;
}

void set_attr_buffer_nowrite(struct attr_buffer_s *ab)
{
    ab->ops=&ab_nowrite;
}

void set_attr_buffer_write(struct attr_buffer_s *ab, char *buffer, unsigned int len)
{
    ab->buffer=(unsigned char *) buffer;
    ab->pos=ab->buffer;
    ab->left=(int) len;
    ab->len=len;
    ab->ops=&ab_write;
}

struct attr_buffer_s init_read_attr_buffer = {
    .buffer			= NULL,
    .pos			= NULL,
    .left			= 0,
    .len			= 0,
    .ops			= &ab_read,
};

struct attr_buffer_s init_nowrite_attr_buffer = {
    .buffer			= NULL,
    .pos			= NULL,
    .left			= 0,
    .len			= 0,
    .ops			= &ab_nowrite,
};
