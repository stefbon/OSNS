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

#ifndef _SFTP_ATTR_BUFFER_H
#define _SFTP_ATTR_BUFFER_H

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
