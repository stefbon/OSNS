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

#include "ssh-utils.h"
#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-data.h"
#include "ssh-send.h"
#include "ssh-keyexchange.h"
#include "ssh-extensions.h"

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
    logoutput_debug("receive_msg_ignore: seq %u", payload->sequence);
}

/* reply to a request which is not implemented
    byte		SSH_MSG_UNIMPLEMENTED
    uint32		sequence number of rejected message
*/

static void receive_msg_unimplemented(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{

    if (payload->len >= 5) {
	struct ssh_receive_s *receive=&connection->receive;
	struct shared_signal_s *signal=&receive->signal;
	unsigned int sequence=get_uint32(&payload->buffer[1]);

	logoutput_info("receive_msg_unimplemented: received a unimplemented message for number %i", sequence);

	/* signal any waiting thread */

	signal_lock(signal);
	receive->sequence_error.sequence_number_error=sequence;
	receive->sequence_error.errcode=EOPNOTSUPP;
	signal_broadcast(signal);
	signal_unlock(signal);

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

static void receive_msg_service_request(struct ssh_connection_s *sshc, struct ssh_payload_s *payload)
{
    struct ssh_session_s *session=get_ssh_connection_session(sshc);

    if (session->flags & SSH_SESSION_FLAG_SERVER) {
	struct shared_signal_s *signal=sshc->setup.signal;

	signal_lock(signal);

	if (sshc->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

            signal_unlock(signal);
	    free_payload(&payload);
	    payload=NULL;

	} else if (sshc->setup.flags & SSH_SETUP_FLAG_TRANSPORT) {

            signal_unlock(signal);
	    queue_ssh_payload(&sshc->setup.queue, payload);
	    queue_ssh_broadcast(&sshc->setup.queue);
	    payload=NULL;

	}

    } else {

	/* error: receiving a service request from the server in this phase is not ok */

	logoutput_info("receive_msg_service_request: error: received a service request from server....");
	free_payload(&payload);
	disconnect_ssh_connection(sshc);

    }

}

/* service accept, reply on service request for ssh-userauth or ssh-connection */

static void receive_msg_service_accept(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct shared_signal_s *signal=connection->setup.signal;

    logoutput("receive_msg_service_accept");

    signal_lock(signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

        signal_unlock(signal);
	free_payload(&payload);
	payload=NULL;

    } else if (connection->setup.flags & SSH_SETUP_FLAG_TRANSPORT) {

        signal_unlock(signal);
	queue_ssh_payload(&connection->setup.queue, payload);
	queue_ssh_broadcast(&connection->setup.queue);
	payload=NULL;

    }

    if (payload) {

	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

static void receive_msg_ext_info(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct shared_signal_s *signal=connection->setup.signal;

    logoutput("receive_msg_ext_info");

    signal_lock(signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

	free_payload(&payload);
	payload=NULL;

    }

    signal_unlock(signal);

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

static void receive_msg_kexinit(struct ssh_connection_s *sshc, struct ssh_payload_s *payload)
{
    struct ssh_setup_s *setup=&sshc->setup;
    struct shared_signal_s *signal=setup->signal;
    unsigned int error=0;
    int result=-1;

    /* start (re)exchange. See: https://tools.ietf.org/html/rfc4253#section-9 */

    logoutput_debug("receive_msg_kexinit: setup status %u", setup->status);

    signal_lock(signal);

    if (setup->flags & SSH_SETUP_FLAG_DISCONNECT)  {

        signal_unlock(signal);
	free_payload(&payload);
	payload=NULL;

    } else if (setup->status==SSH_SETUP_PHASE_TRANSPORT) {

	/* when in transport phase it's possible this during the setup of the connection or rekey
	    it does not matter: queue it */

	if (setup->phase.transport.status==SSH_TRANSPORT_TYPE_GREETER || setup->phase.transport.status==SSH_TRANSPORT_TYPE_KEX) {

	    if ((setup->phase.transport.status==SSH_TRANSPORT_TYPE_KEX) && (setup->phase.transport.type.kex.flags & SSH_KEX_FLAG_KEXINIT_S2C)) {

		signal_unlock(signal);
		goto disconnect;

	    }

	    /* transport is being setup: in kex or greeter, queue it anyway */

	    logoutput_debug("receive_msg_kexinit: queue");
	    signal_unlock(signal);

	    queue_ssh_payload(&setup->queue, payload);
	    queue_ssh_broadcast(&setup->queue);
	    payload=NULL;

	    return;

	}

    } else if (setup->flags & SSH_SETUP_FLAG_TRANSPORT) {

	/* connection is setup, and no (re)kexinit: start it here */

	if ((setup->flags & SSH_SETUP_FLAG_SETUPTHREAD)==0) {
	    int result=0;

	    setup->thread=pthread_self();
	    setup->flags |= SSH_SETUP_FLAG_SETUPTHREAD;
	    signal_unlock(signal);
	    init_ssh_connection_setup(sshc, "transport", SSH_TRANSPORT_TYPE_KEX);
	    queue_ssh_payload(&setup->queue, payload);
	    payload=NULL;

            queue_ssh_broadcast(&setup->queue);

	    result=key_exchange(sshc);
	    logoutput("receive_msg_kexinit: rekey exchange %s", (result==0) ? "success" : "failed");

	    finish_ssh_connection_setup(sshc, "transport", SSH_TRANSPORT_TYPE_KEX);
	    finish_ssh_connection_setup(sshc, "transport", 0);
	    finish_ssh_connection_setup(sshc, "setup", 0);
	    if (result==-1) goto disconnect;

	    return;

	} else {

            signal_unlock(signal);
    	    queue_ssh_payload(&setup->queue, payload);
    	    queue_ssh_broadcast(&setup->queue);
	    payload=NULL;

	}

    }

    if (payload) free_payload(&payload);
    return;

    disconnect:

    if (payload) free_payload(&payload);
    disconnect_ssh_connection(sshc);

}

static void receive_msg_queue_shared(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct shared_signal_s *signal=setup->signal;

    signal_lock(signal);

    if (setup->flags & SSH_SETUP_FLAG_DISCONNECT) {

        signal_unlock(signal);
	free_payload(&payload);
	payload=NULL;

    } else if (setup->status==SSH_SETUP_PHASE_TRANSPORT) {

	if (setup->phase.transport.status==SSH_TRANSPORT_TYPE_KEX) {

            signal_unlock(signal);
	    queue_ssh_payload(&setup->queue, payload);
	    queue_ssh_broadcast(&setup->queue);
	    payload=NULL;

	} else {

            signal_unlock(signal);

        }

    }

    if (payload) {

	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

static void receive_msg_newkeys(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    logoutput_debug("receive_msg_newkeys");
    receive_msg_queue_shared(connection, payload);
}

/*
    free_payload(&payload);
    payload=NULL;

    if (change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_NEWKEYS_S2C, 0, setup_cb_newkeys, NULL)==-1)
	disconnect_ssh_connection(connection);
*/


static void receive_msg_kexdh_reply(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    logoutput_debug("receive_msg_kexdh_reply");
    receive_msg_queue_shared(connection, payload);
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
    struct shared_signal_s *signal=connection->setup.signal;
    struct ssh_string_s name=SSH_STRING_INIT;

    if (read_ssh_string(&payload->buffer[1], payload->len - 1, &name) > 3) {

	logoutput("receive_msg_global_request: received request %.*s", name.len, name.ptr);

    } else {

	logoutput("receive_msg_global_request: received request, cannot read name");

    }

    signal_lock(signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

        signal_unlock(signal);
	free_payload(&payload);

    } else if (connection->setup.flags & SSH_SETUP_FLAG_SERVICE_CONNECTION) {
	struct payload_queue_s *queue = &connection->setup.queue;

        signal_unlock(signal);
	queue_ssh_payload(queue, payload);
	queue_ssh_broadcast(queue);
	payload=NULL;

    }

    if (payload) free_payload(&payload);

}

static void receive_msg_request_common(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct shared_signal_s *signal=connection->setup.signal;

    logoutput("receive_msg_request_common");

    signal_lock(signal);

    if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

        signal_unlock(signal);
	free_payload(&payload);

    } else if (connection->setup.flags & SSH_SETUP_FLAG_SERVICE_CONNECTION) {
	struct payload_queue_s *queue = &connection->setup.queue;

	/* queue it in the general channel independant queue */

        signal_unlock(signal);
	queue_ssh_payload(queue, payload);
	queue_ssh_broadcast(queue);
	payload=NULL;

    }

    connection->flags &= ~SSH_CONNECTION_FLAG_GLOBAL_REQUEST;

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
