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
#include "protocol.h"

#include "extensions.h"
#include "extensions/setprefix.h"
#include "extensions/statvfs.h"
#include "extensions/fsync.h"
#include "extensions/mapextension.h"

static struct sftp_protocol_extension_s extensions[] = {
    {.flags=0, .name="setprefix@sftp.osns.net", 	.code=0, .cb=cb_ext_setprefix,		.op=sftp_op_setprefix},
    {.flags=0, .name="statvfs@openssh.com", 		.code=0, .cb=cb_ext_statvfs, 		.op=sftp_op_statvfs},
    {.flags=0, .name="fsync@openssh.com", 		.code=0, .cb=cb_ext_fsync, 		.op=sftp_op_fsync},
    {.flags=0, .name="mapextension@sftp.osns.net", 	.code=0, .cb=cb_ext_mapextension, 	.op=sftp_op_mapextension}
    };

//    {.flags=0, .name="opendir@osns.net", .code=0, .cb=cb_ext_opendir},

static unsigned ext_maxnr=(sizeof(extensions)/sizeof(struct sftp_protocol_extension_s)) - 1; /* starting at zero */

struct sftp_protocol_extension_s *get_next_sftp_protocol_extension(struct sftp_protocol_extension_s *ext, unsigned int mask)
{
    unsigned int index=0;

    try:

    if (ext==NULL) {

	ext=&extensions[0];

    } else {
	unsigned int size=sizeof(extensions);

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

    /* only if mask is set and there is an extension found: test it is set in the mask ... */

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

    /* message should at least have 4 bytes for the name string; an empty name is not allowed
	note that no extension data is allowed */

    if (len>4 && len<=payload->len) {

	struct sftp_protocol_extension_s *ext=find_sftp_protocol_extension(&name, payload->sftp->extensions.mask);

	if (ext) {

	    (* ext->cb)(payload, len);
	    return;

	} else {

	    status=SSH_FX_OP_UNSUPPORTED;

	}

    }

    logoutput("sftp_op_extension: status %i", status);
    reply_sftp_status_simple(payload->sftp, payload->id, status);

}
