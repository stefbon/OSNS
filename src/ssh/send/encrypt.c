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
#include "libosns-misc.h"

#include "ssh-common.h"
#include "ssh-connections.h"
#include "ssh-utils.h"
#include "ssh-receive.h"
#include "encryptors.h"

static struct list_header_s list_encrypt_ops=INIT_LIST_HEADER;

struct encrypt_ops_s *get_encrypt_ops_container(struct list_element_s *list)
{
    return (struct encrypt_ops_s *) (((char *) list) - offsetof(struct encrypt_ops_s, list));
}

void add_encrypt_ops(struct encrypt_ops_s *ops)
{
    add_list_element_last(&list_encrypt_ops, &ops->list);
}

struct encrypt_ops_s *get_next_encrypt_ops(struct encrypt_ops_s *ops)
{
    struct list_element_s *list=(ops) ? get_next_element(&ops->list) : get_list_head(&list_encrypt_ops);
    return (list) ? get_encrypt_ops_container(list) : NULL;
}

void init_ssh_encrypt(struct ssh_connection_s *connection)
{
    struct ssh_send_s *send=&connection->send;
    struct ssh_encrypt_s *encrypt=&send->encrypt;

    encrypt->flags=0;
    memset(encrypt->ciphername, '\0', sizeof(encrypt->ciphername));
    memset(encrypt->hmacname, '\0', sizeof(encrypt->hmacname));
    encrypt->count=0;
    encrypt->max_count=0;
    init_list_header(&encrypt->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    encrypt->ops=NULL;
    init_ssh_string(&encrypt->cipher_key);
    init_ssh_string(&encrypt->cipher_iv);
    init_ssh_string(&encrypt->hmac_key);
    strcpy(encrypt->ciphername, "none");
    strcpy(encrypt->hmacname, "none");
    set_encrypt_generic(encrypt);

}

void reset_ssh_encrypt(struct ssh_connection_s *connection, struct algo_list_s *algo_cipher, struct algo_list_s *algo_hmac)
{
    struct ssh_send_s *send=&connection->send;
    struct ssh_encrypt_s *encrypt=&send->encrypt;
    struct ssh_keyexchange_s *kex=&connection->setup.phase.transport.type.kex;
    char *ciphername=NULL;
    char *hmacname=NULL;
    struct encrypt_ops_s *ops=(struct encrypt_ops_s *) algo_cipher->ptr;

    logoutput("reset_encrypt");

    /* remove the previous encryptors and keys */

    remove_encryptors(encrypt);
    clear_ssh_string(&encrypt->cipher_key);
    clear_ssh_string(&encrypt->cipher_iv);
    clear_ssh_string(&encrypt->hmac_key);

    memset(encrypt->ciphername, '\0', sizeof(encrypt->ciphername));
    memset(encrypt->hmacname, '\0', sizeof(encrypt->hmacname));

    /* start with the new ones */

    ciphername=algo_cipher->sshname;
    if (algo_hmac) hmacname=algo_hmac->sshname;

    if ((* ops->get_encrypt_flag)(ciphername, hmacname, "parallel")==1) {
	struct ssh_session_s *session=get_ssh_connection_session(connection);

	encrypt->max_count=session->config.max_receiving_threads;
	encrypt->flags |= SSH_DECRYPT_FLAG_PARALLEL;

    } else {

	encrypt->flags &= ~SSH_DECRYPT_FLAG_PARALLEL;
	encrypt->max_count=1;

    }

    /* move the keys from the keyexchange to the encryption, and use the new encrypt ops
	the keys will be used on the fly by the new encryptors (cipher and mac) */

    encrypt->ops=ops;
    strcpy(encrypt->ciphername, ciphername);
    if (hmacname) strcpy(encrypt->hmacname, hmacname);
    move_ssh_string(&encrypt->cipher_key, &kex->cipher_key_c2s, 0);
    move_ssh_string(&encrypt->cipher_iv, &kex->cipher_iv_c2s, 0);
    move_ssh_string(&encrypt->hmac_key, &kex->hmac_key_c2s, 0);

}

unsigned int build_cipher_list_c2s(struct ssh_connection_s *c, struct algo_list_s *alist, unsigned int start)
{
    struct encrypt_ops_s *ops=NULL;

    ops=get_next_encrypt_ops(NULL);

    while (ops) {

	start=(* ops->populate_cipher)(c, ops, alist, start);
	ops=get_next_encrypt_ops(ops);

    }

    return start;

}

unsigned int build_hmac_list_c2s(struct ssh_connection_s *c, struct algo_list_s *alist, unsigned int start)
{
    struct encrypt_ops_s *ops=NULL;

    ops=get_next_encrypt_ops(NULL);

    while (ops) {

	start=(* ops->populate_hmac)(c, ops, alist, start);
	ops=get_next_encrypt_ops(ops);

    }

    return start;

}

void init_encrypt_once()
{
    init_list_header(&list_encrypt_ops, SIMPLE_LIST_TYPE_EMPTY, 0);
}
