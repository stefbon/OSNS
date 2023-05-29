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
#include "ssh-utils.h"
#include "ssh-send.h"
#include "ssh-common-crypto.h"

static char fallback_buffer[sizeof(struct ssh_encryptor_s)];
static struct ssh_encryptor_s *fallback=(struct ssh_encryptor_s *) fallback_buffer;

static int write_hmac_error(struct ssh_encryptor_s *e, struct ssh_packet_s *packet)
{
    packet->error=EIO;
    return -1;
}

static int encrypt_packet_error(struct ssh_encryptor_s *e, struct ssh_packet_s *packet)
{
    packet->error=EIO;
    return -1;
}
static unsigned char get_message_padding_error(struct ssh_encryptor_s *e, unsigned int l)
{
    return 0;
}
static void dummy_encryptor(struct ssh_encryptor_s *e)
{
}

static void queue_ssh_encryptor(struct ssh_cryptoactor_s *ca)
{
    struct ssh_encryptor_s *encryptor=(struct ssh_encryptor_s *)((char *)ca - offsetof(struct ssh_encryptor_s, common));
    struct ssh_encrypt_s *encrypt=encryptor->encrypt;
    struct ssh_send_s *send=(struct ssh_send_s *) (((char *) encrypt) - offsetof(struct ssh_send_s, encrypt));

    if (ca->kexctr==send->kexctr) {

	add_ssh_cryptoactor_list(&encrypt->header, ca);

    } else {

	/* dealing with an "old" encryptor from before newkeys:
	    do not queue it but clear and free it  */

	(* encryptor->clear)(encryptor);
	free(encryptor);
	change_count_cryptors(&send->signal, &encrypt->count, -1);

    }

}

static void free_ssh_encryptor(struct list_element_s *list)
{
    struct ssh_encryptor_s *encryptor=(struct ssh_encryptor_s *) (((char *) list) - offsetof(struct ssh_encryptor_s, common.list));
    struct ssh_encrypt_s *encrypt=encryptor->encrypt;
    struct ssh_send_s *send=(struct ssh_send_s *) (((char *) encrypt) - offsetof(struct ssh_send_s, encrypt));

    (* encryptor->clear)(encryptor);
    free(encryptor);
    change_count_cryptors(&send->signal, &encrypt->count, -1);
}

static void init_ssh_encryptor(struct ssh_encryptor_s *encryptor, struct ssh_encrypt_s *encrypt, unsigned int size, unsigned int kexctr)
{

    memset(encryptor, 0, sizeof(struct ssh_encryptor_s) + size);
    init_ssh_cryptoactor(&encryptor->common, kexctr, ((encrypt) ? encrypt->count : 0));
    encryptor->encrypt=encrypt;
    encryptor->size=size;

    encryptor->cipher_blocksize=8;
    encryptor->cipher_headersize=8;
    encryptor->hmac_maclen=0;
    encryptor->write_hmac_pre=write_hmac_error;
    encryptor->write_hmac_post=write_hmac_error;
    encryptor->encrypt_packet=encrypt_packet_error;
    encryptor->get_message_padding=get_message_padding_error;
    encryptor->clear=dummy_encryptor;

}

static struct ssh_encryptor_s *create_ssh_encryptor(struct ssh_encrypt_s *encrypt, unsigned int kexctr)
{
    struct encrypt_ops_s *ops=encrypt->ops;
    unsigned int size=(* ops->get_handle_size)(encrypt);
    struct ssh_encryptor_s *encryptor=NULL;

    // logoutput_debug("create_ssh_encryptor: size %u", size);

    encryptor=malloc(sizeof(struct ssh_encryptor_s) + size);
    if (encryptor==NULL) return fallback;
    init_ssh_encryptor(encryptor, encrypt, size, kexctr);
    encryptor->common.queue=queue_ssh_encryptor;
    encryptor->common.free=free_ssh_encryptor;

    if ((* ops->init)(encryptor)==0) {
        struct ssh_send_s *send=(struct ssh_send_s *) (((char *) encrypt) - offsetof(struct ssh_send_s, encrypt));

        change_count_cryptors(&send->signal, &encrypt->count, 1);
        return encryptor;

    }

    free(encryptor);
    outfallback:
    return fallback;

}

static struct ssh_cryptoactor_s *create_ssh_cryptoactor(unsigned int kexctr, void *ptr)
{
    struct ssh_encrypt_s *encrypt=(struct ssh_encrypt_s *) ptr;
    struct ssh_encryptor_s *encryptor=create_ssh_encryptor(encrypt, kexctr);
    return &encryptor->common;
}

static unsigned int interrupt_get_cryptoactor(void *ptr)
{
    struct ssh_encrypt_s *encrypt=(struct ssh_encrypt_s *) ptr;
    struct ssh_send_s *send=(struct ssh_send_s *) (((char *) encrypt) - offsetof(struct ssh_send_s, encrypt));

    return ((send->flags & SSH_SEND_FLAG_DISCONNECT) ? ENOTCONN : 0);
}

static void error_get_cryptoactor(unsigned int errcode, void *ptr)
{
    /* 20221119: log for now */

    logoutput_warning("error_get_cryptoactor: error %u - %s", errcode, strerror(errcode));
}

struct ssh_encryptor_s *get_encryptor(struct ssh_send_s *send, unsigned int *error)
{
    struct ssh_encrypt_s *encrypt=&send->encrypt;
    struct ssh_cryptoactor_s *ca=get_cryptoactor(&encrypt->header, &send->signal, send->kexctr, &encrypt->max_count, &encrypt->count,
                                                    create_ssh_cryptoactor, interrupt_get_cryptoactor, error_get_cryptoactor, &fallback->common, (void *) encrypt);
    return (struct ssh_encryptor_s *) ((char *) ca - offsetof(struct ssh_encryptor_s, common));
}


void remove_encryptors(struct ssh_encrypt_s *encrypt)
{
    remove_ssh_cryptoactor_list(&encrypt->header);
}

void init_encryptors_once()
{
    init_ssh_encryptor(fallback, NULL, 0, 0);
}
