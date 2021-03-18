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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_CONNECT_H
#define OSNS_SSH_SUBSYSTEM_SFTP_CONNECT_H

#include "osns_sftp_subsystem.h"

#define SFTP_CONNECTION_TYPE_STD		1

/* prototypes */

int init_sftp_connection(struct sftp_connection_s *connection, unsigned char type);
int connect_sftp_connection(struct sftp_connection_s *c);
int add_sftp_connection_eventloop(struct sftp_connection_s *connection);
void remove_sftp_connection_eventloop(struct sftp_connection_s *connection);
void free_sftp_connection(struct sftp_connection_s *connection);
void disconnect_sftp_connection(struct sftp_connection_s *connection, unsigned char senddisconnect);

int start_thread_sftp_connection_problem(struct sftp_connection_s *connection);

#endif
