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

#ifndef OSNS_CLIENT_H
#define OSNS_CLIENT_H

/* prototypes */

void disconnect_osns_connection(struct osns_connection_s *oc);

void osns_client_handle_close(struct connection_s *c, unsigned char remote);
void osns_client_handle_error(struct connection_s *c, struct generic_error_s *e);
void osns_client_handle_dataavail(struct connection_s *conn);

void osns_client_process_data(struct osns_receive_s *r, char *data, unsigned int len, struct osns_control_s *ctrl);
int osns_client_send_data(struct osns_receive_s *r, char *data, unsigned int len, int (* send_cb)(struct system_socket_s *sock, char *data, unsigned int size, void *ptr), void *ptr);

#endif