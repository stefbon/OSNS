/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"

#include "ssh-utils.h"
#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-hash.h"
#include "ssh-connections.h"
#include "ssh-hostinfo.h"
#include "ssh-keyexchange.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-data.h"
#include "ssh-channel.h"

#include "ssh-userauth.h"
#include "ssh-connections.h"
#include "ssh-extensions.h"
#include "alloc/init.h"

#include "ssh-signal-client.h"

int init_ssh_identity_client(struct ssh_session_s *session, uid_t uid)
{
    struct ssh_identity_s *identity=&session->identity;
    struct passwd *result=NULL;

    init_ssh_identity(session);

    getpw:

    memset(&identity->pwd, 0, sizeof(struct passwd));
    result=NULL;
    identity->buffer=realloc(identity->buffer, identity->size);
    if(identity->buffer==NULL) goto error;

    if (getpwuid_r(uid, &identity->pwd, identity->buffer, identity->size, &result)==-1) {

	if (errno==ERANGE) {

	    identity->size+=128;
	    goto getpw; /* size buffer too small, increase and try again */

	}

	goto error;

    }

    logoutput("init_ssh_identity_client: found user %s (uid=%i, info %s) home %s", result->pw_name, result->pw_uid, result->pw_gecos, result->pw_dir);
    return 0;

    error:
    return -1;

}

unsigned int get_ssh_session_buffer_size()
{
    return sizeof(struct ssh_session_s);
}

int init_ssh_session_client(struct ssh_session_s *session, uid_t uid, void *ctx, struct shared_signal_s *signal)
{
    logoutput("_init_ssh_session: init user uid %i", (unsigned int) uid);

    if (init_ssh_backend(signal)==-1) goto error;

    init_ssh_session(session, uid, ctx, signal);
    init_ssh_session_signals_client(&session->context);

    if (init_ssh_identity_client(session, uid)==-1) {

	logoutput("_init_ssh_session: error getting user identity for uid %i", (unsigned int) uid);
	goto error;

    }

    return 0;

    error:
    free_ssh_hostinfo(session);
    free_ssh_session_data(session);
    free_ssh_channels_table(session);
    free_ssh_identity(session);
    free_ssh_connections(session);
    return -1;

}

static void disconnect_ssh_connection_cb(struct connection_s *c, unsigned char remote)
{

    disconnect_cb_default(c, remote);

    if (remote) {
	struct ssh_connection_s *sshc=(struct ssh_connection_s *)((char *)c - offsetof(struct ssh_connection_s, connection));

	/* when initiated by remote side signal the context (==workspace)
	    here the context == the channels using this connection */

	struct ssh_session_s *session=get_ssh_connection_session(sshc);

	if (session) {
	    struct channel_table_s *table=&session->channel_table;
	    struct osns_lock_s lock;

	    if (channeltable_readlock(table, &lock)==0) {
		struct ssh_channel_s *channel=get_next_channel(session, NULL);

		if (channel->connection==sshc) (* channel->context.signal_channel2ctx)(channel, "event:disconnect:", NULL, INTERFACE_CTX_SIGNAL_TYPE_SSH_CHANNEL);
		channel=get_next_channel(session, channel);

		channeltable_unlock(table, &lock);

	    }

	}

    }

}

int connect_ssh_session_client(struct ssh_session_s *session, struct host_address_s *target, struct network_port_s *port, struct beventloop_s *loop)
{
    struct ssh_connection_s *connection=NULL;
    struct io_option_s option;
    int fd=-1;
    struct shared_signal_s *signal=NULL;

    /* get the ctx for values like:
	- shared mutex and cond for shared event signalling when for example the connection and/or
	is disconnected and the waiting thread wants to be informed about that (while waiting for a response)
	- timeout
    */

    init_io_option(&option, _IO_OPTION_TYPE_PVOID);

    if (set_ssh_connections_signal(session, session->signal)==-1) {

	logoutput("connect_ssh_session_client: error setting shared signal");
	goto out;

    }

    init_io_option(&option, _IO_OPTION_TYPE_INT);

    if (add_main_ssh_connection(session)==0) {

	logoutput("connect_ssh_session_client: main connection added to session");

    } else {

	logoutput("connect_ssh_session_client: error adding main connection");
	goto out;

    }

    connection=session->connections.main;
    connection->connection.ops.client.disconnect=disconnect_ssh_connection_cb;
    connection->connection.ops.client.dataavail=read_ssh_connection_socket;

    fd=connect_ssh_connection(connection, target, port, loop);

    if (fd>0) {

	logoutput("connect_ssh_session_client: connected with fd %i", fd);

    } else {

	logoutput("connect_ssh_session_client: unable to connect");

    }

    out:

    return fd;

}
