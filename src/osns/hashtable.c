/*

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
#include "libosns-network.h"
#include "libosns-misc.h"
#include "libosns-list.h"
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-eventloop.h"
#include "libosns-lock.h"
#include "libosns-connection.h"

#include "osns/receive.h"
#include "receive.h"

/* hashtable to lookup original request using id */

#define HASHTABLE_SIZE					32

#define HASHTABLE_STATUS_MSG_ID				1 /* bit set when getting a new id */
#define HASHTABLE_STATUS_INIT				2 /* bit set after hastable is initialized */

static struct list_header_s hashtable[HASHTABLE_SIZE];
static unsigned int hashtable_status=0;
static uint32_t msg_id=0;
static struct osns_packet_s dummy_packet;

void init_osns_client_hashtable()
{
    for (unsigned int i=0; i<HASHTABLE_SIZE; i++) init_list_header(&hashtable[i], SIMPLE_LIST_TYPE_EMPTY, NULL);
    init_osns_packet(&dummy_packet);
}

uint32_t get_osns_msg_id(struct osns_receive_s *r)
{
    uint32_t id=0;

    signal_lock_flag(r->signal, &hashtable_status, HASHTABLE_STATUS_MSG_ID);
    id=msg_id;
    msg_id++;
    signal_unlock_flag(r->signal, &hashtable_status, HASHTABLE_STATUS_MSG_ID);

    return  id;
}

void hash_osns_packet(struct osns_receive_s *r, struct osns_packet_s *packet)
{
    unsigned int hashvalue = 0;
    struct list_header_s *header=NULL;

    hashvalue = (packet->id % HASHTABLE_SIZE);
    header=&hashtable[hashvalue];

    /* wait to be able to write this header */
    write_lock_list_header(header);
    add_list_element_last(header, &packet->list);
    write_unlock_list_header(header);

}

void unhash_osns_packet(struct osns_receive_s *r, struct osns_packet_s *packet)
{
    unsigned int hashvalue = 0;
    struct list_header_s *header=NULL;

    hashvalue = (packet->id % HASHTABLE_SIZE);
    header=&hashtable[hashvalue];

    /* wait to be able to write this header */
    write_lock_list_header(header);
    remove_list_element(&packet->list);
    write_unlock_list_header(header);

}

void process_osns_reply(struct osns_receive_s *r, unsigned char type, uint32_t id, char *data, unsigned int len, struct osns_control_s *ctrl)
{
    struct osns_packet_s *packet=&dummy_packet;
    unsigned int hashvalue = (id % HASHTABLE_SIZE);
    struct list_header_s *header=&hashtable[hashvalue];
    struct list_element_s *list=NULL;

    logoutput_debug("process_osns_reply: id %u", id);

    read_lock_list_header(header);
    list=get_list_head(header, 0);

    while (list) {

	packet=(struct osns_packet_s *)((char *)list - offsetof(struct osns_packet_s, list));
	if (packet->id==id) {

	    signal_set_flag(r->signal, &packet->status, OSNS_PACKET_STATUS_RESPONSE);
	    break;

	}

	list=get_next_element(list);
	packet=&dummy_packet; /* fallback to the "not found" dummy packet */

    }

    read_unlock_list_header(header);

    /* run cb -> is the cb for the packet when a packet is found ...
	this does nothing when no packet is found
	note the package is the dummy packet when not found, so never NULL */

    (* packet->cb)(packet, type, data, len, ctrl);
}

void wait_osns_packet(struct osns_receive_s *r, struct osns_packet_s *packet)
{
    struct shared_signal_s *signal=r->signal;
    unsigned char found=0;
    struct system_timespec_s expire;
    int result=0;

    logoutput_debug("wait_osns_packet");

    get_current_time_system_time(&expire);
    system_time_add(&expire, SYSTEM_TIME_ADD_ZERO, 20);

    /* add packet to hashtable */
    hash_osns_packet(r, packet);

    signal_lock(signal);

    startcheckwaitLabel:
    logoutput_debug("wait_osns_packet: startcheckwait (id=%u, status=%u)", packet->id, packet->status);

    if (packet->status & OSNS_PACKET_STATUS_FINISH) {

	signal_unlock(signal);
	if ((packet->status & OSNS_PACKET_STATUS_PERMANENT)==0) unhash_osns_packet(r, packet);
	return;

    } else if (r->status & OSNS_RECEIVE_STATUS_DISCONNECT) {

	signal_unlock(signal);
	unhash_osns_packet(r, packet);
	signal_set_flag(r->signal, &packet->status, OSNS_PACKET_STATUS_ERROR);
	return;

    }

    result=signal_condtimedwait(signal, &expire);

    if (result>0) {
	unsigned int toset=OSNS_PACKET_STATUS_ERROR;

	if (result==ETIMEDOUT) {

	    if ((packet->status & OSNS_PACKET_STATUS_RESPONSE) && (packet->status & OSNS_PACKET_STATUS_ERROR)==0) {

		/* there is already a response, wait still a bit for the cb to finish */
		get_current_time_system_time(&expire);
		system_time_add(&expire, SYSTEM_TIME_ADD_MICRO, 4);
		goto startcheckwaitLabel;

	    }

	    toset=OSNS_PACKET_STATUS_TIMEDOUT;

	}

	signal_unlock(signal);
	unhash_osns_packet(r, packet);
	signal_set_flag(r->signal, &packet->status, toset);
	return;

    } else if ((r->status & OSNS_RECEIVE_STATUS_DISCONNECT) || (packet->status & OSNS_PACKET_STATUS_ERROR)) {

	signal_unlock(signal);
	unhash_osns_packet(r, packet);
	signal_set_flag(r->signal, &packet->status, OSNS_PACKET_STATUS_ERROR);
	return;

    } else {

	goto startcheckwaitLabel;

    }

    signal_unlock(signal);

}

void remove_permanent_packet(uint32_t id)
{
    struct osns_packet_s *packet=NULL;
    unsigned int hashvalue = (id % HASHTABLE_SIZE);
    struct list_header_s *header=&hashtable[hashvalue];
    struct list_element_s *list=NULL;

    logoutput_debug("remove_permanent_packet: id %u", id);

    write_lock_list_header(header);
    list=get_list_head(header, 0);

    while (list) {

	packet=(struct osns_packet_s *)((char *)list - offsetof(struct osns_packet_s, list));
	if (packet->id==id) {

	    remove_list_element(&packet->list);
	    free(packet);
	    break;

	}

	list=get_next_element(list);
	packet=NULL;

    }

    write_unlock_list_header(header);

}
