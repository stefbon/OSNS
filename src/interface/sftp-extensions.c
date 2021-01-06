/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define LOGGING
#include "log.h"

#include "workspace-interface.h"
#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/extensions.h"

int send_sftp_statvfs_ctx(struct context_interface_s *interface, struct sftp_request_s *sftp_r, unsigned int *error)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    return send_sftp_statvfs(sftp, sftp_r, error);
}

int send_sftp_fsync_ctx(struct context_interface_s *interface, struct sftp_request_s *sftp_r, unsigned int *error)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    return send_sftp_fsync(sftp, sftp_r, error);
}

void set_sftp_fsync_unsupp_ctx(struct context_interface_s *interface)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    set_sftp_fsync_unsupp(sftp);
}

void set_sftp_statvfs_unsupp_ctx(struct context_interface_s *interface)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    set_sftp_statvfs_unsupp(sftp);
}
