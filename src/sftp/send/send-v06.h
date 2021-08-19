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

#ifndef SFTP_SEND_SEND_V06_H
#define SFTP_SEND_SEND_V06_H

/* prototypes */

int send_sftp_symlink_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r);
int send_sftp_block_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r);
int send_sftp_unblock_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r);
int send_sftp_realpath_v06(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r);

struct sftp_send_ops_s *get_sftp_send_ops_v06();

#endif

