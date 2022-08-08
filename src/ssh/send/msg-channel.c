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

static char *get_name_ssh_channel(unsigned char type)
{
    char *name="";

    if (type==_CHANNEL_TYPE_SESSION) {

	name="session";

    } else if (type==_CHANNEL_TYPE_DIRECT_STREAMLOCAL) {

	name="direct-streamlocal@openssh.com";

    } else if (type==_CHANNEL_TYPE_DIRECT_TCPIP) {

	name="direct-tcpip";

    }

    return name;

}

static char *get_subsys_name_session_channel(unsigned char type)
{
    char *name="";

    if (type==_CHANNEL_SESSION_TYPE_EXEC) {

	name="exec";

    } else if (type==_CHANNEL_SESSION_TYPE_SHELL) {

	name="shell";

    } else if (type==_CHANNEL_SESSION_TYPE_SUBSYSTEM) {

	name="subsystem";

    }

    return name;
}

static int _send_channel_open_message(struct msg_buffer_s *mb, struct ssh_channel_s *channel)
{
    struct ssh_session_s *session=channel->session;

    logoutput_debug("_send_channel_open_message");

    msg_write_byte(mb, SSH_MSG_CHANNEL_OPEN);
    msg_write_ssh_string(mb, 'c', (void *) get_name_ssh_channel(channel->type));
    msg_store_uint32(mb, channel->local_channel);
    msg_store_uint32(mb, channel->local_window);
    msg_store_uint32(mb, get_max_packet_size(session));

    if (channel->type==_CHANNEL_TYPE_DIRECT_STREAMLOCAL) {

	msg_write_ssh_string(mb, 'c', (void *) channel->target.direct_streamlocal.path);

	/* 20170528: string and uint32 for future use, now empty */

	msg_store_uint32(mb, 0);
	msg_store_uint32(mb, 0);

    } else if (channel->type==_CHANNEL_TYPE_DIRECT_TCPIP) {
	char *target=NULL;
	struct ip_address_s *orig_ip=&channel->target.direct_tcpip.orig_ip;
	char *ip=NULL;

	translate_context_host_address(&channel->target.direct_tcpip.address, &target, NULL);
	ip=((orig_ip->family==IP_ADDRESS_FAMILY_IPv6) ? orig_ip->addr.v6 : orig_ip->addr.v4);

	msg_write_ssh_string(mb, 'c', (void *) target);
	msg_store_uint32(mb, channel->target.direct_tcpip.portnr);
	msg_write_ssh_string(mb, 'c', (void *) ip);
	msg_store_uint32(mb, channel->target.direct_tcpip.orig_portnr);

    }

    return mb->pos;

}

int send_channel_open_message(struct ssh_channel_s *channel, uint32_t *seq)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=_send_channel_open_message(&mb, channel);
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;

    logoutput_debug("send_channel_open_message");

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_CHANNEL_OPEN;
    set_msg_buffer_payload(&mb, payload);
    payload->len=_send_channel_open_message(&mb, channel);

    return write_ssh_packet(channel->connection, payload, seq);

}

/* close a channel */

int send_channel_close_message(struct ssh_channel_s *channel)
{
    char buffer[sizeof(struct ssh_payload_s) + 5];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    uint32_t seq=0;
    char *pos=payload->buffer;

    logoutput_debug("send_channel_close_message");

    init_ssh_payload(payload, 5);
    payload->type=SSH_MSG_CHANNEL_CLOSE;

    *pos=(unsigned char) SSH_MSG_CHANNEL_CLOSE;
    pos++;
    store_uint32(pos, channel->remote_channel);
    pos+=4;
    return write_ssh_packet(channel->connection, payload, &seq);

}

/* window adjust */

int send_channel_window_adjust_message(struct ssh_channel_s *channel, unsigned int increase)
{
    char buffer[sizeof(struct ssh_payload_s) + 9];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    uint32_t seq=0;
    char *pos=payload->buffer;

    logoutput_debug("send_channel_window_adjust_message");

    init_ssh_payload(payload, 9);
    payload->type=SSH_MSG_CHANNEL_WINDOW_ADJUST;

    *pos=(unsigned char) SSH_MSG_CHANNEL_WINDOW_ADJUST;
    pos++;
    store_uint32(pos, channel->remote_channel);
    pos+=4;
    store_uint32(pos, increase);
    pos+=4;
    return write_ssh_packet(channel->connection, payload, &seq);

}

static unsigned int get_session_channel_data_len(struct ssh_channel_s *channel)
{
    unsigned int len=5;

    if (channel->type==_CHANNEL_TYPE_SESSION) {

	len += 4 + strlen(get_subsys_name_session_channel(channel->target.session.type)) +
	    4 + ((channel->target.session.type==_CHANNEL_SESSION_TYPE_EXEC) || (channel->target.session.type==_CHANNEL_SESSION_TYPE_SUBSYSTEM) ? strlen(channel->target.session.buffer) : 0);

    }

    len++;
    return len;

}

/*
    want a subsystem or exec a command (RFC4254 6.5. Starting a Shell or a Command)

    - byte 	SSH_MSG_CHANNEL_REQUEST
    - uint32    remote channel
    - string	"subsystem"/"exec"/"shell"
    - boolean	want reply
    - string 	subsystem name/command
*/

int send_channel_start_command_message(struct ssh_channel_s *channel, unsigned char reply, uint32_t *seq)
{
    unsigned int len=get_session_channel_data_len(channel);
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_CHANNEL_REQUEST;

    *pos=(unsigned char) SSH_MSG_CHANNEL_REQUEST;
    pos++;
    store_uint32(pos, channel->remote_channel);
    pos+=4;
    pos+=write_ssh_string(pos, 0, 'c', (void *) get_subsys_name_session_channel(channel->target.session.type));
    *pos=(reply>0) ? 1 : 0;
    pos++;

    /* with exec provide also the command, and with subsystem the name */

    if ((channel->target.session.type==_CHANNEL_SESSION_TYPE_EXEC) || (channel->target.session.type==_CHANNEL_SESSION_TYPE_SUBSYSTEM)) {

	pos+=write_ssh_string(pos, 0, 'c', (void *) channel->target.session.buffer);

    }

    payload->len=(unsigned int)(pos - payload->buffer);
    return write_ssh_packet(channel->connection, payload, seq);

}

/*
    send a channel data (RFC4254 5.2. Data Transfer)

    - byte 	SSH_MSG_CHANNEL_DATA
    - uint32    remote channel
    - uint32	len
    - byte[len]
*/

int send_channel_data_message_connected(struct ssh_channel_s *channel, char *data, unsigned int size, uint32_t *seq)
{
    unsigned int len=9 + size;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_CHANNEL_DATA;

    *pos=(unsigned char) SSH_MSG_CHANNEL_DATA;
    pos++;

    store_uint32(pos, channel->remote_channel);
    pos+=4;
    store_uint32(pos, size);
    pos+=4;
    memcpy(pos, data, size);
    pos+=size;
    payload->len=(unsigned int)(pos - payload->buffer);

    return write_ssh_packet(channel->connection, payload, seq);

}

int send_channel_data_message_error(struct ssh_channel_s *channel, char *data, unsigned int len, uint32_t *seq)
{
    return -1;
}

int send_channel_data_message(struct ssh_channel_s *channel, char *data, unsigned int len, unsigned int *seq)
{
    (* channel->process_outgoing_bytes)(channel, len);
    return (* channel->send_data_msg)(channel, data, len, seq);
}

int send_channel_request_message(struct ssh_channel_s *channel, const char *request, unsigned char reply, struct ssh_string_s *data, uint32_t *seq)
{
    unsigned int len=1 + 4 + 4 + strlen(request) + 1 + ((data) ? data->len : 0);
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;
    unsigned int tmp=strlen(request);

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_CHANNEL_DATA;

    *pos=(unsigned char) SSH_MSG_CHANNEL_REQUEST;
    pos++;
    store_uint32(pos, channel->remote_channel);
    pos+=4;
    store_uint32(pos, tmp);
    pos+=4;
    memcpy(pos, request, tmp);
    pos+=tmp;
    *pos=(reply) ? 1 : 0;
    pos++;
    if (data && data->len>0) {

	memcpy(pos, data->ptr, data->len);
	pos+=data->len;

    }

    payload->len=(unsigned int)(pos - payload->buffer);
    return write_ssh_packet(channel->connection, payload, seq);
}

int send_channel_open_confirmation(struct ssh_channel_s *channel, struct ssh_string_s *data, uint32_t *seq)
{
    unsigned int len=1 + 4 + 4 + 4 + 4 + ((data) ? data->len : 0);
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_CHANNEL_OPEN_CONFIRMATION;

    *pos=(unsigned char) SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
    pos++;
    store_uint32(pos, channel->remote_channel);
    pos+=4;
    store_uint32(pos, channel->local_channel);
    pos+=4;
    store_uint32(pos, channel->local_window);
    pos+=4;
    store_uint32(pos, channel->max_packet_size);
    pos+=4;

    if (data && data->len>0) {

	memcpy(pos, data->ptr, data->len);
	pos+=data->len;

    }

    payload->len=(unsigned int)(pos - payload->buffer);
    return write_ssh_packet(channel->connection, payload, seq);
}

int send_channel_open_failure(struct ssh_connection_s *c, unsigned int remote_channel, uint32_t code, uint32_t *seq)
{
    unsigned int len=1 + 4 + 4 + 4 + 4;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    char *pos=payload->buffer;

    init_ssh_payload(payload, len);
    payload->type=SSH_MSG_CHANNEL_OPEN_FAILURE;

    *pos=(unsigned char) SSH_MSG_CHANNEL_OPEN_FAILURE;
    pos++;
    store_uint32(pos, remote_channel);
    pos+=4;
    store_uint32(pos, code);
    pos+=4;
    store_uint32(pos, 0);
    pos+=4;
    store_uint32(pos, 0);
    pos+=4;

    payload->len=(unsigned int)(pos - payload->buffer);
    return write_ssh_packet(c, payload, seq);
}
