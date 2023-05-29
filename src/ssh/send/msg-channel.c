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

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-send.h"
#include "ssh-utils.h"

/*
    opening a channel for a session and simular like direct-streamlocal
    message has the form:
    - byte			SSH_MSG_CHANNEL_OPEN
    - string			channel type
    - uint32			local channel
    - uint32			window size
    - uint32			max packet size
    - ....			additional data depends on the type of open channel

    like the default session:

    - byte			SSH_MSG_CHANNEL_OPEN
    - string			"session"
    - uint32			local channel
    - uint32			window size
    - uint32			max packet size

    and direct-tcpip

    - byte      		SSH_MSG_CHANNEL_OPEN
    - string			"direct-tcpip"
    - uint32			sender channel
    - uint32			initial window size
    - uint32			maximum packet size
    - string			host to connect
    - uint32			port to connect
    - string			originator IP address
    - uint32			originator port

    and direct-streamlocal

    - byte			SSH_MSG_CHANNEL_OPEN
    - string			"direct-streamlocal@openssh.com"
    - uint32			sender channel
    - uint32			initial window size
    - uint32			maximum packet size
    - string			socket path
    - string			reserved
    - uint32			reserved

*/

static char *get_name_ssh_channel(unsigned char type, unsigned char subtype)
{
    char *name="";

    if (type==SSH_CHANNEL_TYPE_SESSION) {

	name="session";

    } else if (type==SSH_CHANNEL_TYPE_DIRECT) {

	if (subtype==SSH_CHANNEL_SOCKET_TYPE_TCPIP) {

	    name="direct-tcpip";

	} else if (subtype==SSH_CHANNEL_SOCKET_TYPE_STREAMLOCAL) {

	    name="direct-streamlocal@openssh.com";

	}

    }

    return name;

}

static char *get_subsys_name_session_channel(unsigned char type)
{
    char *name="";

    if (type==SSH_CHANNEL_SESSION_TYPE_EXEC) {

	name="exec";

    } else if (type==SSH_CHANNEL_SESSION_TYPE_SHELL) {

	name="shell";

    } else if (type==SSH_CHANNEL_SESSION_TYPE_SUBSYSTEM) {

	name="subsystem";

    }

    return name;
}

static int _send_ssh_channel_open_message(struct msg_buffer_s *mb, unsigned char type, unsigned int lcnr, struct ssh_channel_open_msg_s *openmsg)
{
    struct ssh_channel_open_data_s *data=openmsg->data;

    msg_write_byte(mb, SSH_MSG_CHANNEL_OPEN);
    msg_write_ssh_string(mb, 'c', (void *) get_name_ssh_channel(type, ((data) ? data->type : 0)));
    msg_store_uint32(mb, lcnr);
    msg_store_uint32(mb, openmsg->windowsize);
    msg_store_uint32(mb, openmsg->maxpacketsize);

    if ((type==SSH_CHANNEL_TYPE_DIRECT) || (type==SSH_CHANNEL_TYPE_FORWARDED)) {

	if (data->type==SSH_CHANNEL_SOCKET_TYPE_STREAMLOCAL) {

	    msg_write_ssh_string(mb, 's', (void *) &data->data.local.path);

	    /* 20170528: string and uint32 for future use, now empty */

	    msg_store_uint32(mb, 0);
	    msg_store_uint32(mb, 0);

	} else if (data->type==SSH_CHANNEL_SOCKET_TYPE_TCPIP) {

	    msg_write_ssh_string(mb, 's', (void *) &data->data.tcpip.address);
	    msg_store_uint32(mb, data->data.tcpip.portnr);
	    msg_write_ssh_string(mb, 's', (void *) &data->data.tcpip.orig_ip);
	    msg_store_uint32(mb, data->data.tcpip.orig_portnr);

	}

    }

    return mb->pos;

}

int send_ssh_channel_open_msg(struct ssh_connection_s *c, unsigned char type, union ssh_message_u *msg)
{
    struct ssh_channel_open_msg_s *openmsg=&msg->channel.type.open;
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int size=_send_ssh_channel_open_message(&mb, type, msg->channel.lcnr, openmsg);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, size);
    payload->type=SSH_MSG_CHANNEL_OPEN;
    set_msg_buffer_payload(&mb, payload);
    payload->len=_send_ssh_channel_open_message(&mb, type, msg->channel.lcnr, openmsg);
    return write_ssh_packet(c, payload);

}

/*  send a channel open confirmation (RFC4254 5.1. Opening a Channel)

    - byte 	SSH_MSG_CHANNEL_OPEN_CONFIRMATION
    - uint32    remote channel
    - uint32	sender channel
    - uint32	initial window size
    - uint32    max packet size
    - channel specific data

    ------ +
    1 + 4 + 4 + 4 + len = 13 + len
*/

unsigned int get_length_ssh_channel_open_confirmation_msg(struct ssh_channel_open_msg_s *msg)
{
    return (13);
}

unsigned int write_ssh_channel_open_comfirmation_msg(unsigned int lcnr, struct ssh_channel_open_msg_s *msg, char *buffer)
{
    char *pos=buffer;

    *pos=(unsigned char) SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
    pos++;
    store_uint32(pos, msg->rcnr);
    pos+=4;
    store_uint32(pos, lcnr);
    pos+=4;
    store_uint32(pos, msg->windowsize);
    pos+=4;
    store_uint32(pos, msg->maxpacketsize);
    pos+=4;

    /*    if ((msg->data) && (msg->len)) {

	memcpy(pos, msg->data, msg->len);
	pos+=msg->len;

    }*/

    return (unsigned int)(pos - buffer);
}

int send_ssh_channel_open_confirmation(struct ssh_channel_s *channel, union ssh_message_u *msg)
{
    struct ssh_channel_open_msg_s *confirm=&msg->channel.type.open;
    unsigned int size=get_length_ssh_channel_open_confirmation_msg(confirm);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, size);
    payload->type=SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
    payload->len=write_ssh_channel_open_comfirmation_msg(channel->lcnr, confirm, payload->buffer);

    return write_ssh_packet(channel->connection, payload);
}

/*  send a channel open failure (RFC4254 5.1. Opening a Channel)

    - byte 	SSH_MSG_CHANNEL_OPEN_FAILURE
    - uint32    remote channel
    - uint32	reason cose
    - string    description
    - string    language tag

    ------ +
    1 + 4 + 4 + strlen(description) + 4 + strlen(language)
*/

unsigned int get_length_ssh_channel_open_failure_msg(struct ssh_channel_open_failure_msg_s *msg)
{
    return 13 + msg->description.len + msg->language.len;
}

unsigned int write_ssh_channel_open_failure_msg(unsigned int rcnr, struct ssh_channel_open_failure_msg_s *msg, char *buffer)
{
    char *pos=buffer;

    *pos=(unsigned char) SSH_MSG_CHANNEL_OPEN_FAILURE;
    pos++;
    store_uint32(pos, rcnr);
    pos+=4;
    store_uint32(pos, msg->reason);
    pos+=4;
    pos+=write_ssh_string(pos, 0, 's', (void *) &msg->description);
    pos+=write_ssh_string(pos, 0, 's', (void *) &msg->language);

    return (unsigned int)(pos - buffer);
}

int send_ssh_channel_open_failure_msg(struct ssh_connection_s *c, unsigned int rcnr, union ssh_message_u *msg)
{
    struct ssh_channel_open_failure_msg_s *failure=&msg->channel.type.open_failure;
    unsigned int size=get_length_ssh_channel_open_failure_msg(failure);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, size);
    payload->type=SSH_MSG_CHANNEL_OPEN_FAILURE;
    payload->len=write_ssh_channel_open_failure_msg(rcnr, failure, payload->buffer);

    return write_ssh_packet(c, payload);
}

/* close a channel */

unsigned int get_length_ssh_channel_close_msg(struct ssh_channel_close_msg_s *close)
{
    return 5;
}

unsigned int write_ssh_channel_close_msg(unsigned int rcnr, struct ssh_channel_close_msg_s *close, char *buffer)
{
    char *pos=buffer;

    *pos=(unsigned char) close->type;
    pos++;
    store_uint32(pos, rcnr);
    pos+=4;

    return (unsigned int)(pos - buffer);
}

int send_ssh_channel_eofclose_msg(struct ssh_channel_s *channel, union ssh_message_u *msg)
{
    struct ssh_channel_close_msg_s *close=&msg->channel.type.close;
    unsigned int size=get_length_ssh_channel_close_msg(close);
    char buffer[sizeof(struct ssh_payload_s) + 5];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, 5);
    payload->type=close->type;
    payload->len=write_ssh_channel_close_msg(channel->rcnr, close, payload->buffer);

    return write_ssh_packet(channel->connection, payload);
}

/* window adjust */

unsigned int get_length_ssh_channel_windowadjust_msg(struct ssh_channel_windowadjust_msg_s *adjust)
{
    return 9;
}

unsigned int write_ssh_channel_windowadjust_msg(unsigned int rcnr, struct ssh_channel_windowadjust_msg_s *adjust, char *buffer)
{
    char *pos=buffer;

    *pos=SSH_MSG_CHANNEL_WINDOW_ADJUST;
    pos++;
    store_uint32(pos, rcnr);
    pos+=4;
    store_uint32(pos, adjust->increase);
    pos+=4;

    return (unsigned int)(pos - buffer);
}

int send_ssh_channel_window_adjust_msg(struct ssh_channel_s *channel, union ssh_message_u *msg)
{
    struct ssh_channel_windowadjust_msg_s *adjust=&msg->channel.type.windowadjust;
    unsigned int size=get_length_ssh_channel_windowadjust_msg(adjust);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, size);
    payload->type=SSH_MSG_CHANNEL_WINDOW_ADJUST;
    payload->len=write_ssh_channel_windowadjust_msg(channel->rcnr, adjust, payload->buffer);
    return write_ssh_packet(channel->connection, payload);
}

/*
    want a channel specific request 

    - byte 		SSH_MSG_CHANNEL_REQUEST
    - uint32    	remote channel
    - string		name
    - boolean		want reply
    - bytes[size] 	type-specific data

    ------- +
    1 + 4 + strlen(name) + 4 + 1 + size = 10 + strlen(name) + size

    see: https://datatracker.ietf.org/doc/html/rfc4254#section-5.4
    RFC4254 5.4.  Channel-Specific Requests
*/

unsigned int get_length_ssh_channel_request_msg(struct ssh_channel_request_msg_s *msg)
{
    return (10 + msg->type.len + msg->len);
}

unsigned int write_ssh_channel_request_msg(unsigned int rcnr, struct ssh_channel_request_msg_s *msg, char *buffer)
{
    char *pos=buffer;

    *pos=(unsigned char) SSH_MSG_CHANNEL_REQUEST;
    pos++;
    store_uint32(pos, rcnr);
    pos+=4;
    pos+=write_ssh_string(pos, 0, 's', (void *) &msg->type);
    *pos=msg->reply;
    pos++;

    if (msg->len) {

        memcpy(pos, msg->data, msg->len);
        pos+=msg->len;

    }

    return (unsigned int)(pos - buffer);
}

int send_ssh_channel_request_msg(struct ssh_channel_s *channel, union ssh_message_u *msg)
{
    struct ssh_channel_request_msg_s *request=&msg->channel.type.request;
    unsigned int size=get_length_ssh_channel_request_msg(request);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, size);
    payload->type=SSH_MSG_CHANNEL_REQUEST;
    payload->len=write_ssh_channel_request_msg(channel->rcnr, request, payload->buffer);

    return write_ssh_packet(channel->connection, payload);
}

/*
    exec a command or start a subsystem/shell
    see: https://datatracker.ietf.org/doc/html/rfc4254#section-6.5
    RFC4254 6.5. Starting a Shell or a Command
*/

int send_ssh_channel_start_command_msg(struct ssh_channel_s *channel, unsigned char reply)
{

    if (channel->type==SSH_CHANNEL_TYPE_SESSION) {
        unsigned char type=channel->target.session.type;
	char *name=get_subsys_name_session_channel(type);

	if (name) {
	    char *command=channel->target.session.buffer;
	    unsigned int size=strlen(command);
	    char data[size + 4];
	    unsigned int tmp=write_ssh_string(data, size+4, 'c', (void *) command); /* is command or subsystem name */
	    union ssh_message_u msg;

            memset(&msg, 0, sizeof(union ssh_message_u));
            set_ssh_string(&msg.channel.type.request.type, 'c', name);
            msg.channel.type.request.reply=reply;

            if ((type==SSH_CHANNEL_SESSION_TYPE_EXEC) || (type==SSH_CHANNEL_SESSION_TYPE_SUBSYSTEM)) {

                msg.channel.type.request.len=(size+4);
                msg.channel.type.request.data=data;

            }

	    return send_ssh_channel_request_msg(channel, &msg);

	}

    }

    return -1;

}

/*  send a channel data (RFC4254 5.2. Data Transfer)

    - byte 	SSH_MSG_CHANNEL_DATA
    - uint32    remote channel
    - uint32	len
    - byte[len]

    --------- +
    1 + 4 + 4 + len = 9 + len
*/

unsigned int get_length_ssh_channel_data_msg(struct ssh_channel_data_msg_s *msg)
{
    return (9 + msg->len);
}

unsigned int write_ssh_channel_data_msg(unsigned int rcnr, struct ssh_channel_data_msg_s *msg, char *buffer)
{
    char *pos=buffer;

    *pos=(unsigned char) SSH_MSG_CHANNEL_DATA;
    pos++;
    store_uint32(pos, rcnr);
    pos+=4;
    store_uint32(pos, msg->len);
    pos+=4;
    memcpy(pos, msg->data, msg->len);
    pos+=msg->len;
    return (unsigned int)(pos - buffer);
}

int send_ssh_channel_data_msg(struct ssh_channel_s *channel, union ssh_message_u *msg)
{
    struct ssh_channel_data_msg_s *data=&msg->channel.type.data;
    unsigned int size=get_length_ssh_channel_data_msg(data);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, size);
    payload->type=SSH_MSG_CHANNEL_DATA;
    payload->len=write_ssh_channel_data_msg(channel->rcnr, data, payload->buffer);

    return write_ssh_packet(channel->connection, payload);

}

/*  send a channel extended data (RFC4254 5.2. Data Transfer)

    - byte 	SSH_MSG_CHANNEL_EXTENDED_DATA
    - uint32    remote channel
    - uint32	code
    - uint32	len
    - byte[len]

    ------ +
    1 + 4 + 4 + 4 + len = 13 + len
*/

unsigned int get_length_ssh_channel_xdata_msg(struct ssh_channel_xdata_msg_s *msg)
{
    return (13 + msg->len);
}

unsigned int write_ssh_channel_xdata_msg(unsigned int rcnr, struct ssh_channel_xdata_msg_s *msg, char *buffer)
{
    char *pos=buffer;

    *pos=(unsigned char) SSH_MSG_CHANNEL_EXTENDED_DATA;
    pos++;
    store_uint32(pos, rcnr);
    pos+=4;
    store_uint32(pos, msg->code);
    pos+=4;
    store_uint32(pos, msg->len);
    pos+=4;
    memcpy(pos, msg->data, msg->len);
    pos+=msg->len;
    return (unsigned int)(pos - buffer);
}

int send_ssh_channel_xdata_msg(struct ssh_channel_s *channel, union ssh_message_u *msg)
{
    struct ssh_channel_xdata_msg_s *xdata=&msg->channel.type.xdata;
    unsigned int size=get_length_ssh_channel_xdata_msg(xdata);
    char buffer[sizeof(struct ssh_payload_s) + size];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    init_ssh_payload(payload, size);
    payload->type=SSH_MSG_CHANNEL_EXTENDED_DATA;
    payload->len=write_ssh_channel_xdata_msg(channel->rcnr, xdata, payload->buffer);

    return write_ssh_packet(channel->connection, payload);
}
