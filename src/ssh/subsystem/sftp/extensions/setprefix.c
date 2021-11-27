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
#include "misc.h"
#include "datatypes.h"

#include "threads.h"
#include "eventloop.h"
#include "users.h"
#include "mountinfo.h"

#include "misc.h"
#include "osns_sftp_subsystem.h"

#include "../protocol.h"
#include "../send.h"

/* SSH_FXP_EXTENSION: setprefix
    message has the form:

    - string				prefix
    - uint32				flags

    flags is conbination of following flags:
    SFTP_PREFIX_FLAG_IGNORE_SYMLINK_XDEV				: do not allow symlinks pointing outside prefix
    SFTP_PREFIX_FLAG_IGNORE_SPECIAL_FILES				: ignore special files like socket and character devices (only dir, file and symlink)

*/

void cb_ext_setprefix(struct sftp_payload_s *payload, unsigned int pos)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    if (sftp->prefix.path.len>0) {

	status=SSH_FX_FAILURE;
	goto out;

    }

    /* message should at least have 4 bytes extra for the path string,
	and 4 for the flags
	note an empty path is possible */

    if (payload->len >= pos + 8) {
	char *data=payload->data;
	struct sftp_identity_s *user=&sftp->identity;
	struct ssh_string_s path=SSH_STRING_INIT;

	path.len=get_uint32(&data[pos]);
	pos+=4;
	path.ptr=&data[pos];
	pos+=path.len;

	if (pos + path.len + 8 <= payload->len) {
	    struct system_stat_s stat;
	    struct fs_location_path_s location=FS_LOCATION_PATH_INIT;
	    struct convert_sftp_path_s convert;
	    unsigned int size=(* sftp->prefix.get_length_fullpath)(sftp, &path, &convert);
	    char tmp[size +1];
	    int result=0;
	    unsigned int flags=get_uint32(&data[pos]);

	    set_buffer_location_path(&location, tmp, size+1, 0);
	    (* convert.complete)(sftp, &path, &location);
	    result=system_getstat(&location, SYSTEM_STAT_TYPE | SYSTEM_STAT_MODE, &stat);

	    logoutput("sftp extension setprefix: %.*s result %i", location.len, location.ptr, result);

	    if (result==0) {

		/* it does exist, it must be a directory */

		if (system_stat_test_ISDIR(&stat)) {
		    struct ssh_string_s *prefix=&sftp->prefix.path;

		    /* set prefix for sftp subsystem */

		    if (create_ssh_string(&prefix, location.len, location.ptr, SSH_STRING_FLAG_ALLOC)) {

			if (flags) {
			    unsigned int allflags=(SFTP_PREFIX_FLAG_IGNORE_XDEV_SYMLINKS | SFTP_PREFIX_FLAG_IGNORE_SPECIAL_FILES);

			    if ((flags & ~allflags)) logoutput_warning("cb_ext_setprefix: flags %i not reckognized", (flags & ~allflags));

			    if (flags & SFTP_PREFIX_FLAG_IGNORE_XDEV_SYMLINKS) {

				sftp->prefix.flags |= SFTP_PREFIX_FLAG_IGNORE_XDEV_SYMLINKS;
				logoutput("sftp extension setprefix: set flag ignore xdev symlinks");

			    }

			    if (flags & SFTP_PREFIX_FLAG_IGNORE_SPECIAL_FILES) {

				sftp->prefix.flags |= SFTP_PREFIX_FLAG_IGNORE_SPECIAL_FILES;
				logoutput("sftp extension setprefix: set flag ignore special files");

			    }

			}

			reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
			return;

		    }

		    logoutput_warning("sftp extension setprefix: unable to create prefix");
		    status=SSH_FX_FAILURE;

		} else {

		    logoutput_warning("sftp extension setprefix: prefix is not a directory");
		    status=SSH_FX_NOT_A_DIRECTORY;

		}

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

    out:

    logoutput("sftp extension setprefix: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

/* set prefix when being called having a code (mapped to this extension) of it's own

    SSH_FXP_custom
    message has the form:
    - byte 				custom
    - uint32				id
    - string				path
    - uint32				flags

*/

void sftp_op_setprefix(struct sftp_payload_s *payload)
{
    cb_ext_setprefix(payload, 0);
}

