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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"

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
}

static void forward_payload2channel(struct ssh_connection_s *connection, unsigned int local_channel, struct ssh_payload_s **p_payload)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct channel_table_s *table=&session->channel_table;
    struct ssh_channel_s *channel=NULL;

    channel=lookup_session_channel_for_payload(table, local_channel, p_payload);

}

static void forward_data2channel(struct ssh_connection_s *connection, unsigned int local_channel, struct ssh_payload_s **p_payload)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct channel_table_s *table=&session->channel_table;
    struct ssh_channel_s *channel=NULL;

    channel=lookup_session_channel_for_data(table, local_channel, p_payload);

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

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;

	logoutput("receive_msg_open_confirmation: local channel %i", local_channel);
	forward_payload2channel(connection, local_channel, &payload);

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

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;

	logoutput("receive_msg_open_failure: local channel %i", local_channel);
	forward_payload2channel(connection, local_channel, &payload);

    }

    if (payload) free_payload(&payload);

}

struct set_window_size_hlpr_s
{
    int					size;
};

void set_window_size_cb(struct ssh_channel_s *channel, void *ptr)
{
    struct set_window_size_hlpr_s *hlpr=(struct set_window_size_hlpr_s *) ptr;

    channel->remote_window-=hlpr->size;
}

static void receive_msg_channel_window_adjust(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<9) {

	logoutput("receive_msg_channel_window_adjust: message too small (size: %i)", payload->len);

    } else {
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	unsigned int pos=1;
	unsigned int local_channel=0;
	struct set_window_size_hlpr_s hlpr;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	hlpr.size=get_uint32(&payload->buffer[pos]);

	logoutput("receive_msg_channel_window_adjust: channel %i size %i", local_channel, hlpr.size);

	lookup_session_channel_for_cb(table, local_channel, set_window_size_cb, &hlpr);

    }

    free_payload(&payload);

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

	    /* make sure the relevant data is at start of buffer */

	    memmove(payload->buffer, &payload->buffer[pos], len);
	    memset(&payload->buffer[len], 0, payload->len - len);
	    payload->len=len;
	    forward_data2channel(connection, local_channel, &payload);

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

	    /* TODO: more extended data streams? */

	    if (code==SSH_EXTENDED_DATA_STDERR) {

		/* make sure the relevant data is at start of buffer */

		memmove(payload->buffer, &payload->buffer[pos], len);
		memset(&payload->buffer[len], 0, payload->len - len);
		payload->len=len;
		payload->flags |= SSH_PAYLOAD_FLAG_ERROR;
		forward_data2channel(connection, local_channel, &payload);

	    }

	}

    }

    if (payload) free_payload(&payload);

}

struct set_channel_flag_hlpr_s {
    unsigned int				flags;
    unsigned int				exit_status;
    unsigned int				exit_signal;
};

static void set_channel_flag_cb(struct ssh_channel_s *channel, void *ptr)
{
    struct ssh_signal_s *signal=channel->queue.signal;
    struct set_channel_flag_hlpr_s *hlpr=(struct set_channel_flag_hlpr_s *) ptr;

    signal_set_flag(channel->signal, &channel->flags, hlpr->flags);
}

static void receive_msg_channel_eof(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<5) {

	logoutput("receive_msg_channel_eof: message too small (size: %i)", payload->len);

    } else {
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct ssh_channel_s *channel=NULL;
	struct set_channel_flag_hlpr_s hlpr;
	unsigned int pos=1;
	unsigned int local_channel=0;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	logoutput_debug("receive_msg_channel_eof: channel %i", local_channel);

	hlpr.flags = CHANNEL_FLAG_SERVER_EOF;
	lookup_session_channel_for_cb(table, local_channel, set_channel_flag_cb, (void *) &hlpr);

    }

    free_payload(&payload);

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
	struct ssh_session_s *session=get_ssh_connection_session(connection);
	struct channel_table_s *table=&session->channel_table;
	struct set_channel_flag_hlpr_s hlpr;
	unsigned int pos=1;
	unsigned int local_channel=0;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	logoutput_debug("receive_msg_channel_close: channel %i", local_channel);

	hlpr.flags = CHANNEL_FLAG_SERVER_CLOSE;
	lookup_session_channel_for_cb(table, local_channel, set_channel_flag_cb, (void *) &hlpr);

    }

    free_payload(&payload);
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

	    /* make sure the relevant data is at start of buffer */

	    memmove(payload->buffer, &payload->buffer[pos], len);
	    memset(&payload->buffer[len], 0, payload->len - len);
	    payload->len=len;

	    forward_data2channel(connection, local_channel, &payload);

	} else {

	    logoutput("receive_msg_channel_request_s: received invalid message local channel %i", local_channel);

	}

    }

    free_payload(&payload);
}

/* Receive exit status and/or exit signal from remote shell, command and subsystem
    see: https://www.rfc-editor.org/rfc/rfc4254.html#section-6.10 Returning Exit Status
*/

static void set_channel_exit_status_cb(struct ssh_channel_s *channel, void *ptr)
{
    struct ssh_signal_s *signal=channel->queue.signal;
    struct set_channel_flag_hlpr_s *hlpr=(struct set_channel_flag_hlpr_s *) ptr;

    channel->exit_status=hlpr->exit_status;
    signal_set_flag(channel->signal, &channel->flags, hlpr->flags);
}

static void set_channel_exit_signal_cb(struct ssh_channel_s *channel, void *ptr)
{
    struct ssh_signal_s *signal=channel->queue.signal;
    struct set_channel_flag_hlpr_s *hlpr=(struct set_channel_flag_hlpr_s *) ptr;

    channel->exit_signal=hlpr->exit_signal;
    signal_set_flag(channel->signal, &channel->flags, hlpr->flags);
}

static void receive_msg_channel_request_c(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
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
	    struct ssh_string_s request=SSH_STRING_SET(len, &payload->buffer[pos]);

	    pos+=len;

	    if (compare_ssh_string(&request, 'c', "exit-status")==0) {
		struct set_channel_flag_hlpr_s hlpr;

		pos++; /* skip the boolean */
		hlpr.exit_status=get_uint32(&payload->buffer[pos]);
		hlpr.flags=CHANNEL_FLAG_EXIT_STATUS;
		logoutput("receive_msg_channel_request_c: local channel %i exit status %i", local_channel, hlpr.exit_status);
		lookup_session_channel_for_cb(table, local_channel, set_channel_exit_status_cb, (void *) &hlpr);

	    } else if (compare_ssh_string(&request, 'c', "exit-signal")==0) {
		struct ssh_string_s name=SSH_STRING_INIT;
		struct ssh_string_s message=SSH_STRING_INIT;
		struct set_channel_flag_hlpr_s hlpr;

		pos++; /* skip the boolean */
		pos += read_ssh_string(&payload->buffer[pos], payload->len - pos, &name);
		pos++; /* skip the boolean (core dumped) */
		pos += read_ssh_string(&payload->buffer[pos], payload->len - pos, &message);

		logoutput("receive_msg_channel_request_c: local channel %i exit signal %.*s", local_channel, name.len, name.ptr);

		hlpr.exit_signal=get_ssh_channel_exit_signal(&name);
		hlpr.flags=CHANNEL_FLAG_EXIT_SIGNAL;
		lookup_session_channel_for_cb(table, local_channel, set_channel_exit_signal_cb, (void *) &hlpr);

	    } else {

		logoutput("receive_msg_channel_request_c: request %.*s local channel %i not supported", request.len, request.ptr, local_channel);

	    }

	} else {

	    logoutput("receive_msg_channel_request_c: received invalid message local channel %i", local_channel);

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
	unsigned int local_channel=0;
	unsigned int pos=1;

	local_channel=get_uint32(&payload->buffer[pos]);
	pos+=4;
	forward_payload2channel(connection, local_channel, &payload);

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
