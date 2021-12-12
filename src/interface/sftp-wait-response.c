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
#include "workspace.h"
#include "fuse.h"

#include "sftp/common-protocol.h"
#include "sftp/common.h"
#include "sftp/request-hash.h"

unsigned char wait_sftp_response_ctx(struct context_interface_s *interface, struct sftp_request_s *sftp_r, struct system_timespec_s *timeout)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    return wait_sftp_response(sftp, sftp_r, timeout);
}

static int send_sftp_request_data_default(struct sftp_request_s *r, char *data, unsigned int size, uint32_t *seq, struct list_element_s *list)
{
    struct context_interface_s *interface=r->interface;
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    int result=(* sftp->context.send_data)(sftp, data, size, seq, list);
    r->status |= SFTP_REQUEST_STATUS_SEND;
    return result;
}

static int send_sftp_request_data_blocked(struct sftp_request_s *r, char *data, unsigned int size, uint32_t *seq, struct list_element_s *list)
{
    logoutput("send_sftp_request_data_blocked");
    r->reply.error=EINTR;
    return -1;
}

void set_sftp_request_blocked(struct sftp_request_s *r)
{
    r->send=send_sftp_request_data_blocked;
}

static void set_sftp_request_status(struct fuse_request_s *f_request)
{

    if (f_request->flags & FUSE_REQUEST_FLAG_INTERRUPTED) {
	struct sftp_request_s *r=(struct sftp_request_s *) f_request->followup;

	if (r && (r->status & SFTP_REQUEST_STATUS_WAITING)) {

	    r->status|=SFTP_REQUEST_STATUS_INTERRUPT;
	    r->send=send_sftp_request_data_blocked;

	}

    }

}

void init_sftp_request(struct sftp_request_s *r, struct context_interface_s *i, struct fuse_request_s *f_request)
{

    memset(r, 0, sizeof(struct sftp_request_s));

    r->status = SFTP_REQUEST_STATUS_WAITING;
    r->interface=i;
    r->unique=i->unique;
    r->send=send_sftp_request_data_default;
    r->ptr=(void *) f_request;

    set_fuse_request_flags_cb(f_request, set_sftp_request_status);
    f_request->followup=(void *) r;

}

void init_sftp_request_minimal(struct sftp_request_s *r, struct context_interface_s *i)
{

    memset(r, 0, sizeof(struct sftp_request_s));

    r->status = SFTP_REQUEST_STATUS_WAITING;
    r->interface=i;
    r->unique=i->unique;
    r->send=send_sftp_request_data_default;
    r->ptr=NULL;

}

void get_sftp_request_timeout_ctx(struct context_interface_s *interface, struct system_timespec_s *timeout)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) (* interface->get_interface_buffer)(interface);
    get_sftp_request_timeout(sftp, timeout);
}
