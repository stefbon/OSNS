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
#include "init.h"
#include "attr.h"
#include "send.h"
#include "handle.h"
#include "path.h"

/* SSH_FXP_MKDIR
    message has the form:
    - byte 				SSH_FXP_MKDIR
    - uint32				id
    - string				path [UTF-8]
    - ATTR				attrs
    */

void sftp_op_mkdir(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp; 
    unsigned int status=SSH_FX_BAD_MESSAGE;

    logoutput("sftp_op_mkdir (%i)", (int) gettid());

    /* message should at least have 8 bytes for the path string and the valid
	note an empty path is possible */

    if (payload->len>=8) {
	char *buffer=payload->data;
	struct ssh_string_s path=SSH_STRING_INIT;
	unsigned int pos=0;

	path.len=get_uint32(&buffer[pos]);
	pos+=4;
	path.ptr=&buffer[pos];
	pos+=path.len;

	/* sftp packet size is at least:
	    - 4 + len ... path (len maybe zero) */

	if (payload->len >= path.len + 8) {
	    struct fs_location_path_s location=FS_LOCATION_PATH_INIT;
	    struct convert_sftp_path_s convert;
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert); /* get size of buffer for path */
	    char tmp[size+1];
	    unsigned int error=0;
	    struct attr_buffer_s abuff;
	    struct system_stat_s stat;
	    struct fs_init_s init;
	    struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
	    unsigned int valid_bits=0;

	    assign_buffer_location_path(&location, tmp, size+1);
	    (* convert.complete)(sftp, &path, &location);

	    logoutput("sftp_op_mkdir : %.*s", location.len, location.ptr);

	    /* initialize the stat with sane values */

	    memset(&stat, 0, sizeof(struct system_stat_s));
	    set_uid_system_stat(&stat, sftp->identity.pwd.pw_uid);
	    set_gid_system_stat(&stat, sftp->identity.pwd.pw_gid); /* is this also the group this process is running under ... ?? */
	    set_mode_system_stat(&stat, (S_IRWXU | S_IRWXO | S_IROTH | S_IXOTH)); /* set mode to 775 for now */

	    /* read the atributes received from client */

	    set_attr_buffer_read(&abuff, &buffer[pos], payload->len - pos);

	    valid_bits=(* abuff.ops->rw.read.read_uint32)(&abuff);
	    read_attributes_generic(&sftp->attrctx, &abuff, &r, &stat, valid_bits);

	    /* do here a check attributes are valid */

	    /* do this different */

	    init.mode=get_mode_system_stat(&stat);

	    if (system_create_dir(&location, &init)==0) {

		reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
		return;

	    }

	    status=SSH_FX_FAILURE;

	}

    }

    error:

    logoutput("sftp_op_remove: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}
