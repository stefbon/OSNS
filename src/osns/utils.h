/*
  2010, 2011, 2012, 2013, 2014, 2015 Stef Bon <stefbon@gmail.com>

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

#ifndef OSNS_UTILS_H
#define OSNS_UTILS_H

#include "libosns-datatypes.h"

#define OSNS_BUFFER_FLAG_READ				1
#define OSNS_BUFFER_FLAG_NOWRITE			2
#define OSNS_BUFFER_FLAG_WRITE				4

struct osns_buffer_s;

struct osns_readbuffer_s {
    uint8_t			(* read_uint8)(struct osns_buffer_s *b);
    uint16_t			(* read_uint16)(struct osns_buffer_s *b);
    uint32_t			(* read_uint32)(struct osns_buffer_s *b);
    uint64_t			(* read_uint64)(struct osns_buffer_s *b);
    int64_t			(* read_int64)(struct osns_buffer_s *b);
    void			(* read_timespec)(struct osns_buffer_s *b, struct system_timespec_s *st);
};

struct osns_writebuffer_s {
    void			(* write_uint8)(struct osns_buffer_s *b, uint8_t v);
    void			(* write_uint16)(struct osns_buffer_s *b, uint16_t v);
    void			(* write_uint32)(struct osns_buffer_s *b, uint32_t v);
    void			(* write_uint64)(struct osns_buffer_s *b, uint64_t v);
    void			(* write_int64)(struct osns_buffer_s *b, int64_t v);
    void			(* write_timespec)(struct osns_buffer_s *b, struct system_timespec_s *st);
    void			(* write_data)(struct osns_buffer_s *b, char *data, unsigned int len);
};

struct osns_buffer_s {
    unsigned int		flags;
    char			*buffer;
    unsigned int		pos;
    unsigned int		len;
    int				count;
    union _osns_ops_u {
	struct osns_writebuffer_s w;
	struct osns_readbuffer_s r;
    } ops;
};

extern struct osns_buffer_s default_osns_buffer_nowrite;

#define INIT_OSNS_BUFFER_NOWRITE	default_osns_buffer_nowrite

/* prototypes */

unsigned int osns_size_uint8();
unsigned int osns_size_uint16();
unsigned int osns_size_uint32();
unsigned int osns_size_uint64();
unsigned int osns_size_int64();
unsigned int osns_size_timespec();

void init_osns_buffer_read(struct osns_buffer_s *b, char *buffer, unsigned int size);
void init_osns_buffer_write(struct osns_buffer_s *b, char *buffer, unsigned int size);
void set_osns_buffer(struct osns_buffer_s *b, char *buffer, unsigned int size);

void set_connection_cap(struct osns_connection_s *oc, unsigned int version, unsigned int services, unsigned int flags);

unsigned int create_osns_version(unsigned int major, unsigned int minor);
unsigned int get_osns_major(unsigned int version);
unsigned int get_osns_minor(unsigned int version);

unsigned int get_osns_protocol_info(struct osns_connection_s *oc, const char *what);

void parse_options_commalist(char *list, unsigned int size, void (* cb)(char *entry, void *ptr), void *ptr);

#endif
