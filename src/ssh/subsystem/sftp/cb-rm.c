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
#include <sys/vfs.h>
#include <pwd.h>
#include <sys/syscall.h>

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "init.h"
#include "attr.h"
#include "send.h"
#include "handle.h"
#include "path.h"

/* SSH_FXP_REMOVE or SSH_FXP_RMDIR
    message has the form:
    - byte 				SSH_FXP_REMOVE/RMDIR
    - uint32				id
    - string				path [UTF-8]
    */

#define REMOVE_TYPE_DIR			1

static void sftp_op_remove_common(struct sftp_payload_s *payload, unsigned char what)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_remove (%i)", (int) gettid());

    /* message should at least have 4 bytes for the path string
	note an empty path is possible */

    if (payload->len>=4) {
	char *buffer=payload->data;
	struct ssh_string_s path=SSH_STRING_INIT;
	unsigned int pos=0;

	path.len=get_uint32(&buffer[pos]);
	pos+=4;
	path.ptr=&buffer[pos];

	/* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero) */

	if (payload->len >= path.len + 4) {
	    struct sftp_identity_s *user=&sftp->identity;
	    struct fs_location_path_s location=FS_LOCATION_PATH_INIT;
	    struct convert_sftp_path_s convert={NULL};
	    unsigned int size=get_fullpath_size(user, &path, &convert); /* get size of buffer for path */
	    char tmp[size+1];
	    unsigned int error=0;

	    set_buffer_location_path(&location, tmp, size+1, 0);
	    (* convert.complete)(user, &path, &location);
	    pos+=path.len;

	    if (what==REMOVE_TYPE_DIR) {

		if (system_remove_dir(&location)==0) {

		    reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
		    return;

		}

	    } else {

		if (system_remove_file(&location)==0) {

		    reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
		    return;

		}

	    }

	    status=SSH_FX_FAILURE;

	}

    }

    error:

    logoutput("sftp_op_remove: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

void sftp_op_remove(struct sftp_payload_s *payload)
{
    sftp_op_remove_common(payload, 0);
}

void sftp_op_rmdir(struct sftp_payload_s *payload)
{
    sftp_op_remove_common(payload, REMOVE_TYPE_DIR);
}
