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

#ifndef LIB_SFTP_ATTR_WRITE_ATTR_V03_H
#define LIB_SFTP_ATTR_WRITE_ATTR_V03_H

/* prototypes */

void write_attr_zero(struct attr_context_s *ctx, struct attr_buffer_s *b, struct rw_attr_result_s *r, struct sftp_attr_s *a);

void write_attr_size_v03(struct attr_context_s *ctx, struct attr_buffer_s *b, struct rw_attr_result_s *r, struct sftp_attr_s *attr);
void write_attr_uidgid_v03(struct attr_context_s *ctx, struct attr_buffer_s *b, struct rw_attr_result_s *r, struct sftp_attr_s *a);
void write_attr_permissions_v03(struct attr_context_s *ctx, struct attr_buffer_s *b, struct rw_attr_result_s *r, struct sftp_attr_s *a);
void write_attr_acmodtime_v03(struct attr_context_s *ctx, struct attr_buffer_s *b, struct rw_attr_result_s *r, struct sftp_attr_s *a);

void write_attributes_v03(struct attr_context_s *ctx, struct attr_buffer_s *b, struct rw_attr_result_s *r, struct sftp_attr_s *a);
unsigned int write_attributes_len_v03(struct attr_context_s *ctx, struct rw_attr_result_s *r, struct sftp_attr_s *a);

#endif
