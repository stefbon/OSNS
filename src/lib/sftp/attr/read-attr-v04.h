/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef LIB_SFTP_ATTR_READ_ATTR_V04_H
#define LIB_SFTP_ATTR_READ_ATTR_V04_H

/* prototypes */

void read_attr_type_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_ownergroup_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_permissions_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_accesstime_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_accesstime_n_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_createtime_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_createtime_n_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_modifytime_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_modifytime_n_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);
void read_attr_acl_v04(struct attr_context_s *ctx, struct attr_buffer_s *buffer, struct rw_attr_result_s *r, struct system_stat_s *stat);

void read_name_name_response_v04(struct attr_context_s *actx, struct attr_buffer_s *buffer, struct ssh_string_s *name);
#endif