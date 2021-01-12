/*
  2016, 2017, 2018 Stef Bon <stefbon@gmail.com>

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

#include "main.h"
#include "log.h"
#include "misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-hostinfo.h"
#include "ssh-utils.h"
#include "userauth/utils.h"

/*
    function to handle authentication based on host key
    see:
    https://tools.ietf.org/html/rfc4252#section-9
*/
    /*
	create a signature of

	- string		session identifier
	- byte			SSH_MSG_USERAUTH_REQUEST
	- string		username
	- string		service
	- string		"hostbased"
	- string 		algo
	- string		pubkey of this host
	- string		client hostname
	- string		username on this host

	using the private key
    */

static unsigned int msg_write_hb_signature(struct msg_buffer_s *mb, struct ssh_connection_s *connection, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    msg_write_ssh_string(mb, 's', (void *) &session->data.sessionid);
    msg_write_userauth_hostbased_request(mb, s_u, service, pkey, c_h, c_u);
    return mb->pos;
}

static signed char create_hb_signature(struct ssh_connection_s *connection, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u, struct ssh_key_s *skey, struct ssh_string_s *signature)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=msg_write_hb_signature(&mb, connection, s_u, service, pkey, c_h, c_u) + 64;
    unsigned char buffer[len];

    set_msg_buffer(&mb, (char *)buffer, len);
    len=msg_write_hb_signature(&mb, connection, s_u, service, pkey, c_h, c_u);

    logoutput("create_hb_signature: input data len %i hash %s", len, pkey->algo->hash);

    /* create a signature of this data using the private key belonging to the host key */

    if ((* skey->sign)(skey, buffer, len, signature, pkey->algo->hash)<0) {

	logoutput("create_hb_signature: error creating signature");
	return -1;

    }

    return 0;

}

static signed char verify_hb_signature(struct ssh_connection_s *connection, struct ssh_string_s *s_u, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_string_s *c_h, struct ssh_string_s *c_u, struct ssh_string_s *signature)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=msg_write_hb_signature(&mb, connection, s_u, service, pkey, c_h, c_u) + 64;
    unsigned char buffer[len];

    set_msg_buffer(&mb, (char *)buffer, len);
    len=msg_write_hb_signature(&mb, connection, s_u, service, pkey, c_h, c_u);

    logoutput("verify_hb_signature: len %i hash %s", len, pkey->algo->hash);

    /* create a signature of this data using the private key belonging to the host key */

    if ((* pkey->verify)(pkey, buffer, len, signature, pkey->algo->hash)<0) {

	logoutput("verify_hb_signature: error checking signature");
	return -1;

    }

    return 0;

}

static int ssh_send_hb_signature(struct ssh_connection_s *connection, struct ssh_string_s *service, struct ssh_key_s *pkey, struct ssh_key_s *skey)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    struct ssh_string_s signature=SSH_STRING_INIT;
    int result=-1;
    uint32_t seq=0;

    logoutput("ssh_send_hostbased_signature");

    if (create_hb_signature(connection, &auth->s_username, service, pkey, &auth->c_hostname, &auth->c_username, skey, &signature)==-1) {

	logoutput("ssh_send_hostbased_signature: creating public hostkey signature failed");
	goto out;

    }

    logoutput("ssh_send_hostbased_signature: created hash %i bytes", signature.len);

    /* send userauth hostbased request to server with signature */

    if (send_userauth_hostbased_message(connection, &auth->s_username, service, pkey, &auth->c_hostname, &auth->c_username, &signature, &seq)==0) {
	struct ssh_payload_s *payload=NULL;

	payload=receive_message_common(connection, select_userauth_reply, NULL, NULL);
	if (payload==NULL) goto out;

	if (payload->type == SSH_MSG_USERAUTH_SUCCESS) {

	    logoutput("ssh_send_hostbased_signature: success");
	    auth->required=0;
	    result=0;

	} else if (payload->type == SSH_MSG_USERAUTH_FAILURE) {

	    logoutput("ssh_send_hostbased_signature: failed");
	    result=handle_auth_failure(payload, auth);

	}

	free_payload(&payload);

    } else {

	logoutput("ssh_send_hostbased_signature: error sending SSH_MSG_SERVICE_REQUEST");

    }

    out:
    clear_ssh_string(&signature);
    return result;

}

/* perform hostbased authentication try every public hostkey found
    get the public hostkeys from the standard location
    is it known here which type to use?

    TODO: look for the hostkey in the desired format as negotiated in
    https://tools.ietf.org/html/rfc4253#section-7.1 Algorithm Negotiation
    try that first, if failed then try the remaining hostkeys
*/

struct pk_identity_s *send_userauth_hostbased_request(struct ssh_connection_s *connection, struct ssh_string_s *service, struct pk_list_s *pkeys)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_auth_s *auth=&setup->phase.service.type.auth;
    struct pk_identity_s *host_identity=NULL;

    logoutput("send_userauth_hostbased_request");

    host_identity=get_next_pk_identity(pkeys, "host");

    while (host_identity) {
	struct ssh_key_s pkey;
	struct ssh_key_s skey;
	int result=-1;

	init_ssh_key(&pkey, SSH_KEY_TYPE_PUBLIC, NULL);
	init_ssh_key(&skey, SSH_KEY_TYPE_PRIVATE, NULL);

	if (read_key_param(host_identity, &pkey)==-1) {

	    logoutput("send_userauth_hostbased_request: error reading public key");
	    goto next;

	}

	init_ssh_key(&skey, SSH_KEY_TYPE_PRIVATE, pkey.algo);

	if (read_key_param(host_identity, &skey)==-1) {

	    logoutput("send_userauth_hostbased_request: error reading private key");
	    goto next;

	}

	if (ssh_send_hb_signature(connection, service, &pkey, &skey)==0) {

	    logoutput("send_userauth_hostbased_request: server accepted hostkey");
	    result=0;

	}

	next:

	free_ssh_key(&pkey);
	free_ssh_key(&skey);
	if (result==0) break;
	free(host_identity);
	host_identity=get_next_pk_identity(pkeys, "host");

    }

    finish:
    return host_identity;

}

int respond_userauth_hostbased_request(struct ssh_connection_s *connection, struct ssh_string_s *s_username, struct ssh_string_s *service, struct ssh_string_s *data, struct timespec *expire)
{
    /*
	data contains:
	- string		algo name
	- string		key data for hostkey
	- string		client fqdn
	- string		username on client
	- string		signature
    */

    struct msg_buffer_s mb1=INIT_SSH_MSG_BUFFER;
    struct ssh_string_s algo=SSH_STRING_INIT;
    struct ssh_pkalgo_s *pkalgo=NULL;
    struct ssh_key_s pkey;
    struct ssh_string_s c_fqdn=SSH_STRING_INIT;
    struct ssh_string_s c_username=SSH_STRING_INIT;
    struct ssh_string_s signature=SSH_STRING_INIT;
    uint32_t seq=0;
    int result=-1;

    set_msg_buffer_string(&mb1, data);

    /* read, test algo exists and is ok to sign */

    msg_read_ssh_string(&mb1, &algo);
    pkalgo=get_pkalgo_string(&algo, NULL);
    if (pkalgo==NULL) goto out;
    if (test_algo_publickey(connection, pkalgo)<1) goto out;

    /* read key */

    init_ssh_key(&pkey, 0, pkalgo);
    msg_read_pkey(&mb1, &pkey, PK_DATA_FORMAT_SSH);
    msg_read_ssh_string(&mb1, &c_fqdn);
    msg_read_ssh_string(&mb1, &c_username);

    if (msg_read_pksignature(&mb1, NULL, &signature) != pkalgo) {

	logoutput("respond_userauth_hostbased_request: pk algo for signature differs from hostkey %s", pkalgo->sshname);
	goto out;

    }

    if (mb1.error>0) {

	logoutput("respond_userauth_hostbased_request: error reading request message");
	goto out;

    }

    /*
	1) here: additional checks about the fqdn of the client: check it by doing a dns lookup
	20201214: but how, which library to use
	glb has g_resolver_lookup_by_address ()
	2) check for the username 
    */

    /* test signature is ok */

    if (verify_hb_signature(connection, s_username, service, &pkey, &c_fqdn, &c_username, &signature)==0) {

	logoutput("respond_userauth_hostbased_request: signature verified");
	result=0;

    } else {

	logoutput("respond_userauth_hostbased_request: signature not ok");

    }

    out:
    return result;
}
