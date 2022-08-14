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
#include "libosns-network.h"

#include "osns-protocol.h"
#include "common.h"
#include "receive.h"
#include "send.h"
#include "hashtable.h"

static void cb_mountcmd_response(struct osns_packet_s *p, unsigned char type, char *data, unsigned int size, struct osns_control_s *ctrl)
{
    struct osns_receive_s *r=p->r;

    logoutput_debug("cb_mountcmd_response: id %u type=%u size=%u", p->id, type, size);

    p->reply=type;

    if (type==OSNS_MSG_MOUNTED) {

	if (ctrl->type==OSNS_CONTROL_TYPE_FD) {
	    struct osns_socket_s *sock=(struct osns_socket_s *) p->ptr;
	    struct osns_control_info_s info;
	    unsigned int pos=0;

	    /* control info is optional */

	    if (size>=2) {

		memset(&info, 0, sizeof(struct osns_control_info_s));
		pos += read_osns_control_info(data, size, &info);

	    }

	    if (pos<=2) {

		info.info.osns_socket.type=OSNS_SOCKET_TYPE_DEVICE;
		info.info.osns_socket.flags=(OSNS_SOCKET_FLAG_CHAR_DEVICE | OSNS_SOCKET_FLAG_RDWR);

	    }

	    logoutput("cb_mountcmd_response: received fd %i socket type %u flags %u", ctrl->data.fd, info.info.osns_socket.type, info.info.osns_socket.flags);
	    // init_osns_socket(sock, info.info.osns_socket.type, info.info.osns_socket.flags);
	    (* sock->set_unix_fd)(sock, ctrl->data.fd);
	    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	    return;

	}

    } else if (type==OSNS_MSG_STATUS) {

	if (size>=4) {
	    unsigned int status=get_uint32(data);
	    logoutput_debug("cb_openquery_response: status %u", status);
	    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_FINISH);
	    return;

	}

    }

    signal_set_flag(r->signal, &p->status, OSNS_PACKET_STATUS_ERROR);

}

int process_mountcmd(struct osns_connection_s *oc, unsigned char type, unsigned int maxread, struct osns_socket_s *sock)
{
    struct osns_receive_s *r=&oc->receive;
    struct osns_packet_s packet;
    int result=-1;

    init_osns_packet(&packet);

    packet.cb=cb_mountcmd_response;
    packet.id=get_osns_msg_id(r);
    packet.r=r;
    packet.ptr=(void *) sock;

    logoutput_debug("process_mountcmd: send mountcmd (id=%u, type=%u)", packet.id, type);

    /* for now send a default maximum readsize */

    if (send_osns_msg_mountcmd(r, packet.id, type, maxread)>0) {

	packet.reply=0;
	wait_osns_packet(r, &packet);

	if (packet.status & OSNS_PACKET_STATUS_FINISH) {

	    result=0;

	    if (packet.reply == OSNS_MSG_MOUNTED) {

		logoutput_debug("process_mountcmd: received mounted");
		result=1;

	    } else if (packet.reply==OSNS_MSG_STATUS) {

		logoutput_debug("process_mountcmd: received a status");

	    } else {

		logoutput_debug("process_mountcmd: received a %i message", packet.reply);
		goto out;

	    }

	} else if (packet.status & OSNS_PACKET_STATUS_TIMEDOUT) {

	    logoutput_debug("process_mountcmd: timedout");

	} else if (packet.status & OSNS_PACKET_STATUS_ERROR) {

	    logoutput_debug("process_mountcmd: error");

	} else {

	    logoutput_debug("process_mountcmd: failed, unknown reason");

	}

    } else {

	logoutput("process_mountcmd: unable to send mountcmd");
	goto out;

    }

    out:
    logoutput("process_mountcmd: finish");
    return result;

}
