/*
  2018, 2019 Stef Bon <stefbon@gmail.com>

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

#include "logging.h"
#include "main.h"
#include "misc.h"

#include "ssh-common.h"
#include "ssh-channel.h"
#include "uri.h"

/* functions to translate an uri in a specific channel
    20170720: only sockets://path and tcp/ip connections to a host:port are supported
*/

int translate_channel_uri(struct ssh_channel_s *channel, char *uri)
{

    if (strncmp(uri, "socket://", 9)==0) {
	char *sep=strchr(&uri[8], ':');
	struct ssh_string_s *path=&channel->target.direct_streamlocal.path;

	if (sep) *sep='\0';
	channel->type = _CHANNEL_TYPE_DIRECT_STREAMLOCAL;
	channel->name = _CHANNEL_NAME_DIRECT_STREAMLOCAL_OPENSSH_COM;

	create_ssh_string(&path, strlen(&uri[9]), &uri[9], SSH_STRING_FLAG_ALLOC);

    } else if (strncmp(uri, "ssh://", 6)==0) {
	unsigned int len=strlen(uri);
	char *sep=NULL;
	char target[len];

	channel->type = _CHANNEL_TYPE_DIRECT_TCPIP;
	channel->name = _CHANNEL_NAME_DIRECT_TCPIP;

	/* there must be a host and a port in the uri like
	ssh://192.168.2.10:4400 */

	strcpy(target, &uri[6]);
	sep=strchr(target, ':');
	if (sep) {
	    struct ssh_string_s *host=&channel->target.direct_tcpip.host;

	    *sep='\0';
	    create_ssh_string(&host, strlen(target), target, SSH_STRING_FLAG_ALLOC);
	    channel->target.direct_tcpip.port=atoi(sep+1);

	} else {

	    return -1;

	}

    } else if (strncmp(uri, "exec://", 7)==0) {
	struct ssh_string_s *command=&channel->target.session.use.exec.command;

	channel->type = _CHANNEL_TYPE_SESSION;
	channel->target.session.type=_CHANNEL_SESSION_TYPE_EXEC;
	channel->target.session.name=_CHANNEL_SESSION_NAME_EXEC;
	create_ssh_string(&command, strlen(&uri[7]), &uri[7], SSH_STRING_FLAG_ALLOC);

    } else if (strncmp(uri, "shell://", 8)==0) {

	channel->type = _CHANNEL_TYPE_SESSION;
	channel->target.session.type=_CHANNEL_SESSION_TYPE_SHELL;
	channel->target.session.name=_CHANNEL_SESSION_NAME_SHELL;

    } else if (strncmp(uri, "subsystem://", 12)==0) {

	channel->type = _CHANNEL_TYPE_SESSION;
	channel->target.session.type=_CHANNEL_SESSION_TYPE_SUBSYSTEM;
	channel->target.session.name=_CHANNEL_SESSION_NAME_SUBSYSTEM;

	if (strcmp(&uri[12], "sftp")==0) {

	    channel->target.session.use.subsystem.type=_CHANNEL_SUBSYSTEM_TYPE_SFTP;
	    channel->target.session.use.subsystem.name=_CHANNEL_SUBSYSTEM_NAME_SFTP;

	}

    }

    return 0;

}


