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
#include "log.h"
#include "datatypes.h"

#include "sftp/common-protocol.h"
#include "buffer.h"

#if __BIG_ENDIAN__

# define htonll(x) (x)
# define ntohll(x) (x)

#else

# define htonll(x) ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
# define ntohll(x) ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

#endif

static uint32_t convert_int32_2c(int32_t value)
{
    uint32_t result=0;

    if (value >= 0) {

	result=(uint32_t) value;

    } else {

	result = ~(-value);
	result++;

    }

    return result;
}

#define HIGHEST_BIT_32		0x80000000

static int32_t convert_2c_int32(uint32_t value)
{
    int32_t result=0;

    if (value & HIGHEST_BIT_32) {

	/* negative */

	value--;
	result = -(~value); /* does the compiler accept this ?? */

    } else {

	result = value;

    }

    return result;
}

static uint64_t convert_int64_2c(int64_t value)
{
    uint64_t result=0;

    if (value >= 0) {

	result=(uint64_t) value;

    } else {

	result = ~((uint64_t) -value);
	result++;

    }

    return result;
}

#define HIGHEST_BIT_64		0x8000000000000000

static int64_t convert_2c_int64(uint64_t value)
{
    int64_t result=0;

    if (value & HIGHEST_BIT_64) {

	/* negative */

	value--;
	result = -(~value); /* does the compiler accept this ?? */

    } else {

	result = value;

    }

    return result;
}

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

static void nowrite_int64(struct attr_buffer_s *ab, int64_t value)
{
    ab->pos += 8;
    ab->left -= 8;
}

static void write_int64(struct attr_buffer_s *ab, int64_t value)
{
    uint64_t tc_value=convert_int64_2c(value);
    unsigned char *eight=(unsigned char *) ab->pos;

    eight[0] = (tc_value >> 56) & 0xFF;
    eight[1] = (tc_value >> 48) & 0xFF;
    eight[2] = (tc_value >> 40) & 0xFF;
    eight[3] = (tc_value >> 32) & 0xFF;
    eight[4] = (tc_value >> 24) & 0xFF;
    eight[5] = (tc_value >> 16) & 0xFF;
    eight[6] = (tc_value >> 8) & 0xFF;
    eight[7] = (tc_value & 0xFF);
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

    logoutput("read_string: s->len %i left %i", s->len, ab->left);

    if (s->len>0 && ab->left>= s->len) {

	s->ptr=(char *) ab->pos;
	ab->pos+=s->len;
	ab->left-=s->len;
	(* cb)(ab, s, ptr);
	return (s->len + 4);

    }

    return 4;

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
    uint32_t result=get_uint32((char *) ab->pos);
    logoutput_debug("read_uint32: %i", result);
    ab->pos+=4;
    ab->left-=4;
    return result;
}

static uint64_t read_uint64(struct attr_buffer_s *ab)
{
    uint64_t result=get_uint64((char *) ab->pos);
    ab->pos+=8;
    ab->left-=8;
    return result;
}

static int64_t read_int64(struct attr_buffer_s *ab)
{
    unsigned char *tmp=(unsigned char *) ab->pos;
    uint64_t a;

    a = (uint64_t) ((tmp[0] << 56) | (tmp[1] << 48) | (tmp[2] << 40) | (tmp[3] << 32));
    a |= (uint32_t) ((tmp[4] << 24) | (tmp[5] << 16) | (tmp[6] << 8) | tmp[7]);

    ab->pos+=8;
    ab->left-=8;

    return convert_2c_int64(a);
}

static struct attr_buffer_ops_s ab_nowrite = {
    .rw.write.write_uchar		= nowrite_uchar,
    .rw.write.write_uchars		= nowrite_uchars,
    .rw.write.write_uint32		= nowrite_uint32,
    .rw.write.write_uint64		= nowrite_uint64,
    .rw.write.write_int64		= nowrite_int64,
    .rw.write.write_uint16		= nowrite_uint16,
    .rw.write.write_string		= write_string,
    .rw.write.write_skip		= write_skip,
};

static struct attr_buffer_ops_s ab_write = {
    .rw.write.write_uchar		= write_uchar,
    .rw.write.write_uchars		= write_uchars,
    .rw.write.write_uint32		= write_uint32,
    .rw.write.write_uint64		= write_uint64,
    .rw.write.write_int64		= write_int64,
    .rw.write.write_uint16		= write_uint16,
    .rw.write.write_string		= write_string,
    .rw.write.write_skip		= write_skip,
};

static struct attr_buffer_ops_s ab_read = {
    .rw.read.read_uchar			= read_uchar,
    .rw.read.read_string		= read_string,
    .rw.read.read_uint32		= read_uint32,
    .rw.read.read_uint64		= read_uint64,
    .rw.read.read_int64			= read_int64,
    .rw.read.read_uint16		= read_uint16,
};

void set_attr_buffer_read(struct attr_buffer_s *ab, char *buffer, unsigned int len)
{
    ab->flags=ATTR_BUFFER_FLAG_READ;
    ab->buffer=(unsigned char *) buffer;
    ab->pos=ab->buffer;
    ab->left=(int) len;
    ab->len=len;
    ab->count=0;
    ab->ops=&ab_read;
}

void set_attr_buffer_nowrite(struct attr_buffer_s *ab)
{
    ab->flags=ATTR_BUFFER_FLAG_NOWRITE;
    ab->ops=&ab_nowrite;
}

void set_attr_buffer_write(struct attr_buffer_s *ab, char *buffer, unsigned int len)
{
    ab->flags=ATTR_BUFFER_FLAG_WRITE;
    ab->buffer=(unsigned char *) buffer;
    ab->pos=ab->buffer;
    ab->left=(int) len;
    ab->len=len;
    ab->count=0;
    ab->ops=&ab_write;
}

void reset_attr_buffer_write(struct attr_buffer_s *ab)
{
    ab->pos=ab->buffer;
    ab->left=(int) ab->len;
    ab->count=0;
}

struct attr_buffer_s init_read_attr_buffer = {
    .flags			= ATTR_BUFFER_FLAG_READ,
    .buffer			= NULL,
    .pos			= NULL,
    .left			= 0,
    .len			= 0,
    .count			= 0,
    .ops			= &ab_read,
};

struct attr_buffer_s init_nowrite_attr_buffer = {
    .flags			= ATTR_BUFFER_FLAG_NOWRITE,
    .buffer			= NULL,
    .pos			= NULL,
    .left			= 0,
    .len			= 0,
    .count			= 0,
    .ops			= &ab_nowrite,
};
