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
#include <sys/stat.h>

#include "log.h"
#include "main.h"

#include "misc.h"
#include "options.h"

#include "ssh-utils.h"
#include "ssh-hash.h"
#include "ssh-common-protocol.h"
#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-send.h"
#include "ssh-keyexchange.h"
#include "ssh-data.h"

extern struct fs_options_s fs_options;

static unsigned int write_kexdh_init_message(struct msg_buffer_s *mb, struct ssh_keyex_s *k)
{
    msg_write_byte(mb, SSH_MSG_KEXDH_INIT);
    (* k->ops->msg_write_local_key)(mb, k);
    return mb->pos;
}

static int send_kexdh_init_message(struct ssh_connection_s *connection, struct ssh_keyex_s *k)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=write_kexdh_init_message(&mb, k) + 64;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    unsigned int seq=0;

    init_ssh_payload(payload, len);
    set_msg_buffer_payload(&mb, payload);
    payload->len=write_kexdh_init_message(&mb, k);
    return write_ssh_packet(connection, payload, &seq);
}

static unsigned int write_kexdh_reply_message(struct msg_buffer_s *mb, struct ssh_keyex_s *k, struct ssh_key_s *pkey, struct ssh_string_s *signature)
{
    msg_write_byte(mb, SSH_MSG_KEXDH_REPLY);
    (* pkey->msg_write_key)(mb, pkey, PK_DATA_FORMAT_SSH);
    (* k->ops->msg_write_remote_key)(mb, k);
    msg_write_pksignature(mb, pkey->algo, signature);
    return mb->pos;
}

static int send_kexdh_reply_message(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct ssh_key_s *pkey, struct ssh_string_s *signature)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;
    unsigned int len=write_kexdh_reply_message(&mb, k, pkey, signature) + 64;
    char buffer[sizeof(struct ssh_payload_s) + len];
    struct ssh_payload_s *payload=(struct ssh_payload_s *) buffer;
    unsigned int seq=0;

    init_ssh_payload(payload, len);
    set_msg_buffer_payload(&mb, payload);
    payload->len=write_kexdh_reply_message(&mb, k, pkey, signature);
    return write_ssh_packet(connection, payload, &seq);
}

static int create_signature_H(struct ssh_key_s *pkey, struct ssh_hash_s *H, char *hash, struct ssh_string_s *signature)
{
    return -1;
}

static int read_kex_dh_reply(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct ssh_payload_s *payload,
				struct ssh_hostkey_s *hostkey, struct ssh_signature_s *signature)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;

    set_msg_buffer_payload(&mb, payload);

    /*
	message has following form:
	byte 	SSH_MSG_KEXDH_REPLY
	string	server public host key
	mpint	f
	string	signature of H

	rules:
	- algo of server public hostkey and the signature are the same as negotiated during keyinit
    */

    msg_read_byte(&mb, NULL);

    /* read public key or certificate */

    msg_read_ssh_string(&mb, &hostkey->buffer);

    /* read f */

    (* k->ops->msg_read_remote_key)(&mb, k); /* f or Q_S or .. store value in keyx.method.dh or .. */

    /*

    check the received data containing the signature has the right format
    it has the following format (as used by ssh):

    - string			data containing signature of H

    this string is:

    - uint32			len of rest (n)
    - byte[n]			which is a combination of two strings:
	- string 		name of pk algo used to sign like "ssh-rsa" and "ssh-dss" and "rsa-sha2-256" and "rsa-sha2-512"
	- string		signature blob

    see:

    RFC4253

	6.6.  Public Key Algorithms
	and
	8.  Diffie-Hellman Key Exchange

    and

    draft-rsa-dsa-sha2-256

    */

    signature->pkalgo=msg_read_pksignature(&mb, NULL, &signature->data);

    if (mb.error>0) {

	logoutput("read_keyx_dh_reply: reading MSG_KEXDH_REPLY failed: error %i:%s", mb.error, strerror(mb.error));
	return -1;

    }

    return 0;

}

static int read_kex_dh_init(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct ssh_payload_s *payload)
{
    struct msg_buffer_s mb=INIT_SSH_MSG_BUFFER;

    set_msg_buffer_payload(&mb, payload);

    /*
	message has following form:
	byte 	SSH_MSG_KEXDH_INIT
	mpint	f

    */

    msg_read_byte(&mb, NULL);

    /* read f */

    (* k->ops->msg_read_remote_key)(&mb, k); /* f or Q_S or .. store value in keyx.method.dh or .. */

    if (mb.error>0) {

	logoutput("read_keyx_dh_reply: reading MSG_KEXDH_INIT failed: error %i:%s", mb.error, strerror(mb.error));
	return -1;

    }

    return 0;

}

static int select_payload_kexdh(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    struct ssh_keyex_s *k=(struct ssh_keyex_s *) ptr;
    unsigned char type=(k->flags & SSH_KEYEX_FLAG_SERVER) ? SSH_MSG_KEXDH_INIT : SSH_MSG_KEXDH_REPLY;
    return ((payload->type==type) ? 0 : -1);
}

static struct ssh_payload_s *receive_keydh(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct generic_error_s *error)
{
    struct payload_queue_s *queue=&connection->setup.queue;
    struct ssh_payload_s *payload=NULL;
    struct timespec expire;
    unsigned int sequence=0;

    /* wait for SSH_MSG_KEXDH_REPLY when client or KEXDH_INIT when server */

    get_ssh_connection_expire_init(connection, &expire);

    getkexdhreply:
    payload=get_ssh_payload(connection, queue, &expire, &sequence, select_payload_kexdh, k, error);
    if (payload==NULL) logoutput("start_keyexchange: error waiting for KEXDH INIT/REPLY (%s)", get_error_description(error));

    out:
    return payload;

}

static int check_serverkey_localdb(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct ssh_key_s *pkey)
{
    unsigned int done = fs_options.ssh.trustdb;
    int result=-1;

    if (fs_options.ssh.trustdb & _OPTIONS_SSH_TRUSTDB_OPENSSH) {
	struct ssh_session_s *session=get_ssh_connection_session(connection);

	done-=_OPTIONS_SSH_TRUSTDB_OPENSSH;

	if (check_serverkey_localdb_openssh(&connection->connection, &session->identity.pwd, pkey, (pkey->algo->flags & SSH_PKALGO_FLAG_PKC) ? "ca" : "pk")==0) {

	    logoutput("check_serverkey_localdb: check public key server success");
	    result=0;

	} else {

	    logoutput("check_serverkey_localdb: check public key server failed");
	    goto out;

	}

	/* store fp of servers hostkey */

	/* encode/decode first ?? */

	// logoutput("start_kex_dh: creating fp server public hostkey");

	// if (create_ssh_string(&hostinfo->fp, create_hash("sha1", NULL, 0, NULL, &error))>0) {

	    // if (create_hash("sha1", keydata.ptr, keydata.len, &hostinfo->fp, &error)>0) {

		// logoutput("start_kex_dh: servers fp %.*s (len=%i)", hostinfo->fp.len, hostinfo->fp.ptr, hostinfo->fp.len);

	    // }

	//}

    }

    out:
    if (done>0) logoutput_warning("check_serverkey_localdb: not all trustdbs %i supported", done);
    return result;

}

static int check_received_server_hostkey(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct ssh_hostkey_s *hostkey)
{
    char *pos=hostkey->buffer.ptr;
    int left=(int) hostkey->buffer.len;
    struct ssh_pkalgo_s *algo=NULL;
    int nread=0;
    int result=-1;

    algo=read_pkalgo(pos, left, &nread);

    if (algo) {
	unsigned int error=0;

	pos+=nread;
	left-=nread;

	/* check it's the same as agreed in algo exchange
	    they do not have to be exactly the same: the hostkey may begin with "ssh-rsa" for example, and the algo agreed in kexinit "rsa-sha2-512" */

	if (!(k->algo==algo) && !(k->algo->scheme==algo->scheme && strcmp(k->algo->sshname, algo->name)==0)) {

	    logoutput("check_received_server_hostkey: error, algo found in hostkey %i:%s/%s not the same as agreed in KEXINIT %i:%s/%s", 
			algo->scheme, algo->name, ((algo->sshname) ? algo->sshname : "-"), k->algo->scheme, k->algo->name, ((k->algo->sshname) ? k->algo->sshname : "-"));
	    goto out;

	}

	if (algo->flags & SSH_PKALGO_FLAG_PKA) {

	    hostkey->pkey=&hostkey->data.key;
	    init_ssh_key(hostkey->pkey, 0, algo);

	    switch (algo->scheme) {

		case SSH_PKALGO_SCHEME_RSA:

		    result=read_pkey_rsa(hostkey->pkey, pos, left, PK_DATA_FORMAT_PARAM, &error);
		    break;

		case SSH_PKALGO_SCHEME_DSS:

		    result=read_pkey_dss(hostkey->pkey, pos, left, PK_DATA_FORMAT_PARAM, &error);
		    break;

		case SSH_PKALGO_SCHEME_ECC:

		    result=read_pkey_ecc(hostkey->pkey, pos, left, PK_DATA_FORMAT_PARAM, &error);
		    break;

		default:

		    logoutput("check_received_server_hostkey: error, algo %i not reckognized", algo->scheme);

	    }

	} else if (algo->flags & SSH_PKALGO_FLAG_PKC) {

	    /* a certificate like "ssh-rsa-cert-v01@openssh.com "*/

	    if (algo->flags & SSH_PKALGO_FLAG_OPENSSH_COM_CERTIFICATE) {
		struct ssh_session_s *session=get_ssh_connection_session(connection);
		struct ssh_string_s keydata=SSH_STRING_SET(left, pos);

		hostkey->pkey=&hostkey->data.openssh_cert.key;
		init_ssh_cert_openssh_com(&hostkey->data.openssh_cert, algo);

		if (read_cert_openssh_com(&hostkey->data.openssh_cert, &keydata)==-1) {

		    logoutput("check_received_server_hostkey: failed to read openssh.com certificate");
		    goto out;

		}

		/* check certificate is complete and a host certificate, and verify the signature */

		result=check_cert_openssh_com(&hostkey->data.openssh_cert, "host");
		if (result==-1) logoutput("check_received_server_hostkey: certificate not valid");

	    } else {

		logoutput("check_received_server_hostkey: error, certificate %s not supported", algo->name);

	    }

	}

    }

    out:
    return result;
}

int start_diffiehellman_client(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct ssh_hash_s *H)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
    struct ssh_payload_s *payload=NULL;
    struct ssh_signature_s signature;
    struct ssh_hostkey_s hostkey;
    struct ssh_key_s *pkey=NULL;
    struct generic_error_s error=GENERIC_ERROR_INIT;
    int result=-1;

    logoutput("start_diffiehellman_client");

    memset(&hostkey, 0, sizeof(struct ssh_hostkey_s));
    memset(&signature, 0, sizeof(struct ssh_signature_s));

    /* calculate the local key e/Q_C/f/Q_S */

    if ((* k->ops->create_local_key)(k)==-1) {

	logoutput("start_diffiehellman_client: creating kex dh client key failed");
	goto out;

    }

    logoutput("start_diffiehellman_client: kex local key created");

    /* client: send SSH_MSG_KEXDH_INIT message */

    if (send_kexdh_init_message(connection, k)==-1) {

	logoutput("start_diffiehellman_client: error %s sending kex dh init", get_error_description(&error));
	goto out;

    }

    change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXDH_C2S, 0, NULL, NULL);

    /* wait for SSH_MSG_KEXDH_REPLY from server */

    payload=receive_keydh(connection, k, &error);
    if (payload==NULL) {

	logoutput("start_diffiehellman_client: error receiving KEXDH REPLY (%s)", get_error_description(&error));
	goto out;

    }

    change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXDH_S2C, 0, NULL, NULL);

    /* read server hostkey (keydata), the server public keyexchange value f/Q_S, the name of the pk algo used to sign and the signature */

    if (read_kex_dh_reply(connection, k, payload, &hostkey, &signature)==-1) {

	logoutput("start_diffiehellman_client: error reading dh reply");
	goto out;

    }

    logoutput("start_diffiehellman_client: read hostkey (len %i), signature algo name %s and data len %i", hostkey.buffer.len, signature.pkalgo->name, signature.data.len);

    if (check_received_server_hostkey(connection, k, &hostkey)==-1 || hostkey.pkey==NULL) {

	logoutput("start_diffiehellman_client: failed to read received server hostkey");
	goto out;

    }

    pkey=hostkey.pkey;

    /* check the received public hostkey (against a "known hosts file" etcetera)
	TODO: make a check using another local db/agent possible 
	TODO: make a check of certificates possible (look for a CA) */

    if (fs_options.ssh.trustdb == _OPTIONS_SSH_TRUSTDB_NONE) {

	logoutput_info("start_diffiehellman_client: no trustdb used, hostkey is not checked to local db of trusted keys");

    } else {

	if (check_serverkey_localdb(connection, k, pkey)==-1) goto out;

    }

    /* calculate the shared K from the client keyx key (e/Q_C/..) and the server keyx key (f/Q_S/..)*/

    if ((* k->ops->calc_sharedkey)(k)==-1) {

	logoutput("start_diffiehellman_client: calculation shared key K failed");
	goto out;

    }

    /* check the signature is correct by creating the H self
	and verify using the public key of the server */

    if (create_H(connection, k, pkey, H)==-1) {

	logoutput("start_diffiehellman_client: error creating H (len %i size %i)", H->len, H->size);
	goto out;

    } else {

	logoutput("start_diffiehellman_client: created H (%i size %i)", H->len, H->size);

    }

    /* compare the signature with the one created here */

    if ((* pkey->verify)(pkey, H->digest, H->len, &signature.data, k->algo->hash)==-1) {

	logoutput("start_diffiehellman_client: verify signature H failed (hash: %s algo %s)", k->algo->hash, pkey->algo->sshname);
	goto out;

    } else {

	logoutput("start_diffiehellman_client: signature H verified (hash %s algo %s)", k->algo->hash, pkey->algo->sshname);
	result=0;

    }

    out:

    if (payload) free_payload(&payload);
    if (pkey) free_ssh_key(pkey);
    return result;

}

/* start the key agreement by sending public keys and calculation of the shared key
    although the name ends with dh, this also applies to ecdh */

int start_diffiehellman_server(struct ssh_connection_s *connection, struct ssh_keyex_s *k, struct ssh_hash_s *H)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct ssh_payload_s *payload=NULL;
    struct ssh_string_s signature = SSH_STRING_INIT; /* signature */
    struct ssh_key_s *pkey=NULL;
    int result=-1;

    logoutput("start_diffiehellman_server");

    /* TODO:
	- get pkey from kexinit -> read from system location (Linux: for example /etc/ssh/ssh_host_rsa.key.pub)
	 and the algo is supported in this session
	 so walk through every host key found, look in config one is preferred for this domain cq client
    */

    /* calculate the local key e/Q_C/f/Q_S */

    if ((* k->ops->create_local_key)(k)==-1) {

	logoutput("start_diffiehellman_server: creating kex dh client key failed");
	goto out;

    }

    /* wait for SSH_MSG_KEXDH_REPLY from server */

    payload=receive_keydh(connection, k, &error);

    if (payload==NULL) {

	logoutput("start_diffiehellman_server: error receiving KEXDH REPLY (%s)", get_error_description(&error));
	goto out;

    }

    change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXDH_C2S, 0, NULL, NULL);

    /* read client value e/Q_C */

    if (read_kex_dh_init(connection, k, payload)==-1) {

	logoutput("start_kex_dh: error reading dh init");
	goto out;

    }

    /* calculate the shared K from the client keyx key (e/Q_C/..) and the server keyx key (f/Q_S/..)*/

    if ((* k->ops->calc_sharedkey)(k)==-1) {

	logoutput("start_diffiehellman_server: calculation shared key K failed");
	goto out;

    }

    /* create the H */

    if (create_H(connection, k, pkey, H)==-1) {

	logoutput("start_diffiehellman_server: error creating H (len %i size %i)", H->len, H->size);
	goto out;

    } else {

	logoutput("start_diffiehellman_server: created H (%i size %i)", H->len, H->size);

    }

    /* this creates a signature on H, which method?
	where to get the signmethod from (=hostkey algo?) and which sign method (the default one?) 
	get the skey to do this */

    if (create_signature_H(pkey, H, k->algo->hash, &signature)==-1) {

	logoutput("start_diffiehellman_server: create signature H failed (hash: %s algo %s)", k->algo->hash, pkey->algo->sshname);
	goto out;

    } else {

	logoutput("start_diffiehellman_server: signature on H created (hash: %s algo %s)", k->algo->hash, pkey->algo->sshname);

    }

    if (send_kexdh_reply_message(connection, k, pkey, &signature)==-1) {

	logoutput("start_diffiehellman_server: error sending kex dh reply");
	goto out;

    }

    change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXDH_S2C, 0, NULL, NULL);
    result=0;

    out:

    if (payload) free_payload(&payload);
    if (pkey) free_ssh_key(pkey);
    return result;

}

/* start the key agreement by sending public keys and calculation of the shared key
    although the name ends with dh, this also applies to ecdh */

int start_diffiehellman(struct ssh_connection_s *connection, struct ssh_keyex_s *k)
{
    struct ssh_setup_s *setup=&connection->setup;
    unsigned int hashlen=get_hash_size(k->digestname); /* get length of required buffer to store hash */
    char hashdata[sizeof(struct ssh_hash_s) + hashlen];
    struct ssh_hash_s *H=(struct ssh_hash_s *) hashdata;
    int result=-1;

    init_ssh_hash(H, k->digestname, hashlen);

    if (k->flags & SSH_KEYEX_FLAG_SERVER) {

	result=start_diffiehellman_server(connection, k, H);

    } else {

	result=start_diffiehellman_client(connection, k, H);

    }

    if (result==-1) {

	logoutput("start_diffiehellman: failed to do diffie-hellman");
	goto out;

    }

    /* store H as session identifier (only when transport phase is NOT completed) */

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
