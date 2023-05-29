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
#include "ssh-receive.h"

static unsigned int populate_decompress(struct ssh_connection_s *connection, struct decompress_ops_s *ops, struct algo_list_s *alist, unsigned int start)
{

    if (alist) {

	alist[start].type=SSH_ALGO_TYPE_COMPRESS_S2C;
	alist[start].order=SSH_ALGO_ORDER_LOW;
	alist[start].sshname="none";
	alist[start].libname="none";
	alist[start].ptr=(void *)ops;

    }

    start++;

    return start;

}

static unsigned int get_handle_size(struct ssh_decompress_s *d)
{
    return 0;
}

static struct ssh_payload_s *decompress_packet(struct ssh_decompressor_s *d, struct ssh_packet_s *packet, unsigned int *errcode)
{
    unsigned int len = packet->len - 1 - packet->padding; /* length of the payload */
    struct ssh_payload_s *payload=malloc(sizeof(struct ssh_payload_s) + len);

    if (payload) {

        payload->sequence=packet->sequence;
        init_list_element(&payload->list, NULL);
        payload->size=len;
        payload->len=len;
        memcpy(payload->buffer, &packet->buffer[5], len);
        payload->type=(unsigned char) payload->buffer[0];
        logoutput_debug("decompress_packet_: allocated payload (sequence=%u type=%u)", packet->sequence, payload->type);
        return payload;

    }

    logoutput_debug("decompress_packet_: unable to allocate payload (len=%u sequence=%u type=%u)", len, packet->sequence, packet->buffer[5]);
    *errcode=ENOMEM;
    return NULL;

}

static void clear_decompressor(struct ssh_decompressor_s *d)
{
}

static int init_decompressor(struct ssh_decompressor_s *d)
{
    d->decompress_packet	= decompress_packet;
    d->clear			= clear_decompressor;
    return 0;
}

static struct decompress_ops_s none_d_ops = {
    .name			= "none",
    .populate			= populate_decompress,
    .get_handle_size		= get_handle_size,
    .init		        = init_decompressor,
    .list			= {NULL, NULL},
};

void init_decompress_none()
{
    add_decompress_ops(&none_d_ops);
}

void set_decompress_none(struct ssh_connection_s *connection)
{
    struct ssh_decompress_s *decompress=&connection->receive.decompress;
    decompress->ops=&none_d_ops;
}
