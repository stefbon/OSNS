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

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-hostinfo.h"
#include "ssh-utils.h"
#include "userauth/utils.h"

/*
    functions to handle authentication based on public key
    see:
    https://tools.ietf.org/html/rfc4252#section-7
*/

/* create a signature to do public key authentication

    create a signature of

    - string			session identifier
    - byte			SSH_MSG_USERAUTH_REQUEST
    - string			username
    - string			service
    - string			"publickey"
    - boolean			TRUE
    - string 			algo used to sign (defaults to the algo of pubkey)
    - string			pubkey

*/

static unsigned int msg_write_pk_signature(struct msg_buffer_s *mb, struct ssh_connection_s *connection, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *signature)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    msg_write_ssh_string(mb, 's', (void *) &session->data.sessionid);
    msg_write_userauth_pubkey_request(mb, s_username, service, pkey, signature);
    return mb->pos;
}

static signed char create_pk_signature(struct ssh_connection_s *connection, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_key_s *skey, struct ssh_string_s *signature)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len = msg_write_pk_signature(&mb, connection, s_username, service, pkey, signature) + 64;
    unsigned char buffer[len];

    set_msg_buffer(&mb, (char *)buffer, len);
    len = msg_write_pk_signature(&mb, connection, s_username, service, pkey, signature);

    logoutput("create_pk_signature: input data len %i hash %s", len, pkey->algo->hash);

    /* create a signature of this data using the private key */

    if ((* skey->sign)(skey, buffer, len, signature, pkey->algo->hash)<0) {

	logoutput("create_pk_signature: error creating signature");
	return -1;

    }

    return 0;
}

static signed char verify_pk_signature(struct ssh_connection_s *connection, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *signature)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len = msg_write_pk_signature(&mb, connection, s_username, service, pkey, signature) + 64;
    unsigned char buffer[len];

    set_msg_buffer(&mb, (char *)buffer, len);
    len = msg_write_pk_signature(&mb, connection, s_username, service, pkey, signature);

    logoutput("verify_pk_signature: input data len %i hash %s", len, pkey->algo->hash);

    /* verify a signature of this data using public key */

    if ((* pkey->verify)(pkey, buffer, len, signature, pkey->algo->hash)<0) {

	logoutput("verify_pk_signature: error checking signature");
	return -1;

    }

    return 0;
}

/* check the format of the SSH_MSG_USERAUTH_PK_OK message
    it must look exactly like constructed below
    - byte 			SSH_MSG_USERAUTH_PK_OK
    - string			public key algo name from request
    - string 			public key blob from request
*/

static int check_received_pubkey_pk(struct ssh_payload_s *payload, struct ssh_key_s *pkey)
{
    struct msg_buffer_s mb = INIT_SSH_MSG_BUFFER;
    unsigned int len = write_userauth_pubkey_ok_message(&mb, pkey) + 64;
    char buffer[len];
    int result=-1;

    set_msg_buffer(&mb, buffer, len);
    len=write_userauth_pubkey_ok_message(&mb, pkey);

    if (len==payload->len && memcmp(buffer, payload->buffer, len)==0) {

	logoutput("check_received_pubkey_pk: len %i payload len %i message the same", len, payload->len);
	result=0;

    } else {

	if (len==payload->len) {

	    logoutput("check_received_pubkey_pk: len %i payload len %i message differs", len, payload->len);

	} else {

	    logoutput("check_received_pubkey_pk: len %i payload len %i and/or message differs", len, payload->len);

	}

    }

    return result;

}

static int ssh_send_pk_signature(struct ssh_connection_s *connection, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_key_s *skey)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    struct ssh_string_s signature=SSH_STRING_INIT;
    int result=-1;
    uint32_t seq=0;

    if (create_pk_signature(connection, s_username, service, pkey, skey, &signature)==-1) {

	logoutput("ssh_send_pk_signature: creating public key signature failed");
	goto out;

    }

    /* send userauth publickey request to server with signature */

    if (send_userauth_pubkey_message(connection, s_username, service, pkey, &signature, &seq)>0) {
	struct ssh_payload_s *payload=NULL;

	payload=receive_message_common(connection, select_userauth_reply, NULL, NULL);
	if (payload==NULL) goto out;

	if (payload->type==SSH_MSG_USERAUTH_SUCCESS) {

	    auth->required=0;
	    result=0;

	} else if (payload->type==SSH_MSG_USERAUTH_FAILURE) {

	    result=handle_auth_failure(payload, auth);

	}

	free_payload(&payload);

    } else {

	logoutput("ssh_send_pk_signature: error sending SSH_MSG_SERVICE_REQUEST");

    }

    out:
    clear_ssh_string(&signature);
    return result;

}

static int select_userauth_pubkey_reply(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    if (payload->type == SSH_MSG_USERAUTH_PK_OK || payload->type == SSH_MSG_USERAUTH_FAILURE) return 0;
    return -1;
}

/* test pk algo and public key are accepted */

static int send_userauth_pubkey(struct ssh_connection_s *connection, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_key_s *pkey)
{
    unsigned int seq=0;
    int result=-1;

    logoutput("send_userauth_pubkey");

    if (send_userauth_pubkey_message(connection, s_username, service, pkey, NULL, &seq)>0) {
	struct ssh_payload_s *payload=NULL;

	payload=receive_message_common(connection, select_userauth_pubkey_reply, NULL, NULL);
	if (payload==NULL) goto out;

	if (payload->type==SSH_MSG_USERAUTH_PK_OK) {

	    logoutput("send_userauth_pubkey: public key accepted");

	    /*
		public key is accepted by server

		message has the form:
		- byte				SSH_MSG_USERAUTH_PK_OK
		- string			algo name
		- string			public key
	    */

	    /* check the received key is the same as the one send */

	    result=check_received_pubkey_pk(payload, pkey);

	} else if (payload->type==SSH_MSG_USERAUTH_FAILURE) {

	    logoutput("send_userauth_pubkey: pubkey rejected");

	} else {

	    logoutput("send_userauth_pubkey: reply %i not reckognized", payload->type);

	}

	free_payload(&payload);

    } else {

	logoutput("send_userauth_pubkey: error sending SSH_MSG_SERVICE_REQUEST");

    }

    out:
    return result;

}

/* perform pubkey authentication using identities */

struct pk_identity_s *send_userauth_pubkey_request(struct ssh_connection_s *connection, struct ssh_string_s *service, struct pk_list_s *pkeys)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    struct pk_identity_s *user_identity=NULL;
    unsigned int error=0;

    logoutput("send_userauth_pubkey_request");

    /* browse the public key identities for this user */

    user_identity=get_next_pk_identity(pkeys, "user");

    while (user_identity) {
	struct ssh_key_s pkey;
	struct ssh_pksign_s *pksign=NULL;
	char *s_user=NULL;
	struct ssh_string_s s_username=SSH_STRING_INIT;
	int result=-1;

	init_ssh_key(&pkey, SSH_KEY_TYPE_PUBLIC, NULL);

	if (read_key_param(user_identity, &pkey)==-1) {

	    logoutput("ssh_auth_pubkey: error reading public key");
	    goto next;

	}

	/* if there is a remote user with this identity take that one
	    otherwise fall back to the local user */

	s_user=get_pk_identity_user(user_identity);
	if (s_user==NULL) {
	    struct ssh_session_s *session=get_ssh_connection_session(connection);

	    s_user=session->identity.pwd.pw_name;

	}

	set_ssh_string(&s_username, 'c', s_user);

	if (send_userauth_pubkey(connection, &s_username, service, &pkey)==0) {
	    struct ssh_key_s skey;

	    /* confirm the signature method is accepted by server if not already */

	    /* current public key is accepted by server: send signature
		get the private key for this identity */

	    init_ssh_key(&skey, SSH_KEY_TYPE_PRIVATE, pkey.algo);

	    if (read_key_param(user_identity, &skey)==0) {

		result=ssh_send_pk_signature(connection, &s_username, service, &pkey, &skey);

	    } else {

		/*
		    what to do?
		    if private key is not found try next public key ?
		    or break..
		*/

		logoutput_info("ssh_auth_pubkey: private key not found");

	    }

	    free_ssh_key(&skey);

	}

	next:

	free_ssh_key(&pkey);
	if (result==0) break; /* if success then ready */

	free(user_identity);
	user_identity=get_next_pk_identity(pkeys, "user");

    }

    return user_identity;

}

int respond_userauth_publickey_request(struct ssh_connection_s *connection, struct ssh_string_s *username1, struct ssh_string_s *service1, struct ssh_string_s *data1, struct system_timespec_s *expire)
{
    /*
	data contains:
	- boolean==false
	- algo name
	- key data

	server responds the algo and key are acceptable for signing
    */

    struct msg_buffer_s mb1=INIT_SSH_MSG_BUFFER;
    unsigned char boolean1=0;
    struct ssh_string_s algo=SSH_STRING_INIT;
    struct ssh_pkalgo_s *pkalgo=NULL;
    struct ssh_key_s pkey;
    uint32_t seq=0;
    int result=-1;

    set_msg_buffer_string(&mb1, data1);
    msg_read_byte(&mb1, &boolean1);

    /* should be zero */

    if (boolean1>0) goto out;

    /* read, test algo exists and is ok to sign */

    msg_read_ssh_string(&mb1, &algo);
    pkalgo=get_pkalgo_string(&algo, NULL);
    if (pkalgo==NULL) goto out;
    if (test_algo_publickey(connection, pkalgo)<1) goto out;

    /* read key */

    init_ssh_key(&pkey, 0, pkalgo);
    msg_read_pkey(&mb1, &pkey, PK_DATA_FORMAT_SSH);
    if (mb1.error>0) goto out;

    if (send_userauth_pubkey_ok_message(connection, &pkey, &seq)>0) {
	struct ssh_payload_s *payload=NULL;

	/* wait for SSH_MSG_USERAUTH_REQUEST with signature */

	payload=get_ssh_payload(connection, &connection->setup.queue, expire, &seq, select_userauth_pubkey_reply, NULL, NULL);
	if (payload==NULL || payload->len<5) {

	    logoutput("respond_userauth_publickey_request: not received a service request message or message is too small");
	    if (payload) free_payload(&payload);
	    goto out;

	}

	if (payload->type==SSH_MSG_USERAUTH_REQUEST) {
	    struct msg_buffer_s mb2=INIT_SSH_MSG_BUFFER;
	    struct ssh_string_s username2=SSH_STRING_INIT;
	    struct ssh_string_s service2=SSH_STRING_INIT;
	    struct ssh_string_s method2=SSH_STRING_INIT;
	    struct ssh_string_s data2=SSH_STRING_INIT;
	    struct ssh_string_s signature=SSH_STRING_INIT;
	    unsigned char boolean2=0;

	    set_msg_buffer_payload(&mb2, payload);
	    msg_read_byte(&mb2, NULL);
	    msg_read_ssh_string(&mb2, &username2);
	    msg_read_ssh_string(&mb2, &service2);
	    msg_read_ssh_string(&mb2, &method2);

	    /* check for errors */

	    if (mb2.error>0) {

		logoutput("respond_userauth_publickey_request: error reading userauth request message");
		goto out;

	    }

	    /* username, service and methods should be the same as used in first message */

	    if (compare_ssh_string(&username2, 's', username1) != 0 || compare_ssh_string(&service2, 's', service1) != 0 || compare_ssh_string(&method2, 'c', "publickey") != 0) {

		logoutput("respond_userauth_publickey_request: username, service and/or method differ between the first and the second publickey userauth request");
		goto out;

	    }

	    msg_read_byte(&mb2, &boolean2);
	    if (boolean2==0) goto out; /* should be true */

	    /* read signature */

	    if (msg_read_pksignature(&mb2, NULL, &signature) != pkalgo) {

		logoutput("respond_userauth_publickey_request: pk algo from second public key userauth request differs from %s", pkalgo->sshname);
		goto out;

	    }

	    /* verify signature */

	    if (verify_pk_signature(connection, username1, service1, &pkey, &signature)==0) {

		logoutput("respond_userauth_publickey_request: signature verified");
		result=0;

	    } else {

		logoutput("respond_userauth_publickey_request: signature not ok");

	    }

	} else {

	    logoutput("respond_userauth_publickey_request: received unexpected message %i", payload->type);
	    free_payload(&payload);

	}

    } else {

	logoutput("respond_userauth_publickey_request: failed to send pubkey ok message");

    }

    out:
    return result;

}
