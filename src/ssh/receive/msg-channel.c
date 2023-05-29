/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Stef Bon <stefbon@gmail.com>

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
	char *buffer=payload->buffer;
	unsigned int lcnr=get_uint32(&buffer[pos]);

	pos+=4;
	int result=lookup_ssh_channel_for_iocb(lcnr, &payload, SSH_CHANNEL_IOCB_RECV_OPEN);

    }

    if (payload) free_payload(&payload);

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
	char *buffer=payload->buffer;
	unsigned int lcnr=get_uint32(&buffer[pos]);

	pos+=4;
	int result=lookup_ssh_channel_for_iocb(lcnr, &payload, SSH_CHANNEL_IOCB_RECV_OPEN);

    }

    if (payload) free_payload(&payload);

}

struct set_window_size_hlpr_s
{
    int					size;
};

static int set_window_size_cb(struct ssh_channel_s *channel, struct ssh_payload_s **pp, void *ptr)
{
    struct set_window_size_hlpr_s *hlpr=(struct set_window_size_hlpr_s *) ptr;
    channel->rwindowsize+=hlpr->size;
    return 0;
}

static void receive_msg_channel_window_adjust(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len<9) {

	logoutput("receive_msg_channel_window_adjust: message too small (size: %i)", payload->len);

    } else {
	unsigned int pos=1;
	char *buffer=payload->buffer;
	unsigned int lcnr=get_uint32(&buffer[pos]);
	struct set_window_size_hlpr_s hlpr;

	pos+=4;
	hlpr.size=get_uint32(&buffer[pos]);

	int result = lookup_ssh_channel_for_cb(lcnr, &payload, set_window_size_cb, &hlpr);

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

    if (payload->len>8) {
	unsigned int pos=1;
	unsigned int len=0;
	char *buffer=payload->buffer;
	unsigned int lcnr=0;

	lcnr=get_uint32(&buffer[pos]);
	pos+=4;

	len=get_uint32(&buffer[pos]);
	pos+=4;

	if ((len + pos) <= payload->len) {

            memmove(buffer, &buffer[pos], len);
            payload->len = len;
            int result=lookup_ssh_channel_for_iocb(lcnr, &payload, SSH_CHANNEL_IOCB_RECV_DATA);

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
	unsigned int pos=1;
	unsigned int len=0;
	char *buffer=payload->buffer;
	unsigned int lcnr=0;
	unsigned int code=0;

	lcnr=get_uint32(&buffer[pos]);
	pos+=4;
	code=get_uint32(&buffer[pos]);
	pos+=4;
	len=get_uint32(&buffer[pos]);
	pos+=4;

	if ((len + pos) <= payload->len) {

	    memmove(buffer, &buffer[pos], len);
	    payload->len = len;
	    payload->code = code;
	    int result=lookup_ssh_channel_for_iocb(lcnr, &payload, SSH_CHANNEL_IOCB_RECV_XDATA);

        }

    }

    if (payload) free_payload(&payload);

}

struct set_channel_flag_hlpr_s {
    unsigned int				flags;
    unsigned int                                status;
    unsigned int				exit_status;
    unsigned int				exit_signal;
};

static int set_channel_flag_cb(struct ssh_channel_s *channel, struct ssh_payload_s **pp, void *ptr)
{
    struct set_channel_flag_hlpr_s *hlpr=(struct set_channel_flag_hlpr_s *) ptr;

    if (hlpr->exit_status) channel->exit_status=hlpr->exit_status;
    if (hlpr->exit_signal) channel->exit_signal=hlpr->exit_signal;
    if (hlpr->status) signal_set_flag(channel->signal, &channel->flags, hlpr->status);
    if (channel->flags & hlpr->flags) (* channel->iocb[SSH_CHANNEL_IOCB_RECV_OPEN])(channel, pp);
    return 0;

}

static void receive_msg_channel_open_status(struct ssh_connection_s *connection, struct ssh_payload_s *payload, unsigned int flag, unsigned int status)
{

    if (payload->len<5) {

	logoutput("receive_msg_channel_open_status: message too small (size: %i)", payload->len);

    } else {
	char *buffer=payload->buffer;
	unsigned int pos=1;
	unsigned int lcnr=0;
	struct set_channel_flag_hlpr_s hlpr;

        hlpr.flags=flag;
        hlpr.status=status;
        hlpr.exit_status=0;
        hlpr.exit_signal=0;

	lcnr=get_uint32(&buffer[pos]);
	pos+=4;
        int result=lookup_ssh_channel_for_cb(lcnr, &payload, set_channel_flag_cb, (void *)&hlpr);

    }

    if (payload) free_payload(&payload);

}

/*
    messages has the following form:
    - byte		SSH_MSG_CHANNEL_CLOSE/EOF
    - uint32		recipient channel
*/

static void receive_msg_channel_eof(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    receive_msg_channel_open_status(connection, payload, SSH_CHANNEL_FLAG_QUEUE_EOF, SSH_CHANNEL_FLAG_SERVER_EOF);
}

static void receive_msg_channel_close(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    receive_msg_channel_open_status(connection, payload, SSH_CHANNEL_FLAG_QUEUE_CLOSE, SSH_CHANNEL_FLAG_SERVER_CLOSE);
}

static void receive_msg_channel_request_s(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len>10) {
        char *buffer=payload->buffer;
        struct ssh_string_s request=SSH_STRING_INIT;
	unsigned int pos=1;
	unsigned int lcnr=0;

        lcnr=get_uint32(&buffer[pos]);
	pos+=4;

	if (read_ssh_string(&buffer[pos], (payload->len - pos), &request)>0) {

            payload->len -= pos;
	    memmove(buffer, &buffer[pos], payload->len);
            int result=lookup_ssh_channel_for_iocb(lcnr, &payload, SSH_CHANNEL_IOCB_RECV_REQUEST);

        }

    }

    if (payload) free_payload(&payload);
}

/* Receive exit status and/or exit signal from remote shell, command and subsystem
    see: https://www.rfc-editor.org/rfc/rfc4254.html#section-6.10 Returning Exit Status

    There are more requests, like:
    - "signal"                          (rfc4254 6.9 Signals)
    - "xon-xoff"                        (rfc4254 6.8 Local Flow Control)
    - "window-change"                   (rfc4254 6.7 Window Dimension Change Message)
    - "shell"/"exec"/"subsystem"        (rfc4254 6.5 Starting a Shell or a Command)
    - "env"                             (rfc4254 6.4 Environment Variable Passing)
    - "x11-req"                         (rfc4254 6.3.1 Requesting X11 Forwarding)
    - "pty-req"                         (rfc4254 6.2 Requesting a Pseudo-Terminal)

*/
static void receive_msg_channel_request_c(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len>10) {
        struct ssh_string_s request=SSH_STRING_INIT;
        char *buffer=payload->buffer;
	unsigned int pos=1;
	unsigned int lcnr=0;

        lcnr=get_uint32(&buffer[pos]);
	pos+=4;

	if (read_ssh_string(&buffer[pos], (payload->len - pos), &request)>0) {
	    unsigned char doqueue=1;
	    unsigned int tmp=pos;
	    struct set_channel_flag_hlpr_s hlpr;

            hlpr.flags=0;
            hlpr.status=0;
            hlpr.exit_status=0;
            hlpr.exit_signal=0;

	    tmp+=get_ssh_string_length(&request, (SSH_STRING_FLAG_HEADER | SSH_STRING_FLAG_DATA));
	    tmp++; /* ignore byte about reply here ... */

	    if ((compare_ssh_string(&request, 'c', "exit-status")==0) || (compare_ssh_string(&request, 'c', "exit-signal")==0)) {

                if (compare_ssh_string(&request, 'c', "exit-status")==0) {

                    if ((tmp + 4) <= payload->len) {

                        hlpr.exit_status=get_uint32(&buffer[tmp]);
		        logoutput_debug("receive_msg_channel_request_c: local channel %u exit status %u", lcnr, hlpr.exit_status);

                    } else {

                        logoutput_warning("receive_msg_channel_request_c: local channel %u message exit-status not long enough", lcnr);

                    }

                    hlpr.flags=SSH_CHANNEL_FLAG_QUEUE_EXIT_STATUS;

	        } else if (compare_ssh_string(&request, 'c', "exit-signal")==0) {

                    if ((tmp + 13) <= payload->len) {
		        struct ssh_string_s name=SSH_STRING_INIT;
		        struct ssh_string_s message=SSH_STRING_INIT;

		        tmp += read_ssh_string(&buffer[pos], (payload->len - tmp), &name);
		        tmp++; /* skip the boolean (core dumped) */
		        tmp += read_ssh_string(&buffer[pos], (payload->len - tmp), &message);

		        logoutput("receive_msg_channel_request_c: local channel %u exit-signal %.*s message %.*s", lcnr, name.len, name.ptr, message.len, message.ptr);
		        hlpr.exit_signal=get_ssh_channel_exit_signal(&name);

                    } else {

                        logoutput_warning("receive_msg_channel_request_c: local channel %u message exit-signal not long enough", lcnr);

                    }

                    hlpr.flags=SSH_CHANNEL_FLAG_QUEUE_EXIT_SIGNAL;

                }

                payload->len -= pos;
	        memmove(buffer, &buffer[pos], payload->len);
	        int result=lookup_ssh_channel_for_cb(lcnr, &payload, set_channel_flag_cb, (void *)&hlpr);

	    } else {

	        logoutput("receive_msg_channel_request_c: received invalid message local channel %i", lcnr);

	    }

        }

    }

    if (payload) free_payload(&payload);
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
        char *buffer=payload->buffer;
        unsigned int lcnr=0;
	unsigned int pos=1;

        lcnr=get_uint32(&buffer[pos]);
	pos+=4;
        payload->len -= pos;
	memmove(buffer, &buffer[pos], payload->len);
	int result=lookup_ssh_channel_for_iocb(lcnr, &payload, SSH_CHANNEL_IOCB_RECV_REQUEST);

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
