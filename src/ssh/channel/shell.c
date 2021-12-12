/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-channel.h"
#include "ssh-utils.h"
#include "ssh-send.h"

int start_remote_shell(struct ssh_channel_s *channel, unsigned int *error)
{
    unsigned int seq=0;
    int result=-1;

    if (!(channel->flags & CHANNEL_FLAG_OPEN) || channel->flags & CHANNEL_FLAG_NODATA || channel->flags & CHANNEL_FLAG_OPENFAILURE) {

	*error=EIO;
	return -1;

    } else if (!(channel->type==_CHANNEL_TYPE_SESSION)) {

	*error=EINVAL;
	return -1;

    }

    channel->target.session.type=_CHANNEL_SESSION_TYPE_SHELL;
    channel->target.session.name=_CHANNEL_SESSION_NAME_SHELL;

    /* start the remote shell on the channel */

    logoutput("start_remote_shell");

    if (send_channel_start_command_message(channel, 1, &seq)>0) {
	struct timespec expire;
	struct ssh_payload_s *payload=NULL;

	get_channel_expire_init(channel, &expire);

	payload=get_ssh_payload_channel(channel, &expire, &seq, error);

	if (! payload) {

	    logoutput("start_remote_shell: error %i waiting for packet (%s)", *error, strerror(*error));
	    return -1;

	}

	if (payload->type==SSH_MSG_CHANNEL_SUCCESS) {

	    /* ready: channel ready to use */

	    logoutput("start_remote_shell: server started shell");
	    result=0;

	} else if (payload->type==SSH_MSG_CHANNEL_FAILURE) {

	    logoutput("start_remote_shell: server failed to start shell");

	} else {

	    logoutput("start_remote_shell: got unexpected reply %i", payload->type);

	}

	free_payload(&payload);

    } else {

	logoutput("start_remote_shell: error sending shell request");

    }

    if (result==0) {
	struct system_timespec_s expire=SYSTEM_TIME_INIT;
	struct ssh_payload_s *payload=NULL;

	/* process any message from server like banners */

	get_current_time_system_time(&expire);
	system_time_add(&expire, SYSTEM_TIME_ADD_ZERO, 1);

	processdatashell:

	payload=get_ssh_payload_channel(channel, &expire, NULL, error);

	if (payload) {

	    if (payload->type==SSH_MSG_CHANNEL_DATA) {
		unsigned int len=get_uint32(&payload->buffer[5]);
		char buffer[len+1];

		memcpy(buffer, &payload->buffer[9], len);
		buffer[len]='\0';

		replace_cntrl_char(buffer, len, REPLACE_CNTRL_FLAG_TEXT);
		// replace_newline_char(buffer, &len);

		logoutput("start_remote_shell: received %s", buffer);

	    } else {

		logoutput("start_remote_shell: got unexpected reply %i", payload->type);

	    }

	    free(payload);
	    payload=NULL;

	    goto processdatashell;

	}

    }

    return result;

}

void add_shell_channel(struct ssh_session_s *session)
{
    struct ssh_channel_s *channel=NULL;
    struct channel_table_s *table=&session->channel_table;
    unsigned int error=0;

    logoutput("add_shell_channel");

    channel=create_channel(session, session->connections.main, _CHANNEL_TYPE_SESSION);

    if (! channel) {

	logoutput("add_shell_channel: unable to create shell channel");
	return;

    }

    logoutput("add_shell_channel: add channel to table and open it");
    channel->name=_CHANNEL_SESSION_NAME_SHELL;

    if (add_channel(channel, CHANNEL_FLAG_OPEN)==-1) {

	free(channel);
	channel=NULL;
	return;

    }

    /* start a shell on the channel */

    logoutput("add_shell_channel: channel got (%i:%i) and start shell", channel->local_channel, channel->remote_channel);

    if (start_remote_shell(channel, &error)==0) {

	logoutput("add_shell_channel: started remote shell");
	table->shell=channel;

    } else {

	remove_channel(channel, CHANNEL_FLAG_CLIENT_CLOSE | CHANNEL_FLAG_SERVER_CLOSE);
	free_ssh_channel(&channel);
	channel=NULL;

    }

}
