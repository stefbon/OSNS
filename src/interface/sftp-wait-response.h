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

#ifndef INTERFACE_SFTP_WAIT_RESPONSE_H
#define INTERFACE_SFTP_WAIT_RESPONSE_H

/* prototypes */

unsigned char wait_sftp_response_ctx(struct context_interface_s *i, struct sftp_request_s *sftp_r, struct system_timespec_s *timeout, unsigned char (* cb_interrupted)(void *ptr), void *ptr);

void free_sftp_reply(struct sftp_reply_s *reply);
void init_sftp_request(struct sftp_request_s *r, struct context_interface_s *i);

void get_sftp_request_timeout_ctx(struct context_interface_s *i, struct system_timespec_s *timeout);

#endif
