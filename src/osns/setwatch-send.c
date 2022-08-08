/*
  2010, 2011, 2012, 2103, 2014, 2015, 2016, 2017 Stef Bon <stefbon@gmail.com>

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
#include "libosns-datatypes.h"
#include "libosns-threads.h"
#include "libosns-network.h"

#include "osns-protocol.h"
#include "osns_client.h"

#include "osns/send.h"
#include "hashtable.h"

/* osns setwatch cb */

struct _setwatch_cb_data_s {
    unsigned int status;
    void (* cb)(char *data, unsigned int size, void *ptr);
    void *ptr;
};


static void cb_event_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct osns_receive_s *r=p->r;
    struct _setwatch_cb_data_s *setwatch=(struct _setwatch_cb_data_s *) p->ptr;

    if (type==OSNS_MSG_EVENT) {
	unsigned int pos=0;
	unsigned int count=0;

	count=get_uint32(&data[pos]);
	pos+=4;

	for (unsigned int i=0; i<count; i++) {
	    unsigned int len=0;

	    len=get_uint16(&data[pos]);
	    pos+=2;

	    if (size >= (pos + len)) {

		(* setwatch->cb)(&data[pos], len, setwatch->ptr);
		pos+=len;

	    } else {

		logoutput_debug("cb_event_response: invalid format event data");

	    }

	}

    } else {

	logoutput_debug("cb_event_response: received an invalid %u packet", type);

    }

}

static void cb_setwatch_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct osns_receive_s *r=p->r;
    struct _setwatch_cb_data_s *setwatch=(struct _setwatch_cb_data_s *) p->ptr;

    logoutput_debug("cb_setwatch_response: id %u type=%u size=%u", p->id, type, size);

    p->reply=type;

    if (type==OSNS_MSG_STATUS) {

	if (size>=4) {

	    setwatch->status=get_uint32(data);
	    p->cb=cb_event_response;
	    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	    return;

	}

    }

    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_ERROR);

}

int osns_system_setwatch(struct osns_connection_s *oc, struct name_string_s *cmd, void (* cb)(char *data, unsigned int size, void *ptr))
{
    struct osns_receive_s *r=&oc->receive;
    struct osns_packet_s *packet=NULL;
    struct _setwatch_cb_data_s *setwatch=NULL;
    int result=-1;

    packet=malloc(sizeof(struct osns_packet_s));
    setwatch=malloc(sizeof(struct _setwatch_cb_data_s));
    if (packet==NULL || setwatch==NULL) {

	logoutput_debug("osns_system_setwatch: unable to allocate packet");
	if (packet) free(packet);
	if (setwatch) free(setwatch);
	goto out;

    }

    init_osns_packet(packet);

    packet->cb=cb_setwatch_response;
    packet->id=get_osns_msg_id(r);
    packet->status |= OSNS_PACKET_STATUS_PERMANENT; /* make it permanent ... so watch event can still refer to this packet */
    packet->r=r;

    logoutput_debug("osns_system_setwatch: send setwatch (id=%u)", packet->id);

    if (send_osns_msg_setwatch(r, packet->id, cmd)>0) {

	setwatch->status=0;
	setwatch->cb=cb;
	packet->ptr=(void *) setwatch;
	packet->reply=0;

	wait_osns_packet(r, packet);

	if (packet->status & OSNS_PACKET_STATUS_FINISH) {

	    if (packet->reply==OSNS_MSG_STATUS) {

		if (setwatch->status==OSNS_STATUS_OK) {

		    logoutput_debug("osns_system_setwatch: success");
		    result=(int) packet->id;

		} else {

		    logoutput_debug("osns_system_setwatch: received a status %i", setwatch->status);

		}

	    } else {

		logoutput_debug("osns_system_setwatch: received a %i message", packet->reply);
		goto out;

	    }

	} else {

	    if (packet->status & OSNS_PACKET_STATUS_TIMEDOUT) {

		logoutput_debug("osns_system_setwatch: timedout");

	    } else if (packet->status & OSNS_PACKET_STATUS_ERROR) {

		logoutput_debug("osns_system_setwatch: error");

	    } else {

		logoutput_debug("osns_system_setwatch: failed, unknown reason");

	    }


	}

    } else {

	logoutput("osns_system_setwatch: unable to send open query");

    }

    out:
    logoutput("osns_system_setwatch: finish");

    if (result<0) {

	if (packet) {

	    unhash_osns_packet(r, packet);
	    free(packet);

	}

	if (setwatch) free(setwatch);

    }

    return result;

}

static void cb_rmwatch_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct osns_receive_s *r=p->r;

    logoutput_debug("cb_rmwatch_response: id %u type=%u size=%u", p->id, type, size);

    p->reply=type;

    if (type==OSNS_MSG_STATUS) {

	if (size>=4) {

	    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	    return;

	}

    }

    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_ERROR);

}

void osns_system_rmwatch(struct osns_connection_s *oc, uint32_t watchid)
{
    struct osns_receive_s *r=&oc->receive;
    struct osns_packet_s packet;

    init_osns_packet(&packet);

    packet.cb=cb_rmwatch_response;
    packet.id=get_osns_msg_id(r);
    packet.status=0;

    logoutput_debug("osns_system_rmwatch: send rmwatch");

    if (send_osns_msg_rmwatch(r, packet.id, watchid)>0) {

	wait_osns_packet(r, &packet);

    } else {

	logoutput("osns_system_rmwatch: unable to send rmwatch");

    }

    remove_permanent_packet(watchid);

}
