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

    if (channel->flags & CHANNEL_FLAG_CONNECTION_REFCOUNT) {

	decrease_refcount_ssh_connection(channel->connection);
	channel->flags &= ~CHANNEL_FLAG_CONNECTION_REFCOUNT;
    }

    clear_ssh_channel(channel);

    if (channel->flags & CHANNEL_FLAG_ALLOCATED) {

	free(channel);
	*p_channel=NULL;

    }

}

static void process_incoming_bytes_default(struct ssh_channel_s *channel, unsigned int size)
{
    /* decrease local window */
    channel->local_window-=size;
}

static void process_outgoing_bytes_default(struct ssh_channel_s *channel, unsigned int size)
{
    signal_lock_flag(channel->signal, &channel->flags, CHANNEL_FLAG_OUTGOING_DATA);
    channel->remote_window-=size;
    signal_unlock_flag(channel->signal, &channel->flags, CHANNEL_FLAG_OUTGOING_DATA);
}

static int signal_ctx2channel_default(struct ssh_channel_s **p_channel, const char *what, struct io_option_s *o, unsigned int type)
{
    struct ssh_channel_s *channel=*p_channel;
    const char *sender=get_name_interface_signal_sender(type);

    logoutput("signal_ctx2channel_default: %s by %s", what, sender);

    if (strncmp(what, "command:", 8)==0) {
	unsigned int pos=8;

	if (strncmp(&what[pos], "close:", 6)==0 || strncmp(&what[pos], "disconnect:", 11)==0 ||
	    strncmp(&what[pos], "clear:", 6)==0 || strncmp(&what[pos], "free:", 5)==0) {

	    switch_channel_send_data(channel, "close");
	    switch_msg_channel_receive_data(channel, "down", NULL);

	    table_remove_channel(channel);
	    close_channel(channel, CHANNEL_FLAG_CLIENT_CLOSE);

	    if (strncmp(&what[pos], "clear:", 6)==0 || strncmp(&what[pos], "free:", 5)==0) free_ssh_channel(p_channel);

	} else if (strncmp(&what[pos], "timecorrection:", 15)==0) {
	    struct ssh_session_s *session=channel->session;

	    start_timecorrection_ssh_server(session);

	}

    } else if (strncmp(what, "info:", 5)==0) {
	unsigned int pos=5;

	if (strncmp(&what[pos], "getentuser:", 11)==0 || strncmp(&what[pos], "getentgroup:", 12)==0 ||
	    strncmp(&what[pos], "system.getents:", 15)==0 || strncmp(&what[pos], "net.usermapping.sharemodus:", 27)==0) {
	    struct ssh_session_s *session=channel->session;
	    void *ptr=(void *) session;

	    return (* session->context.signal_ctx2ssh)(&ptr, what, o, type);

	}

    }

    return 0;

}

static int signal_channel2ctx_default(struct ssh_channel_s *c, const char *w, struct io_option_s *o, unsigned int type)
{
    return 0; /* nothing here since the context is not known: where to send it to */
}

static void recv_data_default(struct ssh_channel_s *c, char **p_buffer, unsigned int size, uint32_t seq, unsigned char flags)
{

    logoutput("recv_data_default: receiving %i bytes, this should not happen...data is lost");

    if (flags & _CHANNEL_DATA_RECEIVE_FLAG_ALLOC) {
	char *buffer=*p_buffer;

	free(buffer);
	*p_buffer=NULL;

    }

}

void init_ssh_channel(struct ssh_session_s *session, struct ssh_connection_s *connection, struct ssh_channel_s *channel, unsigned char type)
{

    channel->session=session;
    channel->connection=connection;
    channel->signal=session->signal;

    channel->context.unique=0;
    channel->context.ctx=NULL;
    channel->context.signal_ctx2channel=signal_ctx2channel_default;
    channel->context.signal_channel2ctx=signal_channel2ctx_default;
    channel->context.recv_data=recv_data_default;

    channel->type=type;
    channel->flags|=CHANNEL_FLAG_INIT;

    channel->local_channel=0;
    channel->remote_channel=0;
    channel->max_packet_size=0; /* filled later */
    channel->local_window=get_window_size(session);
    channel->process_incoming_bytes=process_incoming_bytes_default;
    channel->remote_window=0; /* to be received from server */
    channel->process_outgoing_bytes=process_outgoing_bytes_default;

    /* make use of the central mutex/cond for announcing payload has arrived */

    init_payload_queue(connection, &channel->queue);

    init_list_element(&channel->list, NULL);
    channel->start=start_channel;
    channel->close=close_channel;

    switch_channel_send_data(channel, "default");
    switch_msg_channel_receive_data(channel, "init", NULL); /* queue */
    increase_refcount_ssh_connection(connection);
    channel->flags |= CHANNEL_FLAG_CONNECTION_REFCOUNT;

}

struct ssh_channel_s *allocate_channel(struct ssh_session_s *session, struct ssh_connection_s *connection, unsigned char type)
{
    struct ssh_channel_s *channel=NULL;

    channel=malloc(sizeof(struct ssh_channel_s));

    if (channel) {

	memset(channel, 0, sizeof(struct ssh_channel_s));
	channel->flags |= CHANNEL_FLAG_ALLOCATED;
	init_ssh_channel(session, connection, channel, type);

    }

    return channel;
}

/* TODO:
    - set_func_signal_ctx_ssh_channel
    - get_func_signal_ssh_channel
    */

unsigned int get_ssh_channel_buffer_size()
{
    return sizeof(struct ssh_channel_s);
}
