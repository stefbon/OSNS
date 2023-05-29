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
#include "ssh-send.h"
#include "ssh-utils.h"

static pthread_mutex_t send_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t send_cond=PTHREAD_COND_INITIALIZER;

int init_ssh_connection_send(struct ssh_connection_s *connection)
{
    struct ssh_session_s *session=get_ssh_connection_session(connection);
    struct ssh_send_s *send=&connection->send;
    struct ssh_encrypt_s *encrypt=&send->encrypt;
    struct ssh_compress_s *compress=&send->compress;

    /* send */

    send->flags=0;
    set_custom_shared_signal(&send->signal, &send_mutex, &send_cond);
    set_system_time(&send->newkeys, 0, 0);
    send->kexctr=0;
    set_ssh_send_behaviour(send, "serial"); /* start with serialized handling of the send queue */
    send->sequence_number=0;
    init_list_header(&send->senders, SIMPLE_LIST_TYPE_EMPTY, NULL);

    /* encrypt */

    init_ssh_encrypt(connection);

    /* compress */

    init_ssh_compress(send);

    return 0;

}

void free_ssh_connection_send(struct ssh_connection_s *connection)
{
    struct ssh_send_s *send=&connection->send;
    struct ssh_encrypt_s *encrypt=&send->encrypt;
    struct ssh_compress_s *compress=&send->compress;

    remove_compressors(compress);
    remove_encryptors(encrypt);
}

void init_ssh_send_once()
{
    init_encrypt_once();
    init_compress_once();

    init_encryptors_once();
    init_compressors_once();

    init_encrypt_generic();
    init_encrypt_chacha20_poly1305_openssh_com();
    init_compress_none();

}
