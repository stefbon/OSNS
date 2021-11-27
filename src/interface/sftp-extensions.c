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
    unsigned int index=interface->backend.sftp.statvfs_index;
    return send_sftp_extension_index(sftp, index, sftp_r);
}

unsigned int get_index_sftp_extension_statvfs(struct context_interface_s *interface)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    struct ssh_string_s name=SSH_STRING_SET(0, "statvfs@openssh.com");
    unsigned int index=get_sftp_protocol_extension_index(sftp, &name);

    logoutput_debug("get_index_sftp_extension_statvfs: index %u", index);
    return index;
}

int send_sftp_fsync_ctx(struct context_interface_s *interface, struct sftp_request_s *sftp_r, unsigned int *error)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    unsigned int index=interface->backend.sftp.fsync_index;
    return send_sftp_extension_index(sftp, index, sftp_r);
}

unsigned int get_index_sftp_extension_fsync(struct context_interface_s *interface)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    struct ssh_string_s name=SSH_STRING_SET(0, "fsync@openssh.com");
    unsigned int index=get_sftp_protocol_extension_index(sftp, &name);

    logoutput_debug("get_index_sftp_extension_fsync: index %u", index);
    return index;
}
