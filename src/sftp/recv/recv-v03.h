/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _SFTP_RECV_V03_H
#define _SFTP_RECV_V03_H

void receive_sftp_status_v03(struct sftp_client_s *sftp, struct sftp_header_s *header);
void receive_sftp_handle_v03(struct sftp_client_s *sftp, struct sftp_header_s *header);
void receive_sftp_data_v03(struct sftp_client_s *sftp, struct sftp_header_s *header);
void receive_sftp_name_v03(struct sftp_client_s *sftp, struct sftp_header_s *header);
void receive_sftp_attr_v03(struct sftp_client_s *sftp, struct sftp_header_s *header);

void receive_sftp_extension_v03(struct sftp_client_s *sftp, struct sftp_header_s *header);
void receive_sftp_extension_reply_v03(struct sftp_client_s *sftp, struct sftp_header_s *header);

struct sftp_recv_ops_s *get_sftp_recv_ops_v03();

#endif
