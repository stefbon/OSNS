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
#include "ssh-signal.h"
#include "ssh-userauth.h"
#include "ssh-connections.h"
#include "ssh-extensions.h"
#include "alloc/init.h"

// #define UINT32_T_MAX				0xFFFFFFFF

#define GLOBAL_STATUS_FLAG_SSH_INIT				1
#define GLOBAL_STATUS_FLAG_BACKEND_INIT				2

static unsigned int global_status=0;

static pthread_mutex_t init_mutex=PTHREAD_MUTEX_INITIALIZER;

void init_ssh_session_config(struct ssh_session_s *session)
{
    struct ssh_config_s *config=&session->config;

    memset(config, 0, sizeof(struct ssh_config_s));

    config->flags=SSH_CONFIG_FLAG_CORRECT_CLOCKSKEW;

    config->max_packet_size=SSH_CONFIG_MAX_PACKET_SIZE;
    config->max_receive_size=SSH_CONFIG_RECEIVE_BUFFER_SIZE;

    config->port=SSH_CONFIG_DEFAULT_PORT;

    config->connection_expire=SSH_CONFIG_CONNECTION_EXPIRE;
    config->userauth_expire=SSH_CONFIG_USERAUTH_EXPIRE;

    config->max_receiving_threads=SSH_CONFIG_MAX_RECEIVING_THREADS;
    config->max_sending_threads=SSH_CONFIG_MAX_SENDING_THREADS;

    config->extensions = 0;
    config->global_requests = 0;
    config->trustdb = SSH_CONFIG_TRUSTDB_OPENSSH;
    config->auth=SSH_CONFIG_AUTH_PUBLICKEY;

}

void free_ssh_identity(struct ssh_session_s *session)
{
    struct ssh_identity_s *identity=&session->identity;

    if (identity->buffer) {

	free(identity->buffer);
	identity->buffer=NULL;

    }

    identity->size=0;
    memset(identity, 0, sizeof(struct ssh_identity_s));
}

void init_ssh_identity(struct ssh_session_s *session)
{
    struct ssh_identity_s *identity=&session->identity;

    memset(identity, 0, sizeof(struct ssh_identity_s));
    identity->buffer=NULL;
    identity->size=128;
    init_ssh_string(&identity->remote_user);
    identity->identity_file=NULL;
}

int init_ssh_backend(struct shared_signal_s *signal)
{
    int result=0;

    if (signal==NULL) signal=get_default_shared_signal();

    if (signal_set_flag(signal, &global_status, GLOBAL_STATUS_FLAG_BACKEND_INIT)) {

	result=init_ssh_backend_library();

    }

    if (result==-1) {

	logoutput_debug("init_ssh_backend: unable to initialize ssh backend library");
	return -1;

    }

    if (signal_set_flag(signal, &global_status, GLOBAL_STATUS_FLAG_SSH_INIT)) {

	init_ssh_send_once();
	init_ssh_receive_once();
	init_ssh_utils();
	init_keyex_once();

    }

    return 0;

}

#define CLOSE_SSH_SESSION_FLAG_CLOSE				1
#define CLOSE_SSH_SESSION_FLAG_CLEAR				2
#define CLOSE_SSH_SESSION_FLAG_FREE				4
#define CLOSE_SSH_SESSION_FLAG_REMOTE				8
#define CLOSE_SSH_SESSION_FLAG_SIGNAL				16

void _close_ssh_session_connections(struct ssh_session_s *session, unsigned char flags)
{
    struct ssh_connections_s *connections=&session->connections;
    struct ssh_connection_s *connection=NULL;
    unsigned char remove=((flags & (CLOSE_SSH_SESSION_FLAG_CLEAR | CLOSE_SSH_SESSION_FLAG_FREE)) ? 1 : 0);

    connection=get_next_ssh_connection(connections, connection, remove);

    while (connection) {
	struct ssh_connection_s *next=get_next_ssh_connection(connections, connection, remove);

	if (flags & CLOSE_SSH_SESSION_FLAG_CLOSE) remove_ssh_connection_eventloop(connection);

	/* send a ssh disconnect message
	    only when initiative is not taken by remote side (then they already know, don't have to be notified) AND
	    not send already */

	if (connection->flags & SSH_CONNECTION_FLAG_MAIN) {

	    if ((flags & CLOSE_SSH_SESSION_FLAG_CLOSE) && (flags & CLOSE_SSH_SESSION_FLAG_REMOTE) && (connection->flags & SSH_CONNECTION_FLAG_DISCONNECT_SEND)==0) {

		send_disconnect_message(connection, SSH_DISCONNECT_BY_APPLICATION);
		connection->flags |= SSH_CONNECTION_FLAG_DISCONNECT_SEND;

	    }

	    if (flags & (CLOSE_SSH_SESSION_FLAG_CLEAR | CLOSE_SSH_SESSION_FLAG_FREE)) connections->main=NULL;

	}

	if (flags & CLOSE_SSH_SESSION_FLAG_CLOSE) {

	    set_ssh_send_behaviour(connection, "disconnect");
	    set_ssh_receive_behaviour(connection, "disconnect");

	}

	if (remove) free_ssh_connection(&connection);
	connection=next;

    }

}

void _walk_ssh_session_channels(struct ssh_session_s *session, unsigned int flags, struct ssh_connection_s *connection)
{
    struct channel_table_s *table=&session->channel_table;
    struct osns_lock_s wlock;
    unsigned char remove=((flags & (CLOSE_SSH_SESSION_FLAG_CLEAR | CLOSE_SSH_SESSION_FLAG_FREE)) ? 1 : 0);

    logoutput_debug("_walk_ssh_session_channels: flags %u", flags);

    if (channeltable_writelock(table, &wlock)==0) {
	struct ssh_channel_s *channel=get_next_channel(session, NULL);

	while (channel) {
	    struct ssh_channel_s *next=get_next_channel(session, channel);

	    if (connection && (channel->connection != connection)) goto next;

	    if (flags & CLOSE_SSH_SESSION_FLAG_CLOSE) {

		switch_channel_send_data(channel, "close");
		switch_msg_channel_receive_data(channel, "down", NULL);

		if (flags & CLOSE_SSH_SESSION_FLAG_SIGNAL) {

		    (* channel->context.signal_channel2ctx)(channel, "event:close:", NULL, INTERFACE_CTX_SIGNAL_TYPE_SSH_CHANNEL);

		}

	    }

	    if (remove) {

		if (flags & CLOSE_SSH_SESSION_FLAG_SIGNAL) (* channel->context.signal_channel2ctx)(channel, "event:free:", NULL, INTERFACE_CTX_SIGNAL_TYPE_SSH_CHANNEL);
		table_remove_channel(channel);
		close_channel(channel, CHANNEL_FLAG_CLIENT_CLOSE);
		free_ssh_channel(&channel);

	    }

	    next:
	    channel=next;

	}

	channeltable_unlock(table, &wlock);

    }

}

void set_ssh_session_config_connection_expire(struct ssh_session_s *session, unsigned int timeout)
{
    session->config.connection_expire=timeout;
}

unsigned int get_window_size(struct ssh_session_s *session)
{
    /* 2 ^ 32 - 1*/
    return (unsigned int)(UINT32_MAX - 1);
}

unsigned int get_max_packet_size(struct ssh_session_s *session)
{
    return session->config.max_packet_size;
}

void set_max_packet_size(struct ssh_session_s *session, unsigned int size)
{
    session->config.max_packet_size=size;
}

void close_ssh_session(struct ssh_session_s *session)
{

    if (signal_set_flag(session->connections.signal, &session->connections.flags, SSH_CONNECTIONS_FLAG_DISCONNECTING)) {

	_walk_ssh_session_channels(session, (CLOSE_SSH_SESSION_FLAG_CLOSE | CLOSE_SSH_SESSION_FLAG_SIGNAL), NULL);
	_close_ssh_session_connections(session, (CLOSE_SSH_SESSION_FLAG_CLOSE | CLOSE_SSH_SESSION_FLAG_SIGNAL));
	signal_set_flag(session->connections.signal, &session->connections.flags, SSH_CONNECTIONS_FLAG_DISCONNECTED);

    }

}

void clear_ssh_session(struct ssh_session_s *session)
{

    if (signal_set_flag(session->signal, &session->flags, SSH_SESSION_FLAG_CLEARING)) {

	_walk_ssh_session_channels(session, CLOSE_SSH_SESSION_FLAG_CLEAR, NULL);
	_close_ssh_session_connections(session, CLOSE_SSH_SESSION_FLAG_CLEAR);
	signal_set_flag(session->signal, &session->flags, SSH_SESSION_FLAG_CLEARED);

    }

    free_ssh_connections(session);
    free_ssh_channels_table(session);
    free_ssh_hostinfo(session);
    free_ssh_identity(session);
    free_ssh_session_data(session);
    free_ssh_pubkey(session);

}

void free_ssh_session(void **p_ptr)
{
    struct ssh_session_s *session=(struct ssh_session_s *) *p_ptr;

    clear_ssh_session(session);

    if (session->flags & SSH_SESSION_FLAG_ALLOCATED) {
	free(session);
	*p_ptr=NULL;

    }

}

struct ssh_session_s *_create_ssh_session(unsigned int flags, struct generic_error_s *error)
{
    struct ssh_session_s *session=NULL;

    session=malloc(sizeof(struct ssh_session_s));

    if (session) {

	flags &= (SSH_SESSION_ALLFLAGS);
	memset(session, 0, sizeof(struct ssh_session_s));
	session->flags = (SSH_SESSION_FLAG_ALLOCATED | flags);

    } else {

	set_generic_error_system(error, ENOMEM, __PRETTY_FUNCTION__);

    }

    return session;
}

static int signal_ssh2ctx_default(struct ssh_session_s *session, const char *what, struct io_option_s *o)
{
    return 0;
}

static int signal_ctx2ssh_default(void **ptr, const char *what, struct io_option_s *o)
{
    return 0;
}

int init_ssh_session(struct ssh_session_s *session, uid_t uid, void *ctx, struct shared_signal_s *signal)
{

    session->context.ctx=ctx;
    session->context.unique=0;
    session->context.signal_ssh2ctx=signal_ssh2ctx_default;
    session->context.signal_ctx2ssh=signal_ctx2ssh_default;
    session->context.signal_ssh2remote=signal_ssh2ctx_default;

    session->signal=signal;
    session->connections.signal=signal;

    init_list_element(&session->list, NULL);
    init_ssh_session_config(session);
    init_ssh_channels_table(session, NULL, CHANNELS_TABLE_SIZE);
    init_ssh_session_data(session);
    init_ssh_hostinfo(session);
    init_ssh_extensions(session);
    init_ssh_pubkey(session);
    init_ssh_connections(session);

    return 0;
}

int setup_ssh_session(struct ssh_session_s *session)
{
    unsigned int error=EIO;
    int result=-1;
    struct ssh_connection_s *connection=session->connections.main;

    logoutput("setup_ssh_session");

    register_transport_cb(connection);
    change_ssh_connection_setup(connection, "setup", 0, SSH_SETUP_FLAG_SETUPTHREAD, 0, NULL, NULL);

    /* setup keyexchange and payload queue here already to make sure there is a queue available
	some servers send a kexinit message just with or just behind the greeter
	it's important there is a payload queue present */

    // if (add_ssh_connection_eventloop(connection, fd, &error)==-1) {

	// logoutput("_setup_ssh_session: error %i adding fd %i to eventloop (%s)", error, fd, strerror(error));
	// goto out_setup;

    // }

    // logoutput("setup_ssh_session: added fd %i to eventloop", fd);

    /* setup greeter */

    init_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_GREETER);

    /* send a greeter and wait for greeter from server */

    if (send_ssh_greeter(connection)==-1) {

	logoutput("setup_ssh_session: error sending greeter");
	goto out_setup;

    }

    logoutput("setup_ssh_session: greeter send");

    /* wait for the greeter from the server */

    if (wait_ssh_connection_setup_change(connection, "transport", SSH_TRANSPORT_TYPE_GREETER, SSH_GREETER_FLAG_S2C | SSH_GREETER_FLAG_C2S, NULL, NULL)==-1) {

	logoutput("setup_ssh_session: failed receiving/reading greeter");
	goto out_setup;

    }

    /* start key exchange */

    logoutput("setup_ssh_session: greeter finished, start key exchange");
    init_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX);

    if (key_exchange(connection)==-1) {

	logoutput("setup_ssh_session: key exchange failed");
	goto out_kex;

    }

    /* immediatly after the newkeys message the server may send a SSH_MSG_EXT_INFO message
	be prepared for this */

    if (check_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, 0)<1) {

	logoutput("setup_ssh_session: error: keyexchange failed");
	finish_ssh_connection_setup(connection, "transport", 0);
	goto out_setup;

    }

    /* finish key exchange
	this means by definition that transport is setup */

    finish_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX);
    finish_ssh_connection_setup(connection, "transport", 0);
    logoutput("setup_ssh_session: key exchange finished");

    /* The Secure Shell (SSH) Transport Layer Protocol completed (RFC4253)
	start the userauth phase */

    init_ssh_connection_setup(connection, "service", SSH_SERVICE_TYPE_AUTH);
    register_userauth_cb(connection, 1);

    /* start the ssh-userauth service before ssh-connection 
	NOTE: the server may send a SSH_MSG_EXT_INFO message preceding (just before)
	the SSH_MSG_USERAUTH_SUCCESS message*/

    result=start_ssh_userauth(session, connection);

    if (result==-1) {

	logoutput("setup_ssh_session: authentification failed");
	goto out_auth;

    }

    if (check_ssh_connection_setup(connection, "service", SSH_SERVICE_TYPE_AUTH, 0)<1) {

	logoutput("setup_ssh_session: error: authentification failed");
	goto out_auth;

    }

    if (session->flags & SSH_SESSION_FLAG_SERVER) {

	if (result==SSH_SERVICE_TYPE_CONNECTION) {

	    /* enable the callbacks for handling channels*/

	    register_channel_cb(connection, 1);

	}

    } else {

	register_channel_cb(connection, 1);

    }

    result=0; /* only when here success*/

    out_auth:

    finish_ssh_connection_setup(connection, "service", SSH_SERVICE_TYPE_AUTH);
    finish_ssh_connection_setup(connection, "service", 0);

    out_kex:
    out_setup:

    finish_ssh_connection_setup(connection, "setup", 0);

    if (result==-1) {

	if (error==0) error=EIO;
	logoutput("setup_ssh_session: exit with error %i (%s)", error, strerror(error));

    } else {

	logoutput("setup_ssh_session: authentication finished");

    }

    return result;

}

static void analyze_ssh_connection_problem(void *ptr)
{
    struct ssh_connection_s *connection=(struct ssh_connection_s *) ptr;
    struct osns_socket_s *sock=&connection->connection.sock;
    unsigned int error=0;

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) return; /* already disconnect(ing/ed)*/

    if ((connection->flags & SSH_CONNECTION_FLAG_TROUBLE)==0) {

	/* this flag should be set */
	logoutput("analyze_ssh_connection_problem: flag SSH_CONNECTION_FLAG_TROUBLE not set as it should be ... warning");

    }

    if (connection->setup.error>0) {

	error=connection->setup.error;
	logoutput("analyze_ssh_connection_problem: found error %i:%s", error, strerror(error));

    } else {

	logoutput("analyze_ssh_connection_problem: errorcode unknown, trying to find it");

    }

    if (error==0) error=get_status_ssh_connection(connection);

    if (error==0) {

	/* error==0 -> maybe a normal shutdown */

	size_t size=10; /* arbitrary size */
	char tmp[size];
	int bytesread=0;

	bytesread=(* sock->sops.connection.recv)(sock, (void *) tmp, size, MSG_PEEK);
	error=errno;

    }

    if (error==0) {

	if (connection->setup.flags & SSH_SETUP_FLAG_RECV_EMPTY) error=ESHUTDOWN;

    }

    if (error>0) {

	if (socket_connection_error(error)) {

	    logoutput("analyze_connection_problem: error %i (%s): disconnecting", error, strerror(error));

	    change_ssh_connection_setup(connection, "setup", 0, SSH_SETUP_FLAG_DISCONNECTING, 0, NULL, 0);
	    remove_ssh_connection_eventloop(connection);
	    disconnect_ssh_connection(connection);
	    change_ssh_connection_setup(connection, "setup", 0, SSH_SETUP_FLAG_DISCONNECTED, 0, NULL, 0);

	    if (connection->refcount>=0) {
		struct ssh_session_s *session=get_ssh_connection_session(connection);

		/* send close to channels context using this connection */

		_walk_ssh_session_channels(session, CLOSE_SSH_SESSION_FLAG_CLOSE, connection);

	    }

	} else {

	    logoutput("analyze_connection_problem: ignoring error %i (%s) : not reckognized", error, strerror(error));

	}

    }

    change_ssh_connection_setup(connection, "setup", 0, SSH_SETUP_FLAG_ANALYZETHREAD, SSH_SETUP_OPTION_UNDO, NULL, NULL);

}

static int setup_cb_thread_connection_problem(struct ssh_connection_s *connection, void *data)
{
    struct generic_error_s error=GENERIC_ERROR_INIT;
    work_workerthread(NULL, 0, analyze_ssh_connection_problem, (void *) connection, &error);
    return 0;
}

int start_thread_ssh_connection_problem(struct ssh_connection_s *connection)
{
    return change_ssh_connection_setup(connection, "setup", 0, SSH_SETUP_FLAG_ANALYZETHREAD, SSH_SETUP_OPTION_XOR, setup_cb_thread_connection_problem, NULL);
}
