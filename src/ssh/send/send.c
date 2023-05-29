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

#include "libosns-basic-system-headers.h"

#include "libosns-log.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh/receive/payload.h"

static int queue_sender_default(struct ssh_send_s *send, struct ssh_sender_s *sender, unsigned int *error)
{
    return 0;
}

static unsigned char sender_wait_first_cb(void *ptr)
{
    struct ssh_sender_s *sender=(struct ssh_sender_s *) ptr;
    return ((list_element_is_first(&sender->list)==0) ? 1 : 0);
}

static int queue_sender_serial(struct ssh_send_s *send, struct ssh_sender_s *sender, unsigned int *error)
{
    wait_lock_list(&sender->list.lock, sender_wait_first_cb, (void *) sender);
    return 0;
}

static int queue_sender_disconnect(struct ssh_send_s *send, struct ssh_sender_s *sender, unsigned int *error)
{
    *error=ENOTCONN;
    return -1;
}

static void pre_send_default(struct ssh_send_s *send, struct ssh_sender_s *sender)
{
    struct list_header_s *h=&send->senders;

    /* wait for it to be first */

    wait_lock_list(&sender->list.lock, sender_wait_first_cb, (void *) sender);
    signal_lock_flag(&send->signal, &send->flags, SSH_SEND_FLAG_LOCK);

    write_lock_list_header(h);
    remove_list_element(&sender->list);
    write_unlock_list_header(h);

}

static void pre_send_serial(struct ssh_send_s *send, struct ssh_sender_s *sender)
{
    signal_lock_flag(&send->signal, &send->flags, SSH_SEND_FLAG_LOCK);
}

static void post_send_default(struct ssh_send_s *send, struct ssh_sender_s *sender, int bytessend)
{
    signal_unlock_flag(&send->signal, &send->flags, SSH_SEND_FLAG_LOCK);
}

static int setup_cb_send_newkeys(struct ssh_connection_s *c, void *data)
{
    struct ssh_send_s *send=&c->send;
    set_ssh_send_behaviour(send, "default");
    return 0;
}

static void post_send_serial(struct ssh_send_s *send, struct ssh_sender_s *sender, int bytessend)
{
    struct list_header_s *h=&send->senders;

    signal_unlock_flag(&send->signal, &send->flags, SSH_SEND_FLAG_LOCK);

    write_lock_list_header(h);
    remove_list_element(&sender->list);

    logoutput_debug("post_send_serial: type %u", sender->type);

    if (sender->type==SSH_MSG_KEXINIT) {
	struct ssh_connection_s *c=(struct ssh_connection_s *)((char *) send - offsetof(struct ssh_connection_s, send));

	int result=change_ssh_connection_setup(c, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_KEXINIT_C2S, 0, NULL, NULL);

    } else if (sender->type==SSH_MSG_NEWKEYS) {
	struct ssh_connection_s *connection=(struct ssh_connection_s *)((char *) send - offsetof(struct ssh_connection_s, send));
	struct ssh_setup_s *setup=&connection->setup;
	struct ssh_keyexchange_s *kex=&setup->phase.transport.type.kex;
	struct algo_list_s *algos=kex->algos;
	int index_compr=kex->chosen[SSH_ALGO_TYPE_COMPRESS_C2S];
	int index_cipher=kex->chosen[SSH_ALGO_TYPE_CIPHER_C2S];
	int index_hmac=kex->chosen[SSH_ALGO_TYPE_HMAC_C2S];
	struct algo_list_s *algo_compr=&algos[index_compr];
	struct algo_list_s *algo_cipher=&algos[index_cipher];
	struct algo_list_s *algo_hmac=(index_hmac>=0) ? &algos[index_hmac] : NULL;

	reset_ssh_compress(send, algo_compr);
	reset_ssh_encrypt(connection, algo_cipher, algo_hmac);
	get_current_time_system_time(&send->newkeys);
	send->kexctr++;

	/* change the setup */
	int result=change_ssh_connection_setup(connection, "transport", SSH_TRANSPORT_TYPE_KEX, SSH_KEX_FLAG_NEWKEYS_C2S, 0, setup_cb_send_newkeys, NULL);

    }

    write_unlock_list_header(h);
}

static void set_sender_default(struct ssh_send_s *send, struct ssh_sender_s *sender)
{
    sender->queue_sender=queue_sender_default;
    sender->pre_send=pre_send_default;
    sender->post_send=post_send_default;
}

static void set_sender_serial(struct ssh_send_s *send, struct ssh_sender_s *sender)
{
    sender->queue_sender=queue_sender_serial;
    sender->pre_send=pre_send_serial;
    sender->post_send=post_send_serial;
}

static void set_sender_disconnect(struct ssh_send_s *send, struct ssh_sender_s *sender)
{
    sender->queue_sender=queue_sender_disconnect;
}

void set_ssh_send_behaviour(struct ssh_send_s *send, const char *what)
{

    if (strcmp(what, "default")==0) {

	send->set_sender=set_sender_default;

    } else if (strcmp(what, "serial")==0) {

	send->set_sender=set_sender_serial;

    } else if (strcmp(what, "disconnect")==0) {

	send->set_sender=set_sender_disconnect;

    }

}

static void set_sender_cb_default(struct ssh_send_s *send, struct ssh_sender_s *sender)
{}

static void set_sender_cb_kexinit(struct ssh_send_s *send, struct ssh_sender_s *sender)
{
    set_ssh_send_behaviour(send, "serial");
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

static int _write_ssh_packet(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void (* set_sender_cb)(struct ssh_send_s *send, struct ssh_sender_s *sender))
{
    struct ssh_send_s *send=&connection->send;
    unsigned int error=0;
    int byteswritten=-1;
    struct ssh_compressor_s *compressor=get_compressor(send, &error);
    unsigned char type=payload->type;

    if ((*compressor->compress_payload)(compressor, &payload, &error)==0) {
	struct ssh_sender_s sender;
	struct list_header_s *h=&send->senders;

	init_list_element(&sender.list, NULL);
	sender.sequence=0;
	sender.type=type;

	write_lock_list_header(h);
	add_list_element_last(h, &sender.list);
	sender.sequence=send->sequence_number;
	send->sequence_number++;
	(* set_sender_cb)(send, &sender);
	(* send->set_sender)(send, &sender);
	write_unlock_list_header(h);

	if ((* sender.queue_sender)(send, &sender, &error)==0) {
	    struct ssh_encryptor_s *encryptor=get_encryptor(send, &error);
	    unsigned char padding=(* encryptor->get_message_padding)(encryptor, payload->len + 5);
	    unsigned int len = 5 + payload->len + padding; /* field length (4 bytes) plus padding field (1 byte) plus payload plus the padding */
	    unsigned int size = len + encryptor->hmac_maclen; /* total size of message */
	    char buffer[size];
	    struct ssh_packet_s packet;
	    char *pos=NULL;

	    payload->sequence=sender.sequence;

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
			struct list_header_s *h=&send->senders;

			/* release encryptor */

			(* encryptor->common.queue)(&encryptor->common);
			encryptor=NULL;

			(* sender.pre_send)(send, &sender); /* when not forced serial wait to be first */

			byteswritten=write_ssh_socket(connection, &packet, &error);

			(* sender.post_send)(send, &sender, byteswritten);

			if (byteswritten==-1) {

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

		(* encryptor->common.queue)(&encryptor->common);
		encryptor=NULL;

	    }

	    if (sender.list.h) {

		write_lock_list_header(h);
		remove_list_element(&sender.list);
		write_unlock_list_header(h);

	    }

	} /* queue sender */

    } /* compress */ else {

	logoutput("_write_ssh_packet: error compress payload");

    }

    (* compressor->common.queue)(&compressor->common);
    compressor=NULL;

    // logoutput_debug("_write_ssh_packet: written %i", byteswritten);
    return byteswritten;

}

int write_ssh_packet(struct ssh_connection_s *c, struct ssh_payload_s *payload)
{
    return _write_ssh_packet(c, payload, set_sender_cb_default);
}

int write_ssh_packet_kexinit(struct ssh_connection_s *c, struct ssh_payload_s *payload)
{
    return _write_ssh_packet(c, payload, set_sender_cb_kexinit);
}

static int select_payload_service_accept(struct ssh_payload_s *payload, void *ptr)
{
    int result=0;
    const char *service=(const char *) ptr;

    if (payload->type==SSH_MSG_SERVICE_ACCEPT) {
	unsigned int len=strlen(service);

	if (payload->len>=5 + len) {
	    char buffer[5 + len];

	    buffer[0]=(unsigned char) SSH_MSG_SERVICE_ACCEPT;
	    store_uint32(&buffer[1], len);
	    memcpy(&buffer[5], service, len);

	    if (memcmp(payload->buffer, buffer, len)==0) result=1;

	}

    }

    return result;
}

int request_ssh_service(struct ssh_connection_s *connection, const char *service)
{
    struct payload_queue_s *queue=&connection->setup.queue;
    struct system_timespec_s expire=SYSTEM_TIME_INIT;
    struct generic_error_s error=GENERIC_ERROR_INIT;
    struct ssh_payload_s *payload=NULL;
    int result=-1;

    if (send_service_request_message(connection, service)==-1) {

	logoutput("request_ssh_service: error sending service request %s", service);
	goto outrequest;

    }

    logoutput("request_ssh_service: request for service %s", service);
    get_ssh_connection_expire_init(connection, &expire);

    getrequest:

    payload=get_ssh_payload(queue, &expire, select_payload_service_accept, NULL, NULL, (void *) service);

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
