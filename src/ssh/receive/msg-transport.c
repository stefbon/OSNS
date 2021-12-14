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

#include "log.h"
#include "main.h"

#include "misc.h"
#include "commonsignal.h"

#include "ssh-utils.h"
#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-data.h"
#include "ssh-send.h"
#include "ssh-keyexchange.h"
#include "ssh-extensions.h"
#include "ssh-signal.h"

/* various callbacks for SSH transport */

/*

    possible values:

    SSH_MSG_DISCONNECT                        	1
    SSH_MSG_IGNORE				2
    SSH_MSG_UNIMPLEMENTED			3
    SSH_MSG_DEBUG				4
    SSH_MSG_SERVICE_REQUEST			5
    SSH_MSG_SERVICE_ACCEPT			6
    SSH_MSG_EXT_INFO				7
    SSH_MSG_KEXINIT				20
    SSH_MSG_NEWKEYS				21

    SSH_MSG_KEXDH_REPLY				31

    SSH_MSG_GLOBAL_REQUEST			80
    SSH_MSG_REQUEST_SUCCESS			81
    SSH_MSG_REQUEST_FAILURE			82

*/


/* disconnect */

static void receive_msg_disconnect(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    unsigned int reason=0;
    unsigned int len=0;

    if (payload->len>=9) {

	reason=get_uint32(&payload->buffer[1]);
	len=get_uint32(&payload->buffer[5]);

    } else {

	reason=SSH_DISCONNECT_PROTOCOL_ERROR;

    }

    /* server send a disconnect: client must also disconnect immediatly  */

    if (len>0 && (9 + len <= payload->len)) {
	char string[len+1];

	memcpy(&string[0], &payload->buffer[9], len);
	string[len]='\0';

	logoutput("receive_msg_disconnect: received disconnect reason %i:%s", reason, string);

    } else {
	const char *string=get_disconnect_reason(reason);

	if (string) {

	    logoutput("receive_msg_disconnect: received disconnect reason %i:%s", reason, string);

	} else {

	    logoutput("receive_msg_disconnect: received disconnect reason %i", reason);

	}

    }

    free_payload(&payload);
    disconnect_ssh_connection(connection);

}

/* ignore */

static void receive_msg_ignore(struct ssh_connection_s *c, struct ssh_payload_s *payload)
{
    free_payload(&payload);
}

/* not implemented */

static void receive_msg_unimplemented(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len >= 5) {
	struct ssh_signal_s *signal=&connection->receive.signal;
	unsigned int sequence=get_uint32(&payload->buffer[1]);

	logoutput_info("receive_msg_unimplemented: received a unimplemented message for number %i", sequence);

	/* signal any waiting thread */

	ssh_signal_lock(signal);
	signal->sequence_number_error=sequence;
	signal->error=EOPNOTSUPP;
	ssh_signal_broadcast(signal);
	ssh_signal_unlock(signal);

	free_payload(&payload);
	payload=NULL;

    }

    if (payload) {

	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

/* debug */

static void receive_msg_debug(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len > 6) {
	unsigned int len=0;

	len=get_uint32(&payload->buffer[2]);

	if (len>0 && len<65) {
	    char string[len+1];

	    memcpy(&string, &payload->buffer[6], len);
	    string[len]='\0';

	    if (payload->buffer[1]) {

		logoutput_debug("receive_msg_debug: %s", string);

	    } else {

		logoutput_info("receive_msg_debug: %s", string);

	    }

	}

	free_payload(&payload);
	payload=NULL;

    }

    if (payload) {

	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

/* service request */

static void receive_msg_service_request(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);

    if (session->flags & SSH_SESSION_FLAG_SERVER) {

	signal_lock(connection->setup.signal);

	if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

	    free_payload(&payload);
	    payload=NULL;

	} else if (connection->setup.flags & SSH_SETUP_FLAG_TRANSPORT) {

	    queue_ssh_payload_locked(&connection->setup.queue, payload);
	    payload=NULL;

	}

	signal_unlock(connection->setup.signal);

    } else {

	/* error: receiving a service request from the server in this phase is not ok */

	logoutput_info("receive_msg_service_request: error: received a service request from server....");
	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

/* service accept, reply on service request for ssh-userauth or ssh-connection */

static void receive_msg_service_accept(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    logoutput("receive_msg_service_accept");

    signal_lock(connection->setup.signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

	free_payload(&payload);
	payload=NULL;

    } else if (connection->setup.flags & SSH_SETUP_FLAG_TRANSPORT) {

	queue_ssh_payload_locked(&connection->setup.queue, payload);
	payload=NULL;

    }

    signal_unlock(connection->setup.signal);

    if (payload) {

	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

static void receive_msg_ext_info(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    logoutput("receive_msg_ext_info");

    signal_lock(connection->setup.signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

	free_payload(&payload);
	payload=NULL;

    }

    signal_unlock(connection->setup.signal);

    if (payload) {

	/* received:
	    - after SSH_MSG_NEWKEYS or
	    - before SSH_MSG_USERAUTH_SUCCESS */

	/* this msg is allowed after first SSH_MSG_NEWKEYS */

	process_msg_ext_info(connection, payload);
	free_payload(&payload);

    }

}

/*
    receiving a kexinit message
    it's possible that it's a kexinit in the setup phase but also
    to initiate the rekeyexchange by the server
*/

static void receive_msg_kexinit(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_setup_s *setup=&connection->setup;
    unsigned int error=0;
    int result=-1;

    /* start (re)exchange. See: https://tools.ietf.org/html/rfc4253#section-9 */

    logoutput("receive_msg_kexinit");

    signal_lock(setup->signal);

    if (setup->flags & SSH_SETUP_FLAG_DISCONNECT)  {

	free_payload(&payload);
	payload=NULL;

    } else if (setup->status==SSH_SETUP_PHASE_TRANSPORT) {

	/* when in transport phase it's possible this during the setup of the connection or rekey
	    it does not matter: queue it */

	if (setup->phase.transport.status==SSH_TRANSPORT_TYPE_GREETER || setup->phase.transport.status==SSH_TRANSPORT_TYPE_KEX) {

	    if ((setup->phase.transport.status==SSH_TRANSPORT_TYPE_KEX) && (setup->phase.transport.type.kex.flags & SSH_KEX_FLAG_KEXINIT_S2C)) {

		signal_unlock(setup->signal);
		goto disconnect;

	    }

	    /* transport is being setup: in kex or greeter, queue it anyway */

	    queue_ssh_payload_locked(&setup->queue, payload);
	    payload=NULL;

	}

    } else if (setup->flags & SSH_SETUP_FLAG_TRANSPORT) {

	/* connection is setup, and no (re)kexinit: start it here */

	if ((setup->flags & SSH_SETUP_FLAG_SETUPTHREAD)==0) {
	    int result=0;

	    setup->thread=pthread_self();
	    setup->flags |= SSH_SETUP_FLAG_SETUPTHREAD;
	    init_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX);
	    queue_ssh_payload_locked(&setup->queue, payload);
	    payload=NULL;
	    signal_broadcast(setup->signal);
	    signal_unlock(setup->signal);

	    result=key_exchange(connection);
	    logoutput("receive_msg_kexinit: rekey exchange %s", (result==0) ? "success" : "failed");

	    finish_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX);
	    finish_ssh_connection_setup(connection, "transport", 0);
	    finish_ssh_connection_setup(connection, "setup", 0);
	    if (result==-1) goto disconnect;

	    return;

	} else {

    	    queue_ssh_payload(&setup->queue, payload);
	    payload=NULL;

	}

    }

    signal_unlock(setup->signal);
    if (payload) free_payload(&payload);
    return;

    disconnect:

    if (payload) free_payload(&payload);
    disconnect_ssh_connection(connection);

}

static int setup_cb_newkeys(struct ssh_connection_s *connection, void *data)
{
    set_ssh_receive_behaviour(connection, "newkeys");
    return 0;
}

static void receive_msg_newkeys(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    logoutput("receive_msg_newkeys");

    free_payload(&payload);
    payload=NULL;

    if (change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_NEWKEYS_S2C, 0, setup_cb_newkeys, NULL)==-1)
	disconnect_ssh_connection(connection);

}

static void receive_msg_kexdh_reply(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_setup_s *setup=&connection->setup;

    logoutput("receive_msg_kexdh_reply");

    signal_lock(setup->signal);

    if (setup->flags & SSH_SETUP_FLAG_DISCONNECT) {

	free_payload(&payload);
	payload=NULL;

    } else if (setup->status==SSH_SETUP_PHASE_TRANSPORT) {

	if (setup->phase.transport.status==SSH_TRANSPORT_TYPE_KEX) {

	    /* keyexchange in setup or connection phase */

	    queue_ssh_payload_locked(&setup->queue, payload);
	    payload=NULL;

	}

    }

    signal_unlock(setup->signal);

    if (payload) {

	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

/*
    - byte		SSH_MSG_GLOBAL_REQUEST
    - string		request name
    - boolean		want reply
    ....		request specific data

    like "hostkeys-00@openssh.com"
*/

static void receive_msg_global_request(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_string_s name=SSH_STRING_INIT;

    if (read_ssh_string(&payload->buffer[1], payload->len - 1, &name) > 3) {

	logoutput("receive_msg_global_request: received request %.*s", name.len, name.ptr);

    } else {

	logoutput("receive_msg_global_request: received request, cannot read name");

    }

    signal_lock(connection->setup.signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

	free_payload(&payload);

    } else if (connection->setup.flags & SSH_SETUP_FLAG_SERVICE_CONNECTION) {
	struct payload_queue_s *queue = &connection->setup.queue;

	queue_ssh_payload_locked(queue, payload);
	payload=NULL;

    }

    signal_unlock(connection->setup.signal);

    if (payload) {

	logoutput("receive_msg_global_request: disconnect");
	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

static void receive_msg_request_common(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    logoutput("receive_msg_request_common");

    signal_lock(connection->setup.signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

	free_payload(&payload);

    } else if (connection->setup.flags & SSH_SETUP_FLAG_SERVICE_CONNECTION) {
	struct payload_queue_s *queue = &connection->setup.queue;

	/* queue it in the general channel independant queue */

	queue_ssh_payload_locked(queue, payload);
	payload=NULL;

    }

    connection->flags &= ~SSH_CONNECTION_FLAG_GLOBAL_REQUEST;

    signal_broadcast(connection->setup.signal);
    signal_unlock(connection->setup.signal);

    if (payload) {

	logoutput("receive_msg_request_common: disconnect");
	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}


void register_transport_cb(struct ssh_connection_s *c)
{
    register_msg_cb(c, SSH_MSG_DISCONNECT, receive_msg_disconnect);
    register_msg_cb(c, SSH_MSG_IGNORE, receive_msg_ignore);
    register_msg_cb(c, SSH_MSG_UNIMPLEMENTED, receive_msg_unimplemented);
    register_msg_cb(c, SSH_MSG_DEBUG, receive_msg_debug);
    register_msg_cb(c, SSH_MSG_SERVICE_REQUEST, receive_msg_service_request);
    register_msg_cb(c, SSH_MSG_SERVICE_ACCEPT, receive_msg_service_accept);
    register_msg_cb(c, SSH_MSG_EXT_INFO, receive_msg_ext_info);

    register_msg_cb(c, SSH_MSG_KEXINIT, receive_msg_kexinit);
    register_msg_cb(c, SSH_MSG_NEWKEYS, receive_msg_newkeys);

    register_msg_cb(c, SSH_MSG_KEXDH_REPLY, receive_msg_kexdh_reply);

    /* global requests like enabling port forwarding on server to client */

    register_msg_cb(c, SSH_MSG_GLOBAL_REQUEST, receive_msg_global_request);
    register_msg_cb(c, SSH_MSG_REQUEST_SUCCESS, receive_msg_request_common);
    register_msg_cb(c, SSH_MSG_REQUEST_FAILURE, receive_msg_request_common);

}
