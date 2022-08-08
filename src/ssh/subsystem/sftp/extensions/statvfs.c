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

/* SSH_FXP_EXTENDED - statvfs@openssh.com
    message has the form:
    - string				path
    */

void cb_ext_statvfs(struct sftp_payload_s *payload, unsigned int pos)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput_debug("cb_ext_statvfs (%i) pos %i payload len %i", (int) gettid(), pos, payload->len);

    if (payload->len >= pos + 4) {
	char *data=payload->data;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	if (pos <= payload->len) {
	    struct system_statvfs_s statvfs;
	    struct fs_location_path_s location=FS_LOCATION_PATH_INIT;
	    struct convert_sftp_path_s convert;
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert);
	    char tmp[size +1];
	    int result=0;

	    assign_buffer_location_path(&location, tmp, size+1);
	    (* convert.complete)(sftp, &path, &location);
	    result=system_getstatvfs(&location, &statvfs);

	    logoutput_debug("cb_ext_statvfs (%i) path %.*s %i", (int) gettid(), location.len, location.ptr, result);

	    if (result==0) {
		char data[88];

		pos=0;

		store_uint64(&data[pos], statvfs.stvfs_blocksize);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_fragmentsize);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_nrblocks);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_freeblocks);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_availblocks);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_nrinodes);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_freeinodes);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_availinodes);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_fsid);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_mountflags);
		pos+=8;
		store_uint64(&data[pos], statvfs.stvfs_namemax);
		pos+=8;

		reply_sftp_extension(sftp, payload->id, data, pos);
		return;

	    } else {

		result=abs(result);
		if (result==ENOENT) {

		    status=SSH_FX_NO_SUCH_FILE;

		} else if (result==EACCES) {

		    status=SSH_FX_PERMISSION_DENIED;

		} else if (result==ENOTDIR) {

		    status=SSH_FX_NOT_A_DIRECTORY;

		} else {

		    status=SSH_FX_FAILURE;

		}

	    }

	}

    }

    logoutput("cb_ext_statvfs: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);
    return;

}

/* get statvfs when being called having a code (mapped to this extension) of it's own

    SSH_FXP_custom
    message has the form:
    - byte 				custom
    - uint32				id
    - string				path
    - uint32				flags

*/

void sftp_op_statvfs(struct sftp_payload_s *payload)
{
    cb_ext_statvfs(payload, 0);
}

