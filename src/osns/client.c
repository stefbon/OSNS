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

#include "osns-protocol.h"
#include "osns/write.h"
#include "osns/receive.h"

#include "common.h"
#include "receive.h"
#include "hashtable.h"
#include "send.h"
#include "utils.h"

void disconnect_osns_connection(struct osns_connection_s *oc)
{
    struct system_socket_s *sock=&oc->connection.sock;

    if (sock->event.type==SOCKET_EVENT_TYPE_BEVENT) {
	struct bevent_s *bevent=sock->event.link.bevent;

	remove_bevent_watch(bevent, BEVENT_REMOVE_FLAG_UNSET);
	free_bevent(&bevent);
	sock->event.link.bevent=0;
	sock->event.type=0;

    }

    (* sock->sops.close)(sock);
}

/*
    receive a init request: version exchange

    byte				OSNS_MSG_VERION
    uint32				version
    uint32				services - mask of services the servers supports
    uint32				flags - flags of services
*/

static void process_msg_version(struct osns_receive_s *r, char *data, unsigned int len)
{
    struct osns_connection_s *client=(struct osns_connection_s *)((char *)r - offsetof(struct osns_connection_s, receive));

    logoutput_debug("process_msg_version: len message %u", len);

    if (len>=5) {
	unsigned int c_version=0;
	unsigned int pos=1;
	unsigned int versionmajor=0;
	unsigned int versionminor=0;

	c_version=get_uint32(&data[pos]);
	pos+=4;
	versionmajor=get_osns_major(c_version);
	versionminor=get_osns_minor(c_version);

	logoutput_debug("process_msg_version: received version %u:%u", versionmajor, versionminor);

	if (c_version==0) {

	    /* illegal version or version has been set before */
	    logoutput_debug("process_msg_version: error ... received version zero or already set");
	    goto disconnect;

	}

	client->protocol.version=c_version;

	if (versionmajor==1) {

	    if (len>=9) {
		unsigned int osns_flags=0;

		osns_flags=get_uint32(&data[pos]);
		pos+=4;

		/* a security/sanity check here ?? */

		client->protocol.level.one.flags=osns_flags;

		/* signal waiting threads version has arrived and set */
		signal_set_flag(r->signal, &client->status, OSNS_CONNECTION_STATUS_VERSION);
		return;

	    }

	} else {

	    logoutput_warning("process_msg_version: version %i not supported", c_version);

	}

    }

    disconnect:
    disconnect_osns_connection(client);

}

void osns_client_handle_close(struct connection_s *c, unsigned char remote)
{
    struct osns_connection_s *client=(struct osns_connection_s *)((char *)c - offsetof(struct osns_connection_s, connection));

    logoutput_debug("osnsctl_handle_close");
    disconnect_osns_connection(client);
}

void osns_client_handle_error(struct connection_s *c, struct generic_error_s *e)
{
    struct osns_connection_s *client=(struct osns_connection_s *)((char *)c - offsetof(struct osns_connection_s, connection));

    /* TODO */

}

void osns_client_handle_dataavail(struct connection_s *conn)
{
    struct osns_connection_s *client=(struct osns_connection_s *)((char *)conn - offsetof(struct osns_connection_s, connection));
    osns_read_available_data(&client->receive);
}

int osns_client_send_data(struct osns_receive_s *r, char *data, unsigned int len, int (* send_cb)(struct system_socket_s *sock, char *data, unsigned int size, void *ptr), void *ptr)
{
    struct osns_connection_s *client=(struct osns_connection_s *)((char *)r - offsetof(struct osns_connection_s, receive));
    return write_osns_socket(&client->connection.sock, data, len, send_cb, ptr);
}

void osns_client_process_data(struct osns_receive_s *r, char *data, unsigned int len, struct osns_control_s *ctrl)
{
    struct osns_connection_s *client=NULL;
    struct osns_packet_s *packet=NULL;
    uint32_t id=0;
    unsigned char type=0;

    if (len<5) {

	logoutput_debug("osns_client_process_data: len %u too short", len);
	goto disconnect;

    }

    type=(unsigned char) data[0];
    logoutput_debug("osns_client_process_data: type %u len %u", (unsigned int) type, len);

    if (type==OSNS_MSG_VERSION) {

	process_msg_version(r, data, len);
	return;

    }

    id=get_uint32(&data[1]);
    memmove(data, &data[5], len-5);
    len-=5;
    process_osns_reply(r, type, id, data, len, ctrl);
    return;

    disconnect:
    client=(struct osns_connection_s *)((char *)r - offsetof(struct osns_connection_s, receive));
    disconnect_osns_connection(client);

}
