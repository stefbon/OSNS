/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _SFTP_ATTR_WRITE_ATTR_V06_H
#define _SFTP_ATTR_WRITE_ATTR_V06_H

/* prototypes */

void write_attr_accesstime_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid);
void write_attr_modifytime_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid);
void write_attr_changetime_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr, unsigned int *valid);

void write_attributes_v06(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);
unsigned int write_attributes_len_v06(struct sftp_client_s *sftp, struct sftp_attr_s *attr);

#endif
