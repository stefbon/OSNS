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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_RECEIVE_H
#define OSNS_SSH_SUBSYSTEM_SFTP_RECEIVE_H

#include "osns_sftp_subsystem.h"

/* prototypes */

void set_sftp_subsystem_process_payload(struct sftp_subsystem_s *sftp, const char *what);

void read_sftp_connection_signal(int fd, void *ptr, struct event_s *event);
int init_sftp_receive(struct sftp_receive_s *receive);
void free_sftp_receive(struct sftp_receive_s *receive);

#endif
