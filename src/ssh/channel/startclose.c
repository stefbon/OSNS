/*
  2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-ssh.h"

#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-channel.h"
#include "ssh-utils.h"
#include "ssh-send.h"

struct _cb_start_ssh_channel_hlpr_s {
    struct ssh_channel_s 			*channel;
};

static unsigned char _cb_finish_start_ssh_channel(void *ptr)
{
    struct _cb_start_ssh_channel_hlpr_s *hlpr=(struct _cb_start_ssh_channel_hlpr_s *) ptr;
    struct ssh_channel_s *channel=hlpr->channel;
    return ((channel->flags & (SSH_CHANNEL_FLAG_SERVER_CLOSE | SSH_CHANNEL_FLAG_SERVER_EOF | SSH_CHANNEL_FLAG_OPENFAILURE)) ? 1 : 0);
}

static int read_ssh_channel_open_confirmation_msg(unsigned char type, struct ssh_payload_s *payload, union ssh_message_u *msg)
{
    char *buffer=payload->buffer;
    unsigned int pos=5;

    if (payload->len<13) {

        logoutput_debug("read_ssh_channel_open_confirmation_msg: unable to read received msg ... too short (len=%u)", payload->len);
        return -1;

    }

    msg->channel.type.open.rcnr=get_uint32(&buffer[pos]);
    pos+=4;
    msg->channel.type.open.windowsize=get_uint32(&buffer[pos]);
    pos+=4;
    msg->channel.type.open.maxpacketsize=get_uint32(&buffer[pos]);
    pos+=4;

    /* 20230123
        TODO: are there ssh channel types where the confirmation has additional data ?
        read this here */

    return (int) pos;

}

static int read_ssh_channel_open_failure_msg(unsigned char type, struct ssh_payload_s *payload, union ssh_message_u *msg)
{
    char *buffer=payload->buffer;
    unsigned int pos=5;
    unsigned int tmp=0;

    if (payload->len<13) {

        logoutput_debug("read_ssh_channel_open_failure_msg: unable to read received msg ... too short (len=%u)", payload->len);
        return -1;

    }

    msg->channel.type.open_failure.reason=get_uint32(&buffer[pos]);
    pos+=4;
    tmp=read_ssh_string(&buffer[pos], (payload->len - pos), &msg->channel.type.open_failure.description);

    if (tmp<4) {

        logoutput_debug("read_ssh_channel_open_failure_msg: unable to read failure description");
        return -1;

    }

    pos+=tmp;
    return (int) pos;

}

int start_ssh_channel(struct ssh_channel_s *channel, struct ssh_channel_open_data_s *data)
{
    struct ssh_session_s *session=channel->session;
    struct ssh_connection_s *sshc=channel->connection;
    unsigned char type=channel->type;
    int result=-1;
    unsigned int seq=0;
    union ssh_message_u msg;

    if (((type==SSH_CHANNEL_TYPE_DIRECT) || (type==SSH_CHANNEL_TYPE_FORWARDED)) && (data==NULL)) {

        logoutput_debug("start_ssh_channel: cannot continue, open data must be specified for ssh channel type %u", type);
        return -1;

    }

    memset(&msg, 0, sizeof(union ssh_message_u));
    msg.channel.lcnr=channel->lcnr;
    msg.channel.type.open.windowsize=get_window_size(session);
    msg.channel.type.open.maxpacketsize=get_max_packet_size(session);
    msg.channel.type.open.data=data;

    if (send_ssh_channel_open_msg(sshc, type, &msg)>0) {
	struct system_timespec_s expire=SYSTEM_TIME_INIT;
	struct ssh_payload_s *payload=NULL;
	struct _cb_start_ssh_channel_hlpr_s hlpr;

	get_ssh_channel_expire_init(channel, &expire);
	hlpr.channel=channel;

	getpayload:

	payload=get_ssh_payload(&channel->queue, &expire, NULL, _cb_finish_start_ssh_channel, NULL, (void *) &hlpr);

	if (! payload) {

	    logoutput("start_ssh_channel: error waiting for packet");
	    goto out;

	}

	if (payload->type==SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {

            channel->lwindowsize=msg.channel.type.open.windowsize;
            channel->maxpacketsize=msg.channel.type.open.maxpacketsize;
            memset(&msg, 0, sizeof(union ssh_message_u));

            result=read_ssh_channel_open_confirmation_msg(channel->type, payload, &msg);

            if (result>0) {

	        channel->rcnr=msg.channel.type.open.rcnr;
	        channel->rwindowsize=msg.channel.type.open.windowsize;
	        channel->maxpacketsize=msg.channel.type.open.maxpacketsize;
	        channel->flags |= SSH_CHANNEL_FLAG_OPEN;
	        logoutput("start_ssh_channel: created a new channel local:remote %i:%i local window %u remote window %u max packet size %i", channel->lcnr, channel->rcnr, channel->lwindowsize, channel->rwindowsize, channel->maxpacketsize);

            }

	    free_payload(&payload);

	} else if (payload->type==SSH_MSG_CHANNEL_OPEN_FAILURE) {
	    int tmp=0;

            memset(&msg, 0, sizeof(union ssh_message_u));
            tmp=read_ssh_channel_open_failure_msg(channel->type, payload, &msg);

            if (tmp>0) {
                unsigned int reason=msg.channel.type.open_failure.reason;
                struct ssh_string_s *descr=&msg.channel.type.open_failure.description;

                logoutput("start_ssh_channel: failed by server reason=%u : %.*s", reason, descr->len, descr->ptr);

	    }

	    channel->flags |= SSH_CHANNEL_FLAG_OPENFAILURE;
	    free_payload(&payload);

        } else if (payload->type==SSH_MSG_CHANNEL_CLOSE) {

            free_payload(&payload);

	} else {

	    logoutput("start_ssh_channel: unexpected reply from server: %i", payload->type);
	    free_payload(&payload);
	    goto getpayload;

	}

    } else {

	logoutput("start_ssh_channel: error sending open channel message");

    }

    out:

    if (result>=0) {
	struct shared_signal_s *signal=channel->queue.signal;

	signal_lock(signal);
	signal_broadcast(signal);
	signal_unlock(signal);

    }

    return result;

}

void close_ssh_channel(struct ssh_channel_s *channel, unsigned int flags)
{

    if (!(channel->flags & SSH_CHANNEL_FLAG_OPEN)) return;

    logoutput("close_ssh_channel: %u", channel->lcnr);

    if ((flags & SSH_CHANNEL_FLAG_CLIENT_CLOSE) && (channel->flags & SSH_CHANNEL_FLAG_CLIENT_CLOSE)==0) {
        union ssh_message_u msg;

        memset(&msg, 0, sizeof(union ssh_message_u));
        msg.channel.lcnr=channel->lcnr;
        msg.channel.type.close.type=SSH_MSG_CHANNEL_CLOSE;

	channel->flags |= SSH_CHANNEL_FLAG_CLIENT_CLOSE;
	if (send_ssh_channel_eofclose_msg(channel, &msg)==-1) logoutput("close_ssh_channel: error sending close channel");

    }

    if ((flags & SSH_CHANNEL_FLAG_SERVER_CLOSE) && (channel->flags & SSH_CHANNEL_FLAG_SERVER_CLOSE)==0) {

        logoutput("close_ssh_channel: waiting for a server close ignored");
        channel->flags |= SSH_CHANNEL_FLAG_SERVER_CLOSE;

    }

    channel->flags &= ~SSH_CHANNEL_FLAG_OPEN;

}
