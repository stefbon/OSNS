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
#include "protocol.h"
#include "extensions.h"
#include "extensions/setprefix.h"
#include "extensions/statvfs.h"

    /* TODO:
	extension which gives the extension a (free) number 210-255
	which is much more efficient than using the names
	something like "mapextension@bononline.nl"

	client sends:
	- byte 			SSH_FXP_EXTENDED
	- uint32		id
	- string		"mapextension@bononline.nl"
	- string		"name of extension to map"

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

*/


static struct sftp_protocol_extension_s extensions[] = {
    {.flags=0, .name="setprefix@osns.net", .code=0, .cb=cb_ext_setprefix},
    {.flags=0, .name="statvfs@openssh.com", .code=0, .cb=cb_ext_statvfs},
//    {.flags=0, .name="opendir@osns.net", .code=0, .cb=cb_ext_opendir},
//    {.flags=0, .name="mapextension@osns.net", .code=0, .cb=cb_ext_mapextension}
    };

static unsigned ext_maxnr=(sizeof(extensions)/sizeof(struct sftp_protocol_extension_s)) - 1; /* starting at zero */

struct sftp_protocol_extension_s *get_next_sftp_protocol_extension(struct sftp_protocol_extension_s *ext, unsigned int mask)
{
    unsigned int index=0;

    try:

    if (ext==NULL) {

	ext=&extensions[0];

    } else {
	unsigned int size=sizeof(extensions);

	getnext:

	if (((char *) ext >= (char *) &extensions[0]) && ((char *) ext < ((char *)&extensions + size))) {

	    index=(unsigned int)(((char *) ext - (char *) &extensions[0]) / sizeof(struct sftp_protocol_extension_s));

	    if (index>=ext_maxnr) {

		ext=NULL;

	    } else {

		index++;
		ext=&extensions[index];

	    }

	} else {

	    ext=NULL;

	}

    }

    if (mask && ext) {

	if (((1 << index) & mask)==0) goto try;

    }

    return ext;
}

struct sftp_protocol_extension_s *find_sftp_protocol_extension(struct ssh_string_s *name, unsigned int mask)
{
    struct sftp_protocol_extension_s *ext=NULL;

    ext=get_next_sftp_protocol_extension(NULL, mask);

    while (ext) {

	if ((name->len==strlen(ext->name)) && memcmp(ext->name, name->ptr, name->len)==0) break;
	ext=get_next_sftp_protocol_extension(ext, mask);

    }

    return ext;
}

/* SSH_FXP_EXTENDED
    message has the form:
    - byte 				SSH_FXP_REMOVE/RMDIR
    - uint32				id
    - string				name of extension
    - ....				extension specific data
    */


void sftp_op_extension(struct sftp_payload_s *payload)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;
    struct ssh_string_s name=SSH_STRING_INIT;
    unsigned int len=read_ssh_string(payload->data, payload->len, &name);

    /* message should at least have 4 bytes for the name string; an empty name is not allowed */

    if (len>4 && len<payload->len) {

	struct sftp_protocol_extension_s *ext=find_sftp_protocol_extension(&name, payload->sftp->extensions.mask);

	if (ext) {

	    (* ext->cb)(payload, len);
	    return;

	}

    }

    logoutput("sftp_op_extension: status %i", status);
    reply_sftp_status_simple(payload->sftp, payload->id, status);

}

