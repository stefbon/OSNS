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
#include "logging.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/request-hash.h"


unsigned char wait_sftp_response_ctx(struct context_interface_s *interface, struct sftp_request_s *sftp_r, struct timespec *timeout)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    return wait_sftp_response(sftp, sftp_r, timeout);
}

static void set_sftp_request_status(struct fuse_request_s *f_request)
{

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {
	struct sftp_request_s *sftp_r=(struct sftp_request_s *) f_request->data;

	if (sftp_r && (sftp_r->status & SFTP_REQUEST_STATUS_WAITING)) sftp_r->status|=SFTP_REQUEST_STATUS_INTERRUPT;

    }

}

void set_sftp_request_fuse(struct sftp_request_s *sftp_r, struct fuse_request_s *f_request)
{
    f_request->data=(void *) sftp_r;
    sftp_r->ptr=(void *)f_request;
    set_fuse_request_flags_cb(f_request, set_sftp_request_status);
}

void get_sftp_request_timeout_ctx(struct context_interface_s *interface, struct timespec *timeout)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    get_sftp_request_timeout(sftp, timeout);
}
