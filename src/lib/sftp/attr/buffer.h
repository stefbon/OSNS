/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Stef Bon <stefbon@gmail.com>

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

#ifndef SRC_LIB_SFTP_ATTR_BUFFER_H
#define SRC_LIB_SFTP_ATTR_BUFFER_H

struct attr_buffer_s;

struct attr_buffer_ops_s {
    union ab_rw_u {
	struct ab_write_s {
	    void 					(* write_uchar)(struct attr_buffer_s *ab, unsigned char b);
	    void					(* write_uchars)(struct attr_buffer_s *ab, unsigned char *b, unsigned int l);
	    void 					(* write_uint32)(struct attr_buffer_s *ab, uint32_t i);
	    void 					(* write_uint64)(struct attr_buffer_s *ab, uint64_t i);
	    void					(* write_uint16)(struct attr_buffer_s *ab, uint16_t i);
	    void					(* write_string)(struct attr_buffer_s *ab, struct ssh_string_s *s);
	    void					(* write_skip)(struct attr_buffer_s *ab, unsigned int l);
	} write;
	struct ab_read_s {
	    unsigned char				(* read_uchar)(struct attr_buffer_s *ab);
	    uint32_t					(* read_string)(struct attr_buffer_s *ab, struct ssh_string_s *s, void (* cb)(struct attr_buffer_s *ab, struct ssh_string_s *s, void *ptr), void *ptr);
	    uint32_t					(* read_uint32)(struct attr_buffer_s *ab);
	    uint64_t					(* read_uint64)(struct attr_buffer_s *ab);
	    uint16_t					(* read_uint16)(struct attr_buffer_s *ab);
	} read;
    } rw;
};

struct attr_buffer_s {
    unsigned char					*buffer;
    unsigned char					*pos;
    int							left;
    unsigned int					len;
    struct attr_buffer_ops_s				*ops;
};


/* prototypes */

extern struct attr_buffer_s init_read_attr_buffer;
extern struct attr_buffer_s init_nowrite_attr_buffer;

#define INIT_ATTR_BUFFER_READ		init_read_attr_buffer
#define INIT_ATTR_BUFFER_NOWRITE	init_nowrite_attr_buffer

void set_attr_buffer_read(struct attr_buffer_s *ab, char *buffer, unsigned int len);
void set_attr_buffer_read_attr_response(struct attr_buffer_s *ab, struct attr_response_s *response);
void set_attr_buffer_nowrite(struct attr_buffer_s *ab);
void set_attr_buffer_write(struct attr_buffer_s *ab, char *buffer, unsigned int len);

#endif
