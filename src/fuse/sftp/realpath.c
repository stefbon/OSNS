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

#include "main.h"
#include "log.h"
#include "misc.h"

#include "workspace-interface.h"
#include "workspace.h"
#include "fuse.h"
#include "datatypes/ssh-uint.h"

#include "sftp/common-protocol.h"
#include "sftp/attr-context.h"
#include "interface/sftp-attr.h"
#include "interface/sftp-send.h"
#include "interface/sftp-wait-response.h"

char *get_realpath_sftp(struct context_interface_s *interface, unsigned char *target, unsigned char **path)
{
    struct sftp_request_s sftp_r;

    memset(&sftp_r, 0, sizeof(struct sftp_request_s));
    sftp_r.id=0;
    sftp_r.call.realpath.path=target;
    sftp_r.call.realpath.len=strlen((const char *)target);
    sftp_r.status=SFTP_REQUEST_STATUS_WAITING;

    if (send_sftp_realpath_ctx(interface, &sftp_r)>0) {
	struct system_timespec_s timeout=SYSTEM_TIME_INIT;

	get_sftp_request_timeout_ctx(interface, &timeout);

	if (wait_sftp_response_ctx(interface, &sftp_r, &timeout)==1) {
	    struct sftp_reply_s *reply=&sftp_r.reply;

	    if (reply->type==SSH_FXP_NAME) {
		char *pos=reply->response.names.buff;
		unsigned int len=0;

		/*
		    reply NAME looks like:
		    - string		filename
		    - ATTRS
		*/

		len=get_uint32((char *)pos);
		memmove(pos, pos+4, len);
		*path=(unsigned char *) pos;
		pos+=len;
		*pos='\0';

		logoutput("get_realpath_sftp: remote target %s", *path);

	    } else if (reply->type==SSH_FXP_STATUS) {

		unsigned int error=reply->response.status.linux_error;
		logoutput("get_realpath_sftp: server reply error %i getting realpath (%s)", error, strerror(error));
		*path=NULL;

	    } else {

		*path=NULL;

	    }

	}

    }

    return (char *) *path;

}
