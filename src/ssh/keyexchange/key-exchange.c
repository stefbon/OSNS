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
#include "ssh-hash.h"
#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-keyexchange.h"
#include "ssh-data.h"

#include "key-exchange-client.h"

extern struct fs_options_s fs_options;

/* start the key agreement by sending public keys and calculation of the shared key
    although the name ends with dh, this also applies to ecdh */

int start_diffiehellman(struct ssh_connection_s *connection, struct ssh_keyex_s *k)
{
    struct ssh_setup_s *setup=&connection->setup;
    unsigned int hashlen=get_hash_size(k->digestname); /* get length of required buffer to store exchange hash H */
    char hashdata[sizeof(struct ssh_hash_s) + hashlen];
    struct ssh_hash_s *H=(struct ssh_hash_s *) hashdata;
    int result=-1;

    init_ssh_hash(H, k->digestname, hashlen);

    //if (k->flags & SSH_KEYEX_FLAG_SERVER) {

	// result=start_diffiehellman_server(connection, k, H);

    // } else {

	result=start_diffiehellman_client(connection, k, H);

    //}

    if (result==-1) {

	logoutput("start_diffiehellman: failed to do diffie-hellman");
	goto out;

    }

    /* store H as session identifier
	(only when transport phase is NOT completed and it's not a rekey) */

    if ((connection->flags & SSH_CONNECTION_FLAG_MAIN) && (setup->flags & SSH_SETUP_FLAG_TRANSPORT)==0) {
	struct ssh_string_s tmp=SSH_STRING_SET(H->len, (char *) H->digest);
	struct ssh_session_s *session=get_ssh_connection_session(connection);

	if (store_ssh_session_id(session, &tmp)==-1) {

	    logoutput_warning("start_diffiehellman: failed to store session identifier");
	    goto out;

	}

	logoutput("start_diffiehellman: stored session identifier (%i bytes)", H->len);

    } else {

	logoutput_warning("start_diffiehellman: not created a session identifier");

    }

    /* now the hostkey is found in some db, the signature is checked and the shared key K is computed,
	create the different hashes with it */

    if (create_keyx_hashes(connection, k, H)==0) {

	logoutput("start_diffiehellman: key hashes created");
	result=0;

    } else {

	logoutput("start_diffiehellman: failed to create key hashes");

    }

    out:
    (* k->ops->free)(k);
    return result;

}
