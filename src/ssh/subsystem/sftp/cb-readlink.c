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

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "attr.h"
#include "send.h"
#include "path.h"
#include "handle.h"
#include "init.h"

/* reply to a valid readlink is a special case of a NAME reply with one name and a dummy ATTR struct:

    byte			SSH_FXP_NAME
    uint32			request-id
    uint32			count
    repeats count times:
	string			filename (UTF-8 for versions>=4)
	ATTR			attr
	(optional for versions>=6) byte eof


    the part with filenames and ATTR (repeated count times) is provided as is to the reply_sftp_names function, so sonstruct that here
    from the target and a dummy ATTR
    */


static int reply_sftp_readlink(struct sftp_subsystem_s *sftp, uint32_t id, struct fs_path_s *target)
{
    char name[4 + fs_path_get_length(target) + 5];
    unsigned int pos=0;

    /* write name as ssh string */

    store_uint32(&name[pos], fs_path_get_length(target));
    pos+=4;

    memcpy(&name[pos], fs_path_get_start(target), fs_path_get_length(target));
    pos+=target->len;

    /* write dummy ATTR == uint32 valid | byte type (both can be zero) */

    memset(&name[pos], 0, 5);
    pos+=5;

    return reply_sftp_names(sftp, id, 1, name, pos, 1);

}



/* SSH_FXP_READLINK/
    message has the form:
    - byte 				SSH_FXP_READLINK
    - uint32				id
    - string				path
    */

void sftp_op_readlink(struct sftp_subsystem_s *sftp, struct sftp_in_header_s *inh, char *data)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;

    /* message should at least have 4 bytes for the path string */

    if (inh->len>=4) {
	unsigned int pos=0;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	logoutput("sftp_op_readlink: path %.*s", path.len, path.ptr);

	if (path.len + 4 <= inh->len) {
	    struct fs_path_s location=FS_PATH_INIT;
	    struct fs_path_s target=FS_PATH_INIT;
	    struct convert_sftp_path_s convert;
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert);
	    char tmp[size+1];
	    int result=-ENOSYS;

	    /* get the fullpath on the local system */

	    fs_path_assign_buffer(&location, tmp, size+1);
	    (* convert.complete)(sftp, &path, &location);

#ifdef __linux__

	    result=fs_path_get_target_unix_symlink(&location, &target);

#endif

	    if (result==0) {

		logoutput("sftp_op_readlink: target %.*s", fs_path_get_length(&target), fs_path_get_start(&target));
		if (reply_sftp_readlink(sftp, inh->id, &target)==-1) logoutput_warning("sftp_op_readlink: error sending target");
		return;

	    } else {

		result=abs(result);
		status=SSH_FX_FAILURE;

		if (result==ENOENT) {

		    status=SSH_FX_NO_SUCH_FILE;

		} else if (result==ENOTDIR) {

		    status=SSH_FX_NO_SUCH_PATH;

		} else if (result==EACCES) {

		    status=SSH_FX_PERMISSION_DENIED;

		}

	    }

	}

    }

    logoutput("sftp_op_readlink: error status %i", status);
    reply_sftp_status_simple(sftp, inh->id, status);

}

