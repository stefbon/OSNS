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

struct attr_context_s *get_sftp_attr_context(struct context_interface_s *i);

void parse_attributes_generic_ctx(struct context_interface_s *i, struct rw_attr_result_s *r, struct system_stat_s *stat, unsigned char what, void (* cb)(unsigned int stat_mask, unsigned int len, unsigned int valid, unsigned int fattr, void *ptr), void *ptr);

void read_sftp_attributes_ctx(struct context_interface_s *i, struct attr_buffer_s *abuff, struct system_stat_s *stat);
void write_attributes_ctx(struct context_interface_s *i, struct attr_buffer_s *abuff, struct rw_attr_result_s *r, struct system_stat_s *stat, struct sftp_valid_s *valid);

void read_name_name_response_ctx(struct context_interface_s *i, struct attr_buffer_s *abuff, struct ssh_string_s *name);

void correct_time_c2s_ctx(struct context_interface_s *i, struct system_timespec_s *t);
void correct_time_s2c_ctx(struct context_interface_s *i, struct system_timespec_s *t);

void enable_attributes_ctx(struct context_interface_s *i, struct sftp_valid_s *valid, const char *name);

uid_t get_sftp_unknown_userid_ctx(struct context_interface_s *i);
gid_t get_sftp_unknown_groupid_ctx(struct context_interface_s *i);

#endif
