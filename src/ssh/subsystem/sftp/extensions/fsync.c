/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#include "osns_sftp_subsystem.h"

#include "../protocol.h"
#include "../send.h"
#include "../handle.h"

/* SSH_FXP_EXTENDED - fsync@openssh.com
    message has the form:
    - string				handle
    */

void cb_ext_fsync(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data, unsigned int pos)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_fsync (%i)", (int) gettid());

    if (inh->len >= pos + 4 + get_fs_handle_buffer_size()) {
	unsigned int len=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_fs_handle_buffer_size()) {
	    unsigned int count=0;
	    struct fs_handle_s *handle=NULL;
	    struct fs_socket_s *sock=NULL;
	    int result=0;

            handle=get_fs_handle(sftp->connection.unique, &data[pos], len, &count);

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_fsync: handle not found");
		goto error;

	    }

            pos+=count;
            sock=&handle->socket;

            result=(* sock->ops.fsync)(sock, 0);

	    if (result==0) {

		reply_sftp_status_simple(sftp, inh->id, SSH_FX_OK);
		return;

	    } else {

		result=abs(result);
		status=SSH_FX_FAILURE;

		if (result==EIO || result==EROFS || result==EINVAL) {

		    status=SSH_FX_FAILURE;

		} else if (result==ENOSPC) {

		    status=SSH_FX_NO_SPACE_ON_FILESYSTEM;

		} else if (result==EDQUOT) {

		    status=SSH_FX_QUOTA_EXCEEDED;

		}

	    }

	} else {

	    logoutput_warning("sftp_op_fsync: invalid handle size %i", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_fsync: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}


/* do fsync when being called having a code (mapped to this extension) of it's own

    SSH_FXP_custom
    message has the form:
    - byte 				custom
    - uint32				id
    - string				handle

    NOTE: the fsync used here is the full sync: data and attributes
*/

void sftp_op_fsync(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    cb_ext_fsync(sftp, inh, data, 0);
}

