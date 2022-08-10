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

#ifndef _SFTP_REQUEST_HASH_H
#define _SFTP_REQUEST_HASH_H

void add_request_hashtable(struct sftp_request_s *sftp_r);
void remove_request_hashtable(struct sftp_request_s *sftp_r);

struct sftp_request_s *get_sftp_request(struct sftp_client_s *sftp, unsigned int id, struct generic_error_s *error);
int signal_sftp_received_id(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r);
void signal_sftp_received_id_error(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, struct generic_error_s *error);

unsigned char wait_sftp_response(struct sftp_client_s *sftp, struct sftp_request_s *sftp_r, struct system_timespec_s *timeout);
unsigned char wait_sftp_service_complete(struct sftp_client_s *sftp, struct timespec *timeout);

void init_sftp_sendhash();
void clear_sftp_reply(struct sftp_reply_s *r);

#endif
