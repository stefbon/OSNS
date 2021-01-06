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
#include <sys/syscall.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "workspace-interface.h"
#include "ssh/ssh-common.h"
#include "sftp/common-protocol.h"
#include "sftp/common.h"

static int channelname_cmp(char **p_name, int *p_left, const char *match)
{
    char *name=*p_name;
    int left=*p_left;
    unsigned int len=strlen(match);

    if (len + 1 <=left) {

	if (memcmp(name, match, len)==0 && name[len]==':') {

	    *p_name+=(len+1);
	    *p_left-=(len+1);
	    return 0;

	}

    } 

    return -1;

}

/* translate a name as service into channel type, name and properties
    name can be something like:
    "ssh-channel:session:*/

void translate_ssh_channel_name(struct ssh_channel_s *channel, char *name)
{
    int left=strlen(name);

    if (left <= 12) {

	logoutput_warning("translate_ssh_channel_name: error, name (%s) is too short", name);
	return;

    }

    if (channelname_cmp(&name, &left, "ssh-channel")==0) {

	if (channelname_cmp(&name, &left, _CHANNEL_NAME_SESSION)==0) {

	    channel->type=_CHANNEL_TYPE_SESSION;
	    channel->name=_CHANNEL_NAME_SESSION;

	    if (channelname_cmp(&name, &left, _CHANNEL_SESSION_NAME_SHELL)==0) {

		channel->target.session.type=_CHANNEL_SESSION_TYPE_SHELL;
		channel->target.session.name=_CHANNEL_SESSION_NAME_SHELL;

	    } else if (channelname_cmp(&name, &left, _CHANNEL_SESSION_NAME_EXEC)==0) {

		channel->target.session.type=_CHANNEL_SESSION_TYPE_EXEC;
		channel->target.session.name=_CHANNEL_SESSION_NAME_EXEC;

	    } else if (channelname_cmp(&name, &left, _CHANNEL_SESSION_NAME_SUBSYSTEM)==0) {

		channel->target.session.type=_CHANNEL_SESSION_TYPE_SUBSYSTEM;
		channel->target.session.name=_CHANNEL_SESSION_NAME_SUBSYSTEM;

		if (channelname_cmp(&name, &left, _CHANNEL_SUBSYSTEM_NAME_SFTP)==0) {

		    channel->target.session.use.subsystem.type=_CHANNEL_SUBSYSTEM_TYPE_SFTP;
		    channel->target.session.use.subsystem.name=_CHANNEL_SUBSYSTEM_NAME_SFTP;

		}

	    }

	} else {

	    if (channelname_cmp(&name, &left, _CHANNEL_NAME_DIRECT_STREAMLOCAL_OPENSSH_COM)==0) {

		channel->type=_CHANNEL_TYPE_DIRECT_STREAMLOCAL;
		channel->name=_CHANNEL_NAME_DIRECT_STREAMLOCAL_OPENSSH_COM;
		channel->target.direct_streamlocal.type=_CHANNEL_DIRECT_STREAMLOCAL_TYPE_OPENSSH_COM;

	    } else if (channelname_cmp(&name, &left, _CHANNEL_NAME_DIRECT_TCPIP)==0) {

		channel->type=_CHANNEL_TYPE_DIRECT_TCPIP;
		channel->name=_CHANNEL_NAME_DIRECT_TCPIP;

	    }

	}

    }

}

