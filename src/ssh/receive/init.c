/*
  2017, 2018 Stef Bon <stefbon@gmail.com>

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
#include "libosns-threads.h"
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-utils.h"
#include "ssh-send.h"

static pthread_mutex_t recv_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t recv_cond=PTHREAD_COND_INITIALIZER;

void msg_not_supported(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    unsigned int seq=payload->sequence;

    logoutput_debug("msg_not_supported: received type %i", payload->type);
    free_payload(&payload);

    if (send_unimplemented_message(connection, seq)>0) {

	logoutput("msg_not_supported: send MSG_UNIMPLEMENTED for seq %i", seq);

    } else {

	logoutput("msg_not_supported: failed to send MSG_UNIMPLEMENTED for seq %i", seq);

    }

}

void register_msg_cb(struct ssh_connection_s *c, unsigned char type, receive_msg_cb_t cb)
{
    c->cb[type]=cb;
}

void process_cb_ssh_payload(struct ssh_connection_s *connection, struct ssh_payload_s *payload)
{
    (* connection->cb[payload->type])(connection, payload);
}

int init_ssh_connection_receive(struct ssh_session_s *session, struct ssh_connection_s *sshc, unsigned int *error)
{
    struct ssh_receive_s *receive=&sshc->receive;
    struct ssh_decrypt_s *decrypt=&receive->decrypt;
    struct ssh_decompress_s *decompress=&receive->decompress;

    logoutput_debug("init_ssh_connection_receive");

    memset(receive, 0, sizeof(struct ssh_receive_s));

    set_custom_shared_signal(&receive->signal, &recv_mutex, &recv_cond);

    receive->status=0;
    receive->threads=0;
    receive->sequence_number=0;
    receive->sequence_error.sequence_number_error=0;
    receive->sequence_error.errcode=0;

    set_system_time(&receive->newkeys, 0, 0);
    set_system_time(&receive->kexinit, 0, 0);
    receive->kexctr=0;

    init_ssh_decrypt(sshc);
    init_ssh_decompress(sshc);
    init_ssh_socket_behaviour(&sshc->connection.sock);
    set_ssh_receive_behaviour(sshc, "greeter");

    /* the maximum size for the buffer RFC4253 6.1 Maximum Packet Length */

    receive->size=session->config.max_receive_size;
    receive->read=0;
    receive->msgsize=0;
    receive->buffer=malloc(receive->size);

    if (receive->buffer) {

	memset(receive->buffer, '\0', receive->size);
	*error=0;
	logoutput_debug("init_ssh_connection_receive: allocated receive buffer (%u bytes)", receive->size);
	set_osns_socket_buffer(&sshc->connection.sock, receive->buffer, receive->size);

    } else {

	logoutput_warning("init_ssh_connection_receive: error allocating receive buffer (%u bytes)", receive->size);
	receive->size=0;
	*error=ENOMEM;
	goto error;

    }

    return 0;

    error:

    if (receive->buffer) {

	free(receive->buffer);
	receive->buffer=NULL;

    }

    return -1;

}

void free_ssh_connection_receive(struct ssh_connection_s *connection)
{
    struct ssh_receive_s *receive=&connection->receive;
    struct ssh_decrypt_s *decrypt=&receive->decrypt;
    struct ssh_decompress_s *decompress=&receive->decompress;

    if (receive->buffer) {

	free(receive->buffer);
	receive->buffer=NULL;

    }

    receive->size=0;
    remove_decryptors(decrypt);
    remove_decompressors(decompress);

}

void init_ssh_receive_once()
{
    init_decrypt_once();
    init_decryptors_once();
    init_decompress_once();
    init_decompressors_once();

    init_decrypt_generic();
    init_decrypt_chacha20_poly1305_openssh_com();
    init_decompress_none();

}
