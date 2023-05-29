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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"

#include "libosns-fspath.h"
#include "lib/system/fsrm.h"

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

static void sftp_op_remove_common(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data, unsigned char what)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_remove (%i)", (int) gettid());

    /* message should at least have 4 bytes for the path string
	note an empty path is possible */

    if (inh->len>=4) {
	struct ssh_string_s path=SSH_STRING_INIT;
	unsigned int pos=0;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	/* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero) */

	if (inh->len >= path.len + 4) {
	    struct fs_path_s location=FS_PATH_INIT;
	    struct convert_sftp_path_s convert={NULL};
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert); /* get size of buffer for path */
	    char tmp[size+1];
	    unsigned int error=0;

	    fs_path_assign_buffer(&location, tmp, size+1);
	    (* convert.complete)(sftp, &path, &location);

	    logoutput("sftp_op_remove_common : %.*s", fs_path_get_length(&location), fs_path_get_start(&location));

            status=SSH_FX_FAILURE;

	    if (what==REMOVE_TYPE_DIR) {

		if (system_remove_dir(&location)==-1) goto error;

	    } else {

		if (system_remove_file(&location)==-1) goto error;

	    }

            reply_sftp_status_simple(sftp, inh->id, SSH_FX_OK);

	}

    }

    error:

    logoutput("sftp_op_remove: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);

}

void sftp_op_remove(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    sftp_op_remove_common(sftp, inh, data, 0);
}

void sftp_op_rmdir(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    sftp_op_remove_common(sftp, inh, data, REMOVE_TYPE_DIR);
}
