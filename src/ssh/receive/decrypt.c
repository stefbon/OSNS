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

#include "log.h"
#include "main.h"
#include "misc.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-receive.h"

static struct list_header_s list_decrypt_ops=INIT_LIST_HEADER;

struct decrypt_ops_s *get_decrypt_ops_container(struct list_element_s *list)
{
    return (struct decrypt_ops_s *) (((char *) list) - offsetof(struct decrypt_ops_s, list));
}

void add_decrypt_ops(struct decrypt_ops_s *d_ops)
{
    add_list_element_last(&list_decrypt_ops, &d_ops->list);
}

struct decrypt_ops_s *get_next_decrypt_ops(struct decrypt_ops_s *ops)
{
    struct list_element_s *list=NULL;

    if (ops) {

	list=get_next_element(&ops->list);

    } else {

	list=get_list_head(&list_decrypt_ops, 0);

    }

    return (list) ? get_decrypt_ops_container(list) : NULL;
}


void reset_decrypt(struct ssh_connection_s *connection, struct algo_list_s *algo_cipher, struct algo_list_s *algo_hmac)
{
    struct ssh_receive_s *receive=&connection->receive;
    struct ssh_decrypt_s *decrypt=&receive->decrypt;
    struct ssh_keyexchange_s *kex=&connection->setup.phase.transport.type.kex;
    char *ciphername=NULL;
    char *hmacname=NULL;
    struct decrypt_ops_s *ops=(struct decrypt_ops_s *) algo_cipher->ptr;

    remove_decryptors(decrypt);
    clear_ssh_string(&decrypt->cipher_key);
    clear_ssh_string(&decrypt->cipher_iv);
    clear_ssh_string(&decrypt->hmac_key);
    memset(decrypt->ciphername, '\0', sizeof(decrypt->ciphername));
    memset(decrypt->hmacname, '\0', sizeof(decrypt->hmacname));

    ciphername=algo_cipher->sshname;
    if (algo_hmac) hmacname=algo_hmac->sshname;

    if ((* ops->get_decrypt_flag)(ciphername, hmacname, "parallel")==1) {

	decrypt->max_count=4; /* seems like a good choice, make it configurable; it's also possible to set this to 0: no limit decryptors allowed*/
	decrypt->flags |= SSH_DECRYPT_FLAG_PARALLEL;

    } else {

	if (decrypt->flags & SSH_DECRYPT_FLAG_PARALLEL) decrypt->flags -= SSH_DECRYPT_FLAG_PARALLEL;
	decrypt->max_count=1;

    }

    decrypt->ops=ops;
    strcpy(decrypt->ciphername, ciphername);
    if (hmacname) strcpy(decrypt->hmacname, hmacname);
    move_ssh_string(&decrypt->cipher_key, &kex->cipher_key_s2c, 0);
    move_ssh_string(&decrypt->cipher_iv, &kex->cipher_iv_s2c, 0);
    move_ssh_string(&decrypt->hmac_key, &kex->hmac_key_s2c, 0);

}

unsigned int build_cipher_list_s2c(struct ssh_connection_s *connection, struct algo_list_s *alist, unsigned int start)
{
    struct decrypt_ops_s *ops=NULL;

    ops=get_next_decrypt_ops(NULL);

    while (ops) {

	start=(* ops->populate_cipher)(connection, ops, alist, start);
	ops=get_next_decrypt_ops(ops);

    }

    return start;

}

unsigned int build_hmac_list_s2c(struct ssh_connection_s *connection, struct algo_list_s *alist, unsigned int start)
{
    struct decrypt_ops_s *ops=NULL;

    ops=get_next_decrypt_ops(NULL);

    while (ops) {

	start=(* ops->populate_hmac)(connection, ops, alist, start);
	ops=get_next_decrypt_ops(ops);

    }

    return start;

}

void init_decrypt_once()
{
    init_list_header(&list_decrypt_ops, SIMPLE_LIST_TYPE_EMPTY, 0);
}
