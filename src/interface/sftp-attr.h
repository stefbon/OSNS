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

#ifndef _INTERFACE_SFTP_ATTR_H
#define _INTERFACE_SFTP_ATTR_H

//#include "fuse.h"

/* prototypes */

void read_sftp_attributes_ctx(struct context_interface_s *interface, struct attr_response_s *response, struct sftp_attr_s *attr);

void write_attributes_ctx(struct context_interface_s *interface, char *buffer, unsigned int len, struct sftp_attr_s *attr);
unsigned int write_attributes_len_ctx(struct context_interface_s *interface, struct sftp_attr_s *attr);

void read_name_nameresponse_ctx(struct context_interface_s *interface, struct fuse_buffer_s *buffer, struct ssh_string_s *name);
void read_attr_nameresponse_ctx(struct context_interface_s *interface, struct fuse_buffer_s *buffer, struct sftp_attr_s *attr);

int get_attribute_info_ctx(struct context_interface_s *interface, unsigned int valid, const char *what);

void correct_time_c2s_ctx(struct context_interface_s *interface, struct timespec *t);
void correct_time_s2c_ctx(struct context_interface_s *interface, struct timespec *t);

void translate_sftp_attr_fattr(struct context_interface_s *interface);

#endif
