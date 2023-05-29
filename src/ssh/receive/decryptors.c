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
#include "libosns-list.h"

#include "ssh-common.h"
#include "ssh-utils.h"
#include "ssh-receive.h"
#include "ssh-common-crypto.h"

static char fallback_buffer[sizeof(struct ssh_decryptor_s)];
static struct ssh_decryptor_s *fallback=(struct ssh_decryptor_s *) fallback_buffer;

static int verify_hmac_error(struct ssh_decryptor_s *d, struct ssh_packet_s *p)
{
    p->error=EIO;
    return -1;
}

static int decrypt_length_error(struct ssh_decryptor_s *d, struct ssh_packet_s *p, char *b, unsigned int l)
{
    p->error=EIO;
    return -1;
}

static int decrypt_packet_error(struct ssh_decryptor_s *d, struct ssh_packet_s *p)
{
    p->error=EIO;
    return -1;
}

static void clear_decryptor(struct ssh_decryptor_s *d)
{
}

static void queue_ssh_decryptor(struct ssh_cryptoactor_s *ca)
{
    struct ssh_decryptor_s *decryptor=(struct ssh_decryptor_s *)((char *) ca - offsetof(struct ssh_decryptor_s, common));
    struct ssh_decrypt_s *decrypt=decryptor->decrypt;
    struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decrypt) - offsetof(struct ssh_receive_s, decrypt));

    if (ca->kexctr==receive->kexctr) {

	add_ssh_cryptoactor_list(&decrypt->header, ca);

    } else {

	/* dealing with an "old" decryptor */

	(* decryptor->clear)(decryptor);
	free(decryptor);
	change_count_cryptors(&receive->signal, &decrypt->count, -1);

    }

}

static void free_ssh_decryptor(struct list_element_s *list)
{
    struct ssh_decryptor_s *decryptor=(struct ssh_decryptor_s *) (((char *) list) - offsetof(struct ssh_decryptor_s, common.list));
    struct ssh_decrypt_s *decrypt=decryptor->decrypt;
    struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decrypt) - offsetof(struct ssh_receive_s, decrypt));

    change_count_cryptors(&receive->signal, &decrypt->count, -1);
    (* decryptor->clear)(decryptor);
    free(decryptor);
}

static void init_ssh_decryptor(struct ssh_decryptor_s *decryptor, struct ssh_decrypt_s *decrypt, unsigned int size, unsigned int kexctr)
{
    memset(decryptor, 0, sizeof(struct ssh_decryptor_s) + size);
    init_ssh_cryptoactor(&decryptor->common, kexctr, ((decrypt) ? decrypt->count : 0));
    decryptor->decrypt=decrypt;
    decryptor->size=size;
    decryptor->verify_hmac_pre=verify_hmac_error;
    decryptor->decrypt_length=decrypt_length_error;
    decryptor->decrypt_packet=decrypt_packet_error;
    decryptor->verify_hmac_post=verify_hmac_error;
    decryptor->clear=clear_decryptor;

    if (decrypt) {

        decryptor->common.queue=queue_ssh_decryptor;
        decryptor->common.free=free_ssh_decryptor;

    }

}

static struct ssh_decryptor_s *create_ssh_decryptor(struct ssh_decrypt_s *decrypt, unsigned int kexctr)
{
    struct decrypt_ops_s *ops=decrypt->ops;
    unsigned int size=(* ops->get_handle_size)(decrypt);
    struct ssh_decryptor_s *decryptor=NULL;

    decryptor=malloc(sizeof(struct ssh_decryptor_s) + size);
    if (decryptor==NULL) return fallback;
    init_ssh_decryptor(decryptor, decrypt, size, kexctr);

    if ((* ops->init)(decryptor)==0) {
        struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decrypt) - offsetof(struct ssh_receive_s, decrypt));

        change_count_cryptors(&receive->signal, &decrypt->count, 1);
        return decryptor;

    }

    free(decryptor);
    outfallback:
    return fallback;

}

static struct ssh_cryptoactor_s *create_ssh_cryptoactor(unsigned int kexctr, void *ptr)
{
    struct ssh_decrypt_s *decrypt=(struct ssh_decrypt_s *) ptr;
    struct ssh_decryptor_s *decryptor=create_ssh_decryptor(decrypt, kexctr);
    return &decryptor->common;
}

static unsigned int interrupt_get_cryptoactor(void *ptr)
{
    struct ssh_decrypt_s *decrypt=(struct ssh_decrypt_s *) ptr;
    struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decrypt) - offsetof(struct ssh_receive_s, decrypt));
    return ((receive->status & SSH_RECEIVE_STATUS_DISCONNECT) ? ENOTCONN : 0);
}

static void error_get_cryptoactor(unsigned int errcode, void *ptr)
{
    /* 20221119: log for now */

    logoutput_warning("error_get_cryptoactor: error %u - %s", errcode, strerror(errcode));
}

struct ssh_decryptor_s *get_decryptor(struct ssh_receive_s *receive, unsigned int *error)
{
    struct ssh_decrypt_s *decrypt=&receive->decrypt;
    struct ssh_cryptoactor_s *ca=get_cryptoactor(&decrypt->header, &receive->signal, receive->kexctr, &decrypt->max_count, &decrypt->count,
                                                    create_ssh_cryptoactor, interrupt_get_cryptoactor, error_get_cryptoactor, &fallback->common, (void *) decrypt);
    return (struct ssh_decryptor_s *) ((char *) ca - offsetof(struct ssh_decryptor_s, common));
}


void remove_decryptors(struct ssh_decrypt_s *decrypt)
{
    remove_ssh_cryptoactor_list(&decrypt->header);
}

void init_decryptors_once()
{
    init_ssh_decryptor(fallback, NULL, 0, 0);
}
