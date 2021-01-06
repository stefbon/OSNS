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

#ifndef _SFTP_ATTR_READ_ATTR_V04_H
#define _SFTP_ATTR_READ_ATTR_V04_H

/* prototypes */

void read_attr_ownergroup_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);
void read_attr_accesstime_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);
void read_attr_accesstime_n_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);
void read_attr_createtime_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);
void read_attr_createtime_n_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);
void read_attr_modifytime_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);
void read_attr_modifytime_n_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);
void read_attr_acl_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av04, struct sftp_attr_s *attr);

void read_attributes_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);

void read_attr_nameresponse_v04(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);

void read_sftp_features_v04(struct sftp_client_s *sftp);
unsigned int get_attribute_mask_v04(struct sftp_client_s *sftp);
int get_attribute_info_v04(struct sftp_client_s *sftp, unsigned int valid, const char *what);

#endif
