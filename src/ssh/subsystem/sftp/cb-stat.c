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

#include "main.h"
#include "log.h"
#include "misc.h"
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "send.h"
#include "path.h"
#include "handle.h"
#include "init.h"
#include "attr.h"

/* SSH_FXP_(L)STAT/
    message has the form:
    - byte 				SSH_FXP_STAT or SSH_FXP_LSTAT
    - uint32				id
    - string				path
    - uint32				flags
    */

static void sftp_op_stat_generic(struct sftp_subsystem_s *sftp, struct sftp_payload_s *payload, int (* cb_stat)(struct fs_location_path_s *p, unsigned int mask, struct system_stat_s *s))
{
    unsigned int status=SSH_FX_BAD_MESSAGE;
    char *data=payload->data;

    /* message should at least have 4 bytes for the path string, and 4 for the flags
	note an empty path is possible */

    if (payload->len>=8) {
	char *data=payload->data;
	struct sftp_identity_s *user=&sftp->identity;
	unsigned int pos=0;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	if (path.len + 8 <= payload->len) {
	    struct system_stat_s stat;
	    struct sftp_valid_s valid;
	    struct fs_location_s location;
	    struct convert_sftp_path_s convert;
	    unsigned int size=get_fullpath_size(user, &path, &convert);
	    char pathtmp[size+1];
	    int result=0;
	    unsigned int mask=0;
	    uint32_t validbits=0;

	    validbits=get_uint32(&data[pos]);
	    convert_sftp_valid_w(&sftp->attrctx, &valid, validbits);

	    memset(&location, 0, sizeof(struct fs_location_s));
	    location.flags=FS_LOCATION_FLAG_PATH;
	    set_buffer_location_path(&location.type.path, pathtmp, size+1, 0);
	    (* convert.complete)(user, &path, &location.type.path);

	    mask=translate_valid_2_stat_mask(&sftp->attrctx, &valid, 'w');
	    result=(* cb_stat)(&location.type.path, mask, &stat);

	    logoutput("sftp_op_stat: result %i valid %i", result, valid.mask);

	    if (result==0) {
		struct rw_attr_result_s r=RW_ATTR_RESULT_INIT;
		unsigned int size=get_size_buffer_write_attributes(&sftp->attrctx, &r, &valid);
		char tmp[size];
		struct attr_buffer_s abuff;

		set_attr_buffer_write(&abuff, tmp, size);
		(* abuff.ops->rw.write.write_uint32)(&abuff, r.valid.mask);
		write_attributes_generic(&sftp->attrctx, &abuff, &r, &stat, &valid);

		if (reply_sftp_attrs(sftp, payload->id, tmp, abuff.len)==-1) logoutput_warning("sftp_op_stat: error sending attr");
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

    logoutput("sftp_op_stat_generic: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

void sftp_op_stat(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    sftp_op_stat_generic(sftp, payload, system_getstat);
}

void sftp_op_lstat(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    sftp_op_stat_generic(sftp, payload, system_getlstat);
}

