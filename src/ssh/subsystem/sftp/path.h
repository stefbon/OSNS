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

#ifndef OSNS_SSH_SUBSYSTEM_SFTP_PATH_H
#define OSNS_SSH_SUBSYSTEM_SFTP_PATH_H

#include "libosns-fspath.h"

void path_append_none(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct fs_path_s *localpath);

#define CONVERT_PATH_INIT		{path_append_none}

/* prototypes */

unsigned int get_length_fullpath_prefix(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct convert_sftp_path_s *convert);
unsigned int get_length_fullpath_noprefix(struct sftp_subsystem_s *sftp, struct ssh_string_s *path, struct convert_sftp_path_s *convert);

#endif
