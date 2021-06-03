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
#include <sys/syscall.h>

#define LOGGING
#include "log.h"

#include "main.h"
#include "options.h"
#include "eventloop.h"

#include "workspace-interface.h"
#include "workspace.h"

#include "ssh/ssh-common.h"
#include "ssh/ssh-common-client.h"
#include "ssh-utils.h"
#include "ssh/send/msg-channel.h"

extern int send_channel_start_command_message(struct ssh_channel_s *channel, unsigned char reply, unsigned int *seq);

/* SSH SESSION callbacks */

static int _connect_interface_ssh_session(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    struct ssh_session_s *session=(struct ssh_session_s *) interface->buffer;
    char *target=NULL;
    unsigned int port=0;
    int fd=-1;
    char *ip=NULL;

    if (host==NULL || service==NULL) {
 
	logoutput("connect_interface_ssh_session: error, host and/or service not set");
	goto out;

    } else if (service->type!=_SERVICE_TYPE_PORT) {

	logoutput("connect_interface_ssh_session: error, connections other than via a network port are not supported (service type=%i)", service->type);
	goto out;

    } else if (service->target.port.type!=_NETWORK_PORT_TCP) {

	logoutput("connect_interface_ssh_session: error, connections other than via an tcp port are not supported (port type=%i)", service->target.port.type);
	goto out;

    }

    translate_context_address_network(host, service, &target, &port, NULL);

    if (port==0) {

	logoutput("connect_interface_ssh_session: connecting to %s (application default)", target);

    } else {

	logoutput("connect_interface_ssh_session: connecting to %s:%i", target, port);

    }

    ip = (host->ip.family==IP_ADDRESS_FAMILY_IPv4) ? host->ip.ip.v4 : host->ip.ip.v6;
    if (ip==NULL || strlen(ip)==0) ip=target;

    fd=connect_ssh_session_client(session, ip, port);

    out:
    return fd;
}

static int _start_interface_ssh_session(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    struct ssh_session_s *session=(struct ssh_session_s *) interface->buffer;
    return setup_ssh_session(session, fd);
}

static int _signal_interface_ssh_session(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    struct ssh_session_s *session=(struct ssh_session_s *) interface->buffer;
    void *ptr=(void *) session;
    logoutput("_signal_interface_ssh_session: %s", what);
    return (* session->context.signal_ctx2ssh)(&ptr, what, option);
}

static int _signal_ssh2ctx(struct ssh_session_s *session, const char *what, struct ctx_option_s *option)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) session - offsetof(struct context_interface_s, buffer));
    return (* interface->signal_context)(interface, what, option);
}

static int add_connection_eventloop(struct ssh_session_s *session, struct ssh_connection_s *conn, unsigned int fd, void (* cb)(int fd, void *ptr, struct event_s *event), void *ptr)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) session - offsetof(struct context_interface_s, buffer));
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct beventloop_s *loop=NULL;
    struct bevent_s *bevent=NULL;

    context=get_root_context_workspace(workspace);
    loop=context->interface.backend.fuse.loop;

    bevent=create_fd_bevent(loop, cb, ptr);
    if (bevent==NULL) return -1;
    set_bevent_unix_fd(bevent, fd);
    set_bevent_watch(bevent, "incoming data");

    if (add_bevent_beventloop(bevent)==0) {

	logoutput("add_connection_eventloop: fd %i added to eventloop", fd);
	conn->connection.io.socket.bevent=bevent;
	return 0;

    }

    logoutput_warning("add_connection_eventloop: failed to add fd %i to eventloop", fd);
    return -1;

}

/* SSH CHANNEL callbacks*/

static int _connect_interface_ssh_channel(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    struct ssh_channel_s *channel=(struct ssh_channel_s *) interface->buffer;
    int result=-1;

    if (!(service->type==_SERVICE_TYPE_SSH_CHANNEL)) {

	logoutput_warning("_connect_interface_ssh_channel: error, connections other than ssh channel (service type=%i)", service->type);
	goto out;

    }

    /* address has form like:
	- ssh-channel:session:shell:
	- ssh-channel:session:subsystem:sftp
	- ssh-channel:direct-tcpip:

	in the latest case the uri has to be defined as sftp://example.domain.org:321 for example

	- ssh-channel:direct-streamlocal@openssh.com:

	socket://somepath

	*/

    if (service->target.channel.uri) {
	char *uri=service->target.channel.uri;

	if (translate_channel_uri(channel, uri)==-1) {

	    logoutput_warning("_connect_interface_ssh_channel: error processing uri %s", uri);
	    goto out;

	}

	if (channel->type==_CHANNEL_TYPE_DIRECT_STREAMLOCAL) {

	    channel->target.direct_streamlocal.type=_CHANNEL_DIRECT_STREAMLOCAL_TYPE_OPENSSH_COM;

	}

    } else {

	/* no uri: take the default, the sftp subsystem */

	channel->type=_CHANNEL_TYPE_SESSION;
	channel->name=_CHANNEL_NAME_SESSION;
	channel->target.session.type=_CHANNEL_SESSION_TYPE_SUBSYSTEM;
	channel->target.session.name=_CHANNEL_SESSION_NAME_SUBSYSTEM;
	channel->target.session.use.subsystem.type=_CHANNEL_SUBSYSTEM_TYPE_SFTP;
	channel->target.session.use.subsystem.name=_CHANNEL_SUBSYSTEM_NAME_SFTP;

    }

    if (channel->type==0) {

	logoutput_warning("_connect_interface_ssh_channel: error: type cannot be determined");
	goto out;

    }

    if (add_channel(channel, CHANNEL_FLAG_OPEN)==0) {

	logoutput_info("_connect_interface_ssh_channel: channel %i open", channel->local_channel);
	result=0;

    } else {

	logoutput_info("_connect_interface_ssh_channel: channel %i failed to open", channel->local_channel);

    }

    out:
    return result;
}

static int _start_interface_ssh_channel(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    struct ssh_channel_s *channel=(struct ssh_channel_s *) interface->buffer;
    int result=-1;
    uint32_t seq=0;

    /* start the channel
	only required for things like exec, shell and subsystem 
	see: https://tools.ietf.org/html/rfc4254#section-6.5
	which is kind of strange cause when you open a channel
	to a remote socket (direct-tcpip fo example) the server
	can also offer different services which have to be started
    */

    logoutput("_start_interface_ssh_channel: send start %s", channel->name);

    if (send_channel_start_command_message(channel, 1, &seq)>0) {
	struct ssh_payload_s *payload=NULL;
	struct timespec expire;
	unsigned int error=0;

	get_channel_expire_init(channel, &expire);
	payload=get_ssh_payload_channel(channel, &expire, NULL, &error);

	if (! payload) {

	    logoutput("_start_interface_ssh_channel: error waiting for packet");

	} else {

	    if (payload->type==SSH_MSG_CHANNEL_SUCCESS) {

		/* ready: channel ready to use */

		logoutput("_start_interface_ssh_channel: server started %s", channel->name);
		result=0;

	    } else if (payload->type==SSH_MSG_CHANNEL_FAILURE) {

		logoutput("_start_interface_ssh_channel: server failed to start %s", channel->name);
		free_payload(&payload);

	    } else {

		logoutput("_start_interface_ssh_channel: got unexpected reply %i", payload->type);
		free_payload(&payload);

	    }

	    free_payload(&payload);

	}

    } else {

	logoutput("_start_interface_ssh_channel: error sending sftp %s", channel->name);

    }

    out:
    return result;

}

/* function called by the context to inform ssh channel of events, it does that by calling the signal_ssh_channel function */

static int _signal_ctx2channel(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    char *buffer=(* interface->get_interface_buffer)(interface);
    struct ssh_channel_s *channel=(struct ssh_channel_s *) interface->buffer;
    return (* channel->context.signal_ctx2channel)(&channel, what, option);
}

/* function called by channel to inform the context, does that by calling the interface->signal_context function */

static int _signal_channel2ctx(struct ssh_channel_s *channel, const char *what, struct ctx_option_s *option)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) channel - offsetof(struct context_interface_s, buffer));
    return (* interface->signal_context)(interface, what, option);
}

static void _receive_data_ssh_channel(struct ssh_channel_s *channel, char **buffer, unsigned int size, uint32_t seq, unsigned char ssh_flags)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) channel - offsetof(struct context_interface_s, buffer));
    unsigned int ctx_flags=0;

    ctx_flags |= (ssh_flags & _CHANNEL_DATA_RECEIVE_FLAG_ALLOC) ? _INTERFACE_BUFFER_FLAG_ALLOC : 0;
    ctx_flags |= (ssh_flags & _CHANNEL_DATA_RECEIVE_FLAG_ERROR) ? _INTERFACE_BUFFER_FLAG_ERROR : 0;

    /* call the interface specific receive data callback */
    (* interface->backend.ssh_channel.receive_data)(interface, buffer, size, seq, ctx_flags);
}

static int _send_data_ssh_channel(struct context_interface_s *interface, char *data, unsigned int len, uint32_t *seq)
{
    struct ssh_channel_s *channel=(struct ssh_channel_s *) interface->buffer;
    return send_channel_data_message(channel, data, len, seq);
}

/*
	INTERFACE OPS
			*/

static unsigned int _populate_ssh_interface(struct context_interface_s *interface, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start)
{

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_SSH_SESSION;
	ilist[start].name="ssh-session";
	ilist[start].ops=(void *) ops;

    }

    start++;

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_SSH_CHANNEL;
	ilist[start].name="ssh-channel";
	ilist[start].ops=(void *) ops;

    }

    start++;
    return start;
}

static unsigned int _get_interface_buffer_size(struct interface_list_s *ilist)
{
    if (ilist->type==_INTERFACE_TYPE_SSH_SESSION) {

	return (get_ssh_session_buffer_size());

    } else if (ilist->type==_INTERFACE_TYPE_SSH_CHANNEL) {

	return (get_ssh_channel_buffer_size());

    }
    return 0;
}

static int _init_interface_buffer_ssh_session(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct ssh_session_s *session=NULL;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct osns_user_s *user=workspace->user;
    struct interface_ops_s *ops=ilist->ops;

    if (interface->size < (* ops->get_buffer_size)(ilist)) {

	logoutput_warning("_init_interface_buffer_ssh_session: buffer size too small (%i, required %i) cannot continue", interface->size, (* ops->get_buffer_size)(ilist));
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_ssh_session: buffer already initialized");
	return 0;

    }

    interface->type=_INTERFACE_TYPE_SSH_SESSION;
    session=(struct ssh_session_s *) interface->buffer;

    if (init_ssh_session_client(session, user->pwd.pw_uid, (void *) interface)==0) {

	interface->connect=_connect_interface_ssh_session;
	interface->start=_start_interface_ssh_session;
	interface->signal_interface=_signal_interface_ssh_session;

	session->context.signal_ssh2ctx=_signal_ssh2ctx;
	session->context.add_connection_eventloop=add_connection_eventloop;

	interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
	return 0;

    }

    return -1;

}

static int _init_interface_buffer_ssh_channel(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct ssh_channel_s *channel=NULL;
    struct ssh_session_s *session=NULL;
    struct service_context_s *context=get_service_context(interface);
    struct service_context_s *pctx=NULL;
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct osns_user_s *user=workspace->user;
    struct context_interface_s *parent=NULL;
    struct interface_ops_s *ops=ilist->ops;

    pctx=get_parent_context(context);
    if (pctx) parent=&pctx->interface;

    if (interface->size < (* ops->get_buffer_size)(ilist)) {

	logoutput_warning("_init_interface_buffer_ssh_channel: buffer size too small (%i, required %i) cannot continue", interface->size, (* ops->get_buffer_size)(ilist));
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_ssh_channel: buffer already initialized");
	return 0;

    }

    if (parent==NULL) {

	logoutput_warning("_init_interface_buffer_ssh_channel: parent interface (= ssh session) not set");
	return -1;

    } else if (parent->type != _INTERFACE_TYPE_SSH_SESSION) {

	logoutput_warning("_init_interface_buffer_ssh_channel: parent interface is not a ssh session (%i)", parent->type);
	return -1;

    }

    interface->type=_INTERFACE_TYPE_SSH_CHANNEL;
    channel=(struct ssh_channel_s *) interface->buffer;
    session=(struct ssh_session_s *) parent->buffer;
    init_ssh_channel(session, session->connections.main, channel, 0);
    interface->connect=_connect_interface_ssh_channel;
    interface->start=_start_interface_ssh_channel;

    interface->signal_interface=_signal_ctx2channel;
    channel->context.signal_channel2ctx=_signal_channel2ctx;
    channel->context.receive_data=_receive_data_ssh_channel;
    interface->backend.ssh_channel.send_data=_send_data_ssh_channel;

    interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
    return 0;

}

static int _init_interface_buffer(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{

    if (ilist->type == _INTERFACE_TYPE_SSH_SESSION) {

	return _init_interface_buffer_ssh_session(interface, ilist, primary);

    } else if (ilist->type == _INTERFACE_TYPE_SSH_CHANNEL) {

	return _init_interface_buffer_ssh_channel(interface, ilist, primary);

    }

    return -1;
}

static void _clear_interface_buffer(struct context_interface_s *interface)
{

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	if ((interface->flags & _INTERFACE_FLAG_BUFFER_CLEAR)==0) {

	    (* interface->signal_interface)(interface, "disconnect", NULL);
	    (* interface->signal_interface)(interface, "close", NULL);
	    (* interface->signal_interface)(interface, "free", NULL);
	    interface->flags |= _INTERFACE_FLAG_BUFFER_CLEAR;

	}

	interface->flags -= _INTERFACE_FLAG_BUFFER_INIT;
	reset_context_interface(interface);

    }

}

static struct interface_ops_s ssh_interface_ops = {
    .name					= "SSH",
    .populate					= _populate_ssh_interface,
    .get_buffer_size				= _get_interface_buffer_size,
    .init_buffer				= _init_interface_buffer,
    .clear_buffer				= _clear_interface_buffer,
};

void init_ssh_session_interface()
{
    add_interface_ops(&ssh_interface_ops);
}
