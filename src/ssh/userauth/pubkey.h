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

#ifndef SSH_USERAUTH_PUBKEY_H
#define SSH_USERAUTH_PUBKEY_H

/* prototypes */

struct pk_identity_s *send_userauth_pubkey_request(struct ssh_connection_s *c, struct ssh_string_s *service, struct pk_list_s *pkeys);
int respond_userauth_publickey_request(struct ssh_connection_s *connection, struct ssh_string_s *username1, struct ssh_string_s *service1, struct ssh_string_s *data1, struct system_timespec_s *expire);

#endif
