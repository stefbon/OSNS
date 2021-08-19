/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_HANDLE_H
#define OSNS_SSH_SUBSYSTEM_SFTP_HANDLE_H

#include "ssh/subsystem/commonhandle.h"

/* prototypes */

struct commonhandle_s *find_sftp_commonhandle_buffer(struct sftp_subsystem_s *sftp, char *buffer, unsigned int *p_count);

void set_sftp_handle_access(struct commonhandle_s *handle, unsigned int access);
void set_sftp_handle_flags(struct commonhandle_s *handle, unsigned int flags);

struct commonhandle_s *init_sftp_filehandle(struct sftp_subsystem_s *sftp, unsigned int inserttype, dev_t dev, uint64_t ino, char *name);
int insert_sftp_filehandle(struct commonhandle_s *new, struct insert_filehandle_s *insert);

void release_sftp_handle_buffer(struct sftp_subsystem_s *sftp, char *buffer);

#endif
