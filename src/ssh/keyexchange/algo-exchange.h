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

#ifndef _SSH_KEYEXCHANGE_ALGO_EXCHANGE_H
#define _SSH_KEYEXCHANGE_ALGO_EXCHANGE_H

int store_local_kexinit(struct ssh_keyexchange_s *kex, struct ssh_payload_s *payload, unsigned int server, struct generic_error_s *error);
void free_kexinit_server(struct ssh_keyexchange_s *kex);
void free_kexinit_client(struct ssh_keyexchange_s *kex);

int start_algo_exchange(struct ssh_connection_s *c);

#endif
