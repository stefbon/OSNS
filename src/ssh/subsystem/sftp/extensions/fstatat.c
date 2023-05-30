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

/* SSH_FXP_EXTENDED - fstatat@sftp.osns.net
    message has the form:
    - string				handle
    - string				name/path
    - uint32				valid
    - uint32				flags
    */

void cb_ext_fstatat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data, unsigned int pos)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_fstatat (%i)", (int) gettid());

    if (inh->len >= pos + 4 + get_fs_handle_buffer_size() + 4 + 4 + 4) {
	unsigned int len=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len==get_fs_handle_buffer_size()) {
	    unsigned int error=0;
	    unsigned int count=0;
	    struct fs_handle_s *handle=NULL;
	    struct fs_socket_s *sock=NULL;
	    struct ssh_string_s path=SSH_STRING_INIT;

            handle=get_fs_handle(sftp->connection.unique, &data[pos], len, &count);

	    if (handle==NULL) {

		status=SSH_FX_INVALID_HANDLE;
		logoutput_warning("sftp_op_fstatat: handle not found");
		goto error;

	    }

            pos += count;
            path.len=get_uint32(&data[pos]);
            pos+=4;

            if (inh->len >= pos + path.len + 8) {
	        struct system_stat_s stat;
	        struct sftp_valid_s valid;
	        unsigned int flags=0;
	        unsigned int validbits=0;
	        unsigned int mask=0;
	        int result=-1;

                path.ptr=&data[pos];
                pos+=path.len;

                /* valid
                    TODO: add support for bits which do not belong to standard stat mask, like SSH_FILEXFER_ATTR_MIME_TYPE */

		validbits=get_uint32(&data[pos]);
		pos+=4;
		flags=get_uint32(&data[pos]);
		pos+=4;

		convert_sftp_valid_w(&sftp->attrctx, &valid, validbits); /* get the valid supported here */
		mask=translate_valid_2_stat_mask(&sftp->attrctx, &valid, 'r'); /* convert to a stat mask (sftp->stat) */
		flags &= (AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
		memset(&stat, 0, sizeof(struct system_stat_s));

                if (path.len==0) {

                    result=(* sock->ops.fgetstat)(sock, mask, &stat);

                } else {

                    if (handle->type==FS_HANDLE_TYPE_DIR) {
                        struct fs_path_s location=FS_PATH_INIT;

                        fs_path_set(&location, 's', (void *) &path);
                        result=(* sock->ops.type.dir.fstatat)(sock, &location, mask, &stat, flags);

                    } else {

                        status=SSH_FX_INVALID_PARAMETER;
			goto error;

                    }

                }

		if (result==0) {

		    if (reply_sftp_attr_from_stat(sftp, inh->id, &valid, &stat)==-1) logoutput_warning("sftp_op_fstatat: error sending attr");
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

	    logoutput_warning("sftp_op_fstatat: invalid handle size %i", len);
	    status=SSH_FX_INVALID_HANDLE;

	}

    }

    error:

    logoutput("sftp_op_fstatat: status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);
    return;

    disconnect:

    finish_sftp_subsystem(sftp);

}

/* do fstatat when being called having a code (mapped to this extension) of it's own */

void sftp_op_fstatat(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    cb_ext_fstatat(sftp, inh, data, 0);
}
