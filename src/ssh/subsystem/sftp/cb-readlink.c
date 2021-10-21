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
#include "datatypes.h"
#include "network.h"

#include "protocol.h"

#include "osns_sftp_subsystem.h"
#include "attributes-write.h"
#include "send.h"
#include "path.h"
#include "handle.h"
#include "init.h"


static int reply_sftp_readlink(struct sftp_subsyste_s *sftp, uint32_t id, char *target, unsigned int len)
{
    /* reply of readlink is a special case of the name reply */
    return reply_sftp_names(sftp, id, 1, target, len, 0);
}

static void search_reply_readlink_target(struct sftp_subsystem_s *sftp, uint32_t id, char *path)
{
    unsigned int status=SSH_FX_BAD_MESSAGE;
    unsigned int size=512;
    char *buffer=NULL;

    allocbuffer:

    buffer=realloc(buffer, size);
    if (buffer==NULL) goto error;

    int bytesread=readlink(path, buffer, size);

    if (bytesread==-1) {

	switch (errno) {

	    case ENOENT:

		status=SSH_FX_NO_SUCH_FILE;
		break;

	    case ENOTDIR:

		status=SSH_FX_NO_SUCH_PATH;
		break;

	    case EACCES:

		status=SSH_FX_PERMISSION_DENIED;
		break;

	    default:

		statuc=SSH_FX_FAILURE; /* the best guess */

	}

	reply_sftp_status_simple(sftp, id, status);

    }

    if (bytesread==size) {

	/* truncation may have been done, there is no way to find out this is the case, so there is nothing else to do 
	    but to increase the buffersize and do it again */

	size+=512;
	goto allocbuffer;

    }

    reply_sftp_readlink(sftp, id, buffer, bytesread;

}

/* SSH_FXP_READLINK/
    message has the form:
    - byte 				SSH_FXP_READLINK
    - uint32				id
    - string				path
    */

void sftp_op_readlink(struct sftp_payload_s *payload)
{
    struct sftp_subsystem_s *sftp=payload->sftp;
    unsigned int status=SSH_FX_BAD_MESSAGE;

    /* message should at least have 4 bytes for the path string */

    if (payload->len>=4) {
	char *data=payload->data;
	struct sftp_identity_s *user=&sftp->identity;
	unsigned int pos=0;
	unsigned int len=0;
	unsigned int valid=0;

	len=get_uint32(&data[pos]);
	pos+=4;

	if (len + 4 <= payload->len) {
	    struct stat st;
	    int result=0;
	    unsigned int error=0;

	    if (len==0) {

		search_reply_readlink_target(sftp, payload->id, user->pwd.pw_home);

	    } else if (data[pos]=='/') {
		char tmp[len+1];

		memcpy(tmp, &data[pos], len);
		tmp[len]='\0';
		search_reply_readlink_target(sftp, payload->id, tmp);

	    } else {
		char tmp[user->len_home + len + 2];
		unsigned int index=0;

		/* not empty and not starting with a slash: relative to homedirectory */

		memcpy(&tmp[index], user->pwd.pw_home, user->len_home);
		index+=user->len_home;
		tmp[index]='/';
		memcpy(&tmp[index], &data[pos], len);
		index+=len;
		tmp[index]='\0';
		search_reply_readlink_target(sftp, payload->id, tmp);

	    }

	    return;

	}

    }

    logoutput("sftp_op_readlink: error status %i", status);
    reply_sftp_status_simple(sftp, payload->id, status);

}

