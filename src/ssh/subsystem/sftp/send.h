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

#ifndef OSNS_SFTP_SUBSYSTEM_SEND_H
#define OSNS_SFTP_SUBSYSTEM_SEND_H

#include "osns_sftp_subsystem.h"

/* prototypes */

int send_sftp_subsystem(struct sftp_subsystem_s *sftp, char *data, unsigned int len);

int reply_sftp_status_simple(struct sftp_subsystem_s *sftp, uint32_t id, unsigned int status);
int reply_sftp_attrs(struct sftp_subsystem_s *sftp, uint32_t id, char *attr, unsigned int len);
int reply_sftp_data(struct sftp_subsystem_s *sftp, uint32_t id, char *data, unsigned int len, unsigned char eof);
int reply_sftp_handle(struct sftp_subsystem_s *sftp, uint32_t id, char *handle, unsigned int len);
int reply_sftp_names(struct sftp_subsystem_s *sftp, uint32_t id, unsigned int count, char *names, unsigned int len, unsigned char eof);
int reply_sftp_extension(struct sftp_subsystem_s *sftp, uint32_t id, char *data, unsigned int len);

void write_ssh_subsystem_connection_signal(int fd, void *ptr, struct event_s *event);

#endif
