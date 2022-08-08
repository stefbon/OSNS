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

#include "osns-protocol.h"

#include "receive.h"
#include "query.h"
#include "osns/reply.h"
#include "osns/write.h"
#include "osns/utils.h"
#include "mountcmd.h"
#include "mount.h"
#include "setwatch-netcache.h"

#define OSNS_SYSTEMCONNECTION_BUFFER_SIZE		9192

static void set_connection_cap(struct osns_receive_s *r, unsigned int version, unsigned int services, unsigned int flags)
{

    /* set the parameters for this connection:
	- cap: what is requested by client and what does server offer
	- c_version: the version agreed by client and server */

    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));

    sc->version=version;
    sc->services=services;
    sc->flags=flags;

}

static void disconnect_osns_systemconnection(struct osns_systemconnection_s *sc)
{
    struct system_socket_s *sock=&sc->connection.sock;

    if (sock->event.type==SOCKET_EVENT_TYPE_BEVENT) {
	struct bevent_s *bevent=sock->event.link.bevent;

	remove_bevent_watch(bevent, BEVENT_REMOVE_FLAG_UNSET);

    }

    (* sock->sops.close)(sock);

    /* cleanup and finish */

    umount_all_fuse_filesystems(sc, sc->receive.signal);

}

static void process_msg_disconnect(struct osns_receive_s *r, uint32_t id, char *d, unsigned int l)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));

    disconnect_osns_systemconnection(sc);

}

/*
    receive a init request: version exchange
    uint32				version

    for version 1:

    uint32				flags requested capabilities

*/

static void process_msg_init(struct osns_receive_s *r, char *d, unsigned int l)
{
    struct osns_systemconnection_s *sc=NULL;

    if (l>=4) {
	unsigned int c_version=0;
	unsigned int pos=1;
	unsigned int versionmajor=0;
	unsigned int versionminor=0;

	c_version=get_uint32(&d[pos]);
	pos+=4;

	/* convert version send over into major:minor */

	versionmajor=get_osns_major(c_version);
	versionminor=get_osns_minor(c_version);

	logoutput_debug("process_msg_init: client send version %u:%u", versionmajor, versionminor);

	if (versionmajor==1) {

	    if (l>=8) {
		unsigned int c_requested=0;
		unsigned int result=0;

		/* SERVICES
		    20220122: only net services are supported (:DNSSD)
		    20220326: added mountinfo */

		c_requested=get_uint32((char *)(&d[pos]));
		pos+=4;
		logoutput_debug("process_msg_init: requested %u", c_requested);

		if ((c_requested & (OSNS_INIT_FLAG_NETCACHE | OSNS_INIT_FLAG_MOUNTINFO | OSNS_INIT_FLAG_MOUNT_NETWORK | OSNS_INIT_FLAG_MOUNT_DEVICES | OSNS_INIT_FLAG_SETWATCH_NETCACHE)) == 0) goto disconnect; /* client wants services this server cannot offer */

		result=(c_requested & (OSNS_INIT_FLAG_NETCACHE | OSNS_INIT_FLAG_MOUNTINFO | OSNS_INIT_FLAG_DNSLOOKUP | OSNS_INIT_FLAG_FILTER_MOUNTINFO | OSNS_INIT_FLAG_FILTER_NETCACHE | OSNS_INIT_FLAG_MOUNT | OSNS_INIT_FLAG_SETWATCH_NETCACHE));
		logoutput_debug("process_msg_init: requested %u supported %u", c_requested, result);

		set_connection_cap(r, c_version, result, 0);
		osns_reply_init(r, c_version, result);
		return;

	    }

	}

    }

    logoutput_debug("process_msg_init: disconnect");

    disconnect:
    sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
    disconnect_osns_systemconnection(sc);

}

static void process_msg_unimplemented(struct osns_receive_s *r, uint32_t id, char *d, unsigned int l)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
    /* does it need a reply ?
	what to do here ? */
    disconnect_osns_systemconnection(sc);
}

/*
    process data when received from client
    NOTE:
	- ctrl holds control data when supported
	    (20220406: no client uses this, sends control data to system ... ignore ... only: this osns system service sends control data to clients (fd))
*/

static void process_osns_data(struct osns_receive_s *r, char *data, unsigned int len, struct osns_control_s *ctrl)
{
    struct osns_systemconnection_s *sc=NULL;
    uint32_t id=0;
    unsigned char type=0;

    /* every packet is at least 5 bytes */

    if (len<5) {

	logoutput_debug("process_osns_data: message too short (size=%i)", len);
	goto disconnectLabel;

    }

    type=(unsigned char) data[0];

    logoutput_debug("process_osns_data: received message size=%u type=%u", len, type);

    if (type==OSNS_MSG_INIT) {

	/* INIT message, the first message, note the message is still here as it is (no memmove)
	    so the init/version data starts after the type */

	process_msg_init(r, data, len);
	return;

    }

    id=get_uint32(&data[1]);
    memmove(data, &data[5], len-5);
    len-=5;

    switch (type) {

	case OSNS_MSG_VERSION:

	    /* this is server ... receiving an version msg is an error -> disconnect */
	    goto disconnectLabel;
	    break;

	case OSNS_MSG_DISCONNECT:

	    goto disconnectLabel;
	    break;

	case OSNS_MSG_UNIMPLEMENTED:

	    process_msg_unimplemented(r, id, data, len);
	    break;

	case OSNS_MSG_OPENQUERY:

	    process_msg_openquery(r, id, data, len);
	    break;

	case OSNS_MSG_READQUERY:

	    process_msg_readquery(r, id, data, len);
	    break;

	case OSNS_MSG_CLOSEQUERY:

	    process_msg_closequery(r, id, data, len);
	    break;

	case OSNS_MSG_MOUNTCMD:

	    process_msg_mountcmd(r, id, data, len);
	    break;

	case OSNS_MSG_UMOUNTCMD:

	    process_msg_umountcmd(r, id, data, len);
	    break;

	case OSNS_MSG_SETWATCH:

	    process_msg_setwatch(r, id, data, len);
	    break;

	default:

	    osns_reply_status(r, id, OSNS_MSG_UNIMPLEMENTED, NULL, 0);
	    break;

    }

    return;

    disconnectLabel:
    sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
    disconnect_osns_systemconnection(sc);

}

static void osns_handle_error(struct connection_s *c, struct generic_error_s *error)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)c - offsetof(struct osns_systemconnection_s, connection));

    /* if error is nit set get socket options */

    /* TODO:
	- add parameter on which level this error happened:
	    eventloop, reading or protocol ? */

}

static void osns_handle_close(struct connection_s *c, unsigned char remote)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)c - offsetof(struct osns_systemconnection_s, connection));
    logoutput_debug("osns_handle_close");
    disconnect_osns_systemconnection(sc);
}

static void osns_handle_dataavail(struct connection_s *c)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)c - offsetof(struct osns_systemconnection_s, connection));

    logoutput_debug("osns_handle_dataavail");
    osns_read_available_data(&sc->receive);
}

static int osns_send_data(struct osns_receive_s *r, char *data, unsigned int len, int (* send_cb)(struct system_socket_s *sock, char *data, unsigned int size, void *ptr), void *ptr)
{
    struct osns_systemconnection_s *sc=(struct osns_systemconnection_s *)((char *)r - offsetof(struct osns_systemconnection_s, receive));
    return write_osns_socket(&sc->connection.sock, data, len, send_cb, ptr);
}

struct connection_s *accept_connection_from_localsocket(struct connection_s *c_conn, struct connection_s *s_conn)
{
    struct connection_s *c=NULL;
    struct osns_systemconnection_s *sc=NULL;
    unsigned int size=sizeof(struct osns_systemconnection_s) + OSNS_SYSTEMCONNECTION_BUFFER_SIZE;  /* plus read buffer */

    logoutput("accept_connection_from_localsocket");

    /* 20220130 TODO:
    - add a check the client is permitted to connect ... who is connecting is in peer */

    /* allocate a new connection to local client adding extra buffer */

    sc=malloc(size);

    if (sc) {

	c=&sc->connection;
	memset(sc, 0, size);
	memcpy(c, c_conn, sizeof(struct connection_s));

	sc->status=0;
	sc->version=0;
	sc->services=0;
	sc->flags=0;

	init_list_header(&sc->mounts, SIMPLE_LIST_TYPE_EMPTY, NULL);

	c->ops.client.disconnect=osns_handle_close;
	c->ops.client.error=osns_handle_error;
	c->ops.client.dataavail=osns_handle_dataavail;

	sc->size=OSNS_SYSTEMCONNECTION_BUFFER_SIZE;

	sc->receive.status=0;
	sc->receive.ptr=(void *) c;
	sc->receive.signal=get_default_shared_signal();
	sc->receive.process_data=process_osns_data;
	sc->receive.send=osns_send_data;

	sc->receive.read=0;
	sc->receive.size=sc->size;
	sc->receive.threads=0;
	sc->receive.buffer=sc->buffer;

    }

    return c;
}

void clear_systemconnection(struct osns_systemconnection_s *sc)
{
}
