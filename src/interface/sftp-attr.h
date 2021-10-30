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

/* prototypes */

void parse_attributes_generic_ctx(struct context_interface_s *interface, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char what, void (* cb)(unsigned int stat_mask, unsigned int len, unsigned int valid, unsigned int fattr, void *ptr), void *ptr);

void read_sftp_attributes_ctx(struct context_interface_s *interface, struct attr_buffer_s *abuff, struct system_stat_s *stat);
void write_attributes_ctx(struct context_interface_s *interface, struct attr_buffer_s *abuff, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned int valid);

void read_name_name_response_ctx(struct context_interface_s *interface, struct attr_buffer_s *abuff, struct ssh_string_s *name);

void correct_time_c2s_ctx(struct context_interface_s *interface, struct timespec *t);
void correct_time_s2c_ctx(struct context_interface_s *interface, struct timespec *t);

#endif
