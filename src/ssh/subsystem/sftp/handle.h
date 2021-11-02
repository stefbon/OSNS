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

#include "lib/system/fshandle.h"

/* prototypes */

unsigned char write_sftp_commonhandle(struct commonhandle_s *handle, char *buffer, unsigned int size);
unsigned char get_sftp_handle_size();

void set_sftp_handle_access(struct commonhandle_s *handle, unsigned int access);
void set_sftp_handle_flags(struct commonhandle_s *handle, unsigned int flags);

struct commonhandle_s *create_sftp_filehandle(struct sftp_subsystem_s *sftp, unsigned int inserttype, dev_t dev, uint64_t ino, char *name, unsigned int flags, unsigned int access);

struct commonhandle_s *find_sftp_commonhandle(struct sftp_subsystem_s *sftp, char *buffer, unsigned int size, unsigned int *p_count);
void release_sftp_handle(struct commonhandle_s **p_handle);

struct commonhandle_s *create_sftp_dirhandle(struct sftp_subsystem_s *sftp, struct fs_location_devino_s *devino);

struct sftp_valid_s *get_valid_sftp_dirhandle(struct commonhandle_s *handle);

#endif
