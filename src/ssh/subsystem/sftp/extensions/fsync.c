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

void cb_ext_fsync(struct sftp_payload_s *payload, unsigned int pos)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_fsync (%i)", (int) gettid());

    if (payload->len >= pos + 4 + get_sftp_handle_size()) {
	char *data=payload->data;
	unsigned int len=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_sftp_handle_size()) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct commonhandle_s *handle=find_sftp_commonhandle(sftp, &data[pos], len, NULL);
	    struct sftp_subsystem_s *tmp=NULL;

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_fsync: handle not found");
		goto error;

	    }

	    tmp=get_sftp_subsystem_commonhandle(handle);

	    if (tmp==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_fsync: handle is not a sftp handle");
		goto error;

	    } else if (tmp != sftp) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_fsync: handle does belong by other sftp server");
		goto error;

	    } else {
		int result=0;

		if (handle->flags & COMMONHANDLE_FLAG_DIR) {
		    struct dirhandle_s *dh=&handle->type.dir;

		    result=(* dh->fsyncdir)(dh, 0);

		} else {
		    struct filehandle_s *fh=&handle->type.file;

		    result=(* fh->fsync)(fh, 0);

		}

		if (result==0) {

		    reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
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

	    }

	} else {

	    logoutput_warning("sftp_op_fsync: invalid handle size %i", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_fsync: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
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

void sftp_op_fsync(struct sftp_payload_s *payload)
{
    cb_ext_fsync(payload, 0);
}

