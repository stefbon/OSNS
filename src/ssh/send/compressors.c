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

static char fallback_buffer[sizeof(struct ssh_compressor_s)];
static struct ssh_compressor_s *fallback=(struct ssh_compressor_s *) fallback_buffer;

static int compress_error(struct ssh_compressor_s *c, struct ssh_payload_s **payload, unsigned int *error)
{
    *error=ENOMEM;
    return -1;
}

static void clear_dummy(struct ssh_compressor_s *c)
{}

void queue_ssh_compressor(struct ssh_cryptoactor_s *ca)
{
    struct ssh_compressor_s *compressor=(struct ssh_compressor_s *)((char *)ca - offsetof(struct ssh_compressor_s, common));
    struct ssh_compress_s *compress=compressor->compress;
    struct ssh_send_s *send=(struct ssh_send_s *) (((char *) compress) - offsetof(struct ssh_send_s, compress));

    if (ca->kexctr==send->kexctr) {

        add_ssh_cryptoactor_list(&compress->header, ca);

    } else {

	/* dealing with an "old" decryptor from before newkeys:
	    do not queue it but clear and free it  */

	(* compressor->clear)(compressor);
	free(compressor);
	change_count_cryptors(&send->signal, &compress->count, -1);

    }

}

static void free_ssh_compressor(struct list_element_s *list)
{
    struct ssh_compressor_s *compressor=(struct ssh_compressor_s *) (((char *) list) - offsetof(struct ssh_compressor_s, common.list));
    struct ssh_compress_s *compress=compressor->compress;
    struct ssh_send_s *send=(struct ssh_send_s *) (((char *) compress) - offsetof(struct ssh_send_s, compress));

    (* compressor->clear)(compressor);
    free(compressor);
    change_count_cryptors(&send->signal, &compress->count, -1);
}

static void init_ssh_compressor(struct ssh_compressor_s *compressor, struct ssh_compress_s *compress, unsigned int size, unsigned int kexctr)
{
    memset(compressor, 0, sizeof(struct ssh_compressor_s) + size);
    init_ssh_cryptoactor(&compressor->common, kexctr, ((compress) ? compress->count : 0));
    compressor->compress=compress;
    compressor->size=size;
    compressor->clear=clear_dummy;
    compressor->compress_payload=compress_error;

}

static struct ssh_compressor_s *create_ssh_compressor(struct ssh_compress_s *compress, unsigned int kexctr)
{
    struct compress_ops_s *ops=compress->ops;
    unsigned int size=(* ops->get_handle_size)(compress);
    struct ssh_compressor_s *compressor=malloc(sizeof(struct ssh_compressor_s) + size);

    if (compressor==NULL) return fallback;
    init_ssh_compressor(compressor, compress, size, kexctr);
    compressor->common.queue=queue_ssh_compressor;
    compressor->common.free=free_ssh_compressor;

    if ((* ops->init)(compressor)==0) {
        struct ssh_send_s *send=(struct ssh_send_s *) (((char *) compress) - offsetof(struct ssh_send_s, compress));

        change_count_cryptors(&send->signal, &compress->count, 1);
        return compressor;

    }

    free(compressor);
    fallback:
    return fallback;

}

static struct ssh_cryptoactor_s *create_ssh_cryptoactor(unsigned int kexctr, void *ptr)
{
    struct ssh_compress_s *compress=(struct ssh_compress_s *) ptr;
    struct ssh_compressor_s *compressor=create_ssh_compressor(compress, kexctr);
    return &compressor->common;
}

static unsigned int interrupt_get_cryptoactor(void *ptr)
{
    struct ssh_compress_s *compress=(struct ssh_compress_s *) ptr;
    struct ssh_send_s *send=(struct ssh_send_s *) (((char *) compress) - offsetof(struct ssh_send_s, compress));

    return ((send->flags & SSH_SEND_FLAG_DISCONNECT) ? ENOTCONN : 0);
}

static void error_get_cryptoactor(unsigned int errcode, void *ptr)
{
    /* 20221119: log for now */

    logoutput_warning("error_get_cryptoactor: error %u - %s", errcode, strerror(errcode));
}

struct ssh_compressor_s *get_compressor(struct ssh_send_s *send, unsigned int *error)
{
    struct ssh_compress_s *compress=&send->compress;
    struct ssh_cryptoactor_s *ca=get_cryptoactor(&compress->header, &send->signal, send->kexctr, &compress->max_count, &compress->count,
                                                    create_ssh_cryptoactor, interrupt_get_cryptoactor, error_get_cryptoactor, &fallback->common, (void *) compress);
    return (struct ssh_compressor_s *) ((char *) ca - offsetof(struct ssh_compressor_s, common));
}

void remove_compressors(struct ssh_compress_s *compress)
{
    remove_ssh_cryptoactor_list(&compress->header);
}

void init_compressors_once()
{
    init_ssh_compressor(fallback, NULL, 0, 0);
}
