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
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh/receive/payload.h"

static int queue_sender_default(struct ssh_send_s *send, struct ssh_sender_s *sender, unsigned int *error)
{

    /* add at tail of senders list default: more senders are allowed */

    pthread_mutex_lock(&send->mutex);
    add_list_element_last(&send->senders, &sender->list);
    sender->sequence=send->sequence_number;
    send->sequence_number++;
    pthread_cond_broadcast(&send->cond);
    pthread_mutex_unlock(&send->mutex);

    return 0;
}

static int queue_sender_serial(struct ssh_send_s *send, struct ssh_sender_s *sender, unsigned int *error)
{
    int success=0;

    /* add at tail of senders list serialized: only one sender is allowed */

    pthread_mutex_lock(&send->mutex);

    while (send->senders.count>0) {

	int result=pthread_cond_wait(&send->cond, &send->mutex);

	if (send->senders.count==0) {

	    break;

	} else if (result>0) {

	    *error=result;
	    success=-1;
	    goto out;

	}

    }

    add_list_element_last(&send->senders, &sender->list);
    sender->sequence=send->sequence_number;
    send->sequence_number++;

    out:

    pthread_cond_broadcast(&send->cond);
    pthread_mutex_unlock(&send->mutex);
    return success;
}

static int queue_sender_disconnected(struct ssh_send_s *send, struct ssh_sender_s *sender, unsigned int *error)
{
    *error=ENOTCONN;
    return -1;
}

void set_ssh_send_behaviour(struct ssh_connection_s *connection, const char *what)
{
    struct ssh_send_s *send=&connection->send;

    pthread_mutex_lock(&send->mutex);

    if (strcmp(what, "default")==0 || strcmp(what, "newkeys")==0 ) {

	send->queue_sender=queue_sender_default;
	if (send->flags & SSH_SEND_FLAG_KEXINIT) send->flags -= SSH_SEND_FLAG_KEXINIT;
	send->flags|=SSH_SEND_FLAG_NEWKEYS;

    } else if (strcmp(what, "kexinit")==0) {

	send->queue_sender=queue_sender_serial;
	if (send->flags & SSH_SEND_FLAG_NEWKEYS) send->flags -= SSH_SEND_FLAG_NEWKEYS;
	send->flags|=SSH_SEND_FLAG_KEXINIT;

    } else if (strcmp(what, "disconnect")==0) {

	send->queue_sender=queue_sender_disconnected;
	send->flags|=SSH_SEND_FLAG_DISCONNECT;

    }

    pthread_mutex_unlock(&send->mutex);

}

/*
	create a complete ssh packet (RFC4253)
	global a packet looks like:
	- uint32	packet_length 			length of packet in bytes, not including 'mac' and the field packet_length self
	- byte		padding_length			length of the padding in bytes
	- byte[n1]	payload0			n1 = packet_length - padding_length - 1
	- byte[n2]	padding				n2 = padding_length
	- byte[m]	mac				m = mac_length

	extra: size(uint32) + 1 + n1 + n2 = multiple  of blocksize of cipher (=8 when no cipher is used)
	so adjust n2 (=padding_length) to follow this rule
	and n2>=4
*/

static int _write_ssh_packet(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void (* post_send)(struct ssh_connection_s *c, int written), unsigned int *seq)
{
    struct ssh_send_s *send=&connection->send;
    unsigned int error=0;
    int written=-1;
    struct ssh_compressor_s *compressor=get_compressor(send, &error);
    unsigned char type=payload->type;

    if ((*compressor->compress_payload)(compressor, &payload, &error)==0) {
	struct ssh_sender_s sender;

	init_list_element(&sender.list, NULL);
	sender.sequence=0;

	if ((* send->queue_sender)(send, &sender, &error)==0) {
	    struct ssh_encryptor_s *encryptor=get_encryptor(send, &error);
	    unsigned char padding=(* encryptor->get_message_padding)(encryptor, payload->len + 5);
	    unsigned int len = 5 + payload->len + padding; /* field length (4 bytes) plus padding field (1 byte) plus payload plus the padding */
	    unsigned int size = len + encryptor->hmac_maclen; /* total size of message */
	    char buffer[size];
	    struct ssh_packet_s packet;
	    char *pos=NULL;

	    *seq=sender.sequence;

	    packet.len 		= len;
	    packet.size 	= size;
	    packet.padding	= padding;
	    packet.sequence	= sender.sequence;
	    packet.error	= 0;
	    packet.type		= type;
	    packet.decrypted	= 0;
	    packet.buffer	= buffer;

	    pos=packet.buffer;
	    store_uint32(pos, packet.len - 4);
	    pos+=4;

	    /* store the number of padding */

	    *(pos) = packet.padding;
	    pos++;

	    /* the ssh payload */

	    memcpy(pos, payload->buffer, payload->len);
	    pos+=payload->len;

	    /* fill the padding bytes */

	    pos += fill_random(pos, packet.padding);

	    /* determine the mac of the unencrypted message (when mac before encryption is used)
		before encryption is the default according to RFC4253 6.4 Data Integrity */

	    if ((* encryptor->write_hmac_pre)(encryptor, &packet)==0) {

		if ((* encryptor->encrypt_packet)(encryptor, &packet)==0) {

		    /* determine the mac of the encrypted message (when mac after encryption is used)
			after encryption is used by chacha20-poly1305@openssh.com */

		    if ((* encryptor->write_hmac_post)(encryptor, &packet)==0) {

			/* release encryptor */

			(* encryptor->queue)(encryptor);
			encryptor=NULL;

			pthread_mutex_lock(&send->mutex);

			/* wait to become first */

			while (list_element_is_first(&sender.list)==-1) {

			    logoutput("_write_ssh_packet: %i not the first, wait", sender.sequence);
			    int result=pthread_cond_wait(&send->cond, &send->mutex);

			}

			pthread_mutex_unlock(&send->mutex);

			written=write_socket(connection, &packet, &error);

			/* function will serialize the sending after kexinit and use newkeys after newkeys
			    in other cases this does nothing 
			    NOTE: the send process is lock protected */

			(* post_send)(connection, written);

			pthread_mutex_lock(&send->mutex);
			remove_list_element(&sender.list);
			pthread_cond_broadcast(&send->cond);
			pthread_mutex_unlock(&send->mutex);

			if (written==-1) {

			    if (error==0) error=EIO;
			    logoutput("_write_ssh_packet: error %i sending packet (%s)", error, strerror(error));

			}

		    } else {

			logoutput("_write_ssh_packet: error writing hmac post");

		    }

		} else {

		    logoutput("_write_ssh_packet: error encrypt pakket");

		}

	    } else {

		logoutput("_write_ssh_packet: error writing hmac pre");

	    }

	    if (encryptor) {

		(* encryptor->queue)(encryptor);
		encryptor=NULL;

	    }

	    if (sender.list.h) {

		pthread_mutex_lock(&send->mutex);
		remove_list_element(&sender.list);
		pthread_cond_broadcast(&send->cond);
		pthread_mutex_unlock(&send->mutex);

	    }

	} /* queue sender */

    } /* compress */ else {

	logoutput("_write_ssh_packet: error compress payload");

    }

    queue_compressor(compressor);
    compressor=NULL;

    logoutput("_write_ssh_packet: written %i", written);

    return written;

}

static void post_send_default(struct ssh_connection_s *s, int written)
{
}

int write_ssh_packet(struct ssh_connection_s *c, struct ssh_payload_s *payload, unsigned int *seq)
{
    return _write_ssh_packet(c, payload, post_send_default, seq);
}

static int setup_cb_send_kexinit(struct ssh_connection_s *c, void *data)
{
    /* after sending kexinit adjust the send behaviour */
    set_ssh_send_behaviour(c, "kexinit");
    return 0;
}

static void post_send_kexinit(struct ssh_connection_s *c, int written)
{
    /* change the setup */
    int result=change_ssh_connection_setup(c, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXINIT_C2S, 0, setup_cb_send_kexinit, NULL);
}

int write_ssh_packet_kexinit(struct ssh_connection_s *c, struct ssh_payload_s *payload, unsigned int *seq)
{
    return _write_ssh_packet(c, payload, post_send_kexinit, seq);
}

static int setup_cb_send_newkeys(struct ssh_connection_s *c, void *data)
{
    set_ssh_send_behaviour(c, "default");
    return 0;
}

static void post_send_newkeys(struct ssh_connection_s *connection, int written)
{
    struct ssh_send_s *send=&connection->send;
    struct ssh_setup_s *setup=&connection->setup;
    struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
    struct algo_list_s *algos=kex->algos;
    int index_compr=kex->chosen[SSH_ALGO_TYPE_COMPRESS_C2S];
    int index_cipher=kex->chosen[SSH_ALGO_TYPE_CIPHER_C2S];
    int index_hmac=kex->chosen[SSH_ALGO_TYPE_HMAC_C2S];
    struct algo_list_s *algo_compr=&algos[index_compr];
    struct algo_list_s *algo_cipher=&algos[index_cipher];
    struct algo_list_s *algo_hmac=(index_hmac>=0) ? &algos[index_hmac] : NULL;

    /* TODO: action depends on written, this maybe -1 when error */

    reset_compress(send, algo_compr);
    reset_encrypt(connection, algo_cipher, algo_hmac);
    get_current_time(&send->newkeys);

    /* change the setup */
    int result=change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_NEWKEYS_C2S, 0, setup_cb_send_newkeys, NULL);
}

int write_ssh_packet_newkeys(struct ssh_connection_s *c, struct ssh_payload_s *p, unsigned int *seq)
{
    return _write_ssh_packet(c, p, post_send_newkeys, seq);
}

static int select_payload_service_accept(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    int result=-1;
    const char *service=(const char *) ptr;

    if (payload->type==SSH_MSG_SERVICE_ACCEPT) {
	unsigned int len=strlen(service);

	if (payload->len>=5 + len) {
	    char buffer[5 + len];

	    buffer[0]=(unsigned char) SSH_MSG_SERVICE_ACCEPT;
	    store_uint32(&buffer[1], len);
	    memcpy(&buffer[5], service, len);

	    if (memcmp(payload->buffer, buffer, len)==0) result=0;

	}

    }

    return result;
}

int request_ssh_service(struct ssh_connection_s *connection, const char *service)
{
    struct payload_queue_s *queue=&connection->setup.queue;
    struct timespec expire;
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct ssh_payload_s *payload=NULL;
    uint32_t seq=0;
    int result=-1;

    logoutput("request_ssh_service: request for service %s", service);

    if (send_service_request_message(connection, service, &seq)==-1) {

	logoutput("request_ssh_service: error sending service request");
	goto outrequest;

    }

    get_ssh_connection_expire_init(connection, &expire);

    getrequest:

    payload=get_ssh_payload(connection, queue, &expire, &seq, select_payload_service_accept, (void *) service, &error);

    if (! payload) {

	logoutput("request_ssh_service: error waiting for server SSH_MSG_SERVICE_REQUEST (%s)", get_error_description(&error));
	goto outrequest;

    }

    if (payload->type == SSH_MSG_SERVICE_ACCEPT) {

	logoutput("request_ssh_service: server accepted service");
	result=0;

    } else {

	logoutput("request_ssh_service: server send unexpected %i: disconnect", payload->type);

    }

    outrequest:
    if (payload) free_payload(&payload);
    return result;
}
