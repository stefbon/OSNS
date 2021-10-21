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

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"
#include "system.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "init.h"
#include "send.h"
#include "handle.h"
#include "path.h"
#include "cb-utils.h"

/* SSH_FXP_CLOSE
    message has the form:
    - byte 				SSH_FXP_CLOSE
    - uint32				id
    - string				handle

    NOTE: handle can be a handle used to open a file, but also a directory
*/

void sftp_op_close(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_close (%i)", (int) gettid());

    if (payload->len >= 4 + get_sftp_handle_size()) {
	char *data=payload->data;
	unsigned int len=0;

	len=get_uint32(&data[0]);

	if (len==get_sftp_handle_size()) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, &data[4], len, NULL);

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_close: handle not found");

	    } else {

		release_sftp_handle(&handle);
		reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
		return;

	    }

	} else {

	    logoutput_warning("sftp_op_close: invalid handle size %i", len);

	}

    }

    logoutput("sftp_op_close: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}
