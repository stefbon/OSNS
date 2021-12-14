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

#ifndef INTERFACE_SFTP_EXTENSIONS_H
#define INTERFACE_SFTP_EXTENSIONS_H

#include "sftp/common-protocol.h"

/* prototypes */

int send_sftp_statvfs_ctx(struct context_interface_s *interface, struct sftp_request_s *sftp_r, unsigned int *error);
int send_sftp_fsync_ctx(struct context_interface_s *interface, struct sftp_request_s *sftp_r, unsigned int *error);

unsigned int get_index_sftp_extension_statvfs(struct context_interface_s *interface);
unsigned int get_index_sftp_extension_fsync(struct context_interface_s *interface);

#endif
