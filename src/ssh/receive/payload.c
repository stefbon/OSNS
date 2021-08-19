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
#include <sys/stat.h>

#include "main.h"
#include "log.h"
#include "threads.h"

#include "misc.h"

#include "ssh-common.h"
#include "ssh-common-protocol.h"
#include "ssh-connections.h"
#include "ssh-receive.h"
#include "ssh-utils.h"
#include "ssh-signal.h"

static int cb_payload_default(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr)
{
    return 0;
}

/* function to test the payload on the queue (there maybe more than one!) is the requested one
    to test it's the one the calling process is waiting for the cb is used */

static struct ssh_payload_s *select_requested_payload(struct ssh_connection_s *connection, struct list_header_s *header, int (* cb)(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr), void *ptr)
{
    struct list_element_s *list=get_list_head(header, 0);

    checklistelement:

    while (list) {
	struct ssh_payload_s *payload=(struct ssh_payload_s *)(((char *) list) - offsetof(struct ssh_payload_s, list));

	/* test it's the one looking for */

	if (cb(connection, payload, ptr)==0) {

	    remove_list_element(list);
	    return payload;

	}

	list=get_next_element(list);

    }

    return NULL;
}

/*  common function to wait for a ssh_payload to arrive
    on a queue; this queue can be the transport queue used to setup a connection
    or the queue to handle key reexchange */

struct ssh_payload_s *get_ssh_payload(struct ssh_connection_s *connection, struct payload_queue_s *queue, struct timespec *expire, uint32_t *seq, int (* cb)(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr), void *ptr, struct generic_error_s *error)
{
    struct ssh_payload_s *payload=NULL;
    struct ssh_signal_s *signal=queue->signal;
    struct list_element_s *list=NULL;
    struct list_header_s *header=&queue->header;

    /* if no cb is defined use the default: select any message */
    if (cb==NULL) cb=cb_payload_default;

    ssh_signal_lock(signal);

    while ((connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT)==0 && (payload=select_requested_payload(connection, header, cb, ptr))==NULL) {

	int result=ssh_signal_condtimedwait(signal, expire);

	if ((payload=select_requested_payload(connection, header, cb, ptr))) {

	    break;

	} else if (result==ETIMEDOUT) {
	    struct fs_connection_s *conn=&connection->connection;

	    ssh_signal_unlock(signal);
	    set_generic_error_system(error, ETIMEDOUT, NULL);

	    /* is there a better error causing this timeout?
		the timeout is possibly caused by connection problems */

	    if (conn->status & FS_CONNECTION_FLAG_DISCONNECT) set_generic_error_system(error, ((conn->error) ? conn->error : ENOTCONN), NULL);
	    return NULL;

	} else if (signal->error>0 && (seq && *seq==signal->sequence_number_error)) {

	    ssh_signal_unlock(signal);
	    set_generic_error_system(error, signal->error, NULL);
	    signal->sequence_number_error=0;
	    signal->error=0;
	    return NULL;

	} else if (connection->setup.flags & SSH_SETUP_FLAG_DISCONNECT) {

	    ssh_signal_unlock(signal);
	    set_generic_error_system(error, ENOTCONN, NULL);
	    return NULL;

	}

    }

    ssh_signal_broadcast(signal);
    ssh_signal_unlock(signal);
    return payload;

}

    /*
	queue a new payload when a packet is found in the buffer
	size:
	header: sizeof(struct ssh_payload)
	buffer: len buffer = packet->len - packet->padding - 1

	remember data coming from a ssh server looks like:
	uint32				packet_len
	byte				padding_len (=n2)
	byte[n1]			payload (n1 = packet_len - padding_len - 1)
	byte[n2]			padded bytes, filled with random
	byte[m]				mac (m = mac_len)

	when here mac and encryption are already processed, the payload is still compressed

	NOTE:
	- first byte of payload (buffer[5]) is type of ssh message
	- if compression is used the payload is still compressed

    */

void queue_ssh_payload_locked(struct payload_queue_s *queue, struct ssh_payload_s *payload)
{
    struct ssh_signal_s *signal=queue->signal;

    add_list_element_last(&queue->header, &payload->list);
    ssh_signal_broadcast(signal);

}

void queue_ssh_payload(struct payload_queue_s *queue, struct ssh_payload_s *payload)
{
    struct ssh_signal_s *signal=queue->signal;

    ssh_signal_lock(signal);
    queue_ssh_payload_locked(queue, payload);
    ssh_signal_unlock(signal);

}

void init_payload_queue(struct ssh_connection_s *connection, struct payload_queue_s *queue)
{
    init_list_header(&queue->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    queue->signal=&connection->receive.signal;
    queue->ptr=NULL;
}

void clear_payload_queue(struct payload_queue_s *queue, unsigned char dolog)
{
    struct list_element_s *list=NULL;

    logoutput("clear_payload_queue");
    if (queue==NULL) return;

    getpayload:

    list=get_list_head(&queue->header, SIMPLE_LIST_FLAG_REMOVE);

    if (list) {
	struct ssh_payload_s *payload=(struct ssh_payload_s *)(((char *) list) - offsetof(struct ssh_payload_s, list));

	if (dolog) logoutput("clear_payload_queue: found payload type %i size %i", payload->type, payload->len);
	free_payload(&payload);
	goto getpayload;

    }

}

struct ssh_payload_s *receive_message_common(struct ssh_connection_s *connection, int (* cb)(struct ssh_connection_s *connection, struct ssh_payload_s *payload, void *ptr), void *ptr, struct generic_error_s *error)
{
    struct timespec expire;
    uint32_t seq=0;

    get_ssh_connection_expire_init(connection, &expire);
    return get_ssh_payload(connection, &connection->setup.queue, &expire, &seq, cb, ptr, error);
}
