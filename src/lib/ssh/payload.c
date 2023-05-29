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

void set_msg_buffer_payload(struct msg_buffer_s *mb, struct ssh_payload_s *p)
{
    set_msg_buffer(mb, p->buffer, p->size);
}

struct ssh_payload_s *realloc_payload_static(struct ssh_payload_s *payload, unsigned int size)
{
    return (struct ssh_payload_s *) malloc(sizeof(struct ssh_payload_s) + size);
}

struct ssh_payload_s *realloc_payload_dynamic(struct ssh_payload_s *payload, unsigned int size)
{
    return (struct ssh_payload_s *) realloc((void *)payload, sizeof(struct ssh_payload_s) + size);
}

char *isolate_payload_buffer_dynamic(struct ssh_payload_s **p_payload, unsigned int pos, unsigned int size)
{
    char *buffer=NULL;
    struct ssh_payload_s *payload=*p_payload;

    if (pos + size <= payload->size) {

	buffer=(char *) payload;
	memmove(buffer, &payload->buffer[pos], size);
	buffer=realloc(buffer, size);
	*p_payload=NULL;

    }

    return buffer;

}

char *isolate_payload_buffer_static(struct ssh_payload_s **p_payload, unsigned int pos, unsigned int size)
{
    char *buffer=NULL;
    struct ssh_payload_s *payload=*p_payload;

    if (pos + size <= payload->size) {

	buffer=malloc(size);
	if (buffer) memmove(buffer, &payload->buffer[pos], size);
	*p_payload=NULL;

    }

    return buffer;

}

void free_payload(struct ssh_payload_s **p)
{
    struct ssh_payload_s *payload=*p;
    free(payload);
    *p=NULL;
}

void init_ssh_payload(struct ssh_payload_s *p, unsigned int size)
{
    memset(p, 0, (sizeof(struct ssh_payload_s) + size));
    init_list_element(&p->list, NULL);
    p->size=size;
}

static int _cb_select_payload_default(struct ssh_payload_s *payload, void *ptr)
{
    return 1; /* default select everything */
}

static unsigned char _cb_break_default(void *ptr)
{
    return 0; /* default do not stop */
}

static void _cb_error_default(unsigned int e, void *ptr)
{}

/* function to test the payload on the queue (there maybe more than one!) is the requested one
    to test it's the one the calling process is waiting for the cb is used */

static int select_requested_payload(struct list_header_s *h, int (* cb_select)(struct ssh_payload_s *payload, void *ptr), void *ptr, struct ssh_payload_s **p_payload)
{
    struct ssh_payload_s *payload=NULL;
    struct list_element_s *list=NULL;
    int result=0;

    write_lock_list_header(h);
    list=get_list_head(h);

    while (list) {

	payload=(struct ssh_payload_s *)(((char *) list) - offsetof(struct ssh_payload_s, list));

	/* test it's the one looking for */

	result=cb_select(payload, ptr);

        if ((result==1) || (result==-1)) {

	    if (result==1) remove_list_element(list);
	    *p_payload=payload;
	    break;

	}

	list=get_next_element(list);
	payload=NULL;

    }

    write_unlock_list_header(h);
    return result;
}
/*
	}  else if ((r->sequence_error.errcode>0) && seq && (*seq==r->sequence_error.sequence_number_error)) {

	    set_generic_error_system(error, r->sequence_error.errcode, NULL);
	    receive->sequence_error.sequence_number_error=0;
	    receive->sequence_error.errcode=0;
	    break;

	}
*/



/*  common function to wait for a ssh_payload to arrive
    on a queue; this queue can be the transport queue used to setup a connection
    or the queue to handle key reexchange */

struct ssh_payload_s *get_ssh_payload(struct payload_queue_s *queue, struct system_timespec_s *expire,
                    int (* cb_select)(struct ssh_payload_s *payload, void *ptr), unsigned char (* cb_break)(void *ptr), void (* cb_error)(unsigned int errcode, void *ptr), void *ptr)
{
    struct ssh_payload_s *payload=NULL;
    struct shared_signal_s *signal=queue->signal;
    struct list_header_s *h=&queue->header;

    /* if no cb is defined use the default: select any message */
    if (cb_select==NULL) cb_select=_cb_select_payload_default;
    if (cb_break==NULL) cb_break=_cb_break_default;
    if (cb_error==NULL) cb_error=_cb_error_default;

    signal_lock(signal);

    while (payload==NULL) {

        int result=select_requested_payload(h, cb_select, ptr, &payload);

        if ((result==1) || (result==-1)) {

	    logoutput_debug("get_ssh_payload: selected payload %u", payload->type);
	    break;

        } else if ((* cb_break)(ptr)==1) {

            logoutput_debug("get_ssh_payload: finish");
	    break;

        }

	payload=NULL;
	result=signal_condtimedwait(signal, expire);
	if (result==ETIMEDOUT) {

            (* cb_error)(ETIMEDOUT, ptr);
            break;

        }

    }

    signal_broadcast(signal);
    signal_unlock(signal);
    return payload;

}

/* queue a new payload */

void queue_ssh_payload(struct payload_queue_s *queue, struct ssh_payload_s *payload)
{
    struct list_header_s *h=&queue->header;

    write_lock_list_header(h);
    add_list_element_last(h, &payload->list);
    write_unlock_list_header(h);
}

void queue_ssh_broadcast(struct payload_queue_s *queue)
{
    struct shared_signal_s *signal=queue->signal;

    signal_lock(signal);
    signal_broadcast(signal);
    signal_unlock(signal);
}

void init_payload_queue(struct shared_signal_s *signal, struct payload_queue_s *queue)
{
    init_list_header(&queue->header, SIMPLE_LIST_TYPE_EMPTY, NULL);
    queue->signal=signal;
    queue->ptr=NULL;
}

void clear_payload_queue(struct payload_queue_s *queue, unsigned char dolog)
{

    if (queue) {
        struct list_element_s *list=NULL;

        logoutput("clear_payload_queue");

        getpayload:

        list=remove_list_head(&queue->header);

        if (list) {
	    struct ssh_payload_s *payload=(struct ssh_payload_s *)(((char *) list) - offsetof(struct ssh_payload_s, list));

	    if (dolog) logoutput("clear_payload_queue: found payload type %i size %i", payload->type, payload->len);
	    free_payload(&payload);
	    goto getpayload;

        }

    }

}
