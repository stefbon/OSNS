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
#include "libosns-eventloop.h"
#include "libosns-threads.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-utils.h"

static int start_ssh_userauth_request(struct ssh_connection_s *connection, struct ssh_string_s *username, struct ssh_string_s *service, struct ssh_string_s *method, struct ssh_string_s *data)
{

    if (compare_ssh_string(service, 'c', "ssh-userauth")==0) {

	/* client requests ssh userauth, some checks:
	    - in setup transport phase (once)
	    - not already activated
	    TODO...*/

	/* this is a failure, already doing ssh-userauth
	    it should be ssh-connection etc. */

    } else if (compare_ssh_string(service, 'c', "ssh-connection")==0) {

	/* 	- protection against too much requests from client
		- only in transport/setup phase
		- not if already done
	*/

	/* method=="none":
	    - return a list of required methods
	    - only when no authentication is used/needed return SSH_MSG_USERAUTH_SUCCESS, otherwise SSH_MSG_USERAUTH_FAILURE

	    see: https://tools.ietf.org/html/rfc4252#section-5.2
	*/

	/* method=="password":
	    - data contains a ssh string with password: is it the right one?
	    - return a list of required methods
	    see: https://tools.ietf.org/html/rfc4252#section-8
	*/

	/* method=="publickey":
	    - data contains algo name as ssh string and the key as ssh string 
	    - server responds with SSH_MSG_USERAUTH_FAILURE when the algo and key is not supported
	    or SSH_MSG_USERAUTH_PK_OK when accepted
	    - clients sends in last case the same message again, but then with signature 
	    - server checks the key is acceptable for authentication, and if so it checks the signature
	    see: https://tools.ietf.org/html/rfc4252#section-7
	*/

	/* method=="hostbased":
	    - data contains algo name of hostkey and hostkey self, client fqdn, username on client and signature
	    see: https://tools.ietf.org/html/rfc4252#section-9
	*/

    }

    return 0;

}

/*
    handlers for receiving ssh messages:

    - SSH_MSG_USERAUTH_REQUEST
    - SSH_MSG_USERAUTH_FAILURE
    - SSH_MSG_USERAUTH_SUCCESS
    - SSH_MSG_USERAUTH_BANNER

    - SSH_MSG_USERAUTH_PK_OK
    - SSH_MSG_USERAUTH_PASSWD_CHANGEREQ
    - SSH_MSG_USERAUTH_INFO_REQUEST
    - SSH_MSG_USERAUTH_INFO_RESPONSE

*/

/*	message looks like:

	- byte				SSH_MSG_USERAUTH_REQUEST
	- string			username (==the connecting user)
	- string			service like ssh-connection (and custom ...)
	- string			method name (like "publickey", "hostbased", "none")
	- method specific fields
*/

static void receive_msg_userauth_request(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_setup_s *setup=&connection->setup;

    signal_lock(setup->signal);

    if ((session->flags & SSH_SESSION_FLAG_SERVER)==0) {

	/* when not server (=client) receiving this is an error */
	logoutput("receive_msg_userauth_request: receiving while not server is an error");

    } else if (setup->flags & SSH_SETUP_FLAG_DISCONNECT) {

	signal_unlock(setup->signal);
	free_payload(&payload);
	return;

    } else if ((setup->flags & SSH_SETUP_FLAG_TRANSPORT) && setup->status==SSH_SETUP_PHASE_SERVICE && setup->phase.service.status==SSH_SERVICE_TYPE_AUTH) {

	/* message must be at least 13 bytes long:
	    1 byte + len(string username) + len(string service) + len(string method) = 1 + 4 + 4 + 4 = 13
	    this is a bare minimum
	*/

	if (payload->len>13) {

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

static void receive_msg_userauth_result_common(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_setup_s *setup=&connection->setup;

    signal_lock(setup->signal);

    if (setup->flags & SSH_SETUP_FLAG_DISCONNECT) {

	signal_unlock(setup->signal);
	free_payload(&payload);

    } else if ((setup->flags & SSH_SETUP_FLAG_TRANSPORT) && setup->status==SSH_SETUP_PHASE_SERVICE && setup->phase.service.status==SSH_SERVICE_TYPE_AUTH) {

	queue_ssh_payload_locked(&setup->queue, payload);
	payload=NULL;

    }

    signal_unlock(setup->signal);

    if (payload) {

	free_payload(&payload);
	disconnect_ssh_connection(connection);

    }

}

static void receive_msg_userauth_failure(struct ssh_connection_s *c, struct ssh_payload_s *payload)
{
    receive_msg_userauth_result_common(c, payload);
}

static void receive_msg_userauth_success(struct ssh_connection_s *c, struct ssh_payload_s *payload)
{
    receive_msg_userauth_result_common(c, payload);
}

    /*
	after receiving this reply "the client MAY then send a signature generated using the private key."
	(RFC4252 7.  Public Key Authentication Method: "publickey")
	so the client can leave it here ??
    */

    /*
	message has the form:
	- byte			SSH_MSG_USERAUTH_PK_OK
	- string		algo name
	- string		public key

	the same code is also used for SSH_MSG_USERAUTH_PASSWD_CHANGEREQ and SSH_MSG_USERAUTH_INFO_REQUEST
	these are used for password and keyboard-interactive but those are not used here (2018-06-02)
    */



static void receive_msg_userauth_commonreply(struct ssh_connection_s *c, struct ssh_payload_s *payload)
{
    receive_msg_userauth_result_common(c, payload);
}

/* banner message
    see: https://tools.ietf.org/html/rfc4252#section-5.4 Banner Message
    This software is running in background, so the message cannot be displayed on screen...
    log it anyway (ignore message)

    message looks like:
    - byte			SSH_MSG_USERAUTH_BANNER
    - string			message in ISO-10646 UTF-8 encoding
    - string			language tag
    */

static void receive_msg_userauth_banner(struct ssh_connection_s *c, struct ssh_payload_s *payload)
{

    if (payload->len>9) {
	unsigned int len=get_uint32(&payload->buffer[1]);

	if (payload->len>=9+len) {
	    char banner[len+1];

	    memcpy(banner, &payload->buffer[5], len);
	    banner[len]='\0';

	    logoutput("receive_msg_userauth_banner: received banner %s", banner);

	}

    }

    free_payload(&payload);

}

void register_userauth_cb(struct ssh_connection_s *c, unsigned char enable)
{

    if (enable) {

	register_msg_cb(c, SSH_MSG_USERAUTH_REQUEST, receive_msg_userauth_request);
	register_msg_cb(c, SSH_MSG_USERAUTH_FAILURE, receive_msg_userauth_failure);
	register_msg_cb(c, SSH_MSG_USERAUTH_SUCCESS, receive_msg_userauth_success);
	register_msg_cb(c, SSH_MSG_USERAUTH_BANNER, receive_msg_userauth_banner);

	/* this cb is also used for SSH_MSG_USERAUTH_PASSWD_CHANGEREQ and SSH_MSG_USERAUTH_INFO_REQUEST
	these do depend on the context they are used */

	register_msg_cb(c, SSH_MSG_USERAUTH_PK_OK, receive_msg_userauth_commonreply);

    } else {

	for (int i=50; i<=79; i++) register_msg_cb(c, i, msg_not_supported);

    }

}
