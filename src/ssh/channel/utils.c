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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "ssh-common.h"
#include "ssh-channel.h"

#include "ssh-hostinfo.h"
#include "ssh-utils.h"

#include "receive/msg-channel.h"
#include "send/msg-channel.h"

extern struct workerthreads_queue_struct workerthreads_queue;

static const char *openfailure_reasons[] = {
	"Open administratively prohibited.",
	"Open connect failed.", 
	"Open unknown channel type.",
	"Open resource shortage."};

const char *get_openfailure_reason(unsigned int reason)
{
    if (reason > 0 && reason <= (sizeof(openfailure_reasons) / sizeof(openfailure_reasons[0]))) return openfailure_reasons[reason-1];
    return "Open unknown failure.";
}

void get_channel_expire_init(struct ssh_channel_s *channel, struct system_timespec_s *expire)
{
    get_current_time_system_time(expire);
    system_time_add(expire, SYSTEM_TIME_ADD_ZERO, 4);
}

unsigned int get_channel_interface_info(struct ssh_channel_s *channel, char *buffer, unsigned int size)
{
    unsigned int result=0;

    if (size>=4) {

	memset(buffer, '\0', size);

	if (channel->flags & CHANNEL_FLAG_OPENFAILURE) {

	    store_uint32(buffer, EFAULT);
	    result=4;

	} else if (channel->flags & (CHANNEL_FLAG_SERVER_EOF | CHANNEL_FLAG_CLIENT_EOF)) {

	    store_uint32(buffer, ENODEV); /* connected with server but backend on server not */
	    result=4;

	} else if (channel->flags & (CHANNEL_FLAG_SERVER_EOF | CHANNEL_FLAG_CLIENT_EOF)) {

	    store_uint32(buffer, ENOTCONN); /* not connected with server */
	    result=4;

	} else {
	    struct fs_connection_s *connection=&channel->connection->connection;

	    if (connection->status & FS_CONNECTION_FLAG_DISCONNECT ) {

		store_uint32(buffer, ENOTCONN); /* not connected with server */
		result=4;

	    }

	}

    }

    return result;

}

static void receive_msg_channel_data_init(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=*p_payload;
    queue_ssh_payload_channel(channel, payload);
    *p_payload=NULL;
}

static void receive_msg_channel_data_down(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload)
{
    free_payload(p_payload);
}

static void receive_msg_channel_data_context(struct ssh_channel_s *channel, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=*p_payload;
    uint32_t seq=payload->sequence;
    unsigned int size=payload->len;
    unsigned char flags = payload->flags;
    char *buffer=(char *) payload;

    // logoutput("receive_msg_channel_data_context: resize from %i to %i", sizeof(struct ssh_payload_s) + size, size);

    memmove(buffer, payload->buffer, size);
    memset(&buffer[size], 0, offsetof(struct ssh_payload_s, buffer));
    *p_payload=NULL;

    (* channel->process_incoming_bytes)(channel, size);
    (* channel->context.receive_data)(channel, &buffer, size, seq, flags);
    if (buffer) free(buffer);
}

void switch_msg_channel_receive_data(struct ssh_channel_s *channel, const char *name, void (* cb)(struct ssh_channel_s *c, char **b, unsigned int size, uint32_t seq, unsigned char f))
{

    logoutput("switch_msg_channel_receive_data: %s", name);

    pthread_mutex_lock(&channel->mutex);

    channel->receive_msg_channel_data=receive_msg_channel_data_down;

    if (strcmp(name, "init")==0) {

	channel->receive_msg_channel_data=receive_msg_channel_data_init;

    } else if (strcmp(name, "context")==0) {

	channel->receive_msg_channel_data=receive_msg_channel_data_context;
	if (cb) channel->context.receive_data=cb;

    } else if (strcmp(name, "down")==0) {

	channel->receive_msg_channel_data=receive_msg_channel_data_down;

    }

    pthread_mutex_unlock(&channel->mutex);

}

void switch_channel_send_data(struct ssh_channel_s *channel, const char *what)
{

    pthread_mutex_lock(&channel->mutex);

    if (strcmp(what, "error")==0 || strcmp(what, "eof")==0 || strcmp(what, "close")==0) {

	channel->send_data_msg=send_channel_data_message_error;

    } else if (strcmp(what, "default")==0) {

	channel->send_data_msg=send_channel_data_message_connected;

    } else {

	logoutput_warning("switch_channel_send_data: status %s not reckognized", what);

    }

    pthread_mutex_unlock(&channel->mutex);

}
