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

#ifndef _SSH_SEND_USERAUTH_H
#define _SSH_SEND_USERAUTH_H

/* prototypes */

/*  public key - client */

void msg_write_userauth_pubkey_request(struct msg_buffer_s *mb, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *sig);
int send_userauth_pubkey_message(struct ssh_connection_s *c, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *sig);
unsigned int write_userauth_pubkey_ok_message(struct msg_buffer_s *mb, struct ssh_key_s *pkey);

/* none - client */

void msg_write_userauth_none_message(struct msg_buffer_s *mb, struct ssh_string_s *user, struct ssh_string_s *service);
int send_userauth_none_message(struct ssh_connection_s *c, struct ssh_string_s *user, struct ssh_string_s *service);

/* hostbased - client */

void msg_write_userauth_hostbased_request(struct msg_buffer_s *mb, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u);
int send_userauth_hostbased_message(struct ssh_connection_s *c, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *key, struct ssh_string_s *c_h, struct ssh_string_s *c_u, struct ssh_string_s *signature);

/* password - client */

int send_userauth_password_message(struct ssh_connection_s *c, char *user, char *pw, struct ssh_string_s *service);

/* public key - server */

int send_userauth_pubkey_ok_message(struct ssh_connection_s *connection, struct ssh_key_s *pkey);

/* common - server */

int send_userauth_request_reply(struct ssh_connection_s *connection, unsigned int methods, unsigned char success);

#endif
