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
#include "../extensions.h"
#include "../send.h"
#include "../handle.h"
#include "../init.h"

    /* TODO:
	extension which gives the extension a (free) number 210-255
	which is much more efficient than using the names
	something like "mapextension@bononline.nl"

	client sends:
	- byte 			SSH_FXP_EXTENDED
	- uint32		id
	- string		"mapextension@sftp.osns.net"
	- string		"name of extension to (un)map"
	- byte			0 = unmap / 1 = map

	when mapping:

	server replies:
	- byte			SSH_FXP_EXTENDED_REPLY
	- uint32		id
	- byte			number to use

	or when error:
	- byte			SSH_FXP_STATUS
	- uint32		id
	- uint32		error code
	- string		error string

	like:

	- SSH_FX_OP_UNSUPPORTED			: extension not supported
	- SSH_FX_INVALID_PARAMETER		: name of extension to map not reckognized or extension already mapped
	- SSH_FX_FAILURE			: failed like too many extensions to map

	when unmapping:

	-server replies:

	- SSH_FX_OK				: when code releashed successfully
	- SSH_FX_INVALID_PARAMETER		: when extension not reckognized
	- SSH_FX_FAILURE			: when extension not mapped before

*/

#define SFTP_MAPPING_CODE_START			210
#define SFTP_MAPPING_CODE_END			255

static unsigned char find_free_sftp_code(struct sftp_subsystem_s *sftp)
{
    unsigned char code=0;

    for (unsigned int i=SFTP_MAPPING_CODE_START; i <= SFTP_MAPPING_CODE_END; i++) {

	if (check_sftp_cb_is_taken(sftp, i)==0) {

	    code=i;
	    break;

	}

    }

    return code;

}

void cb_ext_mapextension(struct sftp_payload_s *payload, unsigned int pos)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    if (payload->len > pos + 4 + 1) {
	char *data=payload->data;
	struct ssh_string_s name=SSH_STRING_INIT;

	name.len=get_uint32(&data[pos]);
	pos+=4;
	name.ptr=&data[pos];
	pos+=name.len;

	if (pos + name.len + 4 + 1 <= payload->len) {
	    struct sftp_extensions_s *extensions=&sftp->extensions;
	    struct sftp_protocol_extension_s *ext=find_sftp_protocol_extension(&name, extensions->mask);

	    if (ext) {
		unsigned char action=data[pos];

		if (action==1) {

		    /* get a code */

		    if (ext->code==0) {

			/* not already mapped */

			unsigned char code=find_free_sftp_code(sftp);

			if (code>0) {
			    char tmp[1];

			    logoutput("cb_ext_mapextension: giving extension %.*s code ", name.len, name.ptr, code);
			    tmp[0]=code;
			    reply_sftp_extension(sftp, payload->id, tmp, 1);

			    set_sftp_cb(sftp, code, ext->op);
			    ext->code=code;

			    return;

			} else {

			    /* no free code available */
			    status=SSH_FX_FAILURE;

			}

		    } else {

			/* extension already has a code */
			status=SSH_FX_INVALID_PARAMETER;

		    }

		} else if (action==0) {

		    /* release a code */

		    if (ext->code>0) {

			/* free the code */

			set_sftp_cb(sftp, ext->code, NULL);
			ext->code=0;
			reply_sftp_status_simple(sftp, payload->id, SSH_FX_OK);
			return;

		    } else {

			/* extension has no code assigned */
			status=SSH_FX_INVALID_PARAMETER;

		    }

		} else {

		    /* wrong action */
		    status=SSH_FX_INVALID_PARAMETER;

		}

	    } else {

		/* no extension found */
		status=SSH_FX_OP_UNSUPPORTED;

	    }

	}

    }

    out:

    logoutput("cb_ext_mapextension: status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

/* get statvfs when being called having a code (mapped to this extension) of it's own

    SSH_FXP_custom
    message has the form:
    - byte 				custom
    - uint32				id
    - string				path
    - uint32				flags

*/

void sftp_op_mapextension(struct sftp_payload_s *payload)
{
    cb_ext_mapextension(payload, 0);
}

