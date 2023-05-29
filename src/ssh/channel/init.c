/*
  2018 Stef Bon <stefbon@gmail.com>

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
#include "ssh-utils.h"
#include "ssh-receive.h"
#include "ssh-hostinfo.h"

#include "startclose.h"
#include "signal.h"
#include "send/msg-channel.h"

void clean_ssh_channel_queue(struct ssh_channel_s *channel)
{
    clear_payload_queue(&channel->queue, 1);
}

void clear_ssh_channel(struct ssh_channel_s *channel)
{
    clean_ssh_channel_queue(channel);
}

void free_ssh_channel(struct ssh_channel_s **p_channel)
{
    struct ssh_channel_s *channel=*p_channel;

    if (channel->flags & SSH_CHANNEL_FLAG_CONNECTION_REFCOUNT) {

	decrease_refcount_ssh_connection(channel->connection);
	channel->flags &= ~SSH_CHANNEL_FLAG_CONNECTION_REFCOUNT;
    }

    clear_ssh_channel(channel);

    if (channel->flags & SSH_CHANNEL_FLAG_ALLOCATED) {

	free(channel);
	*p_channel=NULL;

    }

}

static void process_incoming_bytes_default(struct ssh_channel_s *channel, unsigned int size)
{
    /* decrease local window */
    channel->lwindowsize-=size;
}

static void process_outgoing_bytes_default(struct ssh_channel_s *channel, unsigned int size)
{
    signal_lock_flag(channel->signal, &channel->flags, SSH_CHANNEL_FLAG_OUTGOING_DATA);
    channel->rwindowsize-=size;
    signal_unlock_flag(channel->signal, &channel->flags, SSH_CHANNEL_FLAG_OUTGOING_DATA);
}

static void recv_msg_default(struct ssh_channel_s *c, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=*p_payload;
    queue_ssh_payload(&c->queue, payload);
    queue_ssh_broadcast(&c->queue);
    *p_payload=NULL;
}

void init_ssh_channel(struct ssh_session_s *session, struct ssh_connection_s *connection, struct ssh_channel_s *channel, unsigned char type)
{

    memset(channel, 0, sizeof(struct ssh_channel_s));

    channel->session=session;
    channel->connection=connection;
    channel->signal=session->signal;

    channel->context.unique=0;
    channel->context.ctx=NULL;
    channel->context.signal_ctx2channel=signal_ctx2channel_default;
    channel->context.signal_channel2ctx=signal_channel2ctx_default;

    channel->type=type;
    channel->flags|=SSH_CHANNEL_FLAG_INIT;

    channel->lcnr=(unsigned int) -1;
    channel->rcnr=(unsigned int) -1;
    channel->maxpacketsize=0; /* filled later */
    channel->lwindowsize=0;
    channel->process_incoming_bytes=process_incoming_bytes_default;
    channel->rwindowsize=0; /* to be received from server */
    channel->process_outgoing_bytes=process_outgoing_bytes_default;
    channel->exit_status=0;
    channel->exit_signal=0;

    /* make use of the central mutex/cond for announcing payload has arrived */

    init_payload_queue(channel->signal, &channel->queue);

    init_list_element(&channel->list, NULL);
    channel->start=start_ssh_channel;
    channel->close=close_ssh_channel;

    channel->iocb[SSH_CHANNEL_IOCB_RECV_DATA]=recv_msg_default;
    channel->iocb[SSH_CHANNEL_IOCB_RECV_XDATA]=recv_msg_default;
    channel->iocb[SSH_CHANNEL_IOCB_RECV_REQUEST]=recv_msg_default;
    channel->iocb[SSH_CHANNEL_IOCB_RECV_OPEN]=recv_msg_default;

    increase_refcount_ssh_connection(connection);
    channel->flags |= SSH_CHANNEL_FLAG_CONNECTION_REFCOUNT;

}

struct ssh_channel_s *allocate_ssh_channel(struct ssh_session_s *session, struct ssh_connection_s *connection, unsigned char type)
{
    struct ssh_channel_s *channel=NULL;

    channel=malloc(sizeof(struct ssh_channel_s));

    if (channel) {

	init_ssh_channel(session, connection, channel, type);
	channel->flags |= SSH_CHANNEL_FLAG_ALLOCATED;

    }

    return channel;
}

unsigned int get_ssh_channel_buffer_size()
{
    return sizeof(struct ssh_channel_s);
}
