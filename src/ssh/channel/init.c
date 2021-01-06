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

#include "logging.h"
#include "main.h"
#include "misc.h"

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

    if (channel->type==_CHANNEL_TYPE_DIRECT_STREAMLOCAL) {

	clear_ssh_string(&channel->target.direct_streamlocal.path);

    } else if (channel->type==_CHANNEL_TYPE_DIRECT_TCPIP) {

	clear_ssh_string(&channel->target.direct_tcpip.host);
	clear_ssh_string(&channel->target.direct_tcpip.orig_ip);

    } else if (channel->type==_CHANNEL_TYPE_SESSION) {

	if (channel->target.session.type==_CHANNEL_SESSION_TYPE_EXEC) {

	    clear_ssh_string(&channel->target.session.use.exec.command);

	}

    }

    pthread_mutex_destroy(&channel->mutex);

}

void free_ssh_channel(struct ssh_channel_s **p_channel)
{
    struct ssh_channel_s *channel=*p_channel;

    if (channel->flags & CHANNEL_FLAG_CONNECTION_REFCOUNT) {

	decrease_refcount_ssh_connection(channel->connection);
	channel->flags -= CHANNEL_FLAG_CONNECTION_REFCOUNT;
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
    pthread_mutex_lock(&channel->mutex);

    /* decrease the remote window */
    channel->remote_window-=size;

    /* when remote window < max packet size wait for a window adjust message */

    pthread_mutex_unlock(&channel->mutex);
}

static int signal_ctx2channel_default(struct ssh_channel_s **p_channel, const char *what, struct ctx_option_s *o)
{
    struct ssh_channel_s *channel=*p_channel;

    logoutput("signal_ctx2channel_default: %s", what);

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

	if (strncmp(&what[pos], "getentuser:", 11)==0 || strncmp(&what[pos], "getentgroup:", 12)==0) {
	    struct ssh_session_s *session=channel->session;
	    void *ptr=(void *) session;

	    return (* session->context.signal_ctx2ssh)(&ptr, what, o);

	}

    }

    return 0;

}

static int signal_channel2ctx_default(struct ssh_channel_s *c, const char *w, struct ctx_option_s *o)
{
    return 0; /* nothing here since the context is not known: where to send it to */
}

static void receive_data_default(struct ssh_channel_s *c, char **p_buffer, unsigned int size, uint32_t seq, unsigned char flags)
{

    logoutput("receive_data_default: receiving %i bytes, this should not happen...data is lost");

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

    channel->context.unique=0;
    channel->context.ctx=NULL;
    channel->context.signal_ctx2channel=signal_ctx2channel_default;
    channel->context.signal_channel2ctx=signal_channel2ctx_default;
    channel->context.receive_data=receive_data_default;

    channel->type=type;
    channel->name=NULL;
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

    pthread_mutex_init(&channel->mutex, NULL);
    init_list_element(&channel->list, NULL);
    channel->start=start_channel;
    channel->close=close_channel;

    switch_channel_send_data(channel, "default");
    switch_msg_channel_receive_data(channel, "init", NULL); /* queue */
    increase_refcount_ssh_connection(connection);
    channel->flags |= CHANNEL_FLAG_CONNECTION_REFCOUNT;

}

struct ssh_channel_s *create_channel(struct ssh_session_s *session, struct ssh_connection_s *connection, unsigned char type)
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

struct ssh_channel_s *open_new_channel(struct ssh_connection_s *connection, struct ssh_string_s *type, unsigned int remote_channel, unsigned int windowsize, unsigned int maxpacketsize, struct ssh_string_s *data)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    int result=-1;
    struct ssh_channel_s *channel=NULL;

    if (compare_ssh_string(type, 'c', _CHANNEL_NAME_SESSION)!=0 &&
	compare_ssh_string(type, 'c', _CHANNEL_NAME_DIRECT_STREAMLOCAL_OPENSSH_COM)!=0 &&
	compare_ssh_string(type, 'c', _CHANNEL_NAME_DIRECT_TCPIP)!=0) return NULL;


    channel=create_channel(session, connection, _CHANNEL_TYPE_SESSION);

    if (channel) {

	channel->name=_CHANNEL_NAME_SESSION;
	channel->remote_channel=remote_channel;
	channel->max_packet_size=maxpacketsize;
	channel->local_window=windowsize;

	result=add_channel(channel, 0);

	if (result==-1) {

	    logoutput("open_new_channel: unable to add channel");
	    goto out;

	}

    } else {

	logoutput("open_new_channel: unable to create channel");
	goto out;

    }

    if (compare_ssh_string(type, 'c', _CHANNEL_NAME_SESSION)==0) {

	/* nothing to do */
	result=0;

    } else if (compare_ssh_string(type, 'c', _CHANNEL_NAME_DIRECT_STREAMLOCAL_OPENSSH_COM)==0) {

	if (data->len>8) {
	    struct ssh_string_s *path=&channel->target.direct_streamlocal.path;
	    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
	    struct ssh_string_s socket=SSH_STRING_INIT;

	    set_msg_buffer_string(&mb, data);
	    msg_read_ssh_string(&mb, &socket);

	    if (mb.error>0) goto out;
	    create_ssh_string(&path, socket.len, socket.ptr, SSH_STRING_FLAG_ALLOC);
	    result=0;

	} else {

	    goto out;

	}

    } else if (compare_ssh_string(type, 'c', _CHANNEL_NAME_DIRECT_TCPIP)==0) {

	if (data->len>8) {
	    struct ssh_string_s *host=&channel->target.direct_tcpip.host;
	    struct ssh_string_s *orig_ip=&channel->target.direct_tcpip.orig_ip;
	    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
	    struct ssh_string_s address=SSH_STRING_INIT;
	    struct ssh_string_s ip=SSH_STRING_INIT;

	    set_msg_buffer_string(&mb, data);
	    msg_read_ssh_string(&mb, &address);
	    msg_read_uint32(&mb, &channel->target.direct_tcpip.port);
	    msg_read_ssh_string(&mb, &ip);
	    msg_read_uint32(&mb, &channel->target.direct_tcpip.orig_port);

	    if (mb.error>0) goto out;
	    create_ssh_string(&host, address.len, address.ptr, SSH_STRING_FLAG_ALLOC);
	    create_ssh_string(&orig_ip, ip.len, ip.ptr, SSH_STRING_FLAG_ALLOC);
	    result=0;

	} else {

	    goto out;

	}

    }

    out:

    if (result==-1 && channel) {

	remove_channel(channel, 0);
	free_ssh_channel(&channel);

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
