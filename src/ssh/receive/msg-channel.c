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
#include "ssh-connections.h"
#include "ssh-channel.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-utils.h"
#include "ssh-signal.h"

/*

    possible values:

    SSH_MSG_CHANNEL_OPEN                        90
    SSH_MSG_CHANNEL_OPEN_CONFIRMATION           91
    SSH_MSG_CHANNEL_OPEN_FAILURE                92
    SSH_MSG_CHANNEL_WINDOW_ADJUST               93
    SSH_MSG_CHANNEL_DATA                        94
    SSH_MSG_CHANNEL_EXTENDED_DATA               95
    SSH_MSG_CHANNEL_EOF                         96
    SSH_MSG_CHANNEL_CLOSE                       97
    SSH_MSG_CHANNEL_REQUEST                     98
    SSH_MSG_CHANNEL_SUCCESS                     99
    SSH_MSG_CHANNEL_FAILURE                     100

*/
/*
    message has the following form:

    - byte		SSH_MSG_CHANNEL_OPEN
    - string		channel type
    - uint32		sender channel
    - uint32		initial window size
    - uint32		maximum packet size
    - byte[n]		channel type specific data

*/

static void receive_msg_channel_open(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    struct ssh_string_s type=SSH_STRING_INIT;
    unsigned int senderchannel=0;
    unsigned int windowsize=0;
    unsigned int maxpacketsize=0;
    struct ssh_string_s data=SSH_STRING_INIT;
    struct ssh_channel_s *channel=NULL;
    uint32_t seq=0;
    int result=-1;

    if (payload->len<17) {

	logoutput("receive_msg_open: message too small (size: %i)", payload->len);
	free_payload(&payload);
	goto out;

    }

    set_msg_buffer_payload(&mb, payload);
    msg_read_byte(&mb, NULL);
    msg_read_ssh_string(&mb, &type);
    msg_read_uint32(&mb, &senderchannel);
    msg_read_uint32(&mb, &windowsize);
    msg_read_uint32(&mb, &maxpacketsize);

    if (mb.error>0) {

	logoutput("receive_msg_channel_open: error %i reading message (%s)", mb.error, strerror(mb.error));
	goto out;

    }

    data.len=(mb.len - mb.pos);
    data.ptr=&mb.data[mb.pos];

    /* what type of channel:
	    - "session"					(send by client) -> subsystem, shell, exec !
	    - "x11"					(send by client)
	    - "forwarded-tcpip"				(send by server) -> connect to daemon/service like sql server, print server
	    - "direct-tcpip"				(send by client)
	    - "forwarded-streamlocal@openssh.com"	(send by server) -> idem see forwarded-tcpip
	    - "direct-streamlocal@openssh.com"		(send by client)
    */

    channel=open_new_channel(connection, &type, senderchannel, windowsize, maxpacketsize, &data);
    free_payload(&payload);
    payload=NULL;

    if (channel) {

	logoutput("receive_msg_channel_open: open channel %s success (%i:%i)", channel->name, channel->local_channel, channel->remote_channel);

    } else {

	logoutput("receive_msg_channel_open: failed to open channel");
	goto out;

    }

    if (send_channel_open_confirmation(channel, NULL, &seq)>0) {

	logoutput("receive_msg_channel_open: send open confirmation (%s %i:%i)", channel->name, channel->local_channel, channel->remote_channel);
	result=0;

    } else {

	logoutput("receive_msg_channel_open: failed to send open confirmation (%s %i:%i)", channel->name, channel->local_channel, channel->remote_channel);
	goto out;

    }

    out:

    if (result==-1) {

	if (type.len>0) {

	    if (send_channel_open_failure(connection, senderchannel, SSH_OPEN_UNKNOWN_CHANNEL_TYPE, &seq)>0) {

		logoutput("receive_msg_open: send a channel open failure message");

	    } else {

		logoutput("receive_msg_open: failed to send a channel open failure message");

	    }

	} else {

	    logoutput("receive_msg_open: failed to send a channel open failure message");

	}

	if (channel) free_ssh_channel(&channel);

    }

}


/*
    message has the following form:

    - byte		SSH_MSG_CHANNEL_OPEN_CONFIRMATION
    - uint32		recipient channel
    - uint32		sender channel
    - uint32		initial window size
    - uint32		maximum packet size

*/

static void receive_msg_channel_open_confirmation(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<17) {

	logoutput("receive_msg_open_confirmation: message too small (size: %i)", payload->len);

    } else {
	unsigned int pos=1;
	unsigned int local_channel=0;
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct ssh_channel_s *channel=NULL;
	struct simple_lock_s rlock;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;

	logoutput("receive_msg_open_confirmation: local channel %i", local_channel);

	channeltable_readlock(table, &rlock);
	channel=lookup_session_channel_for_payload(table, local_channel, &payload);
	channeltable_unlock(table, &rlock);

    }

    if (payload) {

	logoutput("receive_msg_open_confirmation: free payload (%i)", payload->type);
	free_payload(&payload);

    }

}

/*
    message has the following form:

    - byte		SSH_MSG_CHANNEL_OPEN_FAILURE
    - uint32		recipient channel
    - uint32		reason code
    - string		description
    - string		language tag

*/

static void receive_msg_channel_open_failure(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<17) {

	logoutput("receive_msg_open_failure: message too small (size: %i)", payload->len);

    } else {
	unsigned int pos=1;
	unsigned int local_channel=0;
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct ssh_channel_s *channel=NULL;
	struct simple_lock_s rlock;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;

	channeltable_readlock(table, &rlock);
	channel=lookup_session_channel_for_payload(table, local_channel, &payload);
	channeltable_unlock(table, &rlock);

    }

    if (payload) free_payload(&payload);

}

static void receive_msg_channel_window_adjust(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    if (payload->len<9) {

	logoutput("receive_msg_channel_window_adjust: message too small (size: %i)", payload->len);

    } else {
	unsigned int pos=1;
	unsigned int local_channel=0;
	unsigned int size=0;
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct ssh_channel_s *channel=NULL;
	struct simple_lock_s rlock;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	size=get_uint32(&payload->buffer[pos]);

	logoutput("receive_msg_channel_window_adjust: channel %i size %i", local_channel, size);

	channeltable_readlock(table, &rlock);
	channel=lookup_session_channel(table, local_channel);
	if (channel) {

	    pthread_mutex_lock(&channel->mutex);
	    channel->remote_window+=size;
	    pthread_mutex_unlock(&channel->mutex);

	}
	channeltable_unlock(table, &rlock);
	free_payload(&payload);

    }

    if (payload) free_payload(&payload);

}

static void receive_msg_channel_data(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    /*
	call the specific handler for the channel

	- byte			SSH_MSG_CHANNEL_DATA
	- uint32		recipient channel
	- string		data

	minimum size: 1 + 4 + 4 + 1 = 10

    */

    if (payload->len>9) {
	unsigned int local_channel=0;
	unsigned int len=0;
	unsigned int pos=1;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	len=get_uint32(&payload->buffer[pos]);
	pos+=4;

	if (len + pos <= payload->len) {
	    struct ssh_session_s *session=get_ssh_connection_session(connection);
	    struct channel_table_s *table=&session->channel_table;
	    struct ssh_channel_s *channel=NULL;
	    struct simple_lock_s rlock;

	    /* make sure the relevant data is at start of buffer */

	    // logoutput_base64encoded("receive_msg_channel_data", &payload->buffer[pos], len);

	    memmove(payload->buffer, &payload->buffer[pos], len);
	    memset(&payload->buffer[len], 0, payload->len - len);

	    // logoutput("receive_msg_channel_data: resized from %i to %i", payload->len, len);

	    payload->len=len;

	    channeltable_readlock(table, &rlock);
	    channel=lookup_session_channel_for_data(table, local_channel, &payload);
	    channeltable_unlock(table, &rlock);

	}

    }

    if (payload) free_payload(&payload);

}

static void receive_msg_channel_extended_data(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    /*
	process the extended data, which will be probably output from stderr
	so it's related to a command and/or subsystem (sftp)
    */

    /*
	- byte					SSH_MSG_CHANNEL_EXTENDED_DATA
	- uint32				recipient channel
	- uint32				data_type_code
	- string				data

	data type can be one of:
	(at this moment 20160919)
	- SSH_EXTENDED_DATA_STDERR		1

    */

    if (payload->len>13) {
	unsigned int local_channel=0;
	unsigned int code=0;
	unsigned int len=0;
	unsigned int pos=1;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	code=get_uint32(&payload->buffer[pos]);
	pos+=4;
	len=get_uint32(&payload->buffer[pos]);
	pos+=4;

	if (len + pos == payload->len) {

	    if (code==SSH_EXTENDED_DATA_STDERR) {
		struct ssh_session_s *session=get_ssh_connection_session(connection);
		struct channel_table_s *table=&session->channel_table;
		struct ssh_channel_s *channel=NULL;
		struct simple_lock_s rlock;

		/* make sure the relevant data is at start of buffer */

		memmove(payload->buffer, &payload->buffer[pos], len);
		memset(&payload->buffer[len], 0, payload->len - len);
		payload->len=len;
		payload->flags |= SSH_PAYLOAD_FLAG_ERROR;

		channeltable_readlock(table, &rlock);
		channel=lookup_session_channel_for_data(table, local_channel, &payload);
		channeltable_unlock(table, &rlock);

	    }

	}

    }

    if (payload) free_payload(&payload);

}



/* TODO: call a handler per channel which will close the channel and anything related like sftp */

static void receive_msg_channel_eof(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<5) {

	logoutput("receive_msg_channel_eof: message too small (size: %i)", payload->len);

    } else {
	unsigned int pos=1;
	unsigned int local_channel=0;
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct ssh_channel_s *channel=NULL;
	struct ssh_signal_s *signal=NULL;
	struct simple_lock_s rlock;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;

	logoutput_debug("receive_msg_channel_eof: channel %i", local_channel);

	channeltable_readlock(table, &rlock);
	channel=lookup_session_channel_for_flag(table, local_channel, CHANNEL_FLAG_SERVER_EOF);
	signal=(channel) ? channel->queue.signal : NULL;
	channeltable_unlock(table, &rlock);

	if (signal) {

	    ssh_signal_lock(signal);
	    ssh_signal_broadcast(signal);
	    ssh_signal_unlock(signal);

	}

    }

    if (payload) free_payload(&payload);

}

/*
    message has the following form:

    - byte		SSH_MSG_CHANNEL_CLOSE
    - uint32		recipient channel
*/

static void receive_msg_channel_close(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<5) {

	logoutput("receive_msg_channel_close: message too small (size: %i)", payload->len);

    } else {
	unsigned int pos=1;
	unsigned int local_channel=0;
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct ssh_channel_s *channel=NULL;
	struct ssh_signal_s *signal=NULL;
	struct simple_lock_s rlock;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;

	logoutput_debug("receive_msg_channel_close: channel %i", local_channel);

	channeltable_readlock(table, &rlock);
	channel=lookup_session_channel_for_flag(table, local_channel, CHANNEL_FLAG_SERVER_CLOSE);
	signal=(channel) ? channel->queue.signal : NULL;
	channeltable_unlock(table, &rlock);

	if (signal) {

	    ssh_signal_lock(signal);
	    ssh_signal_broadcast(signal);
	    ssh_signal_unlock(signal);

	}

    }

    if (payload) free_payload(&payload);
}

static void receive_msg_channel_request_s(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len>10) {
	unsigned int local_channel=0;
	unsigned int len=0;
	unsigned int pos=1;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	len=get_uint32(&payload->buffer[pos]);
	pos+=4;

	if (len + pos <= payload->len) {
	    struct ssh_session_s *session=get_ssh_connection_session(connection);
	    struct channel_table_s *table=&session->channel_table;
	    struct ssh_channel_s *channel=NULL;
	    struct simple_lock_s rlock;

	    /* make sure the relevant data is at start of buffer */

	    memmove(payload->buffer, &payload->buffer[pos], len);
	    memset(&payload->buffer[len], 0, payload->len - len);

	    payload->len=len;

	    channeltable_readlock(table, &rlock);
	    channel=lookup_session_channel_for_data(table, local_channel, &payload);
	    channeltable_unlock(table, &rlock);

	} else {
	    int fd=-1;

	    if (connection->connection.io.socket.bevent) fd=get_bevent_unix_fd(connection->connection.io.socket.bevent);

	    logoutput("receive_msg_channel_request_s: ssh fd %i received invalid message local channel %i", fd, local_channel);

	}

    }

    free_payload(&payload);
}

static void receive_msg_channel_request_c(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len>10) {
	unsigned int local_channel=0;
	unsigned int len=0;
	unsigned int pos=1;
	int fd=-1;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	len=get_uint32(&payload->buffer[pos]);
	pos+=4;

	if (connection->connection.io.socket.bevent) fd=get_bevent_unix_fd(connection->connection.io.socket.bevent);

	if (len + pos <= payload->len) {
	    struct ssh_string_s request=SSH_STRING_SET(len, &payload->buffer[pos]);

	    pos+=len;
	    logoutput("receive_msg_channel_request_c: ssh fd %i received request %.*s local channel %i", fd, request.len, request.ptr, local_channel);

	    if (compare_ssh_string(&request, 'c', "exit-status")==0) {
		unsigned int exitstatus=0;

		pos++;
		exitstatus=get_uint32(&payload->buffer[pos]);
		logoutput("receive_msg_channel_request_c: local channel %i exit status %i", local_channel, exitstatus);

	    } else if (compare_ssh_string(&request, 'c', "exit-signal")==0) {
		struct ssh_string_s signal=SSH_STRING_INIT;
		struct ssh_string_s message=SSH_STRING_INIT;

		pos++;
		pos += read_ssh_string(&payload->buffer[pos], payload->len - pos, &signal);
		pos++;
		pos += read_ssh_string(&payload->buffer[pos], payload->len - pos, &message);

		logoutput("receive_msg_channel_request_c: local channel %i exit signal %.*s message %.*s", local_channel, signal.len, signal.ptr, message.len, message.ptr);


	    }

	} else {

	    logoutput("receive_msg_channel_request_c: ssh fd %i received invalid message local channel %i", fd, local_channel);

	}

    }

    free_payload(&payload);
}

/*
    message looks like:
    - byte		SSH_MSG_CHANNEL_SUCCESS or SSH_MSG_CHANNEL_FAILURE
    - uint32		local channel
*/

static void receive_msg_channel_request_reply(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<5) {

	logoutput("receive_msg_channel_request_reply: message too small (size: %i)", payload->len);

    } else {
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct ssh_channel_s *channel=NULL;
	unsigned int local_channel=0;
	unsigned int pos=1;
	struct simple_lock_s rlock;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;

	channeltable_readlock(table, &rlock);
	channel=lookup_session_channel_for_payload(table, local_channel, &payload);
	channeltable_unlock(table, &rlock);

    }

    if (payload) free_payload(&payload);

}

void register_channel_cb(struct ssh_connection_s *c, unsigned char enable)
{

    if (enable) {
	struct ssh_session_s *session=get_ssh_connection_session(c);

	if (session->flags & SSH_SESSION_FLAG_SERVER) {

	    /* server: client sends a open channel message
		for now (20201211) the server does not send these
		TODO (for direct and forwarded channels) after
		sending a global request to the server to enable forwarding channels
		after GLOBAL_REQUEST for forwarding from server to client */

	    register_msg_cb(c, SSH_MSG_CHANNEL_OPEN, receive_msg_channel_open);

	    /* requests like start a shell, exec, subsystem, window-change, signal */

	    register_msg_cb(c, SSH_MSG_CHANNEL_REQUEST, receive_msg_channel_request_s);

	} else {

	    register_msg_cb(c, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, receive_msg_channel_open_confirmation);
	    register_msg_cb(c, SSH_MSG_CHANNEL_OPEN_FAILURE, receive_msg_channel_open_failure);
	    register_msg_cb(c, SSH_MSG_CHANNEL_REQUEST, receive_msg_channel_request_c);

	}

	register_msg_cb(c, SSH_MSG_CHANNEL_WINDOW_ADJUST, receive_msg_channel_window_adjust);
	register_msg_cb(c, SSH_MSG_CHANNEL_DATA, receive_msg_channel_data);
	register_msg_cb(c, SSH_MSG_CHANNEL_EXTENDED_DATA, receive_msg_channel_extended_data);
	register_msg_cb(c, SSH_MSG_CHANNEL_EOF, receive_msg_channel_eof);
	register_msg_cb(c, SSH_MSG_CHANNEL_CLOSE, receive_msg_channel_close);

	register_msg_cb(c, SSH_MSG_CHANNEL_SUCCESS, receive_msg_channel_request_reply);
	register_msg_cb(c, SSH_MSG_CHANNEL_FAILURE, receive_msg_channel_request_reply);

    } else {

	/* disable */
	for (int i=80; i<=100; i++) register_msg_cb(c, i, msg_not_supported);

    }

}
