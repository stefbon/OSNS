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
#include "workspace.h"
#include "workspace-interface.h"
#include "ssh/ssh-common.h"
#include "sftp/common.h"
#include "sftp/recv.h"
#include "ssh/send/msg-channel.h"
#include "ssh-utils.h"
#include "sftp-attr.h"

extern int send_channel_start_command_message(struct ssh_channel_s *channel, unsigned char reply, unsigned int *seq);

/* SFTP callbacks */

static int _connect_interface_sftp_client(uid_t uid, struct context_interface_s *interface, struct host_address_s *host, struct service_address_s *service)
{
    char *buffer=(* interface->get_interface_buffer)(interface);
    struct sftp_client_s *sftp=(struct sftp_client_s *) buffer;
    struct ssh_channel_s *channel=NULL;
    int result=-1;

    /* secondary interfaces do not have to be connected */
    if (interface->flags & _INTERFACE_FLAG_SECONDARY) return 0;

    if (!(service->type==_SERVICE_TYPE_SFTP_CLIENT)) {

	logoutput_warning("_connect_interface_ssh_channel: error, connections other than ssh channel (service type=%i)", service->type);
	goto out;

    }

    logoutput("_connect_interface_ssh_channel: (service type=%i)", service->type);

    channel=(struct ssh_channel_s *) sftp->context.conn;

    /* when there is an uri it must be for direct tcpip/streamlocal */

    if (service->target.sftp.uri) {
	char *uri=service->target.channel.uri;

	if (translate_channel_uri(channel, uri)==-1) {

	    logoutput_warning("_connect_interface_sftp_client: error processing uri %s", uri);
	    goto out;

	}

	if (channel->type==_CHANNEL_TYPE_DIRECT_STREAMLOCAL) {

	    channel->target.direct_streamlocal.type=_CHANNEL_DIRECT_STREAMLOCAL_TYPE_OPENSSH_COM;

	} else {

	    if (channel->type!=_CHANNEL_TYPE_DIRECT_TCPIP && 
		! (channel->type==_CHANNEL_TYPE_SESSION && channel->target.session.type==_CHANNEL_SESSION_TYPE_SUBSYSTEM &&
		channel->target.session.use.subsystem.type==_CHANNEL_SUBSYSTEM_TYPE_SFTP)) {

		logoutput_warning("_connect_interface_sftp_client: uri %s is not possible as sftp backend", uri);
		return -1;

	    }

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

	logoutput_warning("_connect_interface_sftp_client: type cannot be determined");
	goto out;

    }

    logoutput("_connect_interface_sftp_client: open and add channel");

    if (add_channel(channel, CHANNEL_FLAG_OPEN)==0) {

	logoutput_info("_connect_interface_sftp_client: channel %i open and added", channel->local_channel);
	result=0;

    } else {

	logoutput_warning("_connect_interface_sftp_client: failed to open channel %i", channel->local_channel);

    }

    out:
    return result;
}

static int _start_interface_sftp_client(struct context_interface_s *interface, int fd, struct ctx_option_s *option)
{
    char *buffer=(* interface->get_interface_buffer)(interface);
    struct sftp_client_s *sftp=(struct sftp_client_s *) buffer;
    struct ssh_channel_s *channel=(struct ssh_channel_s *) sftp->context.conn;
    int result=-1;

    if (interface->flags & _INTERFACE_FLAG_SECONDARY) return 0;

    if (channel->type==_CHANNEL_TYPE_SESSION) {
	unsigned int seq=0;

	/* start the channel
	    only required for things like exec, shell and subsystem 
	    see: https://tools.ietf.org/html/rfc4254#section-6.5
	    which is kind of strange cause when you open a channel
	    to a remote socket (direct-tcpip for example) the server
	    can also offer different services which first have to be started
	    but somehow this only counts for internal subsystems
	    I guess that the creators of this thought it is an external
	    address, which can be anything so one cannot safely assume things
	    have to be started extra
	    */

	logoutput("_start_interface_sftp_client: send start subsystem sftp");

	if (send_channel_start_command_message(channel, 1, &seq)>0) {
	    struct ssh_payload_s *payload=NULL;
	    struct system_timespec_s expire=SYSTEM_TIME_INIT;
	    unsigned int error=0;

	    get_channel_expire_init(channel, &expire);
	    payload=get_ssh_payload_channel(channel, &expire, NULL, &error);

	    if (! payload) {

		logoutput("_start_interface_sftp_client: error waiting for packet");

	    } else {

		if (payload->type==SSH_MSG_CHANNEL_SUCCESS) {

		    /* ready: channel ready to use */

		    logoutput("_start_interface_ssh_channel: server started the sftp subsystem");
		    result=0;

		} else if (payload->type==SSH_MSG_CHANNEL_FAILURE) {

		    logoutput("_start_interface_sftp_client: server failed to start the sftp subsystem");

		} else {

		    logoutput("_start_interface_sftp_client: got unexpected reply %i", payload->type);

		}

		free_payload(&payload);

	    }

	} else {

	    logoutput("_start_interface_sftp_client: error sending sftp");

	}

    } else {

	result=0;

    }

    if (result==0) {

	logoutput("_start_interface_sftp_client: start sftp init subsystem");

	/* switch the processing if incoming data for the channel to the sftp subsystem (=context)
	    it's not required to set the cb here...it's already set in the init phase */
	switch_msg_channel_receive_data(channel, "context", NULL);
	result=start_init_sftp_client(sftp);

	if (result==-1) {

	    logoutput_warning("_start_interface_sftp_client: error starting sftp subsystem");
	    goto out;

	}

	/* translate the supported sftp attributes to something fuse understands (FATTR_MODE, FATTR_SIZE, etc) */

	//translate_sftp_attr_fattr(interface);

    }

    out:

    return result;

}

/* function called by the context to inform sftp subsystem of events, it does that by calling the signal_sftp_client function */

static int _signal_interface(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    char *buffer=(* interface->get_interface_buffer)(interface);
    struct sftp_client_s *sftp=(struct sftp_client_s *) buffer;

    /* what to do with a secondary interface ?
	ignore things like "command:close ?
	for now 20201027 ignore everything */

    if (interface->flags & _INTERFACE_FLAG_SECONDARY) return 0;
    return (* sftp->context.signal_ctx2sftp)(&sftp, what, option);
}

/* call all the secondary sftp contexes, which can be an 1:n relation: no other way than to walk every interface and 
    check it's a secundary one and it's primary interface is this one */

static void _signal_sftp2ctx_secondary(struct context_interface_s *interface, const char *what, struct ctx_option_s *option)
{
    struct context_interface_s *search=get_next_context_interface(interface, NULL);
    unsigned int refcount=interface->link.refcount;

    while (search && refcount>0) {

	if ((search->flags & _INTERFACE_FLAG_SECONDARY) && search->link.primary==interface) {

	    int result=(* search->signal_context)(search, what, option);
	    refcount--;

	}

	search=get_next_context_interface(interface, search);

    }

}

static int _signal_sftp2ctx(struct sftp_client_s *sftp, const char *what, struct ctx_option_s *option)
{
    struct context_interface_s *interface=(struct context_interface_s *)((char *) sftp - offsetof(struct context_interface_s, buffer));

    if ((interface->flags & _INTERFACE_FLAG_SECONDARY)==0 && interface->link.refcount>0) {

	/* do not send all kinds of signals to the secondary interfaces
	    only events like disconnect */

	if (strncmp(what, "event:", 6)==0) _signal_sftp2ctx_secondary(interface, what, option);

    }

    return (* interface->signal_context)(interface, what, option);
}

/* signal the context of the channel which is the sftp subsystem */

static int _signal_channel2sftp(struct ssh_channel_s *channel, const char *what, struct ctx_option_s *option)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) channel->context.ctx;
    return (* sftp->context.signal_conn2sftp)(sftp, what, option);
}

static int _signal_sftp2channel(struct sftp_client_s *sftp, const char *what, struct ctx_option_s *option)
{
    struct ssh_channel_s *channel=(struct ssh_channel_s *) sftp->context.conn;
    return (* channel->context.signal_ctx2channel)(&channel, what, option);
}

static void _receive_data_ssh_channel(struct ssh_channel_s *channel, char **buffer, unsigned int size, uint32_t seq, unsigned char ssh_flags)
{
    struct sftp_client_s *sftp=(struct sftp_client_s *) channel->context.ctx;
    unsigned int sftp_flags=0;

    sftp_flags |= (ssh_flags & _CHANNEL_DATA_RECEIVE_FLAG_ALLOC) ? SFTP_RECEIVE_FLAG_ALLOC : 0;
    sftp_flags |= (ssh_flags & _CHANNEL_DATA_RECEIVE_FLAG_ERROR) ? SFTP_RECEIVE_FLAG_ERROR : 0;
    receive_sftp_data(sftp, buffer, size, seq, sftp_flags);

}

static int _send_data_ssh_channel(struct sftp_client_s *sftp, char *buffer, unsigned int size, uint32_t *seq, struct list_element_s *list)
{
    struct ssh_channel_s *channel=(struct ssh_channel_s *) sftp->context.conn;

    /* ignore the list ... do not put on send list */
    // pthread_mutex_lock(&sftp->mutex);
    // add_list_element_last(&sftp->pending, list);
    // pthread_mutex_unlock(&sftp->mutex);

    return send_channel_data_message(channel, buffer, size, seq);
}

static void _correct_time_c2s(struct sftp_client_s *sftp, struct system_timespec_s *time)
{
    struct ssh_channel_s *channel=(struct ssh_channel_s *) sftp->context.conn;
    struct ssh_session_s *session=channel->session;
    (* session->hostinfo.correct_time_c2s)(session, time);
}

static void _correct_time_s2c(struct sftp_client_s *sftp, struct system_timespec_s *time)
{
    struct ssh_channel_s *channel=(struct ssh_channel_s *) sftp->context.conn;
    struct ssh_session_s *session=channel->session;
    (* session->hostinfo.correct_time_s2c)(session, time);
}

static char *get_interface_buffer_default(struct context_interface_s *interface)
{
    return interface->buffer;
}

static char *get_interface_buffer_secondary(struct context_interface_s *interface)
{
    return interface->link.primary->buffer;
}

static void _free_interface_sftp(struct context_interface_s *interface)
{

    clear_ssh_string(&interface->backend.sftp.prefix.path);

    if (interface->backend.sftp.name) {

	free(interface->backend.sftp.name);
	interface->backend.sftp.name=NULL;

    }

}

/*
	INTERFACE OPS
			*/

static unsigned int _populate_sftp_interface(struct context_interface_s *interface, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start)
{

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_SFTP;
	ilist[start].name="sftp";
	ilist[start].ops=ops;

    }
    start++;
    return start;
}

static unsigned int _get_interface_sftp_buffer_size(struct interface_list_s *ilist)
{

    if (ilist->type==_INTERFACE_TYPE_SFTP) {

	if (strcmp(ilist->name, "sftp")==0) {

	    return get_sftp_buffer_size();

	}

    }

    return 0;

}

static int _init_interface_sftp_buffer(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct sftp_client_s *sftp=NULL;
    struct ssh_channel_s *channel=NULL;
    struct ssh_session_s *session=NULL;
    struct service_context_s *context=get_service_context(interface);
    struct service_context_s *pctx=NULL;
    struct context_interface_s *parent=NULL;

    if (strcmp(ilist->name, "sftp")!=0) {

	logoutput_warning("_init_interface_buffer_sftp: not initializing %s", ilist->name);
	return -1;

    }

    logoutput("_init_interface_buffer_sftp");
    interface->type=_INTERFACE_TYPE_SFTP;

    if (primary) {

	interface->flags |= _INTERFACE_FLAG_SECONDARY;

	/* dealing with a secondary interface: no initialization required */

	interface->link.primary=primary;
	primary->link.refcount++;
	interface->get_interface_buffer=get_interface_buffer_secondary;

	interface->connect=_connect_interface_sftp_client;
	interface->start=_start_interface_sftp_client;
	interface->signal_interface=_signal_interface;

	interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;

	return 0;

    }

    pctx=get_parent_context(context);
    if (pctx) parent=&pctx->interface;

    if (interface->size < get_sftp_buffer_size()) {

	logoutput_warning("_init_interface_buffer_sftp: buffer size too small (%i, required %i) cannot continue", interface->size, get_sftp_buffer_size());
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_sftp: buffer already initialized");
	return 0;

    }

    if (parent==NULL) {

	logoutput_warning("_init_interface_buffer_sftp: parent interface (= ssh session) not set");
	return -1;

    } else if (parent->type != _INTERFACE_TYPE_SSH_SESSION) {

	logoutput_warning("_init_interface_buffer_sftp: parent interface is not a ssh session (%i)", parent->type);
	return -1;

    }

    interface->get_interface_buffer=get_interface_buffer_default;
    interface->link.refcount=0;

    session=(struct ssh_session_s *) (* parent->get_interface_buffer)(parent);
    channel=create_channel(session, session->connections.main, 0);

    if (channel==NULL) {

	logoutput_warning("_connect_interface_sftp_client: failed to create channel");
	goto out;

    }

    logoutput("_connect_interface_sftp_client: created channel");

    sftp=(struct sftp_client_s *) interface->buffer;
    channel->context.ctx=(void *) sftp;

    if (init_sftp_client(sftp, session->identity.pwd.pw_uid, &session->hostinfo.mapping)==0) {

	interface->connect=_connect_interface_sftp_client;
	interface->start=_start_interface_sftp_client;
	interface->signal_interface=_signal_interface;
	interface->free=_free_interface_sftp;

	sftp->context.signal_sftp2ctx=_signal_sftp2ctx;
	sftp->context.signal_sftp2conn=_signal_sftp2channel;
	sftp->context.conn=(void *) channel;
	sftp->context.unique=interface->unique;

	sftp->signal.signal=channel->queue.signal->signal;

	sftp->time_ops.correct_time_c2s=_correct_time_c2s;
	sftp->time_ops.correct_time_s2c=_correct_time_s2c;

	channel->context.signal_channel2ctx=_signal_channel2sftp;

	/* connect data transfer:
	    send from sftp to channel and
	    receive from channel to sftp */

	sftp->context.send_data=_send_data_ssh_channel;
	channel->context.receive_data=_receive_data_ssh_channel;

	interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
	return 0;

    }

    out:
    return -1;

}

static int _init_interface_buffer(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{

    if (ilist->type == _INTERFACE_TYPE_SFTP) {

	if (strcmp(ilist->name, "sftp")==0) return _init_interface_sftp_buffer(interface, ilist, primary);

    }

    return -1;
}

static void _clear_interface_buffer(struct context_interface_s *interface)
{

    clear_ssh_string(&interface->backend.sftp.prefix.path);

    if (interface->backend.sftp.name) {

	free(interface->backend.sftp.name);
	interface->backend.sftp.name=NULL;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	if ((interface->flags & _INTERFACE_FLAG_BUFFER_CLEAR)==0) {

	    (* interface->signal_interface)(interface, "command:close:", NULL);
	    (* interface->signal_interface)(interface, "command:free:", NULL);
	    interface->flags |= _INTERFACE_FLAG_BUFFER_CLEAR;

	}

	interface->flags -= _INTERFACE_FLAG_BUFFER_INIT;
	reset_context_interface(interface);

    }

}

static struct interface_ops_s sftp_interface_ops = {
    .name					= "SSH_SUBSYSTEM",
    .populate					= _populate_sftp_interface,
    .get_buffer_size				= _get_interface_sftp_buffer_size,
    .init_buffer				= _init_interface_buffer,
    .clear_buffer				= _clear_interface_buffer,
};

void init_sftp_client_interface()
{
    add_interface_ops(&sftp_interface_ops);
}
