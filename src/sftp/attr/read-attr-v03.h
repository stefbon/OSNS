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

#ifndef _SFTP_ATTR_READ_ATTR_V03_H
#define _SFTP_ATTR_READ_ATTR_V03_H

/* prototypes */

void read_attr_zero(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av, struct sftp_attr_s *attr);

void read_attr_size_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr);
void read_attr_uidgid_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr);
void read_attr_permissions_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr);
void read_attr_acmodtime_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr);
void read_attr_extensions_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct attr_version_s *av03, struct sftp_attr_s *attr);

void read_sftp_attributes_generic(struct sftp_client_s *sftp, struct attr_version_s *av, unsigned int count, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);

void read_attributes_v03(struct sftp_client_s *sftp, struct attr_buffer_s *buffer, struct sftp_attr_s *attr);

void read_sftp_features_v03(struct sftp_client_s *sftp);
unsigned int get_attribute_mask_v03(struct sftp_client_s *sftp);
int get_attribute_info_v03(struct sftp_client_s *sftp, unsigned int valid, const char *what);

#endif
