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
#include "ssh-common-crypto.h"
#include "ssh-utils.h"
#include "ssh-receive.h"

static char fallback_buffer[sizeof(struct ssh_decompressor_s)];
static struct ssh_decompressor_s *fallback=(struct ssh_decompressor_s *) fallback_buffer;

static void clear_dummy(struct ssh_decompressor_s *d)
{
}

static struct ssh_payload_s *decompress_error(struct ssh_decompressor_s *d, struct ssh_packet_s *packet, unsigned int *error)
{
    *error=ENOMEM;
    return NULL;
}

static void queue_ssh_decompressor(struct ssh_cryptoactor_s *ca)
{
    struct ssh_decompressor_s *decompressor=(struct ssh_decompressor_s *) ((char *) ca - offsetof(struct ssh_decompressor_s, common));
    struct ssh_decompress_s *decompress=decompressor->decompress;
    struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decompress) - offsetof(struct ssh_receive_s, decompress));

    if (ca->kexctr==receive->kexctr) {

	/* not earlier -> is later created than newkeys -> ok */

        add_ssh_cryptoactor_list(&decompress->header, ca);

    } else {

	/* dealing with an "old" decryptor from before newkeys:
	    do not queue it but clear and free it  */

	(* decompressor->clear)(decompressor);
	free(decompressor);
	change_count_cryptors(&receive->signal, &decompress->count, -1);

    }

}

static void free_ssh_decompressor(struct list_element_s *list)
{
    struct ssh_decompressor_s *decompressor=(struct ssh_decompressor_s *) (((char *) list) - offsetof(struct ssh_decompressor_s, common.list));
    struct ssh_decompress_s *decompress=decompressor->decompress;
    struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decompress) - offsetof(struct ssh_receive_s, decompress));

    (* decompressor->clear)(decompressor);
    free(decompressor);
    change_count_cryptors(&receive->signal, &decompress->count, -1);
}

static void init_ssh_decompressor(struct ssh_decompressor_s *decompressor, struct ssh_decompress_s *decompress, unsigned int size, unsigned int kexctr)
{
    memset(decompressor, 0, sizeof(struct ssh_decompressor_s) + size);
    init_ssh_cryptoactor(&decompressor->common, kexctr, ((decompress) ? decompress->count : 0));
    decompressor->decompress=decompress;
    decompressor->clear=clear_dummy;
    decompressor->decompress_packet=decompress_error;
    decompressor->size=size;

}

static struct ssh_decompressor_s *create_ssh_decompressor(struct ssh_decompress_s *decompress, unsigned int kexctr)
{
    struct decompress_ops_s *ops=decompress->ops;
    unsigned int size=(* ops->get_handle_size)(decompress);
    struct ssh_decompressor_s *decompressor=malloc(sizeof(struct ssh_decompressor_s) + size);

    if (decompressor==NULL) return fallback;
    init_ssh_decompressor(decompressor, decompress, size, kexctr);
    decompressor->common.queue=queue_ssh_decompressor;
    decompressor->common.free=free_ssh_decompressor;

    if ((* ops->init)(decompressor)==0) {
        struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decompress) - offsetof(struct ssh_receive_s, decompress));

        change_count_cryptors(&receive->signal, &decompress->count, 1);
        return decompressor;

    }

    free(decompressor);
    outfallback:
    return fallback;

}

static struct ssh_cryptoactor_s *create_ssh_cryptoactor(unsigned int kexctr, void *ptr)
{
    struct ssh_decompress_s *decompress=(struct ssh_decompress_s *) ptr;
    struct ssh_decompressor_s *decompressor=create_ssh_decompressor(decompress, kexctr);
    return &decompressor->common;
}

static unsigned int interrupt_get_cryptoactor(void *ptr)
{
    struct ssh_decompress_s *decompress=(struct ssh_decompress_s *) ptr;
    struct ssh_receive_s *receive=(struct ssh_receive_s *) (((char *) decompress) - offsetof(struct ssh_receive_s, decompress));
    return ((receive->status & SSH_RECEIVE_STATUS_DISCONNECT) ? ENOTCONN : 0);
}

static void error_get_cryptoactor(unsigned int errcode, void *ptr)
{
    /* 20221119: log for now */

    logoutput_warning("error_get_cryptoactor: error %u - %s", errcode, strerror(errcode));
}

struct ssh_decompressor_s *get_decompressor(struct ssh_receive_s *receive, unsigned int *error)
{
    struct ssh_decompress_s *decompress=&receive->decompress;
    struct ssh_cryptoactor_s *ca=get_cryptoactor(&decompress->header, &receive->signal, receive->kexctr, &decompress->max_count, &decompress->count,
                                                    create_ssh_cryptoactor, interrupt_get_cryptoactor, error_get_cryptoactor, &fallback->common, (void *) decompress);
    return (struct ssh_decompressor_s *) ((char *) ca - offsetof(struct ssh_decompressor_s, common));
}

void remove_decompressors(struct ssh_decompress_s *decompress)
{
    remove_ssh_cryptoactor_list(&decompress->header);
}

void init_decompressors_once()
{
    init_ssh_decompressor(fallback, NULL, 0, 0);
}
