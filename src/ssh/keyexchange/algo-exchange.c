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
#include "libosns-misc.h"

#include "datatypes/ssh-string.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"

#include "ssh-data.h"
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh-receive.h"
#include "compare.h"

static int _store_kexinit_common(struct ssh_string_s *kexinit, struct ssh_payload_s *payload, struct generic_error_s *error)
{

    if (create_ssh_string(&kexinit, payload->len, payload->buffer, SSH_STRING_FLAG_ALLOC)==NULL) {

	set_generic_error_system(error, ENOMEM, __PRETTY_FUNCTION__);
	kexinit->len=0;
	return -1;

    }

    return 0;

}

int store_remote_kexinit(struct ssh_keyexchange_s *kex, struct ssh_payload_s *payload, unsigned int server, struct generic_error_s *error)
{

    /* if server the remote side is client, otherwise server */

    struct ssh_string_s *kexinit=((server) ? &kex->kexinit_client : &kex->kexinit_server);
    return _store_kexinit_common(kexinit, payload, error);
}

int store_local_kexinit(struct ssh_keyexchange_s *kex, struct ssh_payload_s *payload, unsigned int server, struct generic_error_s *error)
{

    /* if server the local side is server, otherwise client */

    struct ssh_string_s *kexinit=((server) ? &kex->kexinit_server : &kex->kexinit_client);
    return _store_kexinit_common(kexinit, payload, error);
}

void free_kexinit_server(struct ssh_keyexchange_s *kex)
{
    struct ssh_string_s *kexinit=&kex->kexinit_server;
    clear_ssh_string(kexinit);
}

void free_kexinit_client(struct ssh_keyexchange_s *kex)
{
    struct ssh_string_s *kexinit=&kex->kexinit_client;
    clear_ssh_string(kexinit);
}

static int setup_cb_receive_kexinit(struct ssh_connection_s *connection, void *data)
{
    set_ssh_receive_behaviour(connection, "kexinit");
    return 0;
}

static int handle_kexinit_reply(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    return (payload->type==SSH_MSG_KEXINIT) ? 0 : -1;
}

/* get the SSH_MSG_KEXINIT message from server */

int start_algo_exchange(struct ssh_connection_s *connection)
{
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
    struct algo_list_s *algos=kex->algos;
    struct ssh_payload_s *payload=NULL;
    struct generic_error_s error=GENERIC_ERROR_INIT;
    int result=-1;
    int index=0;

    /* send kexinit and wait for server to reply */

    logoutput("start_algo_exchange: send kexinit");

    if (send_kexinit_message(connection)==-1) {

	logoutput("start_algo_exchange: failed sending kexinit packet");
	goto out;

    }

    payload=receive_message_common(connection, handle_kexinit_reply, NULL, &error);

    if (payload) {
	struct ssh_session_s *session=get_ssh_connection_session(connection);

	/* copy the payload for the computation of the H (RFC4253 8.  Diffie-Hellman Key Exchange) */

	if (store_remote_kexinit(kex, payload, (session->flags & SSH_SESSION_FLAG_SERVER), &error)==0) {

	    result=change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXINIT_S2C, 0, setup_cb_receive_kexinit, NULL);

	} else {

	    logoutput("start_algo_exchange: error saving kexinit s2c message (%s)", get_error_description(&error));

	}

    } else {

	logoutput("start_algo_exchange: failed receiving kexinit packet");
	goto out;

    }

    /*
	    The default behaviour is that after the newkeys message the client
	    and the server use the algo's which are selected:
	    an algo for encryption c2s, an algo for mac c2s, and an algo for compression c2s
	    (and vice versa for s2c)

	    Sometimes the name for the cipher is not only a cipher, but also
	    a mac. then it's a cipher and mac combined.
	    like:

	    - chacha20-poly1305@openssh.com

	    in these cases the selected mac algo (which may also be "none") is ignored

	    See:

	    https://tools.ietf.org/html/draft-josefsson-ssh-chacha20-poly1305-openssh-00

	    Here the name of the mac algo is ignored according to the draft, and set to the same name
	    (in this case thus chacha20-poly1305@openssh.com)
	    to match the very custom/not-default behaviour

	    Although this combined cipher/mac has a different behaviour compared to the default algo's
	    here is tried to make the processing of messages (incoming and outgoing) simple and
	    without too much exceptions
    */

    /* compare the different suggested algo's */

    if (compare_msg_kexinit(connection)==-1) {

	logoutput("start_algo_exchange: compare msg kexinit failed");
	goto out;

    }

    /* correct mac names for combined cipher/mac algo's */

    index=kex->chosen[SSH_ALGO_TYPE_CIPHER_C2S];

    if (strcmp(algos[index].sshname, "chacha20-poly1305@openssh.com")==0) {

	/* disable the mac */
	kex->chosen[SSH_ALGO_TYPE_HMAC_C2S]=-1;

    } else {
	unsigned int index2=kex->chosen[SSH_ALGO_TYPE_HMAC_C2S];

	if (algos[index].ptr != algos[index2].ptr) {

	    logoutput("start_algo_exchange: internal error finding common methods");
	    goto out;

	}

    }

    index=kex->chosen[SSH_ALGO_TYPE_CIPHER_S2C];

    if (strcmp(algos[index].sshname, "chacha20-poly1305@openssh.com")==0) {

	/* disable the mac */
	kex->chosen[SSH_ALGO_TYPE_HMAC_S2C]=-1;

    } else {
	unsigned int index2=kex->chosen[SSH_ALGO_TYPE_HMAC_S2C];

	if (algos[index].ptr != algos[index2].ptr) {

	    logoutput("start_algo_exchange: internal error finding common methods");
	    goto out;

	}

    }

    return 0;

    out:
    return result;

}
