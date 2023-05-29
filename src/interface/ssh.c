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
#include "libosns-workspace.h"
#include "libosns-eventloop.h"
#include "libosns-interface.h"
#include "libosns-context.h"
#include "libosns-socket.h"
#include "libosns-ssh.h"

#include "ssh/ssh-common.h"
#include "ssh/ssh-common-client.h"
#include "ssh-utils.h"
#include "ssh/send/msg-channel.h"

#include "fuse.h"

/* SSH SESSION callbacks */

static int _connect_interface_ssh_session(struct context_interface_s *interface, union interface_target_u *target, union interface_parameters_u *param)
{
    struct beventloop_s *loop=get_workspace_beventloop(interface);
    struct ssh_session_s *session=(struct ssh_session_s *) interface->buffer;
    int fd=-1;
    unsigned int portnr=0;

    if (interface->flags & _INTERFACE_FLAG_CONNECT) {

	logoutput("connect_interface_ssh_session: already connected");
	goto out;

    } else if (target==NULL || param==NULL) {
 
	logoutput("connect_interface_ssh_session: error, host and/or service not set");
	goto out;

    }

    fd=connect_ssh_session_client(session, target->ip, param->port, loop);
    if (fd>=0) interface->flags |= _INTERFACE_FLAG_CONNECT;

    out:
    return fd;
}

static int _connect_interface_ssh_session_secondary(struct context_interface_s *interface, union interface_target_u *target, union interface_parameters_u *param)
{
    return 0;
}

static int _start_interface_ssh_session(struct context_interface_s *interface)
{
    struct ssh_session_s *session=(struct ssh_session_s *) interface->buffer;

    if (interface->flags & _INTERFACE_FLAG_START) {

	logoutput("start_interface_ssh_session: already started");
	return -1;

    }

    interface->flags |= _INTERFACE_FLAG_START;
    return setup_ssh_session(session);
}

static int _start_interface_ssh_session_secondary(struct context_interface_s *interface)
{
    return 0;
}

static char *get_interface_buffer_default(struct context_interface_s *interface)
{
    return interface->buffer;
}

static char *get_primary_interface_buffer(struct context_interface_s *interface)
{
    struct context_interface_s *primary=interface->link.primary;
    return primary->buffer;
}

static int _signal_ctx2ssh(struct context_interface_s *i, const char *what, struct io_option_s *option, struct context_interface_s *s, unsigned int type)
{
    struct ssh_session_s *session=(struct ssh_session_s *) i->buffer;
    void *ptr=(void *) session;
    logoutput_debug("_signal_ctx2ssh");

    return (* session->context.signal_ctx2ssh)(&ptr, what, option, type);
}

struct select_ssh2ctx_hlpr_s {
    struct context_interface_s *i;
};

static int select_ssh2ctx_cb(struct context_interface_s *i, void *ptr)
{
    struct select_ssh2ctx_hlpr_s *hlpr=(struct select_ssh2ctx_hlpr_s *) ptr;
    return (((i->type==_INTERFACE_TYPE_SSH_CHANNEL) && (i->link.primary==hlpr->i)) ? 1 : 0);
}

static int _signal_ssh2ctx(struct ssh_session_s *session, const char *what, struct io_option_s *option, unsigned int type)
{
    struct context_interface_s *i=(struct context_interface_s *)((char *) session - offsetof(struct context_interface_s, buffer));
    struct select_ssh2ctx_hlpr_s hlpr;

    /* signal every ctx using this transport, which are all ssh channels */
    hlpr.i=i;
    return signal_selected_ctx(i, 1, what, option, type, select_ssh2ctx_cb, (void *) &hlpr);
}

static struct list_header_s *get_header_connections(struct context_interface_s *i)
{
    char *buffer=(* i->get_interface_buffer)(i);
    struct ssh_session_s *s=(struct ssh_session_s *) buffer;
    return &s->connections.header;
}

static unsigned int get_interface_status(struct context_interface_s *i, struct interface_status_s *status)
{
    char *buffer=(* i->get_interface_buffer)(i);
    struct ssh_session_s *s=(struct ssh_session_s *) buffer;

    status->flags=INTERFACE_STATUS_FLAG_NONE;

    if (s->connections.main) {
        struct connection_s *c=&s->connections.main->connection;
        struct osns_socket_s *sock=&c->sock;

        if (sock->status & (SOCKET_STATUS_CLOSING | SOCKET_STATUS_CLOSED)) {

            status->flags=INTERFACE_STATUS_FLAG_DISCONNECTED;

        } else if (sock->status & SOCKET_STATUS_ERROR) {

            status->flags=INTERFACE_STATUS_FLAG_ERROR;

        } else if (sock->status & SOCKET_STATUS_OPEN) {

            status->flags=INTERFACE_STATUS_FLAG_CONNECTED;

        }

    }

    return status->flags;
}

/*
	INTERFACE OPS
			*/

static unsigned int _populate_interface_ssh_session(struct context_interface_s *interface, struct interface_ops_s *ops, struct interface_list_s *ilist, unsigned int start)
{

    if (ilist) {

	ilist[start].type=_INTERFACE_TYPE_SSH_SESSION;
	ilist[start].name="ssh-session";
	ilist[start].ops=(void *) ops;

    }

    start++;
    return start;
}

static unsigned int _get_interface_buffer_size_ssh_session(struct interface_list_s *ilist, struct context_interface_s *p)
{
    return ((ilist->type==_INTERFACE_TYPE_SSH_SESSION) ? get_ssh_session_buffer_size() : 0);
}

static int _init_interface_buffer_ssh_session(struct context_interface_s *interface, struct interface_list_s *ilist, struct context_interface_s *primary)
{
    struct ssh_session_s *session=NULL;
    struct service_context_s *context=get_service_context(interface);
    struct workspace_mount_s *workspace=get_workspace_mount_ctx(context);
    struct interface_ops_s *ops=ilist->ops;
    uid_t uid=getuid();
    struct shared_signal_s *signal=get_workspace_signal(interface);

    interface->type=_INTERFACE_TYPE_SSH_SESSION;

    if (primary) {

	/* dealing with a "link" to the shared ssh interface */

	interface->flags |= _INTERFACE_FLAG_SECONDARY_1TON;
	interface->link.primary=primary;
	primary->link.secondary.refcount++;

	interface->get_header_connections=get_header_connections;
	interface->get_interface_buffer=get_primary_interface_buffer;
	interface->connect=_connect_interface_ssh_session_secondary;
	interface->start=_start_interface_ssh_session_secondary;
	interface->get_interface_status=get_interface_status;

	interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
	return 0;

    }

    if (interface->size < (* ops->get_buffer_size)(ilist, NULL)) {

	logoutput_warning("_init_interface_buffer_ssh_session: buffer size too small (%i, required %i) cannot continue", interface->size, (* ops->get_buffer_size)(ilist, NULL));
	return -1;

    }

    if (interface->flags & _INTERFACE_FLAG_BUFFER_INIT) {

	logoutput_warning("_init_interface_buffer_ssh_session: buffer already initialized");
	return 0;

    }

    session=(struct ssh_session_s *) interface->buffer;
    if (signal==NULL) signal=get_default_shared_signal();

    if (init_ssh_session_client(session, uid, (void *) interface, signal)>=0) {

	interface->flags |= _INTERFACE_FLAG_PRIMARY_1TON;

	interface->connect=_connect_interface_ssh_session;
	interface->start=_start_interface_ssh_session;
	interface->get_interface_buffer=get_interface_buffer_default;
	interface->get_header_connections=get_header_connections;
	interface->get_interface_status=get_interface_status;

	interface->iocmd.in=_signal_ctx2ssh;
	session->context.signal_ssh2ctx=_signal_ssh2ctx;

	interface->flags |= _INTERFACE_FLAG_BUFFER_INIT;
	return 0;

    }

    return -1;

}

static void _clear_interface_buffer_ssh_session(struct context_interface_s *i)
{
    clear_interface_buffer_default(i);
}

static struct interface_ops_s ssh_interface_ops = {
    .name					= "SSH",
    .populate					= _populate_interface_ssh_session,
    .get_buffer_size				= _get_interface_buffer_size_ssh_session,
    .init_buffer				= _init_interface_buffer_ssh_session,
    .clear_buffer				= _clear_interface_buffer_ssh_session,
};

void init_ssh_session_interface()
{
    add_interface_ops(&ssh_interface_ops);
}

unsigned int get_ssh_channel_max_packet_length(struct context_interface_s *i)
{
    struct ssh_session_s *session=NULL;

    if (i->type==_INTERFACE_TYPE_SSH_CHANNEL) {
        struct ssh_channel_s *channel=(struct ssh_channel_s *) i->buffer;

        session=channel->session;

    } else if (i->type==_INTERFACE_TYPE_SSH_SESSION) {

        session=(struct ssh_session_s *) i->buffer;

    }

    return session ? get_max_packet_size(session) : 0;

}
